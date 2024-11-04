// Logger.hpp

#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "Utils.hpp"
#include <string>
#include <fstream>

class Logger {
public:
    // Méthode pour obtenir l'instance singleton
    static Logger& instance();

    // Méthode pour enregistrer un message avec un niveau spécifique
    void log(LoggerLevel level, const std::string& message);

private:
    // Constructeur et destructeur privés pour le pattern singleton
    Logger();
    ~Logger();

    // Empêcher la copie et l'affectation (déclarés mais non définis)
    Logger(const Logger&);
    Logger& operator=(const Logger&);

    // Méthode pour obtenir la chaîne de caractères correspondant au niveau
    std::string getLevelString(LoggerLevel level);

    // Fichiers de log
    std::ofstream debugFile;
    std::ofstream infoFile;
    std::ofstream warningFile;
    std::ofstream errorFile;

	bool logToStderr;
};

#endif // LOGGER_HPP