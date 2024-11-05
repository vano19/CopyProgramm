#pragma once

#include <span>
#include <memory>

namespace cp {

    class IDataSource {
    public:
        using Ptr = std::unique_ptr<IDataSource>;

        virtual bool readChunk(std::span<char> buffer, std::size_t& bytesRead) = 0;
        virtual ~IDataSource() = default;
    };

} // namespace cp