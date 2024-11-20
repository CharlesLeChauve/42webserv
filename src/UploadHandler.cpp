// UploadHandler.cpp
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include "UploadHandler.hpp"
#include <fstream>
#include <sys/stat.h>
#include "Logger.hpp"

UploadHandler::UploadHandler(const HTTPRequest& request, HTTPResponse& response, const std::string& boundary, const std::string& uploadDir, const ServerConfig& config)
    : _request(request), _response(response), _boundary("--" + boundary), _uploadDir(uploadDir), _config(config) {}

void UploadHandler::handleUpload() {
    std::string requestBody = _request.getBody();
    size_t pos = 0;
    size_t endPos = 0;
    std::string filename;

    while (true) {
        pos = requestBody.find(_boundary, endPos);
        if (pos == std::string::npos) {
            Logger::instance().log(DEBUG, "No more parts to process.");
            break;
        }
        pos += _boundary.length();

        if (requestBody.substr(pos, 2) == "--") {
            Logger::instance().log(DEBUG, "End of multipart data.");
            break;
        }
        if (requestBody.substr(pos, 2) == "\r\n") {
            pos += 2;
        }

        // Extraire les en-têtes de la partie
        size_t headersEnd = requestBody.find("\r\n\r\n", pos);
        if (headersEnd == std::string::npos) {
            Logger::instance().log(WARNING, "Missing \\r\\n\\r\\n in request for Upload");
            _response.setStatusCode(400);
            _response.setBody("Bad Request: Missing headers in request for Upload");
            return;
        }
        std::string partHeaders = requestBody.substr(pos, headersEnd - pos);
        pos = headersEnd + 4; // Position du début du contenu

        // Trouver la prochaine limite
        endPos = requestBody.find(_boundary, pos);
        if (endPos == std::string::npos) {
            Logger::instance().log(ERROR, "End Boundary Marker not found.");
            _response.setStatusCode(400);
            _response.setBody("Bad Request: End Boundary Marker not found.");
            return;
        }
        size_t contentEnd = endPos;

        // Retirer les éventuels \r\n avant la limite
        if (requestBody.substr(contentEnd - 2, 2) == "\r\n") {
            contentEnd -= 2;
        }

        // Extraire le contenu de la partie
        std::string partContent = requestBody.substr(pos, contentEnd - pos);

        // Vérifier si c'est un fichier
        if (partHeaders.find("Content-Disposition") != std::string::npos &&
            partHeaders.find("filename=\"") != std::string::npos)
        {
            size_t filenamePos = partHeaders.find("filename=\"") + 10;
            size_t filenameEnd = partHeaders.find("\"", filenamePos);
            filename = partHeaders.substr(filenamePos, filenameEnd - filenamePos);

            if (filename.empty()) {
                Logger::instance().log(ERROR, "No file selected for upload.");
                _response.setStatusCode(400);
                _response.setBody("No file selected for upload.");
                return;
            }

            // Sanitize filename to prevent directory traversal attacks
            filename = sanitizeFilename(filename);

            // Construct the destination path
            std::string destPath = _uploadDir + "/" + filename;

            if (!isPathAllowed(destPath, _uploadDir)) {
                Logger::instance().log(ERROR, "Attempt to upload outside of allowed path.");
                _response.setStatusCode(403);
                _response.setBody("Attempt to upload outside of allowed path.");
                return;
            }

            try {
                // Save the file to the authorized path
                saveFile(destPath, partContent);
            } catch (const std::exception& e) {
                Logger::instance().log(ERROR, std::string("Error while saving file: ") + e.what());
                _response.setStatusCode(500);
                _response.setBody("Internal Server Error: Error during file upload.");
                return;
            }
        } else {
            Logger::instance().log(ERROR, std::string("Error while parsing the file in the request:") + partHeaders);
            _response.setStatusCode(400);
            _response.setBody("Bad Request: File not found.");
            return;
        }
        endPos = endPos + _boundary.length();
    }
    _response.setStatusCode(201);
    std::string script = "<script type=\"text/javascript\">"
                         "setTimeout(function() {"
                         "    window.location.href = 'index.html';"
                         "}, 3500);"
                         "</script>";
    _response.setBody(script + "<html><body><h1>File successfully uploaded, you'll be redirected on HomePage</h1></body></html>");
    Logger::instance().log(INFO, "Successfully uploaded file: " + filename + " to " + _uploadDir);
}

void UploadHandler::saveFile(const std::string& destPath, const std::string& fileContent) {
    std::ofstream destFile(destPath.c_str(), std::ios::binary);
    if (!destFile.is_open()) {
        Logger::instance().log(ERROR, "Failed to open dest file on server's file system");
        throw std::runtime_error("Failed to open destination file.");
    }
    destFile.write(fileContent.c_str(), fileContent.size());
    Logger::instance().log(INFO, "File saved at: " + destPath);
    destFile.close();
}

std::string UploadHandler::sanitizeFilename(const std::string& filename) {
    std::string sanitized = filename;
    // Implémenter la logique pour supprimer les caractères non autorisés, les chemins relatifs, etc.
    // Par exemple, supprimer les ../ pour éviter la traversée de répertoires
    size_t pos;
    while ((pos = sanitized.find("..")) != std::string::npos) {
        sanitized.erase(pos, 2);
    }
    // Supprimer les caractères spéciaux
    const std::string invalidChars = "\\/:?\"<>|";
    for (size_t i = 0; i < invalidChars.size(); ++i) {
        sanitized.erase(std::remove(sanitized.begin(), sanitized.end(), invalidChars[i]), sanitized.end());
    }
    return sanitized;
}

bool UploadHandler::isPathAllowed(const std::string& path, const std::string& uploadDir) {
    // Vérifier que le chemin est dans le répertoire autorisé
    std::string directoryPath = path.substr(0, path.find_last_of('/'));

    // Résoudre les chemins absolus
    char resolvedDirectoryPath[PATH_MAX];
    char resolvedUploadPath[PATH_MAX];

    if (!realpath(directoryPath.c_str(), resolvedDirectoryPath)) {
        Logger::instance().log(ERROR, "Failed to resolve directory path: " + directoryPath + " Error: " + strerror(errno));
        return false;
    }

    if (!realpath(uploadDir.c_str(), resolvedUploadPath)) {
        Logger::instance().log(ERROR, "Failed to resolve upload path: " + uploadDir + " Error: " + strerror(errno));
        return false;
    }

    std::string directoryPathStr(resolvedDirectoryPath);
    std::string uploadPathStr(resolvedUploadPath);

    // Logger les chemins résolus pour le débogage
    Logger::instance().log(DEBUG, "Resolved directory path: " + directoryPathStr);
    Logger::instance().log(DEBUG, "Resolved upload path: " + uploadPathStr);

    // Vérifier que le chemin du répertoire commence par le chemin autorisé
    return directoryPathStr.find(uploadPathStr) == 0;
}



// #include <fstream>
// #include <iostream>
// #include "Logger.hpp"
// #include "ServerConfig.hpp"
// #include "UploadHandler.hpp"

// bool UploadHandler::checkDestPath(std::string path)
// {
// 	if (path.size() > 0)
// 		return true;
// 	return false;
// }

// UploadHandler::UploadHandler(const std::string& destPath, const std::string& fileContent, const ServerConfig& config)
//     : _destPath(destPath) {
//     (void)config;
//     if (!checkDestPath(destPath)) {
//         throw forbiddenDest();
//     }
//     std::ofstream destFile(destPath.c_str(), std::ios::binary);
//     if (!destFile.is_open()) {
//         Logger::instance().log(ERROR, "Failed to open dest file on server's file system");
//         throw std::runtime_error("Failed to open destination file.");
//     }

//     destFile.write(fileContent.c_str(), fileContent.size());
//     Logger::instance().log(INFO, "Fichier enregistré à : " + destPath);

//     destFile.close();
// }

// UploadHandler::~UploadHandler()
// {
// }
