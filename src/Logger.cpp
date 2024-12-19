// Logger.cpp

#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <cstdio>
#include <ctime>
#include "Logger.hpp"

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : repeatCount(0), logToStderr(false), mute(false) {
    struct stat st;
    if (stat("logs", &st) != 0) {
        mkdir("logs", 0755);
    }

    std::time_t now = std::time(NULL);
    std::tm* now_tm = std::localtime(&now);
    char timestamp[20];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", now_tm);

    _logsDir = std::string("logs/logs_") + timestamp;

    if (stat(_logsDir.c_str(), &st) != 0) {
            mkdir(_logsDir.c_str(), 0755);
    }
    std::string debugFilename = _logsDir + "/debug.log";
    std::string infoFilename = _logsDir + "/info.log";
    std::string warningFilename = _logsDir + "/warning.log";
    std::string errorFilename = _logsDir + "/error.log";

	debugFile.open(debugFilename.c_str(), std::ofstream::out | std::ofstream::trunc);
    infoFile.open(infoFilename.c_str(), std::ofstream::out | std::ofstream::trunc);
    warningFile.open(warningFilename.c_str(), std::ofstream::out | std::ofstream::trunc);
    errorFile.open(errorFilename.c_str(), std::ofstream::out | std::ofstream::trunc);

    if (!debugFile.is_open() || !infoFile.is_open() || !warningFile.is_open() || !errorFile.is_open()) {
        std::cerr << "Erreur lors de l'ouverture des fichiers de log. Les logs seront redirigés vers std::cerr." << std::endl;
        logToStderr = true;

        if (debugFile.is_open()) debugFile.close();
        if (infoFile.is_open()) infoFile.close();
        if (warningFile.is_open()) warningFile.close();
        if (errorFile.is_open()) errorFile.close();
    } else {
        this->log(INFO, std::string("Starting Program logs at : ") + timestamp);
    }
}

Logger::~Logger() {
    std::string user_input;

    // Boucle pour valider l'entrée
    while (true) {
        if (mute)
            break;
        std::cout << "Would you like to [K]eep this session logs or [D]elete? (d/k): ";
        std::getline(std::cin, user_input);

             if (std::cin.eof()) {
            std::cout << "\n EOF received. Logs will be kept by default.\n";
            break;
        }

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
				if (system((std::string(std::string("rm -rf ") + _logsDir)).c_str()))
					std::cout << "Failed to remove dir : " << _logsDir << std::endl;
            }
            break;
        } else {
            std::cout << "Invalid option. Please enter 'd' to delete or 'k' to keep.\n";
        }
    }
}

void Logger::log(LoggerLevel level, const std::string& message) {
    if (mute)
        return ;
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
            std::string hiddenMessage = "[ " + to_string(repeatCount - 2) + " similar lines hidden ]\n";
            writeToLogs(lastLevel, hiddenMessage);
            std::string lastOutput = getLevelString(lastLevel) + ": " + lastMessage + "\n";
            writeToLogs(lastLevel, lastOutput);
        }
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

void Logger::setMute(bool mute_flag) {
    log(INFO, "Muting this Logger");
    mute = mute_flag;
}
