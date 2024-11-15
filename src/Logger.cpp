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
    _logsDir = std::string("logs/logs_") + timestamp;

    // Vérifier et créer le répertoire timestampé s'il n'existe pas
    if (stat(_logsDir.c_str(), &st) != 0) {
            mkdir(_logsDir.c_str(), 0755);
    }

    // Construire les noms de fichiers avec le répertoire timestampé
    std::string debugFilename = _logsDir + "/debug.log";
    std::string infoFilename = _logsDir + "/info.log";
    std::string warningFilename = _logsDir + "/warning.log";
    std::string errorFilename = _logsDir + "/error.log";

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
    } else {
        this->log(INFO, std::string("Starting Program logs at : ") + timestamp);
    }
}

#include <iostream>
#include <string>
#include <cstdio> // pour std::remove

Logger::~Logger() {
    std::string user_input;

    // Boucle pour valider l'entrée
    while (true) {
        std::cout << "Would you like to [K]eep this session logs or [D]elete? (d/k): ";
        std::getline(std::cin, user_input);

        if (user_input == "d" || user_input == "D" || user_input == "k" || user_input == "K") {
            if (debugFile.is_open())
                debugFile.close();
            if (infoFile.is_open())
                infoFile.close();
            if (warningFile.is_open())
                warningFile.close();
            if (errorFile.is_open())
                errorFile.close();
            if (user_input == "d" || user_input == "D") {
                if (std::remove(std::string(_logsDir + "/debug.log").c_str()) == 0)
                    std::cout << "Debug log deleted successfully.\n";
                if (std::remove(std::string(_logsDir + "/info.log").c_str()) == 0)
                    std::cout << "Info log deleted successfully.\n";
                if (std::remove(std::string(_logsDir + "/warning.log").c_str()) == 0)
                    std::cout << "Warning log deleted successfully.\n";
                if (std::remove(std::string(_logsDir + "/error.log").c_str()) == 0)
                    std::cout << "Error log deleted successfully.\n";
                break;
            } else {
                std::cout << "Invalid option. Please enter 'd' to delete or 'k' to keep.\n";
            }
        }
    }
}

void Logger::log(LoggerLevel level, const std::string& message) {
    if (repeatCount == 0) {
        std::string output = getLevelString(level) + ": " + message + "\n";
        writeToLogs(level, output);

        lastMessage = message;
        lastLevel = level;
        repeatCount = 1;
    } else if (message == lastMessage && level == lastLevel) {
        repeatCount++;
    } else {
        if (repeatCount > 1) {
            // Output the summary of hidden lines
            std::string hiddenMessage = "[ " + to_string(repeatCount - 2) + " similar lines hidden ]\n";
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
