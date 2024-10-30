#include <iostream>
#include <chrono>
#include "CopyManager.h"
#include <future>

namespace cp {
    
    // Buffer size for reading and writing
    constexpr std::size_t BUFFER_SIZE = 4 * 1024 * 1024;  // 4 MB

    CopyManager::CopyManager(IDataSource::Ptr source, IDataDestination::Ptr destination, IDataTransport::Ptr transport)
        : source_(std::move(source))
        , destination_(std::move(destination))
        , transport_(std::move(transport)) {

    }

    void CopyManager::start() {
        if (source_) {
            read();
        } else if (destination_) {
            write();
        }
    }


    void CopyManager::read() {
        std::size_t bytesRead = 0;
        std::vector<char> buffer(BUFFER_SIZE);
        while(source->readChunk(buffer, bytesRead)) {
            transport_->sendData(std::span<char>{buffer.data(), bytesRead});
        }
        transport_->finish();
    }

    void CopyManager::write() {
        while(transport_->hasFinished()) {
            std::span<char> buffer = transport_->receiveData();
            if (buffer.size() > 0)
                destination_->writeChunk(buffer);
        }
    }
    
} // namespace cp