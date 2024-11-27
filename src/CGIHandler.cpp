// CGIHandler.cpp
#include "CGIHandler.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstring>
#include "Logger.hpp"

CGIHandler::CGIHandler() : _pid(-1), _exitStatus(-1), _isDone(false), _launched(false), _sendBuffer(""), _bytesSent(0), _receiveBuffer(""), _outputComplete(false) {
    _stdin_pipe[0] = _stdin_pipe[1] = -1;
    _stdout_pipe[0] = _stdout_pipe[1] = -1;
}

CGIHandler::~CGIHandler() {
    if (_stdin_pipe[0] != -1) close(_stdin_pipe[0]);
    if (_stdin_pipe[1] != -1) close(_stdin_pipe[1]);
    if (_stdout_pipe[0] != -1) close(_stdout_pipe[0]);
    if (_stdout_pipe[1] != -1) close(_stdout_pipe[1]);
}

// Implement the endsWith method
bool CGIHandler::endsWith(const std::string& str, const std::string& suffix) const {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool CGIHandler::startCGI(const std::string& scriptPath, const HTTPRequest& request) {
    Logger::instance().log(DEBUG, "startCGI: Starting script: " + scriptPath);

    std::string interpreter_directory_path = "";
    #ifdef __APPLE__
        interpreter_directory_path = "/opt/homebrew/bin/";
    #elif defined(__linux__)
        interpreter_directory_path = "/usr/bin/";
    #else
        interpreter_directory_path = "/usr/bin/";
    #endif

    std::string interpreter_name = "";
    if (endsWith(scriptPath, ".sh")) {
        interpreter_name = "bash";
    } else if (endsWith(scriptPath, ".php")) {
        interpreter_name = "php-cgi";
    }

    std::string interpreter = interpreter_directory_path + interpreter_name;

    Logger::instance().log(DEBUG, "startCGI: Interpreter = " + (interpreter.empty() ? "Shebang" : interpreter));

    // Create pipes for stdin and stdout
    if (pipe(_stdin_pipe) == -1 || pipe(_stdout_pipe) == -1) {
        Logger::instance().log(ERROR, "startCGI: Failed to create pipes: " + std::string(to_string(errno)));
        return false;
    }

    // Set pipes to non-blocking mode
    fcntl(_stdout_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(_stdin_pipe[1], F_SETFL, O_NONBLOCK);

    _pid = fork();
    if (_pid == 0) {
        // Child process
        // Redirect stdin and stdout
        dup2(_stdin_pipe[0], STDIN_FILENO);
        dup2(_stdout_pipe[1], STDOUT_FILENO);

        // Close unused file descriptors
        close(_stdin_pipe[0]);
        close(_stdin_pipe[1]);
        close(_stdout_pipe[0]);
        close(_stdout_pipe[1]);

        setupEnvironment(request, scriptPath);

        // Execute the script
        if (!interpreter.empty()) {
            execl(interpreter.c_str(), interpreter.c_str(), scriptPath.c_str(), NULL);
        } else {
            execl(scriptPath.c_str(), scriptPath.c_str(), NULL);
        }

        Logger::instance().log(ERROR, "startCGI: Failed to execute script: " + std::string(to_string(errno)));
        exit(EXIT_FAILURE);
    } else if (_pid > 0) {
        // Parent process
        // Close unused file descriptors
        close(_stdin_pipe[0]);
        close(_stdout_pipe[1]);

        _launched = true;
        _isDone = false;
        _exitStatus = -1;

        return true;
    } else {
        // Fork failed
        Logger::instance().log(ERROR, "startCGI: Fork failed: " + std::string(to_string(errno)));
        return false;
    }
}

ssize_t CGIHandler::writeCGIInput() {
    const char* bufferPtr = _sendBuffer.c_str() + _bytesSent;
    size_t remaining = _sendBuffer.size() - _bytesSent;

    ssize_t bytesSent = write(_stdin_pipe[1], bufferPtr, remaining);
    if (bytesSent > 0) {
        _bytesSent += bytesSent;
    } 
    return bytesSent;
}

void CGIHandler::closeCGIInput() {
    if (_stdin_pipe[1] != -1) {
        close(_stdin_pipe[1]);
        _stdin_pipe[1] = -1;
    }
}

ssize_t CGIHandler::readCGIOutput() {
    if (_stdout_pipe[0] == -1) {
        return -1;
    }
    char buffer[4096];
    ssize_t bytesRead = read(_stdout_pipe[0], buffer, sizeof(buffer));
    if (bytesRead > 0) {
        _receiveBuffer.append(buffer, bytesRead);
    } else if (bytesRead == 0) {
        close(_stdout_pipe[0]);
        _stdout_pipe[0] = -1;
        _outputComplete = true;
    }
    return bytesRead;
}

bool CGIHandler::isCGIDone() {
    if (_isDone) {
        return true;
    }

    int status;
    pid_t result = waitpid(_pid, &status, WNOHANG);
    if (result == 0) {
        // Process is still running
        return false;
    } else if (result == _pid) {
        // Process has exited
        if (WIFEXITED(status)) {
            _exitStatus = WEXITSTATUS(status);
        } else {
            _exitStatus = -1;
        }
        _isDone = true;
        return true;
    } else {
        // waitpid failed
        Logger::instance().log(ERROR, "isCGIDone: waitpid failed: " + std::string(to_string(errno)));
        _isDone = true;
        _exitStatus = -1;
        return true;
    }
}

bool CGIHandler::allSent() {
    return _bytesSent >= (ssize_t)_sendBuffer.size();
}

int CGIHandler::getCGIExitStatus() const {
    return _exitStatus;
}

bool CGIHandler::getLaunched() const {
    return _launched;
}

bool CGIHandler::getOutputComplete() const {
    return _outputComplete;
}

std::string CGIHandler::getCGIResponse() const {
    return _receiveBuffer;
}

pid_t CGIHandler::getPID() const {
    return _pid;
}

int CGIHandler::getOutputFD() const {
    return _stdout_pipe[0];
}

int CGIHandler::getInputFD() const {
    return _stdin_pipe[1];
}

void    CGIHandler::setSendBuffer(const std::string& buffer) {
    _sendBuffer = buffer;
}

// void CGIHandler::setupEnvironment(const HTTPRequest& request, const std::string& scriptPath) {
//     if (request.getMethod() == "POST") {
// 		setenv("REQUEST_METHOD", "POST", 1);  // Définir POST comme méthode
// 		setenv("CONTENT_TYPE", "application/x-www-form-urlencoded", 1); // Valeur par défaut
// 		setenv("CONTENT_LENGTH", to_string(request.getBody().size()).c_str(), 1);  // Définir la taille du corps de la requête
// 	} else {
// 		setenv("REQUEST_METHOD", "GET", 1);  // Définir GET comme méthode par défaut
// 	}
//     setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
//     setenv("REQUEST_METHOD", request.getMethod().c_str(), 1);
//     setenv("SCRIPT_FILENAME", scriptPath.c_str(), 1);
//     setenv("SCRIPT_NAME", scriptPath.c_str(), 1);
//     setenv("QUERY_STRING", request.getQueryString().c_str(), 1);
//     setenv("CONTENT_TYPE", request.getStrHeader("Content-Type").c_str(), 1);
//     setenv("CONTENT_LENGTH", to_string(request.getBody().size()).c_str(), 1);
//     setenv("REDIRECT_STATUS", "200", 1); // Nécessaire pour php-cgi
//     setenv("SERVER_PROTOCOL", "HTTP/1.1", 1);
//     //setenv("REMOTE_ADDR", request.getClientIP().c_str(), 1);
//     setenv("SERVER_NAME", request.getStrHeader("Host").c_str(), 1);
//     //setenv("SERVER_PORT", request.getPort().c_str(), 1);
// }

void CGIHandler::setupEnvironment(const HTTPRequest& request, const std::string& scriptPath) {
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
    setenv("SERVER_NAME", request.getStrHeader("Host").c_str(), 1);
}


// // CGIHandler.cpp
// #include "CGIHandler.hpp"
// #include "HTTPResponse.hpp"
// #include "Server.hpp"
// #include <unistd.h>  // For fork, exec, pipe
// #include <sys/wait.h>  // For waitpid
// #include <iostream>
// #include <cstring>  // For to_string
// #include "Logger.hpp"
// #include "Utils.hpp"

// CGIHandler::CGIHandler() /*: _server(server)*/ {}

// CGIHandler::~CGIHandler() {}

// // Implémentation de la méthode endsWith
// bool CGIHandler::endsWith(const std::string& str, const std::string& suffix) const {
//     if (str.length() >= suffix.length()) {
//         return (0 == str.compare(str.length() - suffix.length(), suffix.length(), suffix));
//     } else {
//         return false;
//     }
// }

// std::string CGIHandler::executeCGI(const std::string& scriptPath, const HTTPRequest& request) {
//     Logger::instance().log(DEBUG, "executeCGI: Executing script: " + scriptPath);

//     std::string interpreter_directory_path = "";
//     #ifdef __APPLE__
//         interpreter_directory_path = "/opt/homebrew/bin/";
//     #elif defined(__linux__)
//         interpreter_directory_path = "/usr/bin/";
//     #else
//         // Handle other OS or set a default path
//         interpreter_directory_path = "/usr/bin/";
//     #endif

//     std::string interpreter_name = "";
//     if (endsWith(scriptPath, ".sh")) {
//         interpreter_name = "bash";
//     } else if (endsWith(scriptPath, ".php")) {
//         interpreter_name = "php-cgi";
//     }

//     std::string interpreter = interpreter_directory_path + interpreter_name;
//     // .cgi utilisera le shebang du script

//     Logger::instance().log(DEBUG, "executeCGI: Interpreter = " + (interpreter.empty() ? "Shebang" : interpreter));
//     std::string fullPath = scriptPath;
//     HTTPResponse response;

//     int pipefd[2];
//     if (pipe(pipefd) == -1) {
//         Logger::instance().log(ERROR, std::string("executeCGI: Pipe failed: ") + to_string(errno));
//         return response.beError(500, "Internal Server Error: Unable to create pipe.").toString();
//     }

//     int pipefd_in[2];  // Pipe pour l'entrée standard
//     if (pipe(pipefd_in) == -1) {
//         Logger::instance().log(ERROR, std::string("executeCGI: Pipe fir STDIN failed: ") + to_string(errno));
//         return response.beError(500, "Internal Server Error: Unable to create stdin pipe.").toString();
//     }

//     pid_t pid = fork();
//     if (pid == 0) {
//         // Processus enfant : exécution du script CGI
//         close(pipefd[0]);  // Fermer l'extrémité de lecture du pipe pour stdout
//         dup2(pipefd[1], STDOUT_FILENO);  // Rediriger stdout vers le pipe
//         close(pipefd_in[1]);  // Fermer l'extrémité d'écriture du pipe pour stdin
//         dup2(pipefd_in[0], STDIN_FILENO);  // Rediriger stdin vers le pipe

//         setupEnvironment(request, fullPath);

//         // Exécuter le script
//         if (!interpreter.empty()) {
//             execl(interpreter.c_str(), interpreter.c_str(), fullPath.c_str(), NULL);
//         } else {
//             execl(fullPath.c_str(), fullPath.c_str(), NULL);
//         }
//         Logger::instance().log(ERROR, std::string("executeCGI: Failed to execute CGI script: ") + fullPath + std::string(". Error: ") + to_string(errno));
//         exit(EXIT_FAILURE);
//     } else if (pid > 0) {
//         // Processus parent : gestion de la sortie du CGI
//         close(pipefd[1]);
//         close(pipefd_in[0]);

//         if (request.getMethod() == "POST") {
//             int bytes_written = write(pipefd_in[1], request.getBody().c_str(), request.getBody().size());
//             //?? Check 0 ?
//             if (bytes_written == -1) {
//                 Logger::instance().log(WARNING, "500 error (Internal Server Error) for writing"); // To specify i'm tired.
//                 return response.beError(500, "500 error (Internal Server Error) for writing").toString(); // Internal server error
//             }
//         }
//         close(pipefd_in[1]);

//         char buffer[1024];
//         std::string cgiOutput;

//         ssize_t bytesRead;
//         while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
//             cgiOutput.append(buffer, bytesRead);
//         }
//         close(pipefd[0]);

//         int status;
//         waitpid(pid, &status, 0);
//         if (WIFEXITED(status)) {
//             int exitCode = WEXITSTATUS(status);
//             if (exitCode != 0) {
//                 Logger::instance().log(ERROR, std::string("executeCGI: CGI script exited with code: ") + to_string(exitCode));
//                 return response.beError(500, "Internal Server Error: CGI script failed.").toString();
//             }
//         }

//         // Analyse des en-têtes CGI
//         size_t statusPos = cgiOutput.find("Status:");
//         if (statusPos != std::string::npos) {
//             size_t endOfStatusLine = cgiOutput.find("\r\n", statusPos);
//             std::string statusLine = cgiOutput.substr(statusPos, endOfStatusLine - statusPos);
//             int statusCode = std::atoi(statusLine.substr(8, 3).c_str());

//             std::string errorMessage;
//             size_t messageStartPos = statusLine.find(" ");
//             if (messageStartPos != std::string::npos) {
//                 errorMessage = statusLine.substr(messageStartPos + 1);
//             }

//             return response.beError(statusCode, errorMessage).toString();
//         }

//         // Si aucun en-tête "Status:" n'est trouvé, renvoyer la réponse CGI directement
//         if (cgiOutput.find("HTTP/1.1") == std::string::npos) {
//             cgiOutput = "HTTP/1.1 200 OK\r\n" + cgiOutput;
//         }
//         Logger::instance().log(DEBUG, std::string("executeCGI: CGI Output:\n") + cgiOutput);
//         return cgiOutput;
//     } else {
//         Logger::instance().log(ERROR, std::string("executeCGI: Fork failed: ") + to_string(errno));
//         return response.beError(500, "Internal Server Error: Fork failed.").toString();
//     }
// }


// void CGIHandler::setupEnvironment(const HTTPRequest& request, std::string scriptPath) {
// 	if (request.getMethod() == "POST") {
// 		setenv("REQUEST_METHOD", "POST", 1);  // Définir POST comme méthode
// 		setenv("CONTENT_TYPE", "application/x-www-form-urlencoded", 1); // Valeur par défaut
// 		setenv("CONTENT_LENGTH", to_string(request.getBody().size()).c_str(), 1);  // Définir la taille du corps de la requête
// 	} else {
// 		setenv("REQUEST_METHOD", "GET", 1);  // Définir GET comme méthode par défaut
// 	}
//     setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
//     setenv("REQUEST_METHOD", request.getMethod().c_str(), 1);
//     setenv("SCRIPT_FILENAME", scriptPath.c_str(), 1);
//     setenv("SCRIPT_NAME", scriptPath.c_str(), 1);
//     setenv("QUERY_STRING", request.getQueryString().c_str(), 1);
//     setenv("CONTENT_TYPE", request.getStrHeader("Content-Type").c_str(), 1);
//     setenv("CONTENT_LENGTH", to_string(request.getBody().size()).c_str(), 1);
//     setenv("REDIRECT_STATUS", "200", 1); // Nécessaire pour php-cgi
//     setenv("SERVER_PROTOCOL", "HTTP/1.1", 1);
//     //setenv("REMOTE_ADDR", request.getClientIP().c_str(), 1);
//     setenv("SERVER_NAME", request.getStrHeader("Host").c_str(), 1);
//     //setenv("SERVER_PORT", request.getPort().c_str(), 1);
// }
