// CGIHandler.cpp
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <iostream>
#include <cstring>
#include "CGIHandler.hpp"
#include "HTTPResponse.hpp"
#include "Server.hpp"
#include "Logger.hpp"
#include "Utils.hpp"

CGIHandler::CGIHandler::CGIHandler(const std::string& scriptPath, const std::string& interpreterPath, const HTTPRequest& request)
    : _scriptPath(scriptPath), _request(request), _interpreterPath(interpreterPath), _pid(-1), _CGIOutput(""), _bytesSent(0), _started(false), _cgiFinished(false), _cgiExitStatus(-1) {
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

int CGIHandler::isCgiDone() {
    if (_cgiFinished) {
        return _cgiExitStatus;
    }

    int status;
    pid_t result = waitpid(_pid, &status, WNOHANG);
    if (result == 0) {
        // Process is still running
        return 0;
    } else if (result == _pid) {
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            if (WEXITSTATUS(status))
                _cgiExitStatus = WEXITSTATUS(status);
            else if (WTERMSIG(status))
                _cgiExitStatus = WTERMSIG(status);
        } else {
            _cgiExitStatus = -1;
        }
        _cgiFinished = true;
        return _cgiExitStatus;
    } else {
        Logger::instance().log(ERROR, "isCGIDone: waitpid failed: " + std::string(to_string(errno)));
        _cgiFinished = true;
        _cgiExitStatus = -1;
        return _cgiExitStatus;
    }
}

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
        Logger::instance().log(ERROR, "writeToCGI: Write error");
    }

    if (_bytesSent == _CGIInput.size()) {
        // All data sent; close the input pipe
        close(_inputPipeFd[1]);
        _inputPipeFd[1] = -1;
        return 0; // Indicate that writing is complete
    }

    return _bytesSent;
}

bool CGIHandler::hasReceivedBody() {
    std::string sequence = "\r\n\r\n";
    size_t pos = getCGIOutput().find(sequence);

    if (pos == std::string::npos)
        return false;
    return ((pos + 4) < getCGIOutput().length());
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

    std::string interpreter = _interpreterPath;

    Logger::instance().log(DEBUG, "executeCGI: Interpreter = " + interpreter);

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
        close(_outputPipeFd[0]);
        dup2(_outputPipeFd[1], STDOUT_FILENO);
        close(_outputPipeFd[1]);
        close(_inputPipeFd[1]);
        dup2(_inputPipeFd[0], STDIN_FILENO);
        close(_inputPipeFd[0]);

        setupEnvironment(_request, _scriptPath);

        execl(interpreter.c_str(), interpreter.c_str(), _scriptPath.c_str(), NULL);
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
        close(_inputPipeFd[0]);
        close(_inputPipeFd[1]);
        close(_outputPipeFd[0]);
        close(_outputPipeFd[1]);
        return false;
    }
    return true;
}

void CGIHandler::setupEnvironment(const HTTPRequest& request, std::string scriptPath) {
    char absPath[PATH_MAX];
    if (realpath(scriptPath.c_str(), absPath) == NULL) {
        Logger::instance().log(ERROR, "Failed to get absolute path of the CGI script.");
    }
    
    std::string contentType = request.getStrHeader("Content-Type");
    if (!contentType.empty()) {
        setenv("CONTENT_TYPE", contentType.c_str(), 1);
    }

    // Variables CGI standard
    setenv("REQUEST_METHOD", request.getMethod().c_str(), 1);
    setenv("CONTENT_LENGTH", to_string(request.getBody().size()).c_str(), 1);
    setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
    setenv("SCRIPT_FILENAME", absPath, 1);
    setenv("SCRIPT_NAME", scriptPath.c_str(), 1);
    setenv("QUERY_STRING", request.getQueryString().c_str(), 1);
    setenv("REDIRECT_STATUS", "200", 1);
    setenv("SERVER_PROTOCOL", "HTTP/1.1", 1); 
    std::string host = request.getStrHeader("Host");
    setenv("SERVER_NAME", host.c_str(), 1);
    setenv("SERVER_SOFTWARE", "webserv/1.0", 1);
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

