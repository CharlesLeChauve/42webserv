/*****************************************************
 * Logger.hpp
 * 
 * Description:
 * ------------
 * La classe Logger gère l'enregistrement de messages de log 
 * dans différents fichiers en fonction du niveau de sévérité 
 * du message (DEBUG, INFO, WARNING, ERROR). Elle prend en 
 * charge l'écriture simultanée dans plusieurs fichiers selon 
 * la logique suivante :
 * 
 * - ERROR : écrit dans error.log, warning.log, info.log, et debug.log
 * - WARNING : écrit dans warning.log, info.log, et debug.log
 * - INFO : écrit dans info.log et debug.log
 * - DEBUG : écrit uniquement dans debug.log
 * 
 * La classe propose deux principales méthodes d’écriture:
 * 
 * - `log(LoggerLevel, const std::string&)` :
 *   Écrit un message directement dans les fichiers appropriés 
 *   en fonction du niveau spécifié.
 *   Gère les lignes répétées et les remplace comme suit:
 *      line 1
 *      [<n - 2> similar lines hidden]
 *      line n
 *
 * 
 * - `getLogStreams(LoggerLevel)` :
 *   Retourne une liste de flux (`std::ostream*`) permettant 
 *   d'écrire dans tous les fichiers nécessaires pour un niveau 
 *   de log donné. Cette méthode permet d'utiliser l'opérateur 
 *   `<<` pour formater directement les messages.
 * 
 * Utilisation:
 * ------------
 * 1. Instanciez un objet Logger.
 * 2. Appelez `writeToLogs()` pour écrire directement des messages
 *    de log, ou `getLogStreams()` pour obtenir un ensemble de flux
 *    et écrire dans plusieurs fichiers en cascade.
 * 
 * Exemple d'utilisation :
 * -----------------------
 *     Logger logger;
 *     logger.writeToLogs(ERROR, "Critical failure occurred.");
 * 
 *     for (std::ostream* stream : logger.getLogStreams(INFO)) {
 *         *stream << "Informational message.\n";
 *     }
 * 
 * Configuration:
 * --------------
 * - Les fichiers de log sont automatiquement ouverts lors de 
 *   l'initialisation de la classe et fermés lors de la destruction 
 *   de l'objet Logger.
 * - Si `logToStderr` est activé, les messages sont redirigés 
 *   vers `std::cerr` au lieu des fichiers.
 * 
 * Notes:
 * ------
 * - `getLogStreams()` garantit le respect de la cascade d’écriture 
 *   même si les niveaux de log varient.
 * - En cas d'échec d'ouverture de fichier, les messages sont 
 *   redirigés vers `std::cerr`.
 * 
 ****************************************************/

#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "Utils.hpp"
#include <string>
#include <fstream>
#include <iostream>
#include <vector>

class Logger {
public:
    class LoggerStream {
    public:
        LoggerStream(const std::vector<std::ostream*>& streams) : streams_(streams) {}

        // Surcharge de l'opérateur << pour les types génériques
        template<typename T>
        LoggerStream& operator<<(const T& value) {
            for (std::vector<std::ostream*>::iterator it = streams_.begin(); it != streams_.end(); ++it) {
                *(*it) << value;
            }
            return *this;
        }

        // Surcharge pour les manipulateurs (comme std::endl)
        LoggerStream& operator<<(std::ostream& (*manip)(std::ostream&)) {
            for (std::vector<std::ostream*>::iterator it = streams_.begin(); it != streams_.end(); ++it) {
                manip(*(*it));
            }
            return *this;
        }

    private:
        std::vector<std::ostream*> streams_;
    };


    // Méthode pour obtenir l'instance singleton
    static Logger& instance();

    // Méthode pour enregistrer un message avec un niveau spécifique
    void log(LoggerLevel level, const std::string& message);


    template <typename T>
    void writeToLogs(LoggerLevel level, const T& output) {
        if (logToStderr) {
            std::cerr << output;
            std::cerr.flush();
            return;
        }
        switch (level)
        {
            case ERROR:
                errorFile << output;
                errorFile.flush();
                //fall through
            case WARNING:
                warningFile << output;
                warningFile.flush();
                //fall through
            case INFO:
                infoFile << output;
                infoFile.flush();
                //fall through
            case DEBUG:
                debugFile << output;
                debugFile.flush();
                break;
            default:
                break;
        }
    }

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

    std::string lastMessage;
    LoggerLevel lastLevel;
    int repeatCount;

    std::string _logsDir;

	bool logToStderr;
};

#endif // LOGGER_HPP
