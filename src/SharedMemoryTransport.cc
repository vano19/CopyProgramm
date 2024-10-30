#include "SharedMemoryTransport.h"

#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>


namespace cp {

    using namespace boost::interprocess;

    struct SharedMemoryStructure {
        interprocess_mutex mutex;
        interprocess_condition condition;
        int activeProcessCount;
        size_t size;
        // Placeholder for character array (file data)
        char data[1]; // Flexible array member trick
    };
    
    SharedMemoryTransport::SharedMemoryTransport(std::string_view name, std::size buffer)
        : sharedMemoryName_(name)
        , strategy_(EStrategy::E_Read)
        , segment_(open_or_create, sharedMemoryName_.c_str(), buffer + dataOffset + 1)
        , sharedMemory_() {
        
        auto[rawPointer, size] = segment.find<SharedMemoryStructure>("SharedMemoryStructure");
        
        if (!rawPointer) {
            rawPointer = segment.construct<SharedMemoryStructure>("SharedMemoryStructure")();
            new (&rawPointer->mutex) interprocess_mutex;
            new (&rawPointer->condition) interprocess_condition;

            rawPointer->finished = false;
            rawPointer->dataReady = false;
            rawPointer->size = 0;
        }
        
        auto deleter = [this](SharedMemoryStructure* ptr) {
            scoped_lock<interprocess_mutex> lock(ptr->mutex);
            if (--ptr->active_process_count == 0) {
                lock.unlock();
                segment_.destroy<SharedMemoryStructure>("SharedMemoryStructure");
                shared_memory_object::remove(sharedMemoryName_.c_str());
            }
        };

        sharedMemory_ = std::unique_ptr<SharedMemoryStructure, decltype(deleter)>(rawPointer, deleter);

        scoped_lock<interprocess_mutex> lock(sharedMemory_->mutex);
        if (sharedMemory_->activeProcessCount >= 2) {
            throw std::runtime_error("Reading and Writer already exists");
        }

        ++sharedMemory_->activeProcessCount;
        lock.unlock();

        if (sharedMemory_->activeProcessCount == 2) {
            strategy_ = EStrategy::E_Write;
        }
    }

    void SharedMemoryTransport::sendData(std::span<char> buffer) {
        scoped_lock<interprocess_mutex> lock(sharedMemory_->mutex);
        while (sharedMemory_->dataReady) {
            sharedMemory_->condition.wait(lock);
        }
    
        std::memcpy(sharedMemory_->data, buffer.data(), buffer.size());
        sharedMemory->size = buffer.size();
        sharedMemory_->dataReady = true;
        sharedMemory_->condition.notify_all();
    }
    
    std::span<char> SharedMemoryTransport::receiveData() {
        scoped_lock<interprocess_mutex> lock(sharedMemory_->mutex);
        while (!sharedMemory_->dataReady and !sharedMemory_->finished) {
            sharedMemory_->condition.wait(lock);
        }

        std::vector<char> localBuffer;

        if (sharedMemory_->dataReady) {
            localBuffer.resize(dataSize);
            std::memcpy(localBuffer.data(), sharedMemory->data, dataSize);
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