# 42webserv

## Introduction

**42webserv** is a web server developed in C++ as part of the Webserv project at 42 school. The goal of this project is to implement an HTTP 1.0 server compliant with RFC specifications.

## Features

- **Support for HTTP requests** (GET, POST, DELETE)
- **Handling multiple connections** using `poll()`
- **Execution of CGI scripts**
- **Custom HTTP error management**
- **Configurable server via a configuration file**
- **Hosting of static files** 
- **Redirections and route management**

## Installation

### Prerequisites
- Unix-based system (Linux or macOS recommended)
- Compatible C++ compiler (g++ recommended)
- `make`

### Compilation
```sh
make
```

### Execution
```sh
./webserv [config_file]
```
By default, the server will use the configuration file designed to serve the project's demo static site (on MacOs), which is included in this repository.

## Configuration
The server is configurable via a `.conf` configuration file that allows you to define:
- Listening ports
- Virtual hosts
- Routes and their behavior
- Custom error pages
- CGI scripts

## Project Structure
```
42webserv/
├── config/       # Configuration files
├── ressources/   # Test files and external resources
├── src/          # Server source code
├── templates/    # HTML templates for error pages
├── www/          # Public directory containing static files
├── Makefile      # Build file
└── README.md     # Documentation
```

## Authors
- **tgibert** **CharlesLeChauve**
- **anporced** **Tulece**
- **jeguerin** **JayZ66**

## License
Project developed as part of 42 school, under a free license.

