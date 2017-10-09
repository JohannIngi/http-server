
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <glib.h>
#include <time.h>
#include <arpa/inet.h>

#define QUEUED 5
#define MAX_CLIENTS 10

#define GET 0
#define POST 1
#define HEAD 2
#define INVALID 3

#define VERSION_ONE 0
#define VERSION_TWO 1

#define MAX_TIME 30

typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;
typedef struct pollfd pollfd;

typedef struct 
{
    sockaddr_in address;
    int keep_alive;
    time_t timer;
    char time_buffer[64];
} client_info;

typedef struct
{
    sockaddr_in address;
    pollfd fds[MAX_CLIENTS];
    char buffer[4096];
    char body_buffer[1000];
    
}server_info;



void error_handler(char* error_buffer);
int find_next_available_client(pollfd* fds);
void get_time(client_info* client);
void create_get_body(server_info* server, client_info* client);
void create_header(server_info* server, client_info* client, size_t content_len);
void handle_get(server_info* server, client_info* client, int connfd);
void create_post_body(server_info* server, client_info* client, char* fields);
void handle_post(server_info* server, client_info* client, int connfd, int index);
void handle_head(server_info* server, client_info* client, int connfd);
void send_invalid(server_info* server, client_info* client, int connfd);
int get_version(char* buffer, int* index);
int is_home(char* buffer, int* index);
int get_method(char* buffer, int* index);
void write_to_log(client_info* client, server_info* server);
void client_handle(server_info* server, client_info* client, int connfd);
void setup_multiple_clients(server_info* server);
void add_new_client(server_info* server, client_info* clients);
void check_for_timeouts(client_info* clients, server_info* server);
void run_server(server_info* server, client_info* clients);
void startup_server(server_info* server, const char* port);

/*********************************************************************************************************/
/*  main function starts here and declares instances of the structs that are used throughout the server    */
/*********************************************************************************************************/
int main(int argc, char **argv){
    if (argc != 2) error_handler("invalid arguments");
    char* welcome_port = argv[1];
    server_info server;
    client_info clients[MAX_CLIENTS];
    fprintf(stdout, "Listening to server number: %s\n", welcome_port); fflush(stdout);  
    startup_server(&server, welcome_port);
    run_server(&server, clients);
}
/*************************************************************************/
/*  Write out the error message that are sent to this function           */
/*************************************************************************/
void error_handler(char* error_buffer){
    //printing error using the function perror()
    perror(error_buffer);
    //exiting using a predefined macro
    exit(EXIT_FAILURE);
}

