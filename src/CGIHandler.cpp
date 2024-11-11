// CGIHandler.cpp
#include "CGIHandler.hpp"
#include "HTTPResponse.hpp"
#include "Server.hpp"
#include <unistd.h>  // For fork, exec, pipe
#include <sys/wait.h>  // For waitpid
#include <iostream>
#include <cstring>  // For strerror
#include "Logger.hpp"
#include "Utils.hpp"

CGIHandler::CGIHandler() /*: _server(server)*/ {}

CGIHandler::~CGIHandler() {}

// Implémentation de la méthode endsWith
bool CGIHandler::endsWith(const std::string& str, const std::string& suffix) const {
    if (str.length() >= suffix.length()) {
        return (0 == str.compare(str.length() - suffix.length(), suffix.length(), suffix));
    } else {
        return false;
    }
}

std::string CGIHandler::executeCGI(const std::string& scriptPath, const HTTPRequest& request) {
    Logger::instance().log(DEBUG, "executeCGI: Executing script: " + scriptPath);

    std::string interpreter_directory_path = "";
    #ifdef __APPLE__
        interpreter_directory_path = "/opt/homebrew/bin/";
    #elif defined(__linux__)
        interpreter_directory_path = "/usr/bin/";
    #else
        // Handle other OS or set a default path
        interpreter_directory_path = "/usr/bin/";
    #endif

    std::string interpreter_name = "";
    if (endsWith(scriptPath, ".sh")) {
        interpreter_name = "bash";
    } else if (endsWith(scriptPath, ".php")) {
        interpreter_name = "php-cgi";
    }

    std::string interpreter = interpreter_directory_path + interpreter_name;
    // .cgi utilisera le shebang du script

    Logger::instance().log(DEBUG, "executeCGI: Interpreter = " + (interpreter.empty() ? "Shebang" : interpreter));
    std::string fullPath = scriptPath;
    HTTPResponse response;

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        Logger::instance().log(ERROR, std::string("executeCGI: Pipe failed: ") + strerror(errno));
        return response.beError(500, "Internal Server Error: Unable to create pipe.").toString();
    }

    int pipefd_in[2];  // Pipe pour l'entrée standard
    if (pipe(pipefd_in) == -1) {
        Logger::instance().log(ERROR, std::string("executeCGI: Pipe fir STDIN failed: ") + strerror(errno));
        return response.beError(500, "Internal Server Error: Unable to create stdin pipe.").toString();
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Processus enfant : exécution du script CGI
        close(pipefd[0]);  // Fermer l'extrémité de lecture du pipe pour stdout
        dup2(pipefd[1], STDOUT_FILENO);  // Rediriger stdout vers le pipe
        close(pipefd_in[1]);  // Fermer l'extrémité d'écriture du pipe pour stdin
        dup2(pipefd_in[0], STDIN_FILENO);  // Rediriger stdin vers le pipe

        setupEnvironment(request, fullPath);

        // Exécuter le script
        if (!interpreter.empty()) {
            execl(interpreter.c_str(), interpreter.c_str(), fullPath.c_str(), NULL);
        } else {
            execl(fullPath.c_str(), fullPath.c_str(), NULL);
        }
        Logger::instance().log(ERROR, std::string("executeCGI: Failed to execute CGI script: ") + fullPath + std::string(". Error: ") + strerror(errno));
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Processus parent : gestion de la sortie du CGI
        close(pipefd[1]);
        close(pipefd_in[0]);

        if (request.getMethod() == "POST") {
            int bytes_written = write(pipefd_in[1], request.getBody().c_str(), request.getBody().size());
            if (bytes_written == -1) {
                Logger::instance().log(WARNING, "500 error (Internal Server Error) for writing"); // To specify i'm tired.
                return response.beError(500, "500 error (Internal Server Error) for writing").toString(); // Internal server error
            }
        }
        close(pipefd_in[1]);

        char buffer[1024];
        std::string cgiOutput;

        ssize_t bytesRead;
        while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            cgiOutput.append(buffer, bytesRead);
        }
        close(pipefd[0]);

        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            int exitCode = WEXITSTATUS(status);
            if (exitCode != 0) {
                Logger::instance().log(ERROR, std::string("executeCGI: CGI script exited with code: ") + to_string(exitCode));
                return response.beError(500, "Internal Server Error: CGI script failed.").toString();
            }
        }

        // Analyse des en-têtes CGI
        size_t statusPos = cgiOutput.find("Status:");
        if (statusPos != std::string::npos) {
            size_t endOfStatusLine = cgiOutput.find("\r\n", statusPos);
            std::string statusLine = cgiOutput.substr(statusPos, endOfStatusLine - statusPos);
            int statusCode = std::atoi(statusLine.substr(8, 3).c_str());

            std::string errorMessage;
            size_t messageStartPos = statusLine.find(" ");
            if (messageStartPos != std::string::npos) {
                errorMessage = statusLine.substr(messageStartPos + 1);
            }

            return response.beError(statusCode, errorMessage).toString();
        }

        // Si aucun en-tête "Status:" n'est trouvé, renvoyer la réponse CGI directement
        if (cgiOutput.find("HTTP/1.1") == std::string::npos) {
            cgiOutput = "HTTP/1.1 200 OK\r\n" + cgiOutput;
        }
        Logger::instance().log(DEBUG, std::string("executeCGI: CGI Output:\n") + cgiOutput);
        return cgiOutput;
    } else {
        Logger::instance().log(ERROR, std::string("executeCGI: Fork failed: ") + strerror(errno));
        return response.beError(500, "Internal Server Error: Fork failed.").toString();
    }
}


void CGIHandler::setupEnvironment(const HTTPRequest& request, std::string scriptPath) {
	if (request.getMethod() == "POST") {
		setenv("REQUEST_METHOD", "POST", 1);  // Définir POST comme méthode
		setenv("CONTENT_TYPE", "application/x-www-form-urlencoded", 1); // Valeur par défaut
		setenv("CONTENT_LENGTH", to_string(request.getBody().size()).c_str(), 1);  // Définir la taille du corps de la requête
	} else {
		setenv("REQUEST_METHOD", "GET", 1);  // Définir GET comme méthode par défaut
	}
    setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
    setenv("REQUEST_METHOD", request.getMethod().c_str(), 1);
    setenv("SCRIPT_FILENAME", scriptPath.c_str(), 1);
    setenv("SCRIPT_NAME", scriptPath.c_str(), 1);
    setenv("QUERY_STRING", request.getQueryString().c_str(), 1);
    setenv("CONTENT_TYPE", request.getStrHeader("Content-Type").c_str(), 1);
    setenv("CONTENT_LENGTH", to_string(request.getBody().size()).c_str(), 1);
    setenv("REDIRECT_STATUS", "200", 1); // Nécessaire pour php-cgi
    setenv("SERVER_PROTOCOL", "HTTP/1.1", 1);
    //setenv("REMOTE_ADDR", request.getClientIP().c_str(), 1);
    setenv("SERVER_NAME", request.getStrHeader("Host").c_str(), 1);
    //setenv("SERVER_PORT", request.getPort().c_str(), 1);
}
