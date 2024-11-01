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
		case 404: _reasonPhrase = "Not Found"; break;
		case 500: _reasonPhrase = "Internal Server Error"; break;
		case 400: _reasonPhrase = "Bad Request"; break;
		case 403: _reasonPhrase = "Forbidden"; break;
		case 405: _reasonPhrase = "Method Not Allowed"; break;
		default: _reasonPhrase = "Unknown";
	}
}

std::string HTTPResponse::generateErrorPage(std::string infos) {
	std::stringstream page;
	page << "<html><head><title>Error " << _statusCode << "</title>";
	page << "<link rel=\"stylesheet\" href=\"css/err_style.css\"><meta charset=UTF-8></head>";
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
	page << "<link rel=\"stylesheet\" href=\"css/err_style.css\"><meta charset=UTF-8></head>";
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

std::string HTTPResponse::toString() const {
	std::ostringstream oss;
	oss << "HTTP/1.1 " << _statusCode << " " << _reasonPhrase << "\r\n";

	// Ajouter les en-têtes
	for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
		oss << it->first << ": " << it->second << "\r\n";
	}

	oss << "\r\n"; // Fin des en-têtes
	oss << _body;  // Corps de la réponse

	return oss.str();
}