/****************************************************/
/*  Finding the next available client               */
/****************************************************/
int find_next_available_client(pollfd* fds){

    int i;
    for(i = 1; i < MAX_CLIENTS; i++){
        if(fds[i].fd == -1){
            fprintf(stdout, "index found: %d\n", i);
            return i; //returns the next available object
        }
    }
    return -1;
}
/*************************************************/
/*  Setting the current date in a time buffer    */
/*************************************************/
void get_time(client_info* client){
    //setting ISO time
    time_t t;
    time(&t);
    struct tm* t_info;
    memset(client->time_buffer, 0, sizeof(client->time_buffer));
    t_info = localtime(&t);
    strftime(client->time_buffer, sizeof(client->time_buffer), "%a, %d %b %Y %X GMT", t_info);  
}
/*************************************************************************/
/*  Creating a html body for the GET method                              */
/*************************************************************************/
void create_get_body(server_info* server, client_info* client){
    memset(server->body_buffer, 0, sizeof(server->body_buffer));
    strcat(server->body_buffer, "<html><head><title>test</title></head><body>");
    char *ip = inet_ntoa(client->address.sin_addr); // getting the clients ip address
    strcat(server->body_buffer, ip);
    strcat(server->body_buffer, ":");
    char tmp[6];
    memset(tmp, 0, 6);
    sprintf(tmp, "%d", client->address.sin_port); // getting the clients port
    strcat(server->body_buffer, tmp);
    strcat(server->body_buffer, "<br>");
    strcat(server->body_buffer, "<form method=\"post\">Field: <input type=\"text\" name=\"pfield\"><br><input type=\"submit\" value=\"Submit\"></form></body></html>");
}
/*************************************************************************/
/*  Creating a html header to use in all the methods                     */
/*************************************************************************/
void create_header(server_info* server, client_info* client, size_t content_len){
    memset(server->buffer, 0, sizeof(server->buffer));
    strcat(server->buffer, "HTTP/1.1 200 OK\r\nDate: ");
    get_time(client);
    strcat(server->buffer, client->time_buffer);
    strcat(server->buffer, "Content-Type: text/html\r\nContent-Length: ");
    char tmp[10];
    memset(tmp, 0, 10);
    sprintf(tmp, "%zu", content_len);
    strcat(server->buffer, tmp);
    if (content_len > 0) {
        strcat(server->buffer, "\r\n\r\n");
        strcat(server->buffer, server->body_buffer);
    }
}
/*************************************************************************/
/*  Handling GET requests                                                */
/*************************************************************************/
void handle_get(server_info* server, client_info* client, int connfd){
    create_get_body(server, client);
    create_header(server, client, strlen(server->body_buffer));
    send(connfd, server->buffer, strlen(server->buffer), 0);
}
/*************************************************************************/
/*  Creating a html body for the POST method                             */
/*************************************************************************/
void create_post_body(server_info* server, client_info* client, char* fields){
    memset(server->body_buffer, 0, sizeof(server->body_buffer));
    strcat(server->body_buffer, "<html><head><title>test</title></head><body>http://foo.com/");
    char *ip = inet_ntoa(client->address.sin_addr);
    strcat(server->body_buffer, ip);
    strcat(server->body_buffer, ":");
    char tmp[6];
    memset(tmp, 0, 6);
    sprintf(tmp, "%d", client->address.sin_port);
    strcat(server->body_buffer, tmp);
    strcat(server->body_buffer, "<br>");
    strcat(server->body_buffer, "<form method=\"post\">Field: <input type=\"text\" name=\"pfield\"><br><input type=\"submit\" value=\"Submit\"></form><br>");

    //accessing the data fields using parsing
    gchar** split = g_strsplit(fields, "=", -1);
    int start = 0;
    while (split[start] != NULL){
        strcat(server->body_buffer, split[start]);
        start++;
        if (split[start] != NULL) {
            strcat(server->body_buffer, " -> ");
            strcat(server->body_buffer, split[start]);
            start++;
        }
        strcat(server->body_buffer, "<br>");
    }
    strcat(server->body_buffer, "</body></html>");
    g_strfreev(split);
}
/*************************************************************************/
/*  Handling POST requests                                               */
/*************************************************************************/
void handle_post(server_info* server, client_info* client, int connfd, int index){
    char fields[1000]; // <--- move to server struct
    memset(fields, 0, sizeof(fields));
    strcpy(fields, server->buffer + index);

    create_post_body(server, client, fields);
    create_header(server, client, strlen(server->body_buffer));
    send(connfd, server->buffer, strlen(server->buffer), 0);
}
/*************************************************************************/
/*  Handling HEAD requests                                               */
/*************************************************************************/
void handle_head(server_info* server, client_info* client, int connfd){
    create_header(server, client, 0);
    send(connfd, server->buffer, strlen(server->buffer), 0);
}
/*************************************************************************/
/*  Sending a 404 error if invalid requests                              */
/*************************************************************************/
void send_invalid(server_info* server, client_info* client, int connfd){
    fprintf(stdout, "404 error"); fflush(stdout);
    memset(server->buffer, 0, sizeof(server->buffer));
    strcat(server->buffer, "HTTP/1.1 404 \r\nDate: ");
    get_time(client);
    strcat(server->buffer, client->time_buffer);
    strcat(server->buffer, "Content-Type: text/html\r\nContent-Length: 75\r\n\r\n");
    strcat(server->buffer, "<html><head><title>test</title></head><body>");
    strcat(server->buffer, "ERROR - NOT FOUND</body></html>");
    send(connfd, server->buffer, strlen(server->buffer), 0);

}
/*************************************************************************/
/*  Acquiring the version of the http protocol                           */
/*************************************************************************/
int get_version(char* buffer, int* index){
    *index += 8;
    while (buffer[*index] == '\r' || buffer[*index] == '\n') {
        *index += 1;
    }
    return !strncmp(buffer + *index, "HTTP/1.1", 8) ? VERSION_TWO : VERSION_ONE;
}
/*************************************************************************/
/*  Returns true if url is legal                                         */
/*************************************************************************/
int is_home(char* buffer, int* index){
    int valid = buffer[*index] == '/' && buffer[*index + 1] == ' ';
    if (valid) {
        *index = *index + 2;
    }
    return valid;
}
/*************************************************************************/
/*  Acquiring the appropriate method                                     */
/*************************************************************************/
int get_method(char* buffer, int* index){
    if(g_str_has_prefix(buffer, "GET")){
        *index = 4;
        return GET;
    } else if(g_str_has_prefix(buffer, "POST")){
        *index = 5;
        return POST;
    } else if (g_str_has_prefix(buffer, "HEAD")) {
        *index = 5;
        return HEAD;
    } else {
        return INVALID;
    }
}


