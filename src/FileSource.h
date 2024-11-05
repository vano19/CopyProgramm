#pragma once

#include <fstream>
#include "IDataSource.h"

namespace cp
{
    class FileSource : public IDataSource {
    public:

        explicit FileSource(std::string_view filename);
        bool readChunk(std::span<char> buffer, std::size_t& bytesRead) override;

    private:
        std::ifstream file_;
    };

} // namespace cp