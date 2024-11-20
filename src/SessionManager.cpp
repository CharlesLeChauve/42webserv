// Si possible, inclure une bibliothèque de hachage MD5 ou SHA1
#include "SessionManager.hpp"

SessionManager::SessionManager(std::string session_id) {
    if (session_id.size() > 0) {
        _session_id = session_id;
        _first_con = false;
        std::cout << "Welcome_back User : " << _session_id << std::endl;
	} else {
        _session_id = generateUUID();
        _first_con = true;
        std::cout << "Welcome to User : " << _session_id << " for his first connection !" << std::endl;
        Logger::instance().log(INFO, "Session id generated " + _session_id);
    }
    loadSession();
}


SessionManager::SessionManager()
{
	_session_id = generateUUID();
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

/*
UUID est une chaine unique (36 car.) qui garantit que l'id de session est unique + sécurise les sessions.
elle repose sur des algo standardisés, utilisés pour générer des clés de sessions, des ids de transaction, etc.
- Utiliser rand() pour générer les octets nécessaires
- Ajuster les bits pour respecter la structure UUID v4
- Convertir les octets en une chaîne avec les tirets.
*/
std::string SessionManager::generateUUID() {
	std::stringstream	uuid;
	for (int i = 0; i < 16; ++i) {
		unsigned char byte = static_cast<unsigned char>(rand() % 256); // Génère un octet
		if (i == 6) { // Identifier la version de l'UUID (V4)
			byte &= 0x0F; // Met à 0 les 4 bits les + significatifs.
			byte |= 0x40; // Add 0100 aux 4 bits les + significatifs
		}
		if (i == 8) { // Fixer la variante, indique quel schéma est utilisé (RFC 4122)
			byte &= 0x3F; // Met à 0 les 2 bits les + sign.
			byte |= 0x80; // Add 10 aux 2 bits.
		}
		uuid << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte); // Convert to format hex
		if (i == 3 || i == 5 || i == 7 || i == 9) // Format UUID : tirets après les xème octet.
			uuid << "-";
	}
	return (uuid.str());
}

void SessionManager::setData(const std::string& key, const std::string& value, bool append) {
    if (append && _session_data.find(key) != _session_data.end()) {
        _session_data[key] += ", " + value;  // Ajout de la nouvelle valeur
    }
    else {
        _session_data[key] = value;
    }
    Logger::instance().log(INFO, "Data set in session: " + key + " = " + _session_data[key]);
}


    
std::string SessionManager::getData(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = _session_data.find(key);
    if (it != _session_data.end()) {
           return it->second;
    }
    return ""; // Return empty string if key is not found
}

const std::string& SessionManager::getSessionId() const {
	return _session_id;
}

bool SessionManager::getFirstCon() const {
	return _first_con;
}


std::string cleanValue(const std::string& value) {
    
    if (value.empty())
        return value;

    size_t start = 0;
    size_t end = value.size();

    while (start < end && isspace(value[start]))
        ++start;
    while (end > start && isspace(value[end - 1]))
        --end;
    return (value.substr(start, end - start));
}

void SessionManager::persistSession() {
    std::string filepath = "sessions/" + _session_id + ".txt"; // Définition du chemin de fichier

    // Avant d'écrire dans le fichier, nettoie les données
    for (std::map<std::string, std::string>::iterator it = _session_data.begin(); it != _session_data.end(); ++it) {
        it->second = cleanValue(it->second);
    }

    // Ouvre le fichier en mode ajout
    std::ofstream file(filepath.c_str(), std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Erreur : impossible de sauvegarder la session dans " << filepath << std::endl;
        return;
    }

    // Ajoute une ligne vide avant [General] si le fichier n'est pas vide
    file.seekp(0, std::ios::end); // Place le curseur à la fin
    if (file.tellp() > 0) {
        file << "\n"; // Ajoute une ligne vide
    }

    // Écrit les nouvelles données sous la section [General]
    file << "[General]\n";
    if (_session_data.find("last_access_time") != _session_data.end()) {
        file << "last_access_time=" << cleanValue(curr_time()) << "\n";
    }
    if (_session_data.find("status") != _session_data.end()) {
        file << "status=" << cleanValue(_session_data["status"]) << "\n";
    }
    if (_session_data.find("user_agent") != _session_data.end()) {
        file << "user_agent=" << cleanValue(_session_data["user_agent"]) << "\n";
    }

    file << "\n[Requests]\n";

    // Écrit les nouvelles données sous la section [Requests]
    if (_session_data.find("requested_pages") != _session_data.end()) {
        file << "Pages=" << cleanValue(_session_data["requested_pages"]) << "\n";
    }
    if (_session_data.find("methods") != _session_data.end()) {
        file << "Methods=" << cleanValue(_session_data["methods"]) << "\n";
    }

    file.close();
}

void SessionManager::loadSession() {
    std::string filepath = "sessions/" + _session_id + ".txt";
    std::ifstream file(filepath.c_str());
    if (!file.is_open()) {
        _first_con = true; // Première connexion si aucun fichier existant
        return;
    }

    _first_con = false;

    std::string line, current_section;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') // Ignore lignes vides ou commentaires
            continue;
        if (line[0] == '[' && line[line.size() - 1] == ']') {
            current_section = line.substr(1, line.size() - 2);
            continue;
        }

        size_t delimiter_pos = line.find('=');
        if (delimiter_pos != std::string::npos) {
            std::string key = line.substr(0, delimiter_pos);
            std::string value = line.substr(delimiter_pos + 1);
            _session_data[key] = value;
        }
    }

    // Vérifie si le statut est défini
    if (_session_data.find("status") == _session_data.end()) {
        _session_data["status"] = "new user";
    }

    file.close();
}


std::string SessionManager::curr_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t raw_time = tv.tv_sec;
    struct tm * time_info;
    char buffer[80];
    
    time_info = localtime(&raw_time);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", time_info);  // Format lisible
    return std::string(buffer);
}


