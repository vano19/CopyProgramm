#include "SharedMemoryTransport.h"

#include <stdexcept>

#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace cp {

    using namespace boost::interprocess;
    
    SharedMemoryTransport::SharedMemoryTransport(std::string_view name, std::size_t bufferSize)
        : sharedMemoryName_(name)
        , bufferSize_(bufferSize)
        , strategy_(EStrategy::E_Read)
        , segment_(std::make_shared<managed_shared_memory>(open_or_create, sharedMemoryName_.c_str(), bufferSize_ + dataOffset + 1))
        , sharedMemory_() {
        try {
            auto [rawPointer, size] = segment_->find<SharedMemoryStructure>("SharedMemoryStructure");
            
            if (!rawPointer) {
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
                    segment->destroy<SharedMemoryStructure>("SharedMemoryStructure");
                    shared_memory_object::remove(smName.c_str());
                }
            };
            sharedMemory_ = SharedMemoryStructurePtr(rawPointer, deleter);

        } catch (const interprocess_exception& ex) {
            shared_memory_object::remove(sharedMemoryName_.c_str());
            throw;
        }

        scoped_lock<interprocess_mutex> lock(sharedMemory_->mutex);
        if (sharedMemory_->activeProcessCount >= 2) {
            throw std::runtime_error("Reader and Writer already exist");
        }

        ++sharedMemory_->activeProcessCount;
        lock.unlock();

        if (sharedMemory_->activeProcessCount == 2) {
            strategy_ = EStrategy::E_Write;
        }
    }
    
    void SharedMemoryTransport::sendData(std::span<char> buffer) {
        scoped_lock<interprocess_mutex> lock(sharedMemory_->mutex);

        boost::posix_time::ptime timeout =boost::posix_time::second_clock::universal_time() + boost::posix_time::seconds(10);
        if (!sharedMemory_->condition.timed_wait(lock, timeout, [this] { return !sharedMemory_->dataReady; })) {
            throw std::runtime_error("Timeout waiting for data to be read");
        }
        
    
        std::memcpy(sharedMemory_->data, buffer.data(), buffer.size());
        sharedMemory_->size = buffer.size();
        sharedMemory_->dataReady = true;
        sharedMemory_->condition.notify_all();
    }
    
    std::span<char> SharedMemoryTransport::receiveData() {
        scoped_lock<interprocess_mutex> lock(sharedMemory_->mutex);
        
        boost::posix_time::ptime timeout = boost::posix_time::second_clock::universal_time() + boost::posix_time::seconds(10);
        if (sharedMemory_->condition.timed_wait(lock, timeout, [this] { return sharedMemory_->dataReady or sharedMemory_->finished; })) {
            throw std::runtime_error("Timeout waiting for data to be written");
        }

        static std::vector<char> localBuffer(bufferSize_);

        if (sharedMemory_->dataReady) {
            std::memcpy(localBuffer.data(), sharedMemory_->data, sharedMemory_->size);
            sharedMemory_->dataReady = false;
            sharedMemory_->condition.notify_all();
        }

        return localBuffer;
    }

    bool SharedMemoryTransport::hasFinished() {
        scoped_lock<interprocess_mutex> lock(sharedMemory_->mutex);
        return sharedMemory_->finished;
    }

    void SharedMemoryTransport::finish() {
        scoped_lock<interprocess_mutex> lock(sharedMemory_->mutex);
        sharedMemory_->finished = true;
        sharedMemory_->condition.notify_all();
    }
} // namespace cp