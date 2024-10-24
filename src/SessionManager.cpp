// Si possible, inclure une bibliothèque de hachage MD5 ou SHA1
#include "SessionManager.hpp"

SessionManager::SessionManager(std::string session_id)
{
	if (session_id.size() > 0) {
		_session_id = session_id;
		_first_con = false;
		std::cout << "!!!!!!!!!!!!!!!!" << std::endl << "Welcome_back User : " << _session_id << std::endl <<  "!!!!!!!!!!!!!!!!" << std::endl;
	 } else {
		_session_id = generate_session_id();
		_first_con = true;
		std::cout << "!!!!!!!!!!!!!!!!" << std::endl << "Welcome to User : " << _session_id << " for his first connection !" << std::endl <<  "!!!!!!!!!!!!!!!!" << std::endl;
	}
}

SessionManager::SessionManager()
{
	_session_id = generate_session_id();
}

SessionManager::~SessionManager()
{
}



std::string SessionManager::generate_session_id() {
    std::stringstream ss;

    // Ajouter l'horodatage avec une meilleure précision
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ss << tv.tv_sec << tv.tv_usec;

    ss << rand() << rand();
    std::string session_id = ss.str(); // À implémenter ou utiliser une bibliothèque externe

    return session_id;
}

const std::string& SessionManager::getSessionId() const {
	return _session_id;
}

bool SessionManager::getFirstCon() const {
	return _first_con;
}
