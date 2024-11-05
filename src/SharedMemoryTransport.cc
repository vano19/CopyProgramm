#include "SharedMemoryTransport.h"

#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>

#include <stdexcept>
#include <iostream>
#include <thread>
#include <chrono>

namespace cp {
    
    using namespace boost::interprocess;
    
    constexpr std::string namedMutex = "shm_init_mutex";

    SharedMemoryTransport::SharedMemoryTransport(std::string_view name)
        : sharedMemoryName_(name)
        , strategy_(EStrategy::E_Read)
        , segment_(std::make_shared<managed_shared_memory>(open_or_create, sharedMemoryName_.c_str(), sizeof(SharedMemoryStructure) * 1.1))
        , sharedMemory_() {
        try {
            memoryInitialization();
        } catch (const interprocess_exception& ex) {
            shared_memory_object::remove(sharedMemoryName_.c_str());
            named_mutex::remove(namedMutex.c_str());
            throw;
        }
    }

    void SharedMemoryTransport::memoryInitialization() {
        boost::interprocess::named_mutex init_mutex(boost::interprocess::open_or_create, namedMutex.c_str());
        boost::interprocess::scoped_lock<boost::interprocess::named_mutex> namedLock(init_mutex);

        auto [rawPointer, size] = segment_->find<SharedMemoryStructure>("SharedMemoryStructure");
            
        if (rawPointer == nullptr) {
            rawPointer = segment_->construct<SharedMemoryStructure>("SharedMemoryStructure")();
            new (&rawPointer->mutex) interprocess_mutex;
            new (&rawPointer->condition) interprocess_condition;

            rawPointer->finished = false;
            rawPointer->dataReady = false;
            rawPointer->size1 = 0;
            rawPointer->size2 = 0;
            rawPointer->activeDataBlock = false;
            rawPointer->activeProcessCount = 0;
        }
        
        namedLock.unlock();

        auto deleter = [segment = segment_, smName = sharedMemoryName_](SharedMemoryStructure* ptr){
            scoped_lock<interprocess_mutex> lock(ptr->mutex);
            if (--ptr->activeProcessCount == 0) {
                lock.unlock();
                
                segment->destroy<SharedMemoryStructure>("SharedMemoryStructure");
                shared_memory_object::remove(smName.c_str());
                named_mutex::remove(namedMutex.c_str());
            }
        };
        sharedMemory_ = SharedMemoryStructurePtr(rawPointer, deleter);

        scoped_lock<interprocess_mutex> lock(sharedMemory_->mutex);
        if (++sharedMemory_->activeProcessCount > 2) {
            throw std::runtime_error("Reader and Writer already exist");
        }

        if (sharedMemory_->activeProcessCount > 1) {
            strategy_ = EStrategy::E_Write;
        }
    }
    
    std::span<char> SharedMemoryTransport::getBuffer() {
        scoped_lock<interprocess_mutex> lock(sharedMemory_->mutex);

        boost::posix_time::ptime timeout = boost::posix_time::second_clock::universal_time() + boost::posix_time::seconds(10);
        if (!sharedMemory_->condition.timed_wait(lock, timeout, [this] { return !sharedMemory_->dataReady; })) {
            throw std::runtime_error("Timeout waiting for data to be read");
        }

        // Return the next writable buffer
        char* targetData = sharedMemory_->activeDataBlock ? sharedMemory_->data2 : sharedMemory_->data1;
        size_t& size = sharedMemory_->activeDataBlock ? sharedMemory_->size2 : sharedMemory_->size1;

        // Reset size since the buffer is for new data
        size = 0;
        
        return std::span<char>(targetData, BUFFER_SIZE);
    }

    void SharedMemoryTransport::sendData(std::span<const char> buffer) {
        scoped_lock<interprocess_mutex> lock(sharedMemory_->mutex);

        size_t& size = sharedMemory_->activeDataBlock ? sharedMemory_->size2 : sharedMemory_->size1;
        size = buffer.size();
        
        sharedMemory_->activeDataBlock = ~sharedMemory_->activeDataBlock;
        
        sharedMemory_->dataReady = true;
        sharedMemory_->condition.notify_all();
    }
    
    std::span<const char> SharedMemoryTransport::receiveData() {
        scoped_lock<interprocess_mutex> lock(sharedMemory_->mutex);
        
        boost::posix_time::ptime timeout = boost::posix_time::second_clock::universal_time() + boost::posix_time::seconds(10);
        if (!sharedMemory_->condition.timed_wait(lock, timeout, [this] { return sharedMemory_->dataReady or sharedMemory_->finished; })) {
            throw std::runtime_error("Timeout waiting for data to be written");
        }
        
        if (sharedMemory_->dataReady) {
            const char* sourceData = sharedMemory_->activeDataBlock ? sharedMemory_->data1 : sharedMemory_->data2;
            const size_t& size = sharedMemory_->activeDataBlock ? sharedMemory_->size1 : sharedMemory_->size2;

            sharedMemory_->dataReady = false;
            sharedMemory_->condition.notify_all();
            return std::span<const char>(sourceData, size);
        }

        return std::span<const char>(static_cast<const char*>(nullptr), 0);
    }

    bool SharedMemoryTransport::hasFinished() {
        scoped_lock<interprocess_mutex> lock(sharedMemory_->mutex);
        return sharedMemory_->finished and !sharedMemory_->dataReady;
    }

    void SharedMemoryTransport::finish() {
        scoped_lock<interprocess_mutex> lock(sharedMemory_->mutex);
        sharedMemory_->finished = true;
        sharedMemory_->condition.notify_all();

        boost::posix_time::ptime timeout = boost::posix_time::second_clock::universal_time() + boost::posix_time::seconds(10);
        sharedMemory_->condition.timed_wait(lock, timeout, [this] { return !sharedMemory_->dataReady; });
    }

    
} // namespace cp