
#ifndef EVOLVELEGACYREBORNLAUNCHER_LOGGER_H
#define EVOLVELEGACYREBORNLAUNCHER_LOGGER_H

#include <string>

class LOGGER {
public:
    explicit LOGGER(const std::string &path);

    //file loggers
    void info(const std::string &message, bool stdOut = true, const char* file = __builtin_FILE(), int line = __builtin_LINE());
    void warn(const std::string &message, bool stdOut = true, const char* file = __builtin_FILE(), int line = __builtin_LINE());
    void error(const std::string &message, bool stdOut = true, const char* file = __builtin_FILE(), int line = __builtin_LINE());

    //console loggers, static as they don't depend on path
    static void cinfo(const std::string &message, const char* file = __builtin_FILE(), int line = __builtin_LINE());
    static void cwarn(const std::string &message, const char* file = __builtin_FILE(), int line = __builtin_LINE());
    static void cerror(const std::string &message, const char* file = __builtin_FILE(), int line = __builtin_LINE());


private:
    std::string path;

    //generic file logger
    void log(const std::string &message);
};

#endif //EVOLVELEGACYREBORNLAUNCHER_LOGGER_H
