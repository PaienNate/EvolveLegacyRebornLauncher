#include <windows.h>
#include <iostream>
#include <filesystem>
#include <thread>
#include "log/Logger.h"

//get path we execute from
static TCHAR szUnquotedPath[MAX_PATH];
static const bool foundPath = GetModuleFileName( nullptr, szUnquotedPath, MAX_PATH );
static std::filesystem::path fullpath(szUnquotedPath);
static const auto folder = fullpath.remove_filename().string();

static LOGGER logger(folder);

int main(int argc, char* argv[]) {
    logger.info("Connecting to N3N Network!");

    /*N3N::connect();
    WaitForSingleObject( N3N::pi.hProcess, 5000 );
    std::cout << "Shutting down!" << std::endl;
    N3N::shutdown();
    return 0;*/

    // Connect to the named pipe
    HANDLE hPipe = CreateFile(
            R"(\\.\pipe\EvolveN3NManager)",  // Pipe name
            GENERIC_READ | GENERIC_WRITE,  // Read and write access
            0,                            // No sharing
            nullptr,                         // Default security attributes
            OPEN_EXISTING,                // Open existing pipe
            0,                            // Default attributes
            nullptr);                        // No template file

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Failed to connect to pipe, error: " << GetLastError() << std::endl;
        return 1;
    }

    // Send a message to the service
    std::string message = "connect";
    DWORD bytesWritten;
    if (!WriteFile(hPipe, message.c_str(), message.length(), &bytesWritten, nullptr))
    {
        logger.error("Failed to write to pipe, error: " + std::to_string(GetLastError()));
        CloseHandle(hPipe);
        return 1;
    }

    logger.info("Message sent to service: " + message);

    // Read the response from the service
    char buffer[512];
    DWORD bytesRead;
    if (!ReadFile(hPipe, buffer, sizeof(buffer), &bytesRead, nullptr))
    {
        logger.error("Failed to read from pipe, error: " + std::to_string(GetLastError()));
        CloseHandle(hPipe);
        return 1;
    }

    // Print the response
    logger.info("Received from service: " + std::string(buffer, bytesRead));

    std::this_thread::sleep_for(std::chrono::seconds(5));

    message = "disconnect";
    if (!WriteFile(hPipe, message.c_str(), message.length(), &bytesWritten, nullptr))
    {
        logger.error("Failed to write to pipe, error: " + std::to_string(GetLastError()));
        CloseHandle(hPipe);
        return 1;
    }

    logger.info("Message sent to service: " + message);

    ZeroMemory(&buffer, sizeof(buffer));

    if (!ReadFile(hPipe, buffer, sizeof(buffer), &bytesRead, nullptr))
    {
        logger.error("Failed to read from pipe, error: " + std::to_string(GetLastError()));
        CloseHandle(hPipe);
        return 1;
    }

    // Print the response
    logger.info("Received from service: " + std::string(buffer, bytesRead));

    // Close the pipe handle
    CloseHandle(hPipe);
    return 0;
}