/*************************************************/
/*  Writing to a log file in the root directory  */
/*************************************************/
void write_to_log(client_info* client, server_info* server){

    get_time(client);
    //opening a file and logging a timestamp to it
    FILE* file;
    file = fopen("timestamp.log", "a");
    if (file == NULL) {
        fprintf(stdout, "Error in opening file!");
        fflush(stdout); 
    } else{
        int index = 0;
        int method = get_method(server->buffer, &index);
        char tmp_method[10];
        switch(method)
        {
            case GET:
                strcat(tmp_method, "GET");
                break;
            case POST:
                strcat(tmp_method, "POST");
                break;
            case HEAD:
                strcat(tmp_method, "HEAD");
                break;
        }
        //writing all necessary things to the timestamp
        fprintf(file, "%s: ", client->time_buffer);
        fprintf(file, "%s:%d ", inet_ntoa(client->address.sin_addr), client->address.sin_port);
        fprintf(file, "%s : ", tmp_method);
        fprintf(file, "%s:", "/");
        fprintf(file, " 200 OK \n");
    }
    fclose(file);
}
/*********************************************************************************************/
/*  client handler that decides what to do based on the client request; get, post or head    */
/*********************************************************************************************/
void client_handle(server_info* server, client_info* client, int connfd){
    write_to_log(client, server);

    fprintf(stdout, "%s\n", server->buffer); fflush(stdout);

    client->keep_alive = -1;

    //get_client_information(client);
    int index = 0;
    int method = get_method(server->buffer, &index);
    int home = is_home(server->buffer, &index);
    if (!home) {
        send_invalid(server, client, connfd); // SEND 404 
        return;
    }
    int version = get_version(server->buffer, &index);
    //parsing all the header
    char* start_of_header = server->buffer + index;
    gchar** split = g_strsplit(start_of_header, "\r\n", -1);
    int start = 0;
    while (split[start] != NULL) {
        size_t s = strlen(split[start]);
        if (s == 0) {
            index += 2;
            break;
        }
        index += s + 2;
        if (g_str_has_prefix(split[start], "Connection")) {
            if (!strcmp(split[start], "Connection: Keep-Alive")) {
                client->keep_alive = 1;
            }
        }
        start++;
    }
    //checks if the keep alive 
    if (client->keep_alive == -1) {
        client->keep_alive = (version == VERSION_TWO) ? 1 : 0;
 
    }
    //if keep alive is not requested or protocol is http 1.0
   if(client->keep_alive != 1){
    close(server->fds->fd);
    server->fds->fd = -1;
    }

    if(g_str_has_prefix(server->buffer, "GET")){
        handle_get(server, client, connfd);
    } else if(g_str_has_prefix(server->buffer, "POST")){
        handle_post(server, client, connfd, index);
    } else if (g_str_has_prefix(server->buffer, "HEAD")) {
        handle_head(server, client, connfd);
    } else {
        send_invalid(server, client, connfd);
    }

    g_strfreev(split);
}


