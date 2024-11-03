#pragma once

#include "IDataTransport.h"

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

            struct SharedMemoryStructure {
                boost::interprocess::interprocess_mutex mutex;
                boost::interprocess::interprocess_condition condition;
                int activeProcessCount;
                bool finished;
                bool dataReady;
                size_t size;
                char data[1];
            };

            SharedMemoryTransport(std::string_view name, std::size_t bufferSize);
            virtual ~SharedMemoryTransport() = default;

            void sendData(std::span<char> buffer) override;
            std::span<char> receiveData() override;
            
            bool hasFinished() override;
            void finish() override;
                
            [[nodiscard]]
            inline EStrategy strategy() {
                return strategy_;
            }

        private:
            using SharedMemoryStructurePtr = std::unique_ptr<SharedMemoryStructure, std::function<void(SharedMemoryStructure*)>>;
            
            std::string sharedMemoryName_;
            const std::size_t bufferSize_;
            EStrategy strategy_;
            std::shared_ptr<boost::interprocess::managed_shared_memory> segment_;
            SharedMemoryStructurePtr sharedMemory_;

            static constexpr std::size_t dataOffset = offsetof(SharedMemoryStructure, data);
    };


} // namespace cp