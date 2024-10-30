#pragma once

#include "IDataTransport.h"

#include <boost/interprocess/managed_shared_memory.hpp>

namespace cp {
    
    enum class EStrategy {
       E_Read = 0,
       E_Write 
    };

    class SharedMemoryTransport : public IDataTransport {
        public:
            using Ptr = std::unique_ptr<SharedMemoryTransport>;

            explicit SharedMemoryTransport(std::string_view name);
            virtual ~SharedMemoryTransport() = default;

            void sendData(std::span<char> buffer) override;
            std::span<char> receiveData() override;

            inline EStrategy strategy() {
                return strategy_;
            }

        private:
            std::string sharedMemoryName_;
            EStrategy strategy_;
            boost::interprocess::managed_shared_memory segment_;

            struct SharedMemoryStructure {
                interprocess_mutex mutex;
                interprocess_condition condition;
                int activeProcessCount;
                bool finished;
                bool dataReady;
                size_t size;
                char data[1];
            };

            std::unique_ptr<SharedMemoryStructure> sharedMemory_;

            static constexpr std::size_t dataOffset = offsetof(SharedMemoryStructure, data);
    };


} // namespace cp