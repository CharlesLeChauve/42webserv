// Logger.cpp

#include "Logger.hpp"
#include <iostream>

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    debugFile.open("logs/debug.log", std::ofstream::out | std::ofstream::trunc);
    infoFile.open("logs/info.log", std::ofstream::out | std::ofstream::trunc);
    warningFile.open("logs/warning.log", std::ofstream::out | std::ofstream::trunc);
    errorFile.open("logs/error.log", std::ofstream::out | std::ofstream::trunc);

    if (!debugFile.is_open() || !infoFile.is_open() || !warningFile.is_open() || !errorFile.is_open()) {
        std::cerr << "Erreur lors de l'ouverture des fichiers de log." << std::endl;
    }
}

Logger::~Logger() {
    if (debugFile.is_open())
        debugFile.close();
    if (infoFile.is_open())
        infoFile.close();
    if (warningFile.is_open())
        warningFile.close();
    if (errorFile.is_open())
        errorFile.close();
}

void Logger::log(LoggerLevel level, const std::string& message) {
    std::string output = getLevelString(level) + ": " + message + "\n";

    // Écrire dans debug.log
    if (debugFile.is_open()) {
        debugFile << output;
        debugFile.flush();
    }

    // Écrire dans info.log si le niveau est INFO
    if ((level == INFO || level == WARNING || level == ERROR) && infoFile.is_open()) {
        infoFile << output;
        infoFile.flush();
    }

    // Écrire dans warning.log si le niveau est WARNING ou ERROR
    if ((level == WARNING || level == ERROR) && warningFile.is_open()) {
        warningFile << output;
        warningFile.flush();
    }

    // Écrire dans error.log si le niveau est ERROR
    if (level == ERROR && errorFile.is_open()) {
        errorFile << output;
        errorFile.flush();
    }
}

std::string Logger::getLevelString(LoggerLevel level) {
    switch (level) {
        case DEBUG:   return "DEBUG";
        case INFO:    return "INFO";
        case WARNING: return "WARNING";
        case ERROR:   return "ERROR";
        default:      return "UNKNOWN";
    }
}