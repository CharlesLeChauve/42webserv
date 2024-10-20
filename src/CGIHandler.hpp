#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <string>
#include "HTTPRequest.hpp"

class Server;

class CGIHandler {
public:
    CGIHandler(Server& server);
    ~CGIHandler();

    // Execute the CGI script and return the output
    std::string executeCGI(const std::string& scriptPath, const HTTPRequest& request);

private:
    // Setup the environment variables required for CGI execution
    void setupEnvironment(const std::string& queryString);

    Server& _server;
};

#endif
