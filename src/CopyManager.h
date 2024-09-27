#pragma once

#include <thread>
#include <condition_variable>
#include <queue>
#include <mutex>

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

        void notifyError();
        
    private:

        IDataSource::Ptr source_;
        IDataDestination::Ptr destination_;
        std::queue<std::vector<char>> queue_;
        std::queue<std::size_t> bytesReadBuffer_;
        std::mutex mutex_;
        std::condition_variable condVar_;
        bool done_;
        bool errorOccurred_;
    };

} // namespace cp