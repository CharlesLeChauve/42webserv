#include "HTTPRequest.hpp"
#include "Utils.hpp"
#include "Location.hpp"
#include "Logger.hpp"
#include <sstream>
#include <iostream>
#include <cstdlib>

HTTPRequest::HTTPRequest()
    : _complete(false), _connectionClosed(false), _maxBodySize(0),
      _contentLength(0), _bodyReceived(0), _headersParsed(false), _requestTooLarge(false) {}

HTTPRequest::HTTPRequest(int max_body_size)
    : _complete(false), _connectionClosed(false), _maxBodySize(max_body_size),
      _contentLength(0), _bodyReceived(0), _headersParsed(false), _requestTooLarge(false) {}

HTTPRequest::~HTTPRequest() {}

bool HTTPRequest::hasHeader(std::string header) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(header);
    if (it != _headers.end())
        return true;
    return false;
}

std::string HTTPRequest::getStrHeader(std::string header) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(header);
    if (it == _headers.end())
        return "";
    return it->second;
}

void HTTPRequest::parseRawRequest(const ServerConfig& config) {
    // Check if headers are fully received
    size_t header_end_pos = _rawRequest.find("\r\n\r\n");

    if (header_end_pos == std::string::npos) {
        return;
    }

    // Parse the request line
    size_t line_end_pos = _rawRequest.find("\r\n");
    std::string request_line = _rawRequest.substr(0, line_end_pos);
    std::istringstream iss(request_line);
    iss >> _method >> _path;

    // Determine max_body_size based on location
    const Location* location = config.findLocation(_path);
    if (location && location->clientMaxBodySize != -1) {
        _maxBodySize = location->clientMaxBodySize;
    }

    // Parse headers
    std::string headers = getRawRequest().substr(0, header_end_pos);
    std::istringstream headers_stream(headers);
    std::string header_line;
    while (std::getline(headers_stream, header_line)) {
        if (!header_line.empty() && header_line[header_line.size() - 1] == '\r') {
        	header_line.erase(header_line.size() - 1); //?? CHECK IF MODIFICATION IS GOOD !
    	}
        size_t colon_pos = header_line.find(":");
        if (colon_pos != std::string::npos) {
            std::string header_name = header_line.substr(0, colon_pos);
            std::string header_value = header_line.substr(colon_pos + 1);
            // Trim whitespace
            header_name.erase(0, header_name.find_first_not_of(" \t"));
            header_name.erase(header_name.find_last_not_of(" \t") + 1);
            header_value.erase(0, header_value.find_first_not_of(" \t"));
            header_value.erase(header_value.find_last_not_of(" \t") + 1);

            if (header_name == "Content-Length") {
                _contentLength = static_cast<size_t>(atoi(header_value.c_str()));
            }
        }
    }

    _headersParsed = true;

    // Check for request too large
    if (_maxBodySize > 0 && _contentLength > static_cast<size_t>(_maxBodySize)) {
        Logger::instance().log(WARNING, "Content-Length exceeds the configured maximum.");
        _requestTooLarge = true;
    }
}

bool HTTPRequest::parse() {
    size_t header_end_pos = _rawRequest.find("\r\n\r\n");
    if (header_end_pos == std::string::npos) {
        Logger::instance().log(ERROR, "Invalid HTTP request: missing header-body separator.");
        return false;
    }

    std::string headers_part = _rawRequest.substr(0, header_end_pos);
    std::string body_part = _rawRequest.substr(header_end_pos + 4);

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
        Logger::instance().log(ERROR, "Unsupported HTTP version: " + version);
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

std::string HTTPRequest::toStringHeaders() const {
    std::ostringstream oss;

    for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
        oss << it->first << ": " << it->second << "\r\n";
    }
    return oss.str();
}

std::string HTTPRequest::toString() const {
    std::ostringstream oss;
    oss << toStringHeaders();

    oss << "\r\n"; // End of headers
    oss << _body;  // Body of the response

    return oss.str();
}

bool HTTPRequest::getHeadersParsed() const { return _headersParsed; }
bool HTTPRequest::getRequestTooLarge() const { return _requestTooLarge; }
size_t HTTPRequest::getContentLength() const { return _contentLength; }
size_t HTTPRequest::getBodyReceived() const { return _bodyReceived; }
int HTTPRequest::getMaxBodySize() const { return _maxBodySize; }
std::string HTTPRequest::getRawRequest() const { return _rawRequest; }
bool HTTPRequest::getConnectionClosed() const { return _connectionClosed; }
unsigned long HTTPRequest::getLastActivity() const {return _lastActivity; }
bool HTTPRequest::isComplete() const { return _complete; }

void HTTPRequest::setBodyReceived(size_t size) { _bodyReceived = size; }
void HTTPRequest::setRequestTooLarge(bool value) { _requestTooLarge = value; }
void HTTPRequest::setConnectionClosed(bool value) { _connectionClosed = value; }
void HTTPRequest::setComplete(bool value) { _complete = value; }

void HTTPRequest::setLastActivity(unsigned long timestamp) { _lastActivity = timestamp; }
