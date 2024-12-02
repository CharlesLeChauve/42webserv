// CGIHandler.hpp
#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <string>
#include <stdlib.h>
#include "HTTPRequest.hpp"

class Server;

class CGIHandler {
public:
    CGIHandler(const std::string& scriptPath, const HTTPRequest& request);
    ~CGIHandler();

    // Execute the CGI script and return the output
    bool startCGI();

    int getPid() const;
    int getInputPipeFd() const;
    int getOutputPipeFd() const;
    std::string getCGIInput() const;
    std::string getCGIOutput() const;

    // Setters
    void setPid(int pid);
    void setInputPipeFd(int inputPipeFd[2]);
    void setOutputPipeFd(int outputPipeFd[2]);
    void setCGIInput(const std::string& CGIInput);
    void setCGIOutput(const std::string& CGIOutput);

    void closeInputPipe();
    void closeOutputPipe();

    int writeToCGI();
    int readFromCGI();

    void terminateCGI();
    bool hasTimedOut() const;

private:
    std::string _scriptPath;
    const HTTPRequest& _request;

    int _pid;
    int _inputPipeFd[2];
    int _outputPipeFd[2];

    std::string _CGIInput;
    std::string _CGIOutput;

    size_t  _bytesSent;
    bool    _started;
    
    unsigned long _startTime;
    static const unsigned long CGI_TIMEOUT_MS = 5000;

    // Setup the environment variables required for CGI execution
	void setupEnvironment(const HTTPRequest&, std::string scriptPath);

    // Méthode auxiliaire pour vérifier l'extension
    bool endsWith(const std::string& str, const std::string& suffix) const;

    //Server& _server;
};

#endif
