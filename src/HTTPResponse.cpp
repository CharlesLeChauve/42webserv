#include "HTTPResponse.hpp"
#include <sstream>

HTTPResponse::HTTPResponse() : _statusCode(200), _reasonPhrase("OK") {}

HTTPResponse::~HTTPResponse() {}

void HTTPResponse::setStatusCode(int code) {
	_statusCode = code;
	// Vous pouvez définir la raison en fonction du code
	switch (code) {
		case 200: _reasonPhrase = "OK"; break;
		case 404: _reasonPhrase = "Not Found"; break;
		case 500: _reasonPhrase = "Internal Server Error"; break;
		case 400: _reasonPhrase = "Bad Request"; break;
		case 403: _reasonPhrase = "Forbidden"; break;
		case 405: _reasonPhrase = "Method Not Allowed"; break;
		default: _reasonPhrase = "Unknown";
	}
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
