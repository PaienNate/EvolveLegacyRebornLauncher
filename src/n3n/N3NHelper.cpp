#include "N3NHelper.h"
#include "../log/Logger.h"
#include <filesystem>
#include <windows.h>

static TCHAR szUnquotedPath[MAX_PATH];
static const bool foundPath = GetModuleFileName( nullptr, szUnquotedPath, MAX_PATH );
static std::filesystem::path fullpath(szUnquotedPath);
static const auto folder = fullpath.remove_filename().string();

static LOGGER logger(folder);

#define MAPPED_FILE_NAME "Global\\KeepN3NRunning"
#define MAPPED_FILE_SIZE sizeof(BOOL)

namespace N3N {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    bool connected = false;
    HANDLE hMapFile;
    BOOL* n3n_keep_on_running;

    bool connect() {
        return connect("main");
    }

    bool connect(const std::string& network) {
        if (connected) {
            shutdown();
        }

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        auto cmdline = ("\"" + folder + R"(bin\n3n-edge.exe" start evolve -c )" + network + R"( -l "evolve.a1btraum.de:27335")");

        if (!std::filesystem::exists(folder + R"(bin\n3n-edge.exe)")) {
            logger.error("Failed to locate n3n-edge.exe. Aborting...");
            SetLastError(ERROR_FILE_NOT_FOUND);
            return false;
        }

        if(!CreateProcessA(
                nullptr,
                (LPSTR) cmdline.c_str(),
                nullptr,
                nullptr,
                FALSE,
                DETACHED_PROCESS,
                nullptr,
                nullptr,
                &si,
                &pi)
        ) {
            logger.error("CreateProcess failed: " + std::to_string(GetLastError()), true);
            return false;
        }

        logger.info("Waiting for 2s for N3N to be ready");
        Sleep(2000);

        logger.info("Connecting to N3N Network!", true);

        // Open the existing memory-mapped file
        hMapFile = OpenFileMapping(
                FILE_MAP_ALL_ACCESS, FALSE, MAPPED_FILE_NAME
        );

        if (hMapFile == nullptr) {
            logger.error("OpenFileMapping failed!");
            return false;
        }

        // Map the shared memory
        n3n_keep_on_running = (BOOL*)MapViewOfFile(
                hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, MAPPED_FILE_SIZE
        );

        if (n3n_keep_on_running == nullptr) {
            logger.error("MapViewOfFile failed!");
            CloseHandle(hMapFile);
            return false;
        }

        connected = true;
        return true;
    }

    void shutdown() {
        if (connected) {
            *n3n_keep_on_running = false;

            //logger.info("Waiting for 1 second for N3N to fully shutdown");
            Sleep(1000);

            UnmapViewOfFile(n3n_keep_on_running);
            CloseHandle(hMapFile);

            // Close process and thread handles.
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            logger.info("Disconnect complete");

            connected = false;
        }
    }
}