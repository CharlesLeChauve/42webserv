#include <fstream>
#include <iostream>
#include "ServerConfig.hpp"
#include "UploadHandler.hpp"

bool UploadHandler::checkDestPath(std::string path)
{
	if (path.size() > 0)
		return true;
	return false;
}

UploadHandler::UploadHandler(const std::string& destPath, std::istream& fileContent, const ServerConfig& config)	: _destPath(destPath) {
    (void)config;
	if (checkDestPath(destPath)) {
        throw forbiddenDest();
    }
    std::ofstream destFile(destPath.c_str(), std::ios::binary);
    if (!destFile.is_open()) {
        throw std::runtime_error("Failed to open destination file.");
    }

    destFile << fileContent.rdbuf();

    std::cerr << "Fichier enregistré à : " << destPath << std::endl;

    destFile.close();
}

UploadHandler::~UploadHandler()
{
}
