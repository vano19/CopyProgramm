#pragma once

#include "IDataTransport.h"
#include "Constants.h"

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>

#include <functional>

namespace cp {
    
    enum class EStrategy {
       E_Read = 0,
       E_Write 
    };

    class SharedMemoryTransport : public IDataTransport {
        public:
            using Ptr = std::unique_ptr<SharedMemoryTransport>;

            SharedMemoryTransport(std::string_view name);
            virtual ~SharedMemoryTransport() = default;

            std::span<char> getBuffer() override;

            void sendData(std::span<const char> buffer) override;
            std::span<const char> receiveData() override;
            
            bool hasFinished() override;
            void finish() override;
                
            [[nodiscard]]
            inline EStrategy strategy() {
                return strategy_;
            }

        private:
            
            void memoryInitialization();

            struct SharedMemoryStructure {
                boost::interprocess::interprocess_mutex mutex;
                boost::interprocess::interprocess_condition condition;
                int activeProcessCount;
                bool finished;
                bool dataReady;
                bool activeDataBlock; // false for data1, true for data2
                size_t size1;
                size_t size2;
                char data1[BUFFER_SIZE] = {0};
                char data2[BUFFER_SIZE] = {0};
            };

            using SharedMemoryStructurePtr = std::unique_ptr<SharedMemoryStructure, std::function<void(SharedMemoryStructure*)>>;

            std::string sharedMemoryName_;
            EStrategy strategy_;
            std::shared_ptr<boost::interprocess::managed_shared_memory> segment_;
            SharedMemoryStructurePtr sharedMemory_;
    };


} // namespace cp