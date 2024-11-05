#pragma once

#include <span>
#include <memory>
namespace cp {

    class IDataTransport {
    public:
        using Ptr = std::unique_ptr<IDataTransport>;
        
        virtual ~IDataTransport() = default;
        
        virtual std::span<char> getBuffer() = 0;

        virtual void sendData(std::span<const char> buffer) = 0;
        virtual std::span<const char> receiveData() = 0;

        virtual bool hasFinished() = 0;
        virtual void finish() = 0;
    };

} // namespace cp