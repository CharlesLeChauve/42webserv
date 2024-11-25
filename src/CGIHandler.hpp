// CGIHandler.hpp
#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <string>
#include <unistd.h>
#include "HTTPRequest.hpp"

class CGIHandler {
public:
    CGIHandler();
    ~CGIHandler();

    bool startCGI(const std::string& scriptPath, const HTTPRequest& request);

    ssize_t writeCGIInput();
    void closeCGIInput();

    ssize_t readCGIOutput();

    void    setSendBuffer(const std::string& buffer);

    bool isCGIDone();
    bool allSent();

    bool getLaunched() const;
    bool getOutputComplete() const;
    int getCGIExitStatus() const;
    pid_t getPID() const;
    int getOutputFD() const;
    int getInputFD() const;
    std::string getCGIResponse() const;


private:
    void setupEnvironment(const HTTPRequest& request, const std::string& scriptPath);
    bool endsWith(const std::string& str, const std::string& suffix) const;

    int _stdin_pipe[2];   // Pipe for CGI's stdin
    int _stdout_pipe[2];  // Pipe for CGI's stdout
    pid_t _pid;           // PID of the CGI process
    int _exitStatus;      // Exit status of the CGI process
    bool _isDone;         // Flag to check if process is done
    bool _launched;

    std::string _sendBuffer;
    ssize_t _bytesSent;
    std::string _receiveBuffer;
    bool _outputComplete;
    // Other member variables as needed
};

#endif
