#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>
#include <map>

class HTTPResponse {
public:
    HTTPResponse();
    ~HTTPResponse();

    void setStatusCode(int code);
    void setReasonPhrase(const std::string& reason);
    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& body);
    HTTPResponse& beError(int err_code, const std::string& errorContent = "");

    int getStatusCode() const;
    std::string getReasonPhrase() const;
    std::string generateErrorPage();
    std::string generateErrorPage(std::string infos);
    std::map<std::string, std::string> getHeaders() const;
    std::string getBody() const;
    std::string getStrHeader(std::string header) const;

    std::string toString() const;
    std::string toStringHeaders() const;

private:
    int _statusCode;
    std::string _reasonPhrase;
    std::map<std::string, std::string> _headers;
    std::string _body;
};

std::string getSorryPath();

#endif
