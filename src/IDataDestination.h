#pragma once

#include <vector>
#include <memory>

namespace cp {

    class IDataDestination {
    public:
        using Ptr = std::unique_ptr<IDataDestination>;

        virtual void writeChunk(const std::vector<char>& buffer, std::size_t bytesToWrite) = 0;
        virtual ~IDataDestination() = default;
    };

} // namespace cp