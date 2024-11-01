#include <fstream>
#include <iostream>
#include "ServerConfig.hpp"


class UploadHandler {
private:
    std::string _destPath;

public:
    class forbiddenDest : public std::exception {
    public:
        forbiddenDest() throw() {}
        virtual const char *what() const throw() {
            return "Access to the requested destination is forbidden.";
        }
    };

	bool checkDestPath(std::string path);

    UploadHandler(const std::string& destPath, const std::string& fileContent, const ServerConfig& config);
    ~UploadHandler();
};
