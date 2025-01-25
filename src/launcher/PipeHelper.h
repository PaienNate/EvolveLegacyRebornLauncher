
#ifndef EVOLVELEGACYREBORNLAUNCHER_PIPEHELPER_H
#define EVOLVELEGACYREBORNLAUNCHER_PIPEHELPER_H

#include <string>

namespace PIPE {
    bool init();
    void destroy();
    bool sendMessage(const std::string& message, unsigned long* bytesWritten);
    bool readMessage(void* buffer, unsigned long size, unsigned long* bytesRead);
}

#endif //EVOLVELEGACYREBORNLAUNCHER_PIPEHELPER_H
