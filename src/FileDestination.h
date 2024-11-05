#pragma once

#include <fstream>
#include "IDataDestination.h"

namespace cp
{
    class FileDestination : public IDataDestination {
    public:

        explicit FileDestination(std::string_view filename);
        void writeChunk(std::span<const char> buffer) override;

    private:
        std::ofstream file_;
    }; 

} // namespace cp
