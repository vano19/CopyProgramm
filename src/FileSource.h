#pragma once

#include <fstream>
#include "IDataSource.h"

namespace cp
{
    class FileSource : public IDataSource {
    public:

        explicit FileSource(const std::string& filename);
        bool readChunk(std::vector<char>& buffer, std::size_t& bytesRead) override;

    private:
        std::ifstream file_;
    };

} // namespace cp