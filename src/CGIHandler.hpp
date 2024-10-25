// CGIHandler.hpp
#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <string>
#include <stdlib.h>
#include "HTTPRequest.hpp"

class Server;

class CGIHandler {
public:
    CGIHandler();
    ~CGIHandler();

    // Execute the CGI script and return the output
    std::string executeCGI(const std::string& scriptPath, const HTTPRequest& request);

private:
    // Setup the environment variables required for CGI execution
	void setupEnvironment(const HTTPRequest&, std::string scriptPath);

    // Méthode auxiliaire pour vérifier l'extension
    bool endsWith(const std::string& str, const std::string& suffix) const;

    //Server& _server;
};

#endif
