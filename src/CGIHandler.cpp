// CGIHandler.cpp
#include "CGIHandler.hpp"
#include "HTTPResponse.hpp"
#include "Server.hpp"
#include <unistd.h>  // For fork, exec, pipe
#include <sys/wait.h>  // For waitpid
#include <signal.h>
#include <iostream>
#include <cstring>  // For strerror
#include "Logger.hpp"
#include "Utils.hpp"

//?? Penser à ajouter un pointeru ou une reference vers la connection parent pour pouvoir faire des beError() quand je return false

CGIHandler::CGIHandler(const std::string& scriptPath, const HTTPRequest& request) : _scriptPath(scriptPath), _request(request), _pid(-1), _CGIOutput(""), _bytesSent(0), _started(false) {
    _outputPipeFd[0] = -1;
    _outputPipeFd[1] = -1;
    _inputPipeFd[0] = -1;
    _inputPipeFd[1] = -1;
    setCGIInput(request.getBody().c_str());
}

CGIHandler::~CGIHandler() {}

// Getters
int CGIHandler::getPid() const { return _pid; }
int CGIHandler::getInputPipeFd() const { return _inputPipeFd[1]; }
int CGIHandler::getOutputPipeFd() const { return _outputPipeFd[0]; }
std::string CGIHandler::getCGIInput() const { return _CGIInput; }
std::string CGIHandler::getCGIOutput() const { return _CGIOutput; }

// Setters
void CGIHandler::setPid(int pid) { _pid = pid; }
void CGIHandler::setInputPipeFd(int inputPipeFd[2]) { 
    _inputPipeFd[0] = inputPipeFd[0];
    _inputPipeFd[1] = inputPipeFd[1]; 
}
void CGIHandler::setOutputPipeFd(int outputPipeFd[2]) { 
    _outputPipeFd[0] = outputPipeFd[0];
    _outputPipeFd[1] = outputPipeFd[1];
}
void CGIHandler::setCGIInput(const std::string& CGIInput) { _CGIInput = CGIInput; }
void CGIHandler::setCGIOutput(const std::string& CGIOutput) { _CGIOutput = CGIOutput; }


int CGIHandler::writeToCGI() {
    if (_inputPipeFd[1] == -1) {
        return -1;
    }

    const char* bufferPtr = _CGIInput.c_str() + _bytesSent;
    ssize_t remaining = _CGIInput.size() - _bytesSent;

    ssize_t bytesWritten = write(_inputPipeFd[1], bufferPtr, remaining);

    if (bytesWritten > 0) {
        _bytesSent += bytesWritten;
    } else if (bytesWritten == -1) {
        // An error occurred (since poll normally provides from getting EAGAIN error)
        Logger::instance().log(ERROR, "writeToCGI: Write error");
    }

    // Check if all data has been sent
    if (_bytesSent == _CGIInput.size()) {
        // All data sent; close the input pipe
        close(_inputPipeFd[1]);
        _inputPipeFd[1] = -1;
        return 0; // Indicate that writing is complete
    }

    return _bytesSent;
    // Not all data sent yet; continue writing
}

int CGIHandler::readFromCGI() {
    if (_outputPipeFd[0] == -1) {
        return -1;
    }
    char buffer[4096];
    ssize_t bytesRead = read(_outputPipeFd[0], buffer, sizeof(buffer));
    if (bytesRead > 0) {
        _CGIOutput.append(buffer, bytesRead);
    } else if (bytesRead == 0) {
        close(_outputPipeFd[0]);
        _outputPipeFd[0] = -1;
    }
    return bytesRead;
}

bool CGIHandler::hasTimedOut() const {
    unsigned long currentTime = curr_time_ms();
    return (currentTime - _startTime) > CGI_TIMEOUT_MS;
}

void CGIHandler::terminateCGI() {
    if (_pid > 0) {
        kill(_pid, SIGKILL);
        waitpid(_pid, NULL, 0);
        _pid = -1;
    }
    closeInputPipe();
    closeOutputPipe();
}

