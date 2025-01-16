#include <fstream>
#include <iostream>
#include "Logger.h"

#ifndef LOGGER_FILENAME
#define LOGGER_FILENAME "Log.log"
#endif

LOGGER::LOGGER(const std::string &path) {
    LOGGER::path = path;
}

// Base logging impl, info, warn and error wrap this and adjust formatting
void LOGGER::log(const std::string& message) {
    std::ofstream logFile(path + LOGGER_FILENAME, std::ios::app);
    if (logFile.is_open()) {
        logFile << message << std::endl;
        logFile.close();
    }
}

// simple logging functions, also log file and line number where we call the function from to easier identify errors
void LOGGER::info(const std::string &message, bool stdOut, const char* file, const int line) {
    LOGGER::log((std::string) "[INFO]\t[" + file + ":" + std::to_string(line) + "]\t" + message);
    if (stdOut)
        LOGGER::cinfo(message, file, line);
}

void LOGGER::warn(const std::string &message, bool stdOut, const char* file, const int line) {
    LOGGER::log((std::string) "[WARN]\t[" + file + ":" + std::to_string(line) + "]\t" + message);
    if (stdOut)
        LOGGER::cwarn(message, file, line);
}

void LOGGER::error(const std::string &message, bool stdOut, const char* file, const int line) {
    LOGGER::log((std::string) "[ERROR]\t[" + file + ":" + std::to_string(line) + "]\t" + message);
    if (stdOut)
        LOGGER::cerror(message, file, line);
}

// simple logging functions but only write to std::cout
void LOGGER::cinfo(const std::string &message, const char *file, int line) {
    std::cout << "[INFO]\t[" << file << ":" << line << "]\t" << message << std::endl;
}

void LOGGER::cwarn(const std::string &message, const char *file, int line) {
    std::cout << "\033[1;33m[WARN]\t[" << file << ":" << line << "]\t" << message << "\033[0m" << std::endl;
}

void LOGGER::cerror(const std::string &message, const char *file, int line) {
    std::cout << "\033[1;31m[ERROR]\t[" << file << ":" << line << "]\t" << message << "\033[0m" << std::endl;
}