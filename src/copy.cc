#include <iostream>
#include "CopyManager.h"
#include "FileDestination.h"
#include "FileSource.h"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: "<< argv[0] << " <source file> <target file>\n";
        return 1;
    }

    try {
        std::string sourceFilename = argv[1];
        std::string targetFilename = argv[2];

        cp::IDataSource::Ptr source = std::make_unique<cp::FileSource>(sourceFilename);
        cp::IDataDestination::Ptr destination = std::make_unique<cp::FileDestination>(targetFilename);

        cp::CopyManager manager(std::move(source), std::move(destination));
        manager.start();

        std::cout << "Copy operation completed successfully.\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
    
    return 0;
}