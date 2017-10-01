
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
    char html[4096];
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
void receive_message(server_info* server, client_info* client, int* connfd){
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
//*************************************
// Insert comment here 
//*************************************
void client_handle(server_info* server, client_info* client){
    //checking if the header_buffer matches a GET request
    if(g_str_match_string("GET", client->header_buffer, 1)){
        //Do getstuff
    }
    //checking if the header_buffer matches a POST request
    else if(g_str_match_string("POST", client->header_buffer, 1)){
        //Do poststuff
    }
    //checking if the header_buffer matches a HEAD request
    else if(g_str_match_string("HEAD", client->header_buffer, 1)){

    }
    else{
        //Error stuff
    }
}
void generate_html(client_info* client){
    char html_stuff[4096] =
    "HTTP/1.1 200 OK\n"
    "Content-Type: text/html; charset=UTF-8\n\n"
    "<!DOCTYPE html>\n"
    "<html><head><title>HTTPServer</title></head>\n"
    "<body><center><h1> Hello mr. Client</h1>\n"
    "</center></body></html>\n";
    strcpy(client->html, html_stuff);
}
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
        receive_message(&server, &client, &connfd);
        get_header_type(&server, &client);
        fprintf(stdout, "Received message:\n\n--------------------\n%s\n", server.buffer);fflush(stdout);
        fprintf(stdout, "HEADER TYPE: %s\n", client.header_buffer);fflush(stdout);


        client_handle(&server, &client);
        //get/url http/1.1

        generate_html(&client);
        send(connfd, client.html, strlen(client.html), 0);
        // Close the connection.
        shutdown(connfd, SHUT_RDWR);
        close(connfd);
    }
}