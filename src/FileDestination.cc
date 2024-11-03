#include "FileDestination.h"

namespace cp
{
    FileDestination::FileDestination(std::string_view filename) 
        : file_(filename.data(), std::ios::binary) {
        
        if (!file_) {
            throw std::runtime_error("Failed to open target file: " + std::string(filename));
        }
    }

    void FileDestination::writeChunk(std::span<char> buffer) {
        if (!file_) {
            throw std::runtime_error("Target file is not open");
        }

        file_.write(buffer.data(), buffer.size());

        if (!file_) {
            throw std::runtime_error("Failed to write to target file");
        }
    }
 
} // namespace cp
