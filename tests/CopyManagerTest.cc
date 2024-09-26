#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <sys/stat.h>

#include "CopyManager.h"
#include "FileSource.h"
#include "FileDestination.h"

namespace fs = std::filesystem;

std::string randomString(int len) {
    constexpr char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string result;

    for (int i = 0; i < len; ++i) {
        result += alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    return result;
}

void createFile(const std::string& filename, std::size_t size) {
    std::ofstream file(filename, std::ios::binary);

    constexpr std::size_t chunkSize = 64 * 1024; // 64 KB chunk size
    const std::string buffer(randomString(std::min(size, chunkSize)));

    std::size_t bytesWritten = 0;
    while (bytesWritten < size) {
        file.write(buffer.data(), buffer.size());
        bytesWritten += buffer.size();
    };
}

bool compareFiles(const std::string& file1, const std::string& file2) {
    std::ifstream f1(file1, std::ios::binary);
    std::ifstream f2(file2, std::ios::binary);

    std::istreambuf_iterator<char> begin1(f1), begin2(f2);
    std::istreambuf_iterator<char> end;

    return std::equal(begin1, end, begin2);
}

TEST_CASE("Test CopyManager functionality", "[CopyManager]") {
    const std::string sourceFilename = "source.txt";
    const std::string targetFilename = "target.txt";
    const std::string permissionFilename = "permission_test_file.txt";

    SECTION("Copy small file") {
        createFile(sourceFilename, 1024); // 1 KB file

        auto source = std::make_unique<cp::FileSource>(sourceFilename);
        auto destination = std::make_unique<cp::FileDestination>(targetFilename);
        {
            cp::CopyManager manager(std::move(source), std::move(destination));
            manager.start();
        }

        REQUIRE(fs::file_size(sourceFilename) == fs::file_size(targetFilename));
        REQUIRE(compareFiles(sourceFilename, targetFilename));
    }

    SECTION("Copy large file") {
        createFile(sourceFilename, 1 * 1024 * 1024 * 20); // 20 MB file

        auto source = std::make_unique<cp::FileSource>(sourceFilename);
        auto destination = std::make_unique<cp::FileDestination>(targetFilename);

        cp::CopyManager manager(std::move(source), std::move(destination));
        manager.start();

        REQUIRE(fs::file_size(sourceFilename) == fs::file_size(targetFilename));
        REQUIRE(compareFiles(sourceFilename, targetFilename));
    }

    SECTION("Copy empty file") {
        if (fs::exists(sourceFilename))
            REQUIRE(std::remove(sourceFilename.c_str()) == 0);

        createFile(sourceFilename, 0); // Empty file

        auto source = std::make_unique<cp::FileSource>(sourceFilename);
        auto destination = std::make_unique<cp::FileDestination>(targetFilename);

        cp::CopyManager manager(std::move(source), std::move(destination));
        manager.start();

        REQUIRE(fs::file_size(sourceFilename) == fs::file_size(targetFilename));
        REQUIRE(compareFiles(sourceFilename, targetFilename));
    }

    SECTION("Non-existent source file") {
        // Ensure source file does not exist
        if (fs::exists(sourceFilename))
            REQUIRE(std::remove(sourceFilename.c_str()) == 0);

        REQUIRE_THROWS_AS([&]() {
            auto source = std::make_unique<cp::FileSource>("nonexistent.txt");
            auto destination = std::make_unique<cp::FileDestination>(targetFilename);
            cp::CopyManager manager(std::move(source), std::move(destination));
        }(), std::runtime_error);
    }

    SECTION("Write permissions issue") {
        createFile(permissionFilename, 1024);
        
        // Set the file as non-writable
        if (chmod(permissionFilename.c_str(), 0444) != 0) {
            FAIL("Failed to change file permissions");
        }

        REQUIRE_THROWS_AS([&]() {
            auto destination = std::make_unique<cp::FileDestination>(permissionFilename);
        }(), std::runtime_error);

        // Cleanup: reset permissions and remove the file
        chmod(permissionFilename.c_str(), 0666);
        std::remove(permissionFilename.c_str());
    }

    // Clean up test files
    std::remove(sourceFilename.c_str());
    std::remove(targetFilename.c_str());
}