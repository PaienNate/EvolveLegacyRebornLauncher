
#include "PipeHelper.h"
#include <string>
#include <filesystem>
#include <windows.h>

namespace PIPE {
    HANDLE hPipe;

    bool init() {
        hPipe = CreateFileA(
                R"(\\.\pipe\EvolveN3NManager)",  // Pipe name
                GENERIC_READ | GENERIC_WRITE,  // Read and write access
                0,                            // No sharing
                nullptr,                         // Default security attributes
                OPEN_EXISTING,                // Open existing pipe
                0,                            // Default attributes
                nullptr);                        // No template file

        return hPipe != INVALID_HANDLE_VALUE;
    }

    void destroy() {
        CloseHandle(hPipe);
    }

    bool sendMessage(const std::string& message, unsigned long* bytesWritten) {
        return WriteFile(hPipe, message.c_str(), message.length(), bytesWritten, nullptr);
    }

    bool readMessage(void* buffer, unsigned long size, unsigned long* bytesRead) {
        ZeroMemory(buffer, size);
        return ReadFile(hPipe, buffer, size, bytesRead, nullptr);
    }
}