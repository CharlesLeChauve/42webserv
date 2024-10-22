#include "HTTPRequest.hpp"
#include <sstream>
#include <iostream>
#include <cstdlib>

HTTPRequest::HTTPRequest() {}

HTTPRequest::~HTTPRequest() {}

bool HTTPRequest::parse(const std::string& raw_request) {
    size_t header_end_pos = raw_request.find("\r\n\r\n");
    if (header_end_pos == std::string::npos) {
        std::cerr << "Invalid HTTP request: missing header-body separator." << std::endl;
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
        std::cerr << "Invalid HTTP request: missing request line." << std::endl;
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
            std::cerr << "Failed to read the entire body." << std::endl;
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
        std::cerr << "Invalid HTTP request line." << std::endl;
        return false;
    }

    // Extract query string if present
    parseQueryString();

    if (version != "HTTP/1.1") {
        std::cerr << "Unsupported HTTP version: " << version << std::endl;
        return false;
    }
    return true;
}

// Extract the query string from the path
void HTTPRequest::parseQueryString() {
    size_t pos = _path.find('?');
    if (pos != std::string::npos) {
        _queryString = _path.substr(pos + 1);
        _path = _path.substr(0, pos);  // Remove the query string from the path
    } else {
        _queryString = "";
    }
}

void HTTPRequest::parseHeaders(const std::string& headers_line) {
    size_t pos = headers_line.find(":");
    if (pos != std::string::npos) {
        std::string key = headers_line.substr(0, pos);
        std::string value = headers_line.substr(pos + 1);
        _headers[key] = value;
    }
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






#ifdef TEST_HTTPREQUEST_MAIN

int main() {
std::string raw_request =
	"POST /submit HTTP/1.1\r\n"
	"Host: localhost:8080\r\n"
	"User-Agent: curl/7.68.0\r\n"
	"Content-Length: 11\r\n\r\n"
	"Hello World";


	HTTPRequest request;
	if (request.parse(raw_request)) {
		std::cout << "Method: " << request.getMethod() << std::endl;
		std::cout << "Path: " << request.getPath() << std::endl;

		std::cout << "Headers:" << std::endl;
		std::map<std::string, std::string> headers = request.getHeaders();
		for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
			std::cout << it->first << ": " << it->second << std::endl;
		}

		std::cout << "Body: " << request.getBody() << std::endl;
	} else {
		std::cout << "Failed to parse HTTP request." << std::endl;
	}

	return 0;
}

#endif