// Implémentation de la méthode endsWith
bool CGIHandler::endsWith(const std::string& str, const std::string& suffix) const {
    if (str.length() >= suffix.length()) {
        return (0 == str.compare(str.length() - suffix.length(), suffix.length(), suffix));
    } else {
        return false;
    }
}

bool CGIHandler::startCGI() {
    _startTime = curr_time_ms();
    Logger::instance().log(DEBUG, "Startin CGI script: " + _scriptPath);

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
    if (endsWith(_scriptPath, ".sh")) {
        interpreter_name = "bash";
    } else if (endsWith(_scriptPath, ".php")) {
        interpreter_name = "php-cgi";
    }
    std::string interpreter = interpreter_directory_path + interpreter_name;
    Logger::instance().log(DEBUG, "executeCGI: Interpreter = " + (interpreter.empty() ? "Shebang" : interpreter));

    if (pipe(_inputPipeFd) == -1) {
        Logger::instance().log(ERROR, std::string("executeCGI: Input pipe failed: ") + strerror(errno));
        return false;
    }
    if (pipe(_outputPipeFd) == -1) {
        Logger::instance().log(ERROR, std::string("executeCGI: Output pipe failed: ") + strerror(errno));
        return false;
    }

    Logger::instance().log(DEBUG, std::string("pipe fds : INPUT 0 : ") + to_string(_inputPipeFd[0]) + " - INPUT 1 : " + to_string(_inputPipeFd[1]) + " - OUTPUT 0 : " + to_string(_outputPipeFd[0])  + " - OUTPUT 1 : " + to_string(_outputPipeFd[1]));

    int pid = fork();
    if (pid == 0) {
        // Processus enfant : exécution du script CGI
        close(_outputPipeFd[0]);  // Fermer l'extrémité de lecture du pipe pour stdout
        dup2(_outputPipeFd[1], STDOUT_FILENO);  // Rediriger stdout vers le pipe
        close(_outputPipeFd[1]);
        close(_inputPipeFd[1]);  // Fermer l'extrémité d'écriture du pipe pour stdin
        dup2(_inputPipeFd[0], STDIN_FILENO);  // Rediriger stdin vers le pipe
        close(_inputPipeFd[0]);


        setupEnvironment(_request, _scriptPath);

        // Exécuter le script
        if (!interpreter.empty()) {
            execl(interpreter.c_str(), _scriptPath.c_str(), NULL);
        } else {
            execl(_scriptPath.c_str(), _scriptPath.c_str(), NULL);
        }
        Logger::instance().log(ERROR, std::string("executeCGI: Failed to execute CGI script: ") + _scriptPath + std::string(". Error: ") + strerror(errno));
        exit(EXIT_FAILURE);
    } else if (pid > 0){
        _pid = pid;
        _started = true;
        close(_outputPipeFd[1]);
        close(_inputPipeFd[0]);
        return true;
    } else if (pid == -1) {
        Logger::instance().log(ERROR, "executeCGI: Fork failed: " + std::string(strerror(errno)));
        // Close all open FDs before returning
        close(_inputPipeFd[0]);
        close(_inputPipeFd[1]);
        close(_outputPipeFd[0]);
        close(_outputPipeFd[1]);
        return false;
    }
    return true;
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

void CGIHandler::closeInputPipe() {
    if (_inputPipeFd[0] != -1) {
        close(_inputPipeFd[0]);
        _inputPipeFd[0] = -1;
    }
    if (_inputPipeFd[1] != -1) {
        close(_inputPipeFd[1]);
        _inputPipeFd[1] = -1;
    }
}

void CGIHandler::closeOutputPipe() {
    if (_outputPipeFd[0] != -1) {
        close(_outputPipeFd[0]);
        _outputPipeFd[0] = -1;
    }
    if (_outputPipeFd[1] != -1) {
        close(_outputPipeFd[1]);
        _outputPipeFd[1] = -1;
    }
}

