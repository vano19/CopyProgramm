#pragma once

#include <condition_variable>
#include <queue>
#include <mutex>
#include <atomic>

#include "IDataSource.h"
#include "IDataDestination.h"

namespace cp {

    class CopyManager {
    public:
        CopyManager(IDataSource::Ptr source, IDataDestination::Ptr destination);

        void start();

    private:

        void read();
        void write();

    private:

        IDataSource::Ptr source_;
        IDataDestination::Ptr destination_;
        std::vector<std::vector<char>> buffers_;
        std::vector<std::size_t> bytesRead_;
        std::size_t readIndex_;
        std::size_t writeIndex_;
        std::mutex mutex_;
        std::condition_variable condVar_;
        bool done_;
        bool changeIndexes_;
    };

} // namespace cp