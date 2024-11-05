// Logger.cpp

#include <ctime>
#include "Logger.hpp"
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : repeatCount(0), logToStderr(false) {
    struct stat st;
    if (stat("logs", &st) != 0) {
        mkdir("logs", 0755);
    }

    // Obtenir la date et l'heure actuelles
    std::time_t now = std::time(NULL);
    std::tm* now_tm = std::localtime(&now);

    // Formater le timestamp
    char timestamp[20];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", now_tm);

    // Créer le nom du répertoire avec le timestamp
    std::string logsDir = std::string("logs/logs_") + timestamp;

    // Vérifier et créer le répertoire timestampé s'il n'existe pas
    if (stat(logsDir.c_str(), &st) != 0) {
        #ifdef _WIN32
            _mkdir(logsDir.c_str());
        #else
            mkdir(logsDir.c_str(), 0755);
        #endif
    }

    // Construire les noms de fichiers avec le répertoire timestampé
    std::string debugFilename = logsDir + "/debug.log";
    std::string infoFilename = logsDir + "/info.log";
    std::string warningFilename = logsDir + "/warning.log";
    std::string errorFilename = logsDir + "/error.log";

	debugFile.open(debugFilename.c_str(), std::ofstream::out | std::ofstream::trunc);
    infoFile.open(infoFilename.c_str(), std::ofstream::out | std::ofstream::trunc);
    warningFile.open(warningFilename.c_str(), std::ofstream::out | std::ofstream::trunc);
    errorFile.open(errorFilename.c_str(), std::ofstream::out | std::ofstream::trunc);

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

void Logger::writeToLogs(LoggerLevel level, const std::string& output) {
    if (logToStderr) {
        std::cerr << output;
        std::cerr.flush();
        return;
    }

    if (debugFile.is_open()) {
        debugFile << output;
        debugFile.flush();
    }

    if ((level == INFO || level == WARNING || level == ERROR) && infoFile.is_open()) {
        infoFile << output;
        infoFile.flush();
    }

    if ((level == WARNING || level == ERROR) && warningFile.is_open()) {
        warningFile << output;
        warningFile.flush();
    }

    if (level == ERROR && errorFile.is_open()) {
        errorFile << output;
        errorFile.flush();
    }
}

void Logger::log(LoggerLevel level, const std::string& message) {
    if (repeatCount == 0) {
        // First message, output it
        std::string output = getLevelString(level) + ": " + message + "\n";
        writeToLogs(level, output);

        // Update the last message and level
        lastMessage = message;
        lastLevel = level;
        repeatCount = 1;
    } else if (message == lastMessage && level == lastLevel) {
        // Same message as before, increment the counter
        repeatCount++;
    } else {
        // Different message
        if (repeatCount > 1) {
            // Output the summary of hidden lines
            std::string hiddenMessage = "[ " + std::to_string(repeatCount - 2) + " similar lines hidden ]\n";
            writeToLogs(lastLevel, hiddenMessage);

            // Output the last repeated message
            std::string lastOutput = getLevelString(lastLevel) + ": " + lastMessage + "\n";
            writeToLogs(lastLevel, lastOutput);
        }

        // Output the new message
        std::string output = getLevelString(level) + ": " + message + "\n";
        writeToLogs(level, output);

        // Reset the repeat counter and update the last message and level
        repeatCount = 1;
        lastMessage = message;
        lastLevel = level;
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