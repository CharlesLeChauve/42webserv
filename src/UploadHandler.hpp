#include <fstream>
#include <iostream>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "ServerConfig.hpp"
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"


class UploadHandler {
private:
    const HTTPRequest& _request;
    HTTPResponse& _response;
    std::string _boundary;
    std::string _uploadDir;
    const ServerConfig& _config;
    std::string _filename;

    void parseMultipartFormData();
    void saveFile(const std::string& filename, const std::string& content);
    std::string sanitizeFilename(const std::string& filename);
    bool isPathAllowed(const std::string& path, const std::string& uploadDir);
    void    handleFile(std::string& partHeaders, std::string& partContent);

public:
    class forbiddenDest : public std::exception {
    public:
        forbiddenDest() throw() {}
        virtual const char *what() const throw() {
            return "Access to the requested destination is forbidden.";
        }
    };
    UploadHandler(const HTTPRequest& request, HTTPResponse& response, const std::string& boundary, const std::string& uploadDir, const ServerConfig& config);
    void handleUpload();
};
