#include "FileSource.h"

namespace cp
{
    FileSource::FileSource(std::string_view filename) 
        : file_(filename.data(), std::ios::binary) {
        
        if (!file_) {
            throw std::runtime_error("Failed to open source file: " + std::string(filename));
        }
    }

    bool FileSource::readChunk(std::vector<char>& buffer, std::size_t& bytesRead) {
        if (!file_) return false;

        file_.read(buffer.data(), buffer.size());
        bytesRead = static_cast<std::size_t>(file_.gcount());

        return bytesRead > 0;
    }
    
} // namespace cp