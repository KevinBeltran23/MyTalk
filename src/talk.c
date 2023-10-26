#include "talk.h"
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/time.h>
#include<netdb.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<poll.h>
#include<ctype.h>
#include<pwd.h>
#define CONTROLC 3
#define BUFFER_SIZE 1024

typedef struct mode{
    int v;
    int a;
    int N;
}mode;

void server(char *port, mode flags);
void client(char *port, char *hostname, mode flags);

int main(int argc, char *argv[]){
    char *hostname, *port;
    struct mode flags;
    int i;
     
    /* parsing command line :) */
    memset(&flags, 0, sizeof(flags));
    for(i = 1; argv[i]; i++){
        if(strcmp(argv[i], "-v") == 0){
                flags.v = 1;
        }
        else if(strcmp(argv[i], "-a") == 0){
                flags.a = 1;
        }
        else if(strcmp(argv[i], "-N") == 0){
                flags.N = 1;
        }
        else if(strlen(argv[i]) > 2){
            break;
        }
    }
    /* no hostname */
    if(argc == i + 1){
        port = argv[i];
        server(port, flags);
    }
    /* yes hostname */
    else if (argc == i + 2){
        hostname = argv[i];
        port = argv[i + 1];
        client(port, hostname, flags);
    }
    else{
        fprintf(stderr, "Usage: mytalk [-v] [-a] [-N] [hostname] port\n");
    }
    return 0;
}

void client(char *port, char *hostname, mode flags){
    char receivebuf[BUFFER_SIZE], sendbuf[BUFFER_SIZE];
    struct addrinfo hints, *res;
    ssize_t bytes_received;
    struct pollfd fds[2];
    struct passwd *pw;
    int connection, ready, serverfd;
    char breaker, *username;
    uid_t uid;

    /*set up server connection*/
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if((getaddrinfo(hostname, port, &hints, &res)) == -1){
        perror("getaddrinfo");
        exit(EXIT_FAILURE);
    }
    if((serverfd = socket(res->ai_family, res->ai_socktype, 
    res->ai_protocol)) == -1){
        perror("socket");
        exit(EXIT_FAILURE);
    }
    /*establish a connection*/
    if((connection = connect(serverfd, res -> ai_addr, 
    res -> ai_addrlen)) == -1){
        perror("connect");
        exit(EXIT_FAILURE);
    }
    /*send packet*/
    memset(sendbuf, 0, BUFFER_SIZE);
    uid = getuid();
    pw = getpwuid(uid);
    if(pw){
        username = pw->pw_name;
    }
    sprintf(sendbuf, "%s", username);
    if((send(serverfd, sendbuf, strlen(sendbuf), 0)) == -1){
        perror("send");
        exit(EXIT_FAILURE);
    }
    /*get accepted or denied a connection*/
    printf("Waiting for response from %s.\n", hostname);
    memset(receivebuf, 0, BUFFER_SIZE);
    if((bytes_received = recv(serverfd, receivebuf, BUFFER_SIZE - 1, 0))==-1){
        perror("recv");
        exit(EXIT_FAILURE);
    }
    if(strcmp(receivebuf, "ok") != 0){
        printf("%s declined connection\n", hostname);
        close(serverfd);
        return;
    }
    /*Server accepted you - start ncurses window to communicate*/
    memset(sendbuf, 0, BUFFER_SIZE);
    if(!flags.N){
        start_windowing();
    }
    set_verbosity(flags.v);
    while(1){
        fds[0].fd = 0;
        fds[0].events = POLLIN;
        fds[1].fd = serverfd;
        fds[1].events = POLLIN;

        if((ready = poll(fds, 2, -1)) == -1){
            perror("poll");
            exit(EXIT_FAILURE);
        }

        if(ready > 0 || !has_hit_eof()){
            if(fds[0].revents & POLLIN){
                update_input_buffer();
                if(has_hit_eof()){
                    break;
                }
                /* only read if you will not block */
                if(has_whole_line() && !has_hit_eof()){
                    read_from_input(sendbuf, BUFFER_SIZE);
                    if((send(serverfd, sendbuf, strlen(sendbuf), 0)) == -1){
                        perror("send");
                        exit(EXIT_FAILURE);
                    }
                    memset(sendbuf, 0, BUFFER_SIZE);
                }
            }
            if(fds[1].revents & POLLIN){
                bytes_received = recv(serverfd, receivebuf, BUFFER_SIZE, 0);
                if(bytes_received == -1){
                    perror("recv");
                    exit(EXIT_FAILURE);
                }
                /* partner disconnected */
                if(bytes_received == 0){
                    sprintf(receivebuf,
                    "Connection closed. ^C to terminate.\n");
                    write_to_output(receivebuf, strlen(receivebuf));
                    memset(receivebuf, 0, BUFFER_SIZE);
                    while(1){
                        scanf("%c", &breaker);
                        if(breaker == CONTROLC){
                            break;
                        }
                    }
                    break;
                }
                else{
                    write_to_output(receivebuf, bytes_received);
                    memset(receivebuf, 0, BUFFER_SIZE);
                }
            }
        }
    }
    stop_windowing();
    close(serverfd);
}

