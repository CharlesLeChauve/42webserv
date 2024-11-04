// Logger.cpp

#include "Logger.hpp"
#include <iostream>

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : logToStderr(false) {
    // Ouvrir les fichiers de log
    debugFile.open("logs/debug.log", std::ofstream::out | std::ofstream::trunc);
    infoFile.open("logs/info.log", std::ofstream::out | std::ofstream::trunc);
    warningFile.open("logs/warning.log", std::ofstream::out | std::ofstream::trunc);
    errorFile.open("logs/error.log", std::ofstream::out | std::ofstream::trunc);

    // Vérifier si tous les fichiers sont ouverts avec succès
    if (!debugFile.is_open() || !infoFile.is_open() || !warningFile.is_open() || !errorFile.is_open()) {
        std::cerr << "Erreur lors de l'ouverture des fichiers de log. Les logs seront redirigés vers std::cerr." << std::endl;
        logToStderr = true;

        // Fermer les fichiers éventuellement ouverts
        if (debugFile.is_open()) debugFile.close();
        if (infoFile.is_open()) infoFile.close();
        if (warningFile.is_open()) warningFile.close();
        if (errorFile.is_open()) errorFile.close();
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

	if (logToStderr) {
        // Rediriger tous les logs vers std::cerr
        std::cerr << output;
        std::cerr.flush();
        return;
    }

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