#pragma once

#include <span>

namespace cp {

    class IDataTransport {
    public:
        using Ptr = std::unique_ptr<IDataSource>;
        
        virtual ~DataTransport() = default;

        virtual void sendData(std::span<char> buffer) = 0;
        virtual std::span<char> receiveData() = 0;
    };

} // namespace cp