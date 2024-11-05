#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <sys/stat.h>

#include "CopyManager.h"
#include "FileSource.h"
#include "FileDestination.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#include <iostream>

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

int runProcess(const char* cmd, const char* source, const char* destination, const char* smname, const char* output) {
    pid_t pid = fork();

    if (pid == -1) {
        throw std::runtime_error(std::string("Failed to fork: ") + strerror(errno));
    } else if (pid == 0) {
        // Child process
        int fd = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            throw std::runtime_error(std::string("Failed to open file '") + output + "': " + strerror(errno));
        }

        // Redirect stdout and stderr to the file
        if (dup2(fd, STDOUT_FILENO) == -1 || dup2(fd, STDERR_FILENO) == -1) {
            close(fd); 
            throw std::runtime_error(std::string("Failed to redirect stdout or stderr: ") + strerror(errno));
        }

        close(fd); 
        execlp(cmd, cmd, source, destination, smname, (char*)NULL);
        
        throw std::runtime_error(std::string("Failed to execute '") + cmd + "': " + strerror(errno));
    } else {
        // Parent process
        return pid;
    }
}

bool sharedMemoryLaunch(std::string_view source, std::string_view destination, bool enableThird = false) {
    std::vector<pid_t> pids;
    // Start two processes
    pids.push_back(runProcess("/copy/build/src/copy", source.data(), destination.data(), "shared_mem", "/copy/build/process1.log"));
    pids.push_back(runProcess("/copy/build/src/copy", source.data(), destination.data(), "shared_mem", "/copy/build/process2.log"));
    if (enableThird) {
        pids.push_back(runProcess("/copy/build/src/copy", source.data(), destination.data(), "shared_mem", "/copy/build/process2.log"));
    }    
    // Wait for processes to finish
    for (pid_t p : pids) {
        int status = 0;
        if (waitpid(p, &status, 0) == -1) {
            throw std::runtime_error(std::string("waitpid is ") + strerror(errno));
        }
        
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            if (exit_status != 0) {
                throw std::runtime_error("Process " + std::to_string(p) + " did not exit properly with " + std::to_string(exit_status));
            }
        } else if (WIFSIGNALED(status)) {
            int term_sig = WTERMSIG(status);
            throw std::runtime_error("Process " + std::to_string(p) + " was terminated by signal " + std::to_string(term_sig));
        }
    }
    
    return true;
}

TEST_CASE("Test CopyManager functionality", "[CopyManager]") {
    const std::string sourceFilename = "source.txt";
    const std::string targetFilename = "target.txt";
    const std::string permissionFilename = "permission_test_file.txt";

    SECTION("Copy small file") {
        createFile(sourceFilename, 1024); // 1 KB file
        {
            REQUIRE_NOTHROW(sharedMemoryLaunch(sourceFilename, targetFilename));
        }

        REQUIRE(fs::file_size(sourceFilename) == fs::file_size(targetFilename));
        REQUIRE(compareFiles(sourceFilename, targetFilename));
    }

    SECTION("Copy large file") {
        createFile(sourceFilename, 1 * 1024 * 1024 * 1024); // 1 GB file
        {
            REQUIRE_NOTHROW(sharedMemoryLaunch(sourceFilename, targetFilename));
        }

        REQUIRE(fs::file_size(sourceFilename) == fs::file_size(targetFilename));
        REQUIRE(compareFiles(sourceFilename, targetFilename));
    }

    SECTION("Copy empty file") {
        if (fs::exists(sourceFilename))
            REQUIRE(std::remove(sourceFilename.c_str()) == 0);

        createFile(sourceFilename, 0); // Empty file
        {
            REQUIRE_NOTHROW(sharedMemoryLaunch(sourceFilename, targetFilename));
        }

        REQUIRE(fs::file_size(sourceFilename) == fs::file_size(targetFilename));
        REQUIRE(compareFiles(sourceFilename, targetFilename));
    }

    SECTION("Non-existent source file") {
        // Ensure source file does not exist
        if (fs::exists(sourceFilename))
            REQUIRE(std::remove(sourceFilename.c_str()) == 0);
        {
            REQUIRE_THROWS_AS(sharedMemoryLaunch("nonexistent.txt", targetFilename), std::runtime_error);
        }
    }

    SECTION("Write permissions issue") {
        createFile(permissionFilename, 1024);

        // Set the file as non-writable
        if (chmod(permissionFilename.c_str(), 0444) != 0) {
            FAIL("Failed to change file permissions");
        }

        {
            REQUIRE_THROWS_AS(sharedMemoryLaunch(permissionFilename, targetFilename), std::runtime_error);
        }

        // Cleanup: reset permissions and remove the file
        chmod(permissionFilename.c_str(), 0666);
        std::remove(permissionFilename.c_str());
    }

    SECTION("Copy small file with 3 proccesses") {
        createFile(sourceFilename, 1024); // 1 KB file
        {
            REQUIRE_THROWS_AS(sharedMemoryLaunch(sourceFilename, targetFilename, true), std::runtime_error);
        }

        REQUIRE(fs::file_size(sourceFilename) == fs::file_size(targetFilename));
        REQUIRE(compareFiles(sourceFilename, targetFilename));
    }

    // Clean up test files
    std::remove(sourceFilename.c_str());
    std::remove(targetFilename.c_str());
}