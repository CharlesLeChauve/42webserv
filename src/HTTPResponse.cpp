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

std::string HTTPResponse::generateErrorPage(std::string infos) {
	std::stringstream page;
	page << "<html><head><title>Error " << _statusCode << "</title>";
	page << "<link rel=\"stylesheet\" href=\"../css/err_style.css\"><meta charset=UTF-8></head>";
	page << "<body><h1>Error " << _statusCode << ": " << _reasonPhrase << "</h1>";
	page << "<h3>" + infos + "</h3>";
	page << "<img src=\"" << getSorryPath() << "\" alt=\"Error Image\">";
	page << "<p>The server encountered an issue processing your request.</p>";
	page << "</body></html>";
	return page.str();
}

std::string HTTPResponse::generateErrorPage() {
	std::stringstream page;
	page << "<html><head><title>Error " << _statusCode << "</title>";
	page << "<link rel=\"stylesheet\" href=\"../css/err_style.css\"><meta charset=UTF-8></head>";
	page << "<body><h1>Error " << _statusCode << ": " << _reasonPhrase << "</h1>";
	page << "<img src=\"" << getSorryPath() << "\" alt=\"Error Image\">";
	page << "<a href=\"index.html\">Retour à l'accueil</a>";
	page << "<p>The server encountered an issue processing your request.</p>";
	page << "</body></html>";
	return page.str();
}

HTTPResponse& HTTPResponse::beError(int err_code, const std::string& errorContent) {
	setStatusCode(err_code);
	if (!errorContent.empty()) {
		setBody(errorContent);
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
