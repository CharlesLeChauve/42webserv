#include "CGIHandler.hpp"
#include "Server.hpp"
#include <unistd.h>  // For fork, exec, pipe
#include <sys/wait.h>  // For waitpid
#include <iostream>
#include <cstring>  // For strerror

CGIHandler::CGIHandler(Server& server) : _server(server) {}

CGIHandler::~CGIHandler() {}

template <typename T>
std::string to_string(T value) {
	std::ostringstream oss;
	oss << value;
	return oss.str();
}

std::string CGIHandler::executeCGI(const std::string& scriptPath, const HTTPRequest& request) {
	std::string fullPath = scriptPath;

	int pipefd[2];
	if (pipe(pipefd) == -1) {
		std::cerr << "Pipe failed: " << strerror(errno) << std::endl;
		return "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n" + _server.generateErrorPage(500, _server.getErrorMessage(500));
	}

	int pipefd_in[2];  // Ajout d'un second pipe pour l'entrée standard
	if (pipe(pipefd_in) == -1) {
		std::cerr << "Pipe for STDIN failed: " << strerror(errno) << std::endl;
		return "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n" + _server.generateErrorPage(500, _server.getErrorMessage(500));
	}

	pid_t pid = fork();
	if (pid == 0) {
		close(pipefd[0]);  // Fermer l'extrémité de lecture du pipe pour stdout
		dup2(pipefd[1], STDOUT_FILENO);  // Rediriger stdout vers le pipe pour la sortie CGI

		close(pipefd_in[1]);  // Fermer l'extrémité d'écriture du pipe pour stdin
		dup2(pipefd_in[0], STDIN_FILENO);  // Rediriger stdin vers le pipe pour l'entrée CGI

		if (request.getMethod() == "POST") {
			setenv("REQUEST_METHOD", "POST", 1);  // Définir POST comme méthode
			setenv("CONTENT_LENGTH", to_string(request.getBody().size()).c_str(), 1);  // Définir la taille du corps de la requête
		} else {
			setenv("REQUEST_METHOD", "GET", 1);  // Définir GET comme méthode par défaut
		}

		setupEnvironment(request.getQueryString());

		execl(fullPath.c_str(), fullPath.c_str(), NULL);  // Exécuter le script CGI
		std::cerr << "Failed to execute CGI script: " << fullPath << ". Error: " << strerror(errno) << std::endl;
		exit(EXIT_FAILURE);
	} else if (pid > 0) {
		close(pipefd[1]);  // Fermer l'extrémité d'écriture du pipe pour stdout
		close(pipefd_in[0]);  // Fermer l'extrémité de lecture du pipe pour stdin

		if (request.getMethod() == "POST") {
			// Envoyer le corps de la requête au processus CGI via stdin
			write(pipefd_in[1], request.getBody().c_str(), request.getBody().size());
		}
		close(pipefd_in[1]);  // Fermer l'extrémité d'écriture après avoir envoyé les données

		char buffer[1024];
		std::string cgiOutput;

		ssize_t bytesRead;
		while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
			cgiOutput.append(buffer, bytesRead);
		}
		close(pipefd[0]);

		int status;
		waitpid(pid, &status, 0);
		if (WIFEXITED(status)) {
			int exitCode = WEXITSTATUS(status);
			if (exitCode != 0) {
				std::string errorPage = _server.generateErrorPage(500, _server.getErrorMessage(500) + ": CGI script failed with exit code " + to_string(exitCode));
				return "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n" + errorPage;
			}
		}

		if (cgiOutput.find("HTTP/1.1") == std::string::npos) {
			cgiOutput = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n" + cgiOutput;
		}

		return cgiOutput;
	} else {
		std::cerr << "Fork failed: " << strerror(errno) << std::endl;
		return "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n" + _server.generateErrorPage(500, _server.getErrorMessage(500));
	}
}

void CGIHandler::setupEnvironment(const std::string& queryString) {
	setenv("QUERY_STRING", queryString.c_str(), 1);
}
