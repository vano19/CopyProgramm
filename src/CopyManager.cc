#include <iostream>
#include <chrono>
#include "CopyManager.h"
#include <future>

namespace cp {
    
    // Buffer size for reading and writing
    constexpr std::size_t BUFFER_SIZE = 4 * 1024 * 1024;  // 4 MB

    CopyManager::CopyManager(IDataSource::Ptr source, IDataDestination::Ptr destination)
        : source_(std::move(source))
        , destination_(std::move(destination))
        , buffers_(2, std::vector<char>(BUFFER_SIZE))
        , bytesRead_(2, 0)
        , readIndex_(0)
        , writeIndex_(1)
        , mutex_()
        , condVar_()
        , done_(false)
        , changeIndexes_(true) {

    }

    void CopyManager::start() {
        auto readFuture = std::async(&CopyManager::read, this);
        auto writeFuture = std::async(&CopyManager::write, this);

        try {
            readFuture.get();
        } catch (const std::exception& ex) {
            std::cerr << "Reader thread encountered an error: " << ex.what() << "\n";
            done_ = true;
            condVar_.notify_one();
        }

        try {
            writeFuture.get();
        } catch (const std::exception& ex) {
            std::cerr << "Writer thread encountered an error: " << ex.what() << "\n";
        }
    }


    void CopyManager::read() {
        std::size_t bytesRead = 0;
        while (!done_ and source_->readChunk(buffers_[readIndex_], bytesRead)) {
            condVar_.notify_one();
            {
                std::unique_lock<std::mutex> lock(mutex_);
                bytesRead_[readIndex_] = bytesRead;
                if(!condVar_.wait_for(lock, std::chrono::seconds(10), [this] { return changeIndexes_ or done_; })) {
                    throw std::runtime_error("Timeout on reading.");
                }
                
                changeIndexes_ = false;
                std::swap(readIndex_, writeIndex_);
            }
        }

        {
            std::unique_lock<std::mutex> lock(mutex_);
            done_ = true;
        }
        condVar_.notify_one();
    }

    void CopyManager::write() {
        bool hasToWrite = true;
        while (hasToWrite) {
            std::size_t bytesToWrite = 0;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condVar_.wait(lock, [this] { return !changeIndexes_ or done_; });

                bytesToWrite = bytesRead_[writeIndex_];
                bytesRead_[writeIndex_] = 0; // Clear the bytesRead for the next round
                if (done_ and bytesToWrite == 0) {
                    hasToWrite = false;
                }              
            }

            if (bytesToWrite > 0) {
                destination_->writeChunk(buffers_[writeIndex_], bytesToWrite);
                changeIndexes_ = true;
                condVar_.notify_one();
            }
        }
    }
    
} // namespace cp