#include <iostream>
#include <chrono>
#include "CopyManager.h"

namespace cp {
    
    // Buffer size for reading and writing
    constexpr std::size_t BUFFER_SIZE = 4 * 1024 * 1024;  // 4 MB

    CopyManager::CopyManager(IDataSource::Ptr source, IDataDestination::Ptr destination)
        : source_(std::move(source))
        , destination_(std::move(destination))
        , queue_()
        , bytesReadBuffer_()
        , mutex_()
        , condVar_()
        , done_(false)
        , errorOccurred_(false) {

    }

    void CopyManager::start() {
        std::jthread readerThread([this] { 
            try {
                read();
            } catch (const std::exception& ex) {
                std::cerr << "Reader thread encountered an error: " << ex.what() << "\n";
                errorOccurred_.store(true, std::memory_order_release);
                condVar_.notify_one();;
            }
        });

        std::jthread writerThread([this] {
            try {
                write();
            } catch (const std::exception& ex) {
                std::cerr << "Writer thread encountered an error: " << ex.what() << "\n";
                errorOccurred_.store(true, std::memory_order_release);
            }
        });
    }


    void CopyManager::read() {
        while (!done_.load(std::memory_order_relaxed) and !errorOccurred_.load(std::memory_order_acquire)) {
            std::vector<char> buffer(BUFFER_SIZE);
            std::size_t bytesRead = 0;
            if (!source_->readChunk(buffer, bytesRead)) {
                done_.store(true, std::memory_order_release);
                condVar_.notify_one();
                break;
            }
            
            {
                std::unique_lock<std::mutex> lock(mutex_);
                queue_.push(std::move(buffer));
                bytesReadBuffer_.push(bytesRead);
            }
            
            condVar_.notify_one();
        }
    }

    void CopyManager::write() {
        bool hasToWrite = true;
        while (hasToWrite) {
            std::vector<char> buffer;
            std::size_t bytesToWrite = 0;

            {
                std::unique_lock<std::mutex> lock(mutex_);
                condVar_.wait_for(lock, 
                    std::chrono::seconds(3), 
                    [this] { return !queue_.empty() or done_.load(std::memory_order_acquire) or errorOccurred_.load(std::memory_order_acquire); });

                if ((queue_.empty() and done_.load(std::memory_order_relaxed)) or errorOccurred_.load(std::memory_order_relaxed)) {
                    hasToWrite = false;
                }

                if (!queue_.empty()) {
                    buffer = std::move(queue_.front());
                    queue_.pop();
                    bytesToWrite = bytesReadBuffer_.front();
                    bytesReadBuffer_.pop();
                }
            }
            if (!buffer.empty()) {
                destination_->writeChunk(buffer, bytesToWrite);
            }
        }
    }
    
} // namespace cp