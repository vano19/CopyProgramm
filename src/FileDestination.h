#pragma once

#include <fstream>
#include "IDataDestination.h"

namespace cp
{
    class FileDestination : public IDataDestination {
    public:

        explicit FileDestination(const std::string& filename);
        void writeChunk(const std::vector<char>& buffer, std::size_t bytesToWrite) override;

    private:
        std::ofstream file_;
    }; 

} // namespace cp