/*************************************************************************/
/*  Setting up multiple clients connections                              */
/*************************************************************************/
void setup_multiple_clients(server_info* server){

    //memset(&server->fds, 0, sizeof(pollfd));
    for(int i = 1; i < MAX_CLIENTS; i++){
        server->fds[i].fd = -1;
    }
    server->fds[0].events = POLLIN;
}
/*************************************************************************/
/*  Adding new clients to the server                                     */
/*************************************************************************/
void add_new_client(server_info* server, client_info* clients) {

    socklen_t len = (socklen_t) sizeof(sockaddr_in);
    int next_client = find_next_available_client(server->fds); //iterates through all the fd's until he returns a client
    if(next_client < 0){ error_handler("Accept failed");}
    memset(&clients[next_client].address, 0, sizeof(struct sockaddr_in));
    server->fds[next_client].fd = accept(server->fds[0].fd, (sockaddr*)&clients[next_client].address, &len);
    clients[next_client].timer = time(NULL);
    server->fds[next_client].events = POLLIN;
}
/*************************************************************************/
/*  If clients time out then close connection                            */
/*************************************************************************/
void check_for_timeouts(client_info* clients, server_info* server){
    time_t now;
    for(int i = 1; i < MAX_CLIENTS; i++){
        if(difftime(time(&now), clients[i].timer) >= MAX_TIME && (server->fds[i].fd != -1)){
            fprintf(stdout, "Client has timedout\n"); fflush(stdout);

            close(server->fds[i].fd);
            server->fds[i].fd = -1;
        }
    }
}

/*************************************************************************/
/*  Running the server loop and calling necessary functions              */
/*************************************************************************/
void run_server(server_info* server, client_info* clients){

    setup_multiple_clients(server);
    fprintf(stdout, "Socket set up complete. Listening for request.\n"); fflush(stdout);
    while(23) {
        int p = poll(server->fds, MAX_CLIENTS, -1);
        if(p < 0){
            error_handler("Poll failed");
        } 
        if (p != 0) {
            for(int i = 0; i < MAX_CLIENTS; i++){
                if (server->fds[i].fd != -1 && (server->fds[i].revents & POLLIN)) {
                    if(i == 0){
                        add_new_client(server, clients);
                    } else{
                        ssize_t recv_msg_len = recv(server->fds[i].fd, server->buffer, sizeof(server->buffer) -1, 0);
                        if(recv_msg_len < 0){ error_handler("Receive failed");}
                        clients[i].timer = time(NULL);

                        if(recv_msg_len == 0){
                            close(server->fds[i].fd);
                            server->fds[i].fd = -1;
                        } else{
                            server->buffer[recv_msg_len] = '\0';
                            client_handle(server, &clients[i], server->fds[i].fd);
                        }
                    }
                }
            }
        }
        check_for_timeouts(clients, server);
    }    
}

/*************************************************************************/
/*  Starting up the server, binding and listening to appropriate port    */
/*************************************************************************/
void startup_server(server_info* server, const char* port){
    server->fds[0].fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&server->address, 0, sizeof(sockaddr_in));
    server->address.sin_family = AF_INET;
    //htonl and htons convert the bytes to be used by the network functions
    server->address.sin_addr.s_addr = htonl(INADDR_ANY);
    server->address.sin_port = htons(atoi(port));
    if(bind(server->fds[0].fd, (sockaddr *) &server->address, (socklen_t) sizeof(sockaddr_in)) < 0){error_handler("bind failed");}
    if(listen(server->fds[0].fd, QUEUED) < 0){error_handler("listen failed");}
}
