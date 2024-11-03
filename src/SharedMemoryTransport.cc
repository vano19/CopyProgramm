#include "SharedMemoryTransport.h"

#include <stdexcept>

#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <iostream>
#include <thread>
#include <chrono>

namespace cp {

    using namespace boost::interprocess;
    
    SharedMemoryTransport::SharedMemoryTransport(std::string_view name, std::size_t bufferSize)
        : sharedMemoryName_(name)
        , bufferSize_(bufferSize)
        , strategy_(EStrategy::E_Read)
        , segment_(std::make_shared<managed_shared_memory>(open_or_create, sharedMemoryName_.c_str(), bufferSize_ + dataOffset + 1))
        , sharedMemory_() {
        //for(int repeat = 0; repeat < 5; ++repeat) {
            try {
                memoryInitialization();
                //break; // stop if everything initialized properly
            } catch (const interprocess_exception& ex) {
                std::cerr << "[" << boost::posix_time::microsec_clock::local_time() << "] code: " << ex.get_native_error() << std::endl;
                if (ex.get_native_error() > 0) {
                    shared_memory_object::remove(sharedMemoryName_.c_str());
                    throw;
                }
                //std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        //}
        std::cout << "[" << boost::posix_time::microsec_clock::local_time() << "] finish." << std::endl;
                
    }

    void SharedMemoryTransport::memoryInitialization() {
        boost::interprocess::named_mutex init_mutex(boost::interprocess::open_or_create, "shm_init_mutex");
        boost::interprocess::scoped_lock<boost::interprocess::named_mutex> namedLock(init_mutex);

        auto [rawPointer, size] = segment_->find<SharedMemoryStructure>("SharedMemoryStructure");
            
        if (rawPointer == nullptr) {
            std::cout << "[" << boost::posix_time::microsec_clock::local_time() << "] " << "Create shared memory" << std::endl;
            rawPointer = segment_->construct<SharedMemoryStructure>("SharedMemoryStructure")();
            new (&rawPointer->mutex) interprocess_mutex;
            new (&rawPointer->condition) interprocess_condition;

            rawPointer->finished = false;
            rawPointer->dataReady = false;
            rawPointer->size = 0;
        }
        
        auto deleter = [segment = segment_, smName = sharedMemoryName_](SharedMemoryStructure* ptr) {
            scoped_lock<interprocess_mutex> lock(ptr->mutex);
            if (--ptr->activeProcessCount == 0) {
                lock.unlock();
                std::cout << "[" << boost::posix_time::microsec_clock::local_time() << "] " << "Delete shared memory" << std::endl;
                segment->destroy<SharedMemoryStructure>("SharedMemoryStructure");
                shared_memory_object::remove(smName.c_str());
            }
        };
        sharedMemory_ = SharedMemoryStructurePtr(rawPointer, deleter);

        scoped_lock<interprocess_mutex> lock(sharedMemory_->mutex);
        if (sharedMemory_->activeProcessCount >= 2) {
            throw std::runtime_error("Reader and Writer already exist");
        }

        ++sharedMemory_->activeProcessCount;
        lock.unlock();

        std::cout << "[" << boost::posix_time::microsec_clock::local_time() << "] " << "I am " << sharedMemory_->activeProcessCount << std::endl;

        if (sharedMemory_->activeProcessCount == 2) {
            strategy_ = EStrategy::E_Write;
        }
    }
    
    void SharedMemoryTransport::sendData(std::span<char> buffer) {
        scoped_lock<interprocess_mutex> lock(sharedMemory_->mutex);

        boost::posix_time::ptime timeout = boost::posix_time::second_clock::universal_time() + boost::posix_time::seconds(10);
        if (!sharedMemory_->condition.timed_wait(lock, timeout, [this] { return !sharedMemory_->dataReady; })) {
            throw std::runtime_error("Timeout waiting for data to be read");
        }
        
        std::cout << "[" << boost::posix_time::microsec_clock::local_time() << "] " << "send: buffer size: " << buffer.size() << std::endl;
    
        std::memcpy(sharedMemory_->data, buffer.data(), buffer.size());
        sharedMemory_->size = buffer.size();
        sharedMemory_->dataReady = true;
        sharedMemory_->condition.notify_all();
    }
    
    std::span<char> SharedMemoryTransport::receiveData() {
        scoped_lock<interprocess_mutex> lock(sharedMemory_->mutex);
        
        boost::posix_time::ptime timeout = boost::posix_time::second_clock::universal_time() + boost::posix_time::seconds(10);
        if (!sharedMemory_->condition.timed_wait(lock, timeout, [this] { return sharedMemory_->dataReady or sharedMemory_->finished; })) {
            throw std::runtime_error("Timeout waiting for data to be written");
        }
        static std::vector<char> localBuffer(bufferSize_);
        
        if (sharedMemory_->dataReady) {
            std::cout << "[" << boost::posix_time::microsec_clock::local_time() << "] " << "get: buffer size: " << sharedMemory_->size << std::endl;
            std::memcpy(localBuffer.data(), sharedMemory_->data, sharedMemory_->size);
            sharedMemory_->dataReady = false;
            sharedMemory_->condition.notify_all();
            return std::span<char>{localBuffer.data(), sharedMemory_->size};
        }

        return std::span<char>{localBuffer.data(), 0};
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
        if (!sharedMemory_->condition.timed_wait(lock, timeout, [this] { return !sharedMemory_->dataReady; })) {
            std::cerr << "[" << boost::posix_time::microsec_clock::local_time() << "] " << "Timeout waiting for data to be read after finish" << std::endl;
        } else {
            std::cout << "[" << boost::posix_time::microsec_clock::local_time() << "] " << "Data read after finish" << std::endl;
        }
    }

    
} // namespace cp