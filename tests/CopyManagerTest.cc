#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <sys/stat.h>

#include "CopyManager.h"
#include "FileSource.h"
#include "FileDestination.h"

#include <unistd.h>
#include <sys/wait.h>

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

int runProcess(const char* cmd, const char* arg1, const char* arg2) {
    pid_t pid = fork();

    if (pid == -1) {
        std::cerr << "fork";
        return -1;
    } else if (pid == 0) {
        // Child process
        execlp(cmd, cmd, arg1, arg2, (char*)NULL);
        // If execlp returns, it must have failed
        std::cerr << "execlp";
        return -1;
    } else {
        // Parent process
        return pid;
    }
}

bool sharedMemoryLaunch(std::string_view source, std::string_view destination) {
    std::vector<pid_t> pids;
    int status;
    pid_t pid;

    // Start two processes
    pids.push_back(runProcess("copy", source.data(), destination.data()));
    pids.push_back(runProcess("customCP", source.data(), destination.data()));

    // Wait for processes to finish
    for (pid_t p : pids) {
        if (waitpid(p, &status, 0) == -1) {
            std::cerr << "waitpid";
            return false;
        }

        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            std::cout << "Process " << p << " exited with status " << exit_status << std::endl;
            if (exit_status != 0) {
                std::cerr << "Process " << p << " failed." << std::endl;
                return false;
            }
        } else {
            std::cerr << "Process " << p << " did not exit properly" << std::endl;
            return false;
        }
    }

    std::cout << "All processes finished successfully" << std::endl;

    return true;
}

TEST_CASE("Test CopyManager functionality", "[CopyManager]") {
    const std::string sourceFilename = "source.txt";
    const std::string targetFilename = "target.txt";
    const std::string permissionFilename = "permission_test_file.txt";

    SECTION("Copy small file") {
        createFile(sourceFilename, 1024); // 1 KB file
        {
            REQUIRE(sharedMemoryLaunch(sourceFilename, targetFilename));
        }

        REQUIRE(fs::file_size(sourceFilename) == fs::file_size(targetFilename));
        REQUIRE(compareFiles(sourceFilename, targetFilename));
    }

    SECTION("Copy large file") {
        createFile(sourceFilename, 1 * 1024 * 1024 * 1024); // 1 GB file
        {
            REQUIRE(sharedMemoryLaunch(sourceFilename, targetFilename));
        }

        REQUIRE(fs::file_size(sourceFilename) == fs::file_size(targetFilename));
        REQUIRE(compareFiles(sourceFilename, targetFilename));
    }

    SECTION("Copy empty file") {
        if (fs::exists(sourceFilename))
            REQUIRE(std::remove(sourceFilename.c_str()) == 0);

        createFile(sourceFilename, 0); // Empty file
        {
            REQUIRE(sharedMemoryLaunch(sourceFilename, targetFilename));
        }

        REQUIRE(fs::file_size(sourceFilename) == fs::file_size(targetFilename));
        REQUIRE(compareFiles(sourceFilename, targetFilename));
    }

    SECTION("Non-existent source file") {
        // Ensure source file does not exist
        if (fs::exists(sourceFilename))
            REQUIRE(std::remove(sourceFilename.c_str()) == 0);
        {
            REQUIRE(sharedMemoryLaunch("nonexistent.txt", targetFilename) == false);
        }
    }

    SECTION("Write permissions issue") {
        createFile(permissionFilename, 1024);

        // Set the file as non-writable
        if (chmod(permissionFilename.c_str(), 0444) != 0) {
            FAIL("Failed to change file permissions");
        }

        {
            REQUIRE(sharedMemoryLaunch(permissionFilename, targetFilename) == false);
        }

        // Cleanup: reset permissions and remove the file
        chmod(permissionFilename.c_str(), 0666);
        std::remove(permissionFilename.c_str());
    }

    // Clean up test files
    std::remove(sourceFilename.c_str());
    std::remove(targetFilename.c_str());
}