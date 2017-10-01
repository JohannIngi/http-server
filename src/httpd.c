
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
    char header_buffer[1024];
} client_info;
typedef struct
{
    int sockfd;
    sockaddr_in address;
    char buffer[4096];
}server_info;
//*************************************
// Insert comment here 
//*************************************
void startup_server(server_info* server, const char* port){
    // Create and bind a TCP socket.
    server->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    //If sockfd is some bullshit throw error!
    memset(&server->address, 0, sizeof(sockaddr_in));
    server->address.sin_family = AF_INET;
    //htonl and htons convert the bytes to be used by the network functions
    server->address.sin_addr.s_addr = htonl(INADDR_ANY);
    server->address.sin_port = htons(atoi(port));
    bind(server->sockfd, (sockaddr *) &server->address, (socklen_t) sizeof(sockaddr_in));
    //Server is listening to the port in order to accept messages. A backlog of five connections is allowed
    listen(server->sockfd, QUEUED);
}
//*************************************
// Accepting a requst from client
//*************************************
int accept_request(client_info* client, server_info* server){
    memset(&client->address, 0, sizeof(sockaddr_in));
    client->len = (socklen_t) sizeof(sockaddr_in);
    int connfd = accept(server->sockfd, (sockaddr *) &client->address, &client->len);
    if (connfd == -1) { //if -1 then the client is inactive
        perror("Exiting...");
        exit(EXIT_FAILURE);
    }
    return connfd;
}
//*************************************
// Insert comment here
//*************************************
void receive_message(server_info* server, int* connfd){
    // Receive from connfd, not sockfd.
    ssize_t recv_msg_len = recv(*connfd, server->buffer, sizeof(server->buffer) - 1, 0);
    server->buffer[recv_msg_len] = '\0';
}
//*************************************
// Insert comment here
//*************************************
void get_header_type(server_info* server, client_info* client){
    char** header_type = g_strsplit(server->buffer, " ", -1);
    strcat(client->header_buffer, *header_type);
    g_strfreev(header_type);
}

/*void client_handle(){}*/
/*void error_handler(){}*/
int main(int argc, char **argv)
{
    char* port = argv[1];
    server_info server;
    client_info client;

    fprintf(stdout, "Listening to server number: %s\n", port); fflush(stdout);
     
    startup_server(&server, port);

    
    fprintf(stdout, "Socket set up complete. Listening for request.\n"); fflush(stdout);
    
    while(1) {

        fprintf(stdout, "Waiting to receive message...\n"); fflush(stdout);

        int connfd = accept_request(&client, &server);
        receive_message(&server, &connfd);
        get_header_type(&server, &client);
        fprintf(stdout, "Received message:\n\n--------------------\n%s\n", server.buffer);fflush(stdout);
        fprintf(stdout, "HEADER TYPE: %s\n", client.header_buffer);fflush(stdout);

        //handle_inc(&client, &server)

        //get/url http/1.1



        //send(connfd, drasl, strlen(drasl), 0);
        // Close the connection.
        shutdown(connfd, SHUT_RDWR);
        close(connfd);
    }
}