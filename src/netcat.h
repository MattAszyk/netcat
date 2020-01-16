#ifndef NETCAT
#define  NETCAT
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#define MAXBUF 60000
struct sockaddr_storage their_addr;
socklen_t sin_size;

typedef struct config
{
    char* hostname;
    char* port;
    int type;
    int family;
} config;

void* udp_input(void*);
void* tcp_input(void*);
// starts client
void client_start(config*);

// method return server's descriptor
int client_get_desc_of_socket(config*);
void* get_in_addr(struct sockaddr*);
void help();
// starts server
void server_start(config*);

// method return server's descriptor
int server_get_desc_of_socket(config*);

#endif