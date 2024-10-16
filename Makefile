# Variables
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

SRCDIR = src
OBJDIR = obj

SRC =

OBJ =

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

re: fclean all
