#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>

// Server structure
struct Server {
    int domain;
    int service;
    int protocol;
    u_long interface;
    int port;
    int backlog;
    int socket;
    struct sockaddr_in address;
    void (*launch)(struct Server *server);
};

// Server constructor
struct Server server_constructor(int domain, int service, int protocol, u_long interface, int port, int backlog, void (*launch)(struct Server *server));
void server_launch(struct Server *server); 
#endif

