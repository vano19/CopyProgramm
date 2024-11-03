#pragma once

#include "IDataTransport.h"
#include "IDataSource.h"
#include "IDataDestination.h"

namespace cp {

    class CopyManager {
    public:
        CopyManager(
            IDataSource::Ptr source, 
            IDataDestination::Ptr destination,
            IDataTransport::Ptr transport,
            std::size_t bufferSize);

        void start();

    private:

        void read();
        void write();

    private:
        const std::size_t bufferSize_;
        IDataSource::Ptr source_;
        IDataDestination::Ptr destination_;
        IDataTransport::Ptr transport_;
    };

} // namespace cp