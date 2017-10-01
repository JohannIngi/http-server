
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <glib.h>

#define QUEUED 5

typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;

typedef struct 
{
    sockaddr address;
    socklen_t len;
} client_info;
typedef struct
{
    int sockfd;
    sockaddr_in address;
    char buffer[4096];
}server_info;
//*************************************
// comment here
//*************************************
void startup_server(server_info* server, const char* port){
    // Create and bind a TCP socket.
    server->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    //If sockfd is some bullshit throw error!
    memset(&server->address, 0, sizeof(sockaddr_in));
    server->address.sin_family = AF_INET;
    // Network functions need arguments in network byte order instead of
    // host byte order. The macros htonl, htons convert the values.
    server->address.sin_addr.s_addr = htonl(INADDR_ANY);
    server->address.sin_port = htons(atoi(port));
    bind(server->sockfd, (sockaddr *) &server->address, (socklen_t) sizeof(sockaddr_in));
    // Before the server can accept messages, it has to listen to the
    // welcome port. A backlog of one connection is allowed.
    listen(server->sockfd, QUEUED);
}
//*************************************
// comment here
//*************************************
int accept_request(client_info* client, server_info* server){
    client->len = (socklen_t) sizeof(sockaddr_in);
    int connfd = accept(server->sockfd, (sockaddr *) &client->address, &client->len);
    if (connfd == -1) { //if -1 then the client is inactive
        perror("Exiting...");
        exit(EXIT_FAILURE);
    }
    return connfd;
}
//*************************************
// comment here
//*************************************
void receive_message(server_info* server, int* connfd){
    // Receive from connfd, not sockfd.
    ssize_t recv_msg_len = recv(*connfd, server->buffer, sizeof(server->buffer) - 1, 0);
    server->buffer[recv_msg_len] = '\0';
}
/*void error_handler(){}*/
/*void client_handle(){}*/
int main(int argc, char **argv)
{
    char* port = argv[1];
    server_info server;
    client_info client;

    fprintf(stdout, "Listening to server number: %s\n", port); fflush(stdout);
     
    startup_server(&server, port);

    memset(&client.address, 0, sizeof(sockaddr_in));
    fprintf(stdout, "Socket set up complete. Listening for request.\n"); fflush(stdout);
    
    for (;;) {

        fprintf(stdout, "Waiting to receive message...\n"); fflush(stdout);

        int connfd = accept_request(&client, &server);
        receive_message(&server, &connfd);

        fprintf(stdout, "Received message:\n\n--------------------\n%s\n", server.buffer);fflush(stdout);

        //handle_inc(&client, &server)

        // if (request is GET)
        // { dostuff get way} 
        // else if { request is Post} {
        // }
        // else if {request is head} {
        // }
        // else {error}

        //send(connfd, drasl, strlen(drasl), 0);
        // Close the connection.
        shutdown(connfd, SHUT_RDWR);
        close(connfd);
    }
}