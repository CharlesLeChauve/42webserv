#include <fstream>
#include <iostream>
#include "Logger.hpp"
#include "ServerConfig.hpp"
#include "UploadHandler.hpp"

bool UploadHandler::checkDestPath(std::string path)
{
	if (path.size() > 0)
		return true;
	return false;
}

UploadHandler::UploadHandler(const std::string& destPath, const std::string& fileContent, const ServerConfig& config)
    : _destPath(destPath) {
    (void)config;
    if (!checkDestPath(destPath)) {
        throw forbiddenDest();
    }
    std::ofstream destFile(destPath.c_str(), std::ios::binary);
    if (!destFile.is_open()) {
        throw std::runtime_error("Failed to open destination file.");
    }

    destFile.write(fileContent.c_str(), fileContent.size());
    Logger::instance().log(INFO, "Fichier enregistré à : " + destPath);

    destFile.close();
}

UploadHandler::~UploadHandler()
{
}
