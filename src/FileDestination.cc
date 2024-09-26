#include "FileDestination.h"

namespace cp
{
    FileDestination::FileDestination(const std::string& filename) 
        : file_(filename, std::ios::binary) {
        
        if (!file_) {
            throw std::runtime_error("Failed to open target file: " + filename);
        }
    }

    void FileDestination::writeChunk(const std::vector<char>& buffer, std::size_t bytesToWrite) {
        if (!file_) {
            throw std::runtime_error("Target file is not open");
        }

        file_.write(buffer.data(), bytesToWrite);

        if (!file_) {
            throw std::runtime_error("Failed to write to target file");
        }
    }
 
} // namespace cp
