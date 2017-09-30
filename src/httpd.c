/*Includes*/
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <glib.h>

/*Defines*/
#define QUEUED 5

/*Typedefs*/
typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;

/*Structs*/
typedef struct 
{
    ssize_t recv_msg;
    sockaddr address;
    socklen_t len;
} client_info;
typedef struct
{
    int sockfd;
    sockaddr_in address;

}server_info;

/*Functions*/
void startup_server(server_info* server, const char* port){
    server->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    //If sockfd is some bullshit throw error!
    memset(&server->address, 0, sizeof(sockaddr_in));
    server->address.sin_family = AF_INET;
    server->address.sin_addr.s_addr = htonl(INADDR_ANY);
    server->address.sin_port = htons(atoi(port));
    bind(server->sockfd, (sockaddr *) &server->address, (socklen_t) sizeof(sockaddr_in));
}
int accept_request(client_info* client, server_info* server){
    client->len = (socklen_t) sizeof(sockaddr_in);
    int connfd = accept(server->sockfd, (sockaddr *) &client->address, &client->len);
    if (connfd == -1) {
        perror("Exiting...");
        exit(EXIT_FAILURE);
    }

    return connfd;
}
/*void receive_message(client_info* client, char* buffer){
    // Receive from connfd, not sockfd.
    client->recv_msg = recv(client->connfd, &buffer, sizeof(&buffer) - 1, 0);
    buffer[client->recv_msg] = '\0';
}*/
/*void error_handler(){}*/
/*void client_handle(){}*/
int main(int argc, char **argv)
{
    char* port = argv[1];
    server_info server;
    client_info client;
    char buffer[4096];
    //GHashTable * html_header = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    fprintf(stdout, "Listening to server number: %s\n", port); fflush(stdout);
    // Create and bind a TCP socket.
    

    // Network functions need arguments in network byte order instead of
    // host byte order. The macros htonl, htons convert the values.
    startup_server(&server, port);

    memset(&client.address, 0, sizeof(sockaddr_in));
    fprintf(stdout, "Socket set up complete. Listening for request.\n"); fflush(stdout);
    // Before the server can accept messages, it has to listen to the
    // welcome port. A backlog of one connection is allowed.
    listen(server.sockfd, QUEUED);
    for (;;) {
        fprintf(stdout, "Waiting to receive...\n"); fflush(stdout);
        // We first have to accept a TCP connection, connfd is a fresh
        // handle dedicated to this connection.
        int connfd = accept_request(&client, &server);
        client.recv_msg = recv(connfd, buffer, sizeof(buffer) - 1, 0);
        buffer[client.recv_msg] = '\0';
        //receive_message(&client, buffer);

        fprintf(stdout, "Received message:\n\n--------------------\n%s\n", buffer);fflush(stdout);

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