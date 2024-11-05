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
            IDataTransport::Ptr transport);

        void start();

    private:

        void read();
        void write();

    private:
        IDataSource::Ptr source_;
        IDataDestination::Ptr destination_;
        IDataTransport::Ptr transport_;
    };

} // namespace cp