void server(char *port, mode flags){
    char receivebuf[BUFFER_SIZE], sendbuf[BUFFER_SIZE], client[NI_MAXHOST];
    struct sockaddr_in server_address, client_address;
    ssize_t bytes_received;
    int serverfd, clientfd, ready;
    struct pollfd fds[2];
    char breaker;
    socklen_t client_len;
    
    /*create a socket*/
    if((serverfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("socket");
        exit(EXIT_FAILURE);
    }
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(atoi(port));
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    
    /*bind it to an address*/
    if(bind(serverfd, (struct sockaddr*)&server_address, 
    sizeof(server_address)) == -1){
        perror("bind");
        exit(EXIT_FAILURE);
    }
    /*listen for clients*/
    if(listen(serverfd, SOMAXCONN) == -1){
        perror("listen");
        exit(EXIT_FAILURE);
    }
    /*accept a client*/
    client_len = sizeof(client_address);
    clientfd = accept(serverfd,(struct sockaddr*)&client_address,&client_len);
    if(clientfd == -1){
        perror("accept");
        exit(EXIT_FAILURE);
    }
    /*receive packet from client*/
    memset(receivebuf, 0, BUFFER_SIZE);
    if(recv(clientfd, receivebuf, BUFFER_SIZE, 0) == -1){
        perror("receive");
        exit(EXIT_FAILURE);
    }
    memset(sendbuf, 0, BUFFER_SIZE);
    if(!flags.a){
        /*accept or deny client*/
        if((getnameinfo((struct sockaddr*)&client_address, client_len, 
        client, NI_MAXHOST, NULL, 0, 0)) == -1){
            perror("getnameinfo");
            exit(EXIT_FAILURE);
        }
        printf("Mytalk request from %s@%s. Accept (y/n) ",receivebuf,client);
        scanf("%s", sendbuf);
    }
    if((strcasecmp(sendbuf, "yes") == 0) || (strcasecmp(sendbuf, "y") == 0) 
    || flags.a){
        memset(sendbuf, 0, BUFFER_SIZE);
        sprintf(sendbuf, "ok");
        if(send(clientfd, sendbuf, strlen(sendbuf), 0) == -1){
            perror("send");
            exit(EXIT_FAILURE);
        }
    }
    else{
        memset(sendbuf, 0, BUFFER_SIZE);
        sprintf(sendbuf, "NO");
        if(send(clientfd, sendbuf, strlen(sendbuf), 0) == -1){
            perror("send");
            exit(EXIT_FAILURE);
        }
        close(serverfd);
        close(clientfd);
        return;
    }

    /*start ncurses windowing to communicate*/
    memset(sendbuf, 0, BUFFER_SIZE);
    memset(receivebuf, 0, BUFFER_SIZE);
    if(!flags.N){
        start_windowing();
    }
    set_verbosity(flags.v);
    while(1){   
        fds[0].fd = 0;
        fds[0].events = POLLIN;
        fds[1].fd = clientfd;
        fds[1].events = POLLIN;

        if((ready = poll(fds, 2, -1)) == -1){
            perror("poll");
            exit(EXIT_FAILURE);
        }

        if(ready > 0){
            if(fds[0].revents & POLLIN){
                update_input_buffer();
                if(has_hit_eof()){
                    break;
                }
                /* only read if you will not block */
                if(has_whole_line() && !has_hit_eof()){
                    read_from_input(sendbuf, BUFFER_SIZE);
                    if((send(clientfd, sendbuf, strlen(sendbuf), 0)) == -1){
                        perror("send");
                        exit(EXIT_FAILURE);
                    }
                    memset(sendbuf, 0, BUFFER_SIZE);
                }
            }
            else if(fds[1].revents & POLLIN){
                bytes_received = recv(clientfd, receivebuf, BUFFER_SIZE, 0);
                if(bytes_received == -1){
                    perror("recv");
                    exit(EXIT_FAILURE);
                }
                /* partner disconnected :( */
                if(bytes_received == 0){
                    sprintf(receivebuf, 
                    "Connection closed. ^C to terminate.\n");
                    write_to_output(receivebuf, strlen(receivebuf));
                    memset(receivebuf, 0, BUFFER_SIZE);
                    while(1){
                        scanf("%c", &breaker);
                        if(breaker == CONTROLC){
                            break;
                        }
                    }
                    break;
                }
                else{
                    write_to_output(receivebuf, bytes_received);
                    memset(receivebuf, 0, BUFFER_SIZE);
                }
            }
        }
    }
    stop_windowing();
    close(clientfd);
    close(serverfd);
}
