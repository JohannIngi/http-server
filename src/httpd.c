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
/*typedef struct 
{
    
} client_info;*/

int main(int argc, char **argv)
{
    char* port = argv[1];
    int sockfd;
    sockaddr_in server, client;
    char buffer[4096];
    //GHashTable * html_header = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    fprintf(stdout, "Listening to server number: %s\n", port); fflush(stdout);
    // Create and bind a TCP socket.
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // Network functions need arguments in network byte order instead of
    // host byte order. The macros htonl, htons convert the values.
    memset(&server, 0, sizeof(sockaddr_in));
    memset(&client, 0, sizeof(sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(atoi(port));
    bind(sockfd, (sockaddr *) &server, (socklen_t) sizeof(server));
    fprintf(stdout, "Socket set up complete. Listening for request.\n"); fflush(stdout);
    // Before the server can accept messages, it has to listen to the
    // welcome port. A backlog of one connection is allowed.
    listen(sockfd, QUEUED);
    for (;;) {
        fprintf(stdout, "Waiting to receive...\n"); fflush(stdout);
        // We first have to accept a TCP connection, connfd is a fresh
        // handle dedicated to this connection.
        socklen_t len = (socklen_t) sizeof(client);

        int connfd = accept(sockfd, (sockaddr *) &client, &len);
        // Receive from connfd, not sockfd.
        ssize_t n = recv(connfd, buffer, sizeof(buffer) - 1, 0);

        buffer[n] = '\0';

        fprintf(stdout, "Received message:\n\n--------------------\n%s\n", buffer);fflush(stdout);

        // Close the connection.
        shutdown(connfd, SHUT_RDWR);
        close(connfd);
    }
}
