#pragma once

#include "Logger.hpp"
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include <string>
#include <cstring>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <sys/time.h>
#include <sys/stat.h>
#include <map>
#include <iostream>
#include <iomanip>
#include <fstream>


class SessionManager
{
private:
	std::string		_session_id;
	bool			_first_con;
	std::map<std::string, std::string> _session_data;
	void setData(const std::string& key, const std::string& value, bool append = false);
	void manageUserSession(HTTPRequest* request, HTTPResponse* response, int client_fd, SessionManager& session);
	void	persistSession();
	void	loadSession();

public:
	SessionManager(std::string session_id);
	SessionManager();
	~SessionManager();

	std::string getData(const std::string& key) const;
	std::string	curr_time();
	void	getManager(HTTPRequest* request, HTTPResponse* response, int client_fd, SessionManager& session);

// 	static void signalHandler(int signal) {
   
//     std::cout << "Signal recu: " << signal << std::endl;
//     std::string userInput;
//     while (true) {
//         std::cout << "Would you like to keep or delete the session file ? (k/d)" << std::endl;
//         std::getline(std::cin, userInput);

//         if (std::cin.eof()) {
//             std::cout << "EOF received, session will be kept." << std::endl;
//             break ; 
//         }

//         if (userInput == "k" || userInput == "K") {
//             std::cout << "Session file has been kept." << std::endl;
//             break ;
//         }

//         else if (userInput == "d" || userInput == "D") {
//             std::string sessionFile = "sessions/" + getSessionId() + ".txt";
//             if (std::remove(sessionFile.c_str()) == 0) {
//                 std::cout << "Session file has been successfully deleted." << std::endl;
//             }
//             else {
//                 std::cout << "Problem while trying to delete session file." << std::endl;
//             }
//             break ;
//         }

//         else {
//             std::cout << "Invalid option. Please enter 'd' to delete or 'k' to keep." << std::endl;
//             continue ;
//     	}
// 	}
// }

	std::string generate_session_id();
	std::string generateUUID();
	const std::string& getSessionId() const;
	bool getFirstCon() const;	
};

