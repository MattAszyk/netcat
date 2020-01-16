#include "netcat.h"
int debug = 0;

int main(int argc, char **argv)
{
    config package = {NULL,NULL,SOCK_STREAM,AF_UNSPEC};
    int imp;
    int server = 0;
    opterr = 0; 

    while((imp = getopt(argc,argv,"ul46h")) != -1)
    {
		switch(imp)
		{
				case 'u':
				package.type = SOCK_DGRAM;
				break;
				case 'l':
				server = 1;
				break;
				case '4':
				package.family = AF_INET;
				break;
				case '6':
				package.family = AF_INET6;
				break;
                case 'h':
                help();
                break;
		}
	}

    switch (server)
    {
        case 0:
            if(optind +2 != argc)
            {
                help();
            }
            package.hostname = argv[optind];
            package.port = argv[optind+1];
            client_start(&package);
        break;
        case 1:
            package.port = (optind + 1 == argc ? argv[optind] : argv[optind+1]);
            package.hostname = (optind + 2 == argc ? argv[optind] : NULL);
            server_start(&package);
        break;
    }
    
    return 0;
}

void* get_in_addr(struct sockaddr* sa)
{
    if(sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

void* tcp_input(void *data)
{
    int* dst = (int*)data;
    int input_length;
    char string[MAXBUF];

    while ((input_length = read(STDIN_FILENO,string,MAXBUF-1)) > 0)
    {
        write(*dst,string,input_length);
    }
    if(input_length == -1)
    {
        perror("Error in response thread.\n");
    }
    return NULL;
}

void* udp_input(void *data)
{
    char string[MAXBUF];
    int socket = *(int*)data;
    while(fgets(string,MAXBUF-1,stdin), !feof(stdin))
    {
        sendto(socket,string,strlen(string),0,
        (struct sockaddr*)&their_addr,sin_size);
    }
    return NULL;
}

int client_get_desc_of_socket(config* conf)
{
    int sockfd;
    int gai_error;
    char server_ip[INET6_ADDRSTRLEN];
    void* inaddr;
    struct addrinfo hints, *servinfo, *p;
    memset(&hints,0,sizeof(hints));
    hints.ai_family = conf->family;
    hints.ai_socktype = conf->type;

    if((gai_error = getaddrinfo(conf->hostname,conf->port, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n",gai_strerror(gai_error));
        exit(0);
    }

    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(sockfd == -1)
        {
            perror("client: socket");
            continue;
        }
        if(connect(sockfd,p->ai_addr,p->ai_addrlen) == -1)
        {
            close(sockfd);
            continue;
        }
        break;
    }
    if(p == NULL)
    {
        printf("Client: failed to establish connection to the server.\n");
        exit(0);
    }
    inaddr = get_in_addr(p->ai_addr);
    inet_ntop(p->ai_family, inaddr, server_ip, sizeof(server_ip));
    freeaddrinfo(servinfo);
    return sockfd;
}


void client_start(config *conf)
{
    int sockfd = client_get_desc_of_socket(conf);
    int input_length;
    char string[MAXBUF];
    pthread_t input;

    if(pthread_create(&input, NULL, tcp_input, (void*)&sockfd) != 0)
    {
        printf("Thread error: can't create new thread.\n");
        close(sockfd);
        exit(0);
    }

    //Read and print response
    while((input_length = read(sockfd,string,MAXBUF)) > 0)
    {
        write(STDOUT_FILENO,string,input_length);
        if(input_length < 0)
        {
            printf("Error while sending input\n");
        }
    }

    pthread_cancel(input);
    pthread_join(input,NULL);
    close(sockfd);
}

int server_get_desc_of_socket(config* conf)
{
    int sockfd;
    int sockopt;
    int rv;
    int yes;
    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = conf->family;
    hints.ai_socktype = conf->type;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(conf->hostname,conf->port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }


    for (p = servinfo; p != NULL; p = p->ai_next) 
    {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1)
        {
            perror("socket");
            continue;
        }

        //ONLY FOR TCP CONNECTIONS
        if(conf->type == SOCK_STREAM)
        {
            sockopt =
                setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
            if (sockopt == -1)
            {
                perror("setsockopt");
                exit(0);
            }
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) 
        {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        perror("server: failed to bind");
        exit(0);
    }

    if (conf->type == SOCK_STREAM && listen(sockfd, 10) == -1)
    {
        perror("listen");
        exit(0);
    }
    
    
    freeaddrinfo(servinfo);

    return sockfd;
}


void server_start(config* conf)
{
    int sockfd = server_get_desc_of_socket(conf);
    int new_fd;
    int length;
    
    sin_size = sizeof their_addr;
    pthread_t input;
    char string[MAXBUF];

    switch (conf->type)
    {
    case SOCK_STREAM: //PART OF TCP
        new_fd = accept(sockfd, (struct sockaddr*)&their_addr,&sin_size);
        if(new_fd == -1)
        {
            perror("accept error");
            exit(0);
        }
        break;
    case SOCK_DGRAM: // PART OF UDP
        new_fd = sockfd; 
        break;
    }
    if(pthread_create(&input, NULL, (conf->type == SOCK_STREAM ? tcp_input : udp_input), (void*)&new_fd) == -1)
	{
			perror("thread");
	}
    if(conf->type == SOCK_STREAM)
    {	
		while((length = read(new_fd,string,MAXBUF)) > 0)
		{
			if(write(STDOUT_FILENO, string, length) == -1 )
			{
				perror("write()");
			}
		}
		if(length == -1)
		{
				perror("read");
		}
		close(new_fd);
	}
	else
	{
		while((length = recvfrom(sockfd,string, MAXBUF -1 , 0 , (struct sockaddr *)&their_addr, &sin_size))>0)
		{
			if(write(STDOUT_FILENO, string, length) == -1 )
			{
				perror("write()");
			}
		}
		if(length == -1)
		{
			perror("recvfrom");
		}
	}
    pthread_cancel(input);
	pthread_join(input, NULL);
	close(sockfd);	
}


void help()
{
    printf("[GENERAL OPTIONS]\n-6 IPv6\n-4 IPv4\n-u UDP Connection\n\
[USAGE]\n\
Client mode:\n\
nc -[FLAGS] [IP/HOSTNAME] [PORT]\n\
Server mode:\n\
nc -l[FLAGS] [PORT]\n");
    exit(0);
}