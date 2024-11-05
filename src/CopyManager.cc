#include "CopyManager.h"
#include <iostream>
#include <chrono>


namespace cp {
    
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
        bool hasToRead = true;
        while(hasToRead) {
            std::span<char> buffer = transport_->getBuffer();
            if(hasToRead = source_->readChunk(buffer, bytesRead); hasToRead) 
                transport_->sendData(std::span<char>{buffer.data(), bytesRead});
        }
        transport_->finish();
    }

    void CopyManager::write() {
        while(!transport_->hasFinished()) {
            std::span<const char> buffer = transport_->receiveData();
            if (buffer.size() > 0)
                destination_->writeChunk(buffer);
        }
    }
    
} // namespace cp