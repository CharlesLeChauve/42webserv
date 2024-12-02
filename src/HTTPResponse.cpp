#include "HTTPResponse.hpp"
#include "Server.hpp"
#include <sstream>
#include "Utils.hpp"

HTTPResponse::HTTPResponse() : _statusCode(200), _reasonPhrase("OK") {}

HTTPResponse::~HTTPResponse() {}

void HTTPResponse::setStatusCode(int code) {
	_statusCode = code;
	// Vous pouvez définir la raison en fonction du code
	switch (code) {
		case 200: _reasonPhrase = "OK"; break;
		case 201: _reasonPhrase = "Created"; break;
		case 301: _reasonPhrase = "Moved Permanently"; break; // indique que la ressource a définitivement été déplacée à l'URL contenue dans l'en-tête Location
		case 303: _reasonPhrase = "See Other"; break; // renvoyé comme résultat d'une opération PUT ou POST, indique que la redirection ne fait pas le lien vers la ressource nouvellement téléversée mais vers une autre page
		case 307: _reasonPhrase = "Temporary Redirect"; break; // indique que la ressource demandée est temporairement déplacée vers l'URL contenue dans l'en-tête Location
		case 308: _reasonPhrase = "Permanent Redirect"; break; // indique que la ressource demandée à définitivement été déplacée vers l'URL contenue dans l'en-tête Location. Un navigateur redirigera vers cette page et les moteurs de recherche mettront à jour leurs liens vers la ressource
		case 400: _reasonPhrase = "Bad Request"; break;
		case 401: _reasonPhrase = "Unauthorized"; break;
		case 403: _reasonPhrase = "Forbidden"; break;
		case 404: _reasonPhrase = "Not Found"; break;
		case 405: _reasonPhrase = "Method Not Allowed"; break;
		case 408: _reasonPhrase = "Request Timeout"; break; // le serveur ne reçoit pas de requête complète dans un délai défini.
		case 413: _reasonPhrase = "Payload Too Large"; break; // fichier téléchargé dépasse la limite autorisée.
		case 415: _reasonPhrase = "Unsupported Media Type"; break; // Si certains types de fichiers ne sont pas acceptés.
		case 418: _reasonPhrase = "I'm a teapot"; break; //?? Where should we implement it ?
		case 429: _reasonPhrase = "Too Many Requests"; break; // trop grand nombre de requêtes en peu de temps (si limite)
		case 500: _reasonPhrase = "Internal Server Error"; break;
		case 501: _reasonPhrase = "Method Not Implemented"; break;
		case 502: _reasonPhrase = "Bad Gateway"; break; // un serveur (agissant comme une passerelle ou un proxy, style NGINX) a reçu une réponse invalide ou inattendue d'un autre serveur en amont
		case 503: _reasonPhrase = "Service Unavailable"; break; // le serveur n'est pas prêt à traiter la requête (surcharge, maintenance, etc.).
		case 504: _reasonPhrase = "Gateway Timeout"; break; //un des serveurs, passerelle ou proxy, n'a pas reçu une réponse à temps de la part d'un autre serveur (ou interface) qu'il a interrogé pour obtenir une réponse à la requête

		default: _reasonPhrase = "Unknown";
	}
}

std::string replacePlaceholders(const std::string& templateStr, const std::map<std::string, std::string>& values) {
    std::string result = templateStr;
    for (std::map<std::string, std::string>::const_iterator it = values.begin(); it != values.end(); ++it) {
        std::string placeholder = "{{" + it->first + "}}";
        size_t pos = result.find(placeholder);
        while (pos != std::string::npos) {
            result.replace(pos, placeholder.length(), it->second);
            pos = result.find(placeholder, pos + it->second.length());
        }
    }
    return result;
}

