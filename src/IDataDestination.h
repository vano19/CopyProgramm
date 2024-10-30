#pragma once

#include <span>
#include <memory>

namespace cp {

    class IDataDestination {
    public:
        using Ptr = std::unique_ptr<IDataDestination>;

        virtual void writeChunk(std::span<char> buffer) = 0;
        virtual ~IDataDestination() = default;
    };

} // namespace cp