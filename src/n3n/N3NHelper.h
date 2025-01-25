#ifndef EVOLVELEGACYREBORNLAUNCHER_N3NHELPER_H
#define EVOLVELEGACYREBORNLAUNCHER_N3NHELPER_H

#include <windows.h>
#include <string>

namespace N3N {
    extern STARTUPINFOA si;
    extern PROCESS_INFORMATION pi;
    extern bool connected;
    extern HANDLE hMapFile;
    extern BOOL* n3n_keep_on_running;
    bool connect();
    bool connect(const std::string&);
    bool connectWithoutElevation();
    bool connectWithoutElevation(const std::string& network);
    void shutdown();
    void shutdownShellExecute();
}

#endif //EVOLVELEGACYREBORNLAUNCHER_N3NHELPER_H