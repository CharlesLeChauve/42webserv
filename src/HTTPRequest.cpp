#include "HTTPRequest.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include <sstream>
#include <iostream>
#include <cstdlib>

HTTPRequest::HTTPRequest() {}

HTTPRequest::~HTTPRequest() {}

bool HTTPRequest::hasHeader(std::string header) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(header);
    if (it != _headers.end())
        return true; // Retourner une chaîne vide si l'en-tête n'existe pas
    return false;
}

std::string HTTPRequest::getStrHeader(std::string header) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(header);
    if (it == _headers.end())
        return ""; // Retourner une chaîne vide si l'en-tête n'existe pas
    return it->second;
}

bool HTTPRequest::parse(const std::string& raw_request) {
	size_t header_end_pos = raw_request.find("\r\n\r\n");
	if (header_end_pos == std::string::npos) {
		Logger::instance().log(ERROR, "Invalid HTTP request: missing header-body separator.");
		return false;
	}

	std::string headers_part = raw_request.substr(0, header_end_pos);
	std::string body_part = raw_request.substr(header_end_pos + 4);

	std::istringstream header_stream(headers_part);
	std::string line;

	if (std::getline(header_stream, line)) {
		if (!parseRequestLine(line)) {
			return false;
		}
	} else {
		Logger::instance().log(ERROR, "Invalid HTTP request: missing request line.");
		return false;
	}

	// Parse headers
	while (std::getline(header_stream, line) && !line.empty()) {
		parseHeaders(line);
	}

	// Handle the body if there's a Content-Length
	std::map<std::string, std::string>::iterator it = _headers.find("Content-Length");
	if (it != _headers.end()) {
		int content_length = std::atoi(it->second.c_str());
		if (body_part.size() < static_cast<size_t>(content_length)) {
			Logger::instance().log(ERROR, "Failed to read the entire body");
			return false;
		}
		parseBody(body_part.substr(0, content_length));
	}

	return true;
}

// Parse the request line and extract method, path, and version
bool HTTPRequest::parseRequestLine(const std::string& line) {
	std::istringstream iss(line);
	std::string version;
	iss >> _method >> _path >> version;

	if (_method.empty() || _path.empty() || version.empty()) {
		Logger::instance().log(ERROR, "Invalid HTTP Request");
		return false;
	}

	// Extract query string if present
	parseQueryString();

	if (version != "HTTP/1.1") {
		Logger::instance().log(ERROR, "Unsupported Http version: " + version);
		return false;
	}
	return true;
}

// Extract the query string from the path
void HTTPRequest::parseQueryString() {
	size_t pos = _path.find('?');
	if (pos != std::string::npos) {
		_queryString = _path.substr(pos + 1);
		_path = _path.substr(0, pos);
	} else {
		_queryString = "";
	}
}

void HTTPRequest::parseHeaders(const std::string& headers_line) {
	size_t pos = headers_line.find(":");
	if (pos != std::string::npos) {
		std::string key = headers_line.substr(0, pos);
		std::string value = headers_line.substr(pos + 1);
		trim(key);
		trim(value);
		_headers[key] = value;
	}
}


std::string HTTPRequest::getHost() const {
	std::map<std::string, std::string>::const_iterator it = _headers.find("Host");
	if (it != _headers.end()) {
		return it->second;
	}
	return "";
}


void HTTPRequest::parseBody(const std::string& body) {
	_body = body;
}

std::string HTTPRequest::getMethod() const {
	return _method;
}

std::string HTTPRequest::getPath() const {
	return _path;
}

std::string HTTPRequest::getQueryString() const {
	return _queryString;
}

std::map<std::string, std::string> HTTPRequest::getHeaders() const {
	return _headers;
}

std::string HTTPRequest::getBody() const {
	return _body;
}

void HTTPRequest::trim(std::string& s) const {
	size_t start = s.find_first_not_of(" \t\r\n");
	size_t end = s.find_last_not_of(" \t\r\n");
	if (start == std::string::npos || end == std::string::npos) {
		s = "";
	} else {
		s = s.substr(start, end - start + 1);
	}
}