std::string readFile(const std::string& filename) {
    std::ifstream file(filename.c_str());
    if (!file) {
        // Gérer l'erreur de fichier non trouvé ou non accessible
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}


std::string HTTPResponse::generateErrorPage(const std::string& infos) {
    std::string templateStr = readFile("templates/error_template.html");
    if (templateStr.empty()) {
        return "<html><body><h1>Error generating page</h1></body></html>";
    }

    std::map<std::string, std::string> values;
    values["STATUS_CODE"] = to_string(_statusCode);
    values["REASON_PHRASE"] = _reasonPhrase;
    values["SORRY_PATH"] = getSorryPath();
    values["INFOS_SECTION"] = infos;
    return replacePlaceholders(templateStr, values);
}

HTTPResponse& HTTPResponse::beError(int err_code, const std::string& errorContent) {
	setStatusCode(err_code);
	if (!errorContent.empty()) {
		setBody(generateErrorPage(errorContent));
	} else {
		setBody(generateErrorPage());
	}
	setHeader("Content-Type", "text/html");
	setHeader("Content-Length", to_string(getBody().size()));
	return *this;
}

void HTTPResponse::setReasonPhrase(const std::string& reason) {
	_reasonPhrase = reason;
}

void HTTPResponse::setHeader(const std::string& key, const std::string& value) {
	_headers[key] = value;
}

void HTTPResponse::setBody(const std::string& body) {
	_body = body;
}

int HTTPResponse::getStatusCode() const {
	return _statusCode;
}

std::string HTTPResponse::getReasonPhrase() const {
	return _reasonPhrase;
}

std::map<std::string, std::string> HTTPResponse::getHeaders() const {
	return _headers;
}

std::string HTTPResponse::getBody() const {
	return _body;
}

std::string HTTPResponse::getStrHeader(std::string header) const {
	std::map<std::string, std::string>::const_iterator it = _headers.find(header);
	if (it == _headers.end())
		return ""; // Retourner une chaîne vide si l'en-tête n'existe pas
	return it->second;
}

std::string HTTPResponse::toStringHeaders() const {
	std::ostringstream oss;
	oss << "HTTP/1.1 " << _statusCode << " " << _reasonPhrase << "\r\n";

	// Ajouter les en-têtes
	for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
		oss << it->first << ": " << it->second << "\r\n";
	}
	return oss.str();
}

std::string HTTPResponse::toString() const {
	std::ostringstream oss;
	oss << toStringHeaders();

	oss << "\r\n"; // Fin des en-têtes
	oss << _body;  // Corps de la réponse

	return oss.str();
}

void HTTPResponse::parseCGIOutput(const std::string& cgiOutput) {
    // Trouver la fin des en-têtes (double saut de ligne)
    size_t headerEnd = cgiOutput.find("\r\n\r\n");
    if (headerEnd != std::string::npos) {
        std::string headers = cgiOutput.substr(0, headerEnd);
        std::string body = cgiOutput.substr(headerEnd + 4);

        // Analyser les en-têtes et remplir les champs appropriés
        parseHeaders(headers);

        // Définir le corps de la réponse
        setBody(body);
    } else {
        // Pas de séparation entre en-têtes et corps trouvée
        // Traiter en conséquence (erreur ou tout mettre dans le corps)
        setBody(cgiOutput);
    }
}

void HTTPResponse::parseHeaders(const std::string& headers) {
    std::istringstream stream(headers);
    std::string line;
    while (std::getline(stream, line)) {
        // Supprimer les retours chariot éventuels
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);
        }
        // Trouver la position du ':'
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string headerName = line.substr(0, colonPos);
            std::string headerValue = line.substr(colonPos + 1);
            // Supprimer les espaces blancs en début et fin
            headerName = trim(headerName);
            headerValue = trim(headerValue);
            // Gestion spécifique de l'en-tête 'Status'
            if (headerName == "Status") {
                // Le format attendu est "Status: 200 OK"
                size_t spacePos = headerValue.find(' ');
                if (spacePos != std::string::npos) {
                    int statusCode = atoi(headerValue.substr(0, spacePos).c_str());
                    setStatusCode(statusCode);
                    setReasonPhrase(headerValue.substr(spacePos + 1));
                } else {
                    int statusCode = atoi(headerValue.c_str());
                    setStatusCode(statusCode);
                }
            } else {
                setHeader(headerName, headerValue);
            }
        }
    }
}


std::string HTTPResponse::trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    if (std::string::npos == first)
    {
        return str;
    }
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

