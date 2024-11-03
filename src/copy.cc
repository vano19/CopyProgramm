#include <iostream>
#include "CopyManager.h"
#include "FileDestination.h"
#include "FileSource.h"
#include "SharedMemoryTransport.h"

// Buffer size for reading and writing
constexpr std::size_t BUFFER_SIZE = 4 * 1024 * 1024;  // 4 MB

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: "<< argv[0] << " <source file> <target file>\n";
        return 1;
    }

    try {
        std::string_view sourceFilename = argv[1];
        std::string_view targetFilename = argv[2];
        std::string_view sharedMemoryName = argv[3];

        if (sourceFilename == targetFilename) {
            std::cout << "Source and destination are the same. Exiting.\n";
            return 0;
        }

        cp::SharedMemoryTransport::Ptr transport = std::make_unique<cp::SharedMemoryTransport>(sharedMemoryName, BUFFER_SIZE);
        cp::IDataSource::Ptr source = nullptr;
        cp::IDataDestination::Ptr destination = nullptr;
        if (cp::EStrategy::E_Read == transport->strategy()) {
            source = std::make_unique<cp::FileSource>(sourceFilename);
        } else {
           destination = std::make_unique<cp::FileDestination>(targetFilename);
        }
        
        cp::CopyManager manager(std::move(source), std::move(destination), std::move(transport), BUFFER_SIZE);
        manager.start();

        std::cout << "Copy operation completed successfully.\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
    
    return 0;
}