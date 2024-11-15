# Variables
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -g -pedantic

SRCDIR = src
OBJDIR = obj

# Liste des fichiers source
SRC = \
	$(SRCDIR)/ConfigParser.cpp \
	$(SRCDIR)/ServerConfig.cpp \
	$(SRCDIR)/Socket.cpp \
	$(SRCDIR)/main.cpp \
	$(SRCDIR)/Server.cpp \
	$(SRCDIR)/CGIHandler.cpp \
	$(SRCDIR)/HTTPResponse.cpp \
	$(SRCDIR)/SessionManager.cpp \
	$(SRCDIR)/UploadHandler.cpp \
	$(SRCDIR)/Logger.cpp \
	$(SRCDIR)/utils.cpp \
	$(SRCDIR)/HTTPRequest.cpp

# Liste des fichiers objets
OBJ = $(SRC:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# Détecter le système d'exploitation
UNAME_S := $(shell uname -s)

# Commande pour vérifier si php-cgi est déjà installé
CHECK_PHP_CGI = $(shell which php-cgi > /dev/null 2>&1; echo $$?)

# Règles
all: webserver

webserver: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f webserver

php:
ifeq ($(CHECK_PHP_CGI), 0)
	@echo "php-cgi is already installed"
else
ifeq ($(UNAME_S), Darwin)  # macOS
	@echo "Installing php-cgi for macOS"
	@brew install php
	@echo "php-cgi installed successfully"
else ifeq ($(UNAME_S), Linux)  # Linux
	@echo "Installing php-cgi for Linux"
	@sudo apt-get update
	@sudo apt-get install -y php-cgi
	@echo "php-cgi installed successfully"
else
	@echo "Unsupported OS"
endif
endif

php_clean:
ifeq ($(UNAME_S), Darwin)  # macOS
	@echo "Uninstalling php-cgi for macOS"
	@brew uninstall php
	@echo "php-cgi uninstalled successfully"
else ifeq ($(UNAME_S), Linux)  # Linux
	@echo "Uninstalling php-cgi for Linux"
	@sudo apt-get remove --purge -y php-cgi
	@sudo apt-get autoremove -y
	@echo "php-cgi uninstalled successfully"
else
	@echo "Unsupported OS"
endif

clean_logs:
	rm -rf logs/*

re: fclean all

PHONY: clean fclean all webserver php php_clean clean_logs
