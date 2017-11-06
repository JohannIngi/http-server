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
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/engine.h>
#include <openssl/conf.h>

#define QUEUED 5
#define MAX_CLIENTS 10

#define GET 1
#define POST 2
#define HEAD 3
#define INVALID 4

#define MAX_TIME 30

// Paths to file.
#define CERTIFICATE "fd.crt"
#define PRIVATE_KEY "fd.key"
#define PASSWORD_FILE "passwd.ini"

typedef int bool;
#define true 1
#define false 0

typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;
typedef struct pollfd pollfd;

//all the variables that each client needs kept in a struct
typedef struct 
{
    sockaddr_in address;
    int keep_alive;
    time_t timer;
    char time_buffer[64];
    char method[8];
    char color[16];
    char data_buffer[1024];
    char query_buffer[1024];
    bool isItColor;
    GHashTable* client_headers;
    GHashTable* client_queries;
} client_info;
//all server variables kept in a struct
typedef struct
{
    sockaddr_in address;
    pollfd fds[MAX_CLIENTS];
    char buffer[4096];
    char fields[1024];
    char header_buffer[4096];
    
}server_info;

//global variable for the SSL authentication
static SSL_CTX *ctx;

void error_handler(char* error_buffer);
int find_next_available_client(pollfd* fds);
void get_time(client_info* client);
void create_header(server_info* server, client_info* client, size_t content_len);
void handle_get(server_info* server, client_info* client);
void handle_post(server_info* server, client_info* client);
void write_to_log(client_info* client);
void client_handle(server_info* server, client_info* client, int connfd);
void setup_multiple_clients(server_info* server);
void add_new_client(server_info* server, client_info* clients);
void check_for_timeouts(client_info* clients, server_info* server);
void run_server(server_info* server, client_info* clients);
void startup_server(server_info* server, const char* port);
void parse(server_info* server, client_info* client, int connfd);
void set_method(client_info* client);
void set_keep_alive(client_info* client);
void generate_error(server_info* server, client_info* client, char* error_version, char* error_msg, size_t content_len);
void send_error();
void parse_URL(server_info* server, client_info* client, int connfd, char** first_line);
void add_queries(gpointer key, gpointer val, gpointer data);
void parse_query(client_info* client, char** query);
void set_color(client_info* client);
void check_content(server_info* server);
void init_SSL();


int main(int argc, char **argv){
    if (argc != 3) error_handler("invalid arguments");
    char* welcome_port = argv[1];
    char* welcome_port_2 = argv[2];
    server_info server;
    memset(&server, 0, sizeof(server));
    client_info clients[MAX_CLIENTS];
    memset(clients, 0, sizeof(clients));
    fprintf(stdout, "Listening to port number: %s\n", welcome_port); fflush(stdout);
    fprintf(stdout, "Listening to port number: %s\n", welcome_port_2); fflush(stdout);
    void init_ssl();
    startup_server(&server, welcome_port);
    run_server(&server, clients);
}
/*************************************************/
/*  Handles all error that occur and shuts down  */
/*************************************************/
void error_handler(char* error_buffer){
    perror(error_buffer);
    exit(EXIT_FAILURE);
}
/*************************************************/
/*  Finding the next available client            */
/*************************************************/
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
/*************************************************/
/*  Writing to a log file in the root directory  */
/*************************************************/
void write_to_log(client_info* client){

    get_time(client);
    //opening a file and logging a timestamp to it
    FILE* file;
    file = fopen("timestamp.log", "a");
    if (file == NULL) {
        fprintf(stdout, "Error in opening file!");
        fflush(stdout); 
    }
    else{
        //writing all necessary things to the timestamp
        fprintf(file, "%s: ", client->time_buffer);
        fprintf(file, "%s:%d ", inet_ntoa(client->address.sin_addr), client->address.sin_port);
        fprintf(file, "%s : ", client->method);
        fprintf(file, "%s:", "/");
        fprintf(file, " 200 OK \n");
    }
    fclose(file);
}
/**************************************************************************************************/
/*  Client handler that decides what to do based on the client request; get, post, head or error  */
/**************************************************************************************************/
void client_handle(server_info* server, client_info* client, int connfd){

    client->isItColor = false;
    parse(server, client, connfd);

    set_method(client);

    set_keep_alive(client);
    
    if(g_strcmp0(client->method, "GET") == 0){
        handle_get(server, client);
        create_header(server, client, strlen(server->buffer));
        check_content(server);
        write_to_log(client);
        send(connfd, server->header_buffer, strlen(server->header_buffer), 0);
    }
    else if(g_strcmp0(client->method, "POST") == 0){
        handle_post(server, client);
        create_header(server, client, strlen(server->buffer));
        check_content(server);
        write_to_log(client);
        send(connfd, server->header_buffer, strlen(server->header_buffer), 0);
    }
    else if(g_strcmp0(client->method, "HEAD") == 0){
        handle_get(server, client);
        int content_length = strlen(server->buffer);
        create_header(server, client, content_length);
        write_to_log(client);
        send(connfd, server->header_buffer, strlen(server->header_buffer), 0);
    }
    else{
        char error_version[] = {"HTTP/1.1 501"};
        char error_msg[] = {"Invalid method!"};
        generate_error(server, client, error_version, error_msg, sizeof(error_msg));
        send_error(server, connfd);
    }
}

/**************************************************************************************/
/*  Checking if there is some html content inside the server buffer to be displayed  */
/*************************************************************************************/
void check_content(server_info* server){
        if (server->buffer != NULL) {
            strcat(server->header_buffer, "\r\n\r\n");
            strcat(server->header_buffer, server->buffer);
        }else{
            strcat(server->header_buffer, "\r\n\r\n");
        }
}
/****************************************************************************************************/
/*  Looks for the "method" key in the headers-hastable and sets the current method if it finds one  */
/*  Else method is null and will send a 501 error in client handle                                  */
/****************************************************************************************************/
void set_method(client_info* client){
    memset(client->method, 0, sizeof(client->method));
    char* tmp = (char*)g_hash_table_lookup(client->client_headers, "METHOD");
    if(g_strcmp0(tmp, "GET") == 0){
        strcat(client->method, tmp);
    }
    else if(g_strcmp0(tmp, "POST") == 0){
        strcat(client->method, tmp);
    }
    else if(g_strcmp0(tmp, "HEAD") == 0){
        strcat(client->method, tmp);
    }

}
/***************************************************************************************************************/
/*  Checks if there has been a color determined with a query or a cookie, by looking in both client hashtables */
/***************************************************************************************************************/
void set_color(client_info* client){
    char* tmp_bg = (char*)g_hash_table_lookup(client->client_queries, "bg");
    if(tmp_bg != NULL){
        strcpy(client->color, tmp_bg);
        return;
    }
    char* tmp_cookie = (char*)g_hash_table_lookup(client->client_headers, "Cookie");
    if(tmp_cookie != NULL){
        //parsing the cookies that are found
        char** cookie_split = g_strsplit(tmp_cookie, "; ", -1);
        int start = 0;
        while(cookie_split[start] != NULL){
            char** value_split = g_strsplit(cookie_split[start], "=", 2);
            if(value_split[0] != NULL && value_split[1] != NULL && !g_strcmp0(value_split[0], "color")){
                strcpy(client->color, value_split[1]);
            }
            start++;
            g_strfreev(value_split);
        }
        g_strfreev(cookie_split);
        return;
    }


}
/*********************************************************************************************************************************/
/* Int keep-alive is initialized at -1, if client asks for keep-alive the variable is 1 if he asks for close connection it is 0 */
/*  server deals further with it in the create header function                                                                    */
/*********************************************************************************************************************************/
void set_keep_alive(client_info* client){
    client->keep_alive = -1;
    char* tmp = (char*)g_hash_table_lookup(client->client_headers, "Connection");
    if(g_strcmp0(tmp, "keep-alive") == 0){
        client->keep_alive = 1; //there is asked for a keep alive
    }
    else if (g_strcmp0(tmp, "close") == 0){
        client->keep_alive = 0; // no need for keep alive
    }
}
/*****************************************************************************************************/
/*  Creates the html needed to send an error, takes in error version and error message as parameters */
/*****************************************************************************************************/
void generate_error(server_info* server, client_info* client, char* error_version, char* error_msg, size_t content_len){
    memset(server->header_buffer, 0, sizeof(server->header_buffer));
    strcat(server->header_buffer, error_version);
    strcat(server->header_buffer, "\r\nDate: ");
    get_time(client);
    strcat(server->header_buffer, client->time_buffer);
    strcat(server->header_buffer, "Content-Type: text/html\r\nContent-Length: ");
    char tmp[10];
    memset(tmp, 0, 10);
    sprintf(tmp, "%zu", content_len + 67);
    strcat(server->header_buffer, tmp);
    strcat(server->header_buffer, "\r\n\r\n");
    strcat(server->header_buffer, "<html><head><title>test</title></head><body><center><h1>");
    strcat(server->header_buffer, error_msg);
    strcat(server->header_buffer, "</h1></center></body></html>");
}
/**********************************/
/*  Sends the error to the client */
/**********************************/
void send_error(server_info* server, int connfd){
    send(connfd, server->header_buffer, strlen(server->header_buffer), 0);
}
/************************************************************************************************************************/
/*  Creates the neccessery elements that are needed to send a header                                                    */
/*  Loooks for the appropriate version the client asked for and takes in content-length as a parameter                  */
/*  Sends the client a cookie if he initializes a color on the color page                                               */
/*  If the keep-alive variable is set to -1 then we know that the client did not specify anything about the connection  */
/*  If that happens the version determines keep-alive, HTTP/1.1 = alive, HTTP/1.0 = close                               */
/************************************************************************************************************************/
void create_header(server_info* server, client_info* client, size_t content_len){
    memset(server->header_buffer, 0, sizeof(server->header_buffer));
    char* tmp_version = (char*)g_hash_table_lookup(client->client_headers, "VERSION");
    if(g_strcmp0(tmp_version, "HTTP/1.1") == 0){
        strcat(server->header_buffer, "HTTP/1.1");
    }
    else{
        strcat(server->header_buffer, "HTTP/1.0");
    }

    strcat(server->header_buffer, " 200 OK\r\nDate: ");
    get_time(client);
    strcat(server->header_buffer, client->time_buffer);

    strcat(server->header_buffer, "\r\nContent-Length: ");
    char tmp[10];
    memset(tmp, 0, 10);
    sprintf(tmp, "%zu", content_len);
    strcat(server->header_buffer, tmp);

    if(client->color[0] != 0){
        strcat(server->header_buffer, "\r\nSet-Cookie: color=");
        strcat(server->header_buffer, client->color);
    }

    strcat(server->header_buffer, "\r\nContent-Type: text/html\r\nConnection: ");
    if(client->keep_alive == -1){
        if(g_strcmp0(tmp_version, "HTTP/1.1") == 0){
            strcat(server->header_buffer, "keep-alive\r\n");

        }
        else if(g_strcmp0(tmp_version, "HTTP/1.0") == 0){
            strcat(server->header_buffer, "close\r\n");
        }
    }
    else if(client->keep_alive == 0){
        strcat(server->header_buffer, "close\r\n");
    }
    else if(client->keep_alive == 1){
        strcat(server->header_buffer, "keep-alive\r\n");
    }

    strcat(server->header_buffer, "\r\n\r\n");


}
/***********************************************************************/
/*  Creating a html body for GET requests                              */
/*  Sets the color for the client                                      */
/*  Puts the clients port and IP address into the html to be displayed */
/***********************************************************************/
void handle_get(server_info* server, client_info* client){
    memset(server->buffer, 0, sizeof(server->buffer));
    strcat(server->buffer, "<html><head><title>test</title></head><body style=\"background-color: ");
    fprintf(stdout, "THIS IS THE COLOR BUFFER in the HTML: %s\n", client->color); fflush(stdout);
    strcat(server->buffer, client->color);
    strcat(server->buffer, "\"><center><h2>http://");
    char* tmp_host = (char*)g_hash_table_lookup(client->client_headers, "Host");
    strcat(server->buffer, tmp_host);
    strcat(server->buffer, "/ ");
    char *ip = inet_ntoa(client->address.sin_addr); // getting the clients ip address
    strcat(server->buffer, ip);
    strcat(server->buffer, ":");
    char tmp[6];
    memset(tmp, 0, 6);
    sprintf(tmp, "%d", client->address.sin_port); // getting the clients port
    strcat(server->buffer, tmp);
    strcat(server->buffer, "</h2><br>");
    strcat(server->buffer, "<form method=\"post\"><h3>To go to the home page type: '/' in the url");
    strcat(server->buffer, "<br>To go to the index page type: '/index' in the url");
    strcat(server->buffer, "<br>To go to the color page type: '/color' in the url (example color query: ?bg=red");
    strcat(server->buffer, "<br>To go to the test page type: '/test' in the url (example query: ?TSAM=AWESOME");
    strcat(server->buffer, "<br>Enter something awesome into this field box and it will be displayed below</h3><br><input type=\"text\" name=\"value\"><br><input type=\"submit\" value=\"Submit\"></form><br>");
    //If the variable isItColor is true then we are on the color page, if not then the server can regard all queries as tests and display them
    if(client->isItColor != true){g_hash_table_foreach(client->client_queries, add_queries, server->buffer);}
    strcat(server->buffer, "</center></body></html>");
}
/************************************************************************/
/*  Creating a html body for POST requests                              */
/*  Sets the color for the client                                       */
/*  Puts the clients port and IP address into the html to be displayed  */
/*  Accesses the host information from the client for display           */
/************************************************************************/
void handle_post(server_info* server, client_info* client){
    memset(server->fields, 0, sizeof(server->fields));
    strcpy(server->fields, server->buffer);
    memset(server->buffer, 0, sizeof(server->buffer));
    strcat(server->buffer, "<html><head><title>test</title></head><<body style=\"background-color: ");
    strcat(server->buffer, client->color);
    strcat(server->buffer, "\"><center><h2>http://");
    char* tmp_host = (char*)g_hash_table_lookup(client->client_headers, "Host");
    strcat(server->buffer, tmp_host);
    strcat(server->buffer, "/ ");
    char *ip = inet_ntoa(client->address.sin_addr); // getting the clients ip address
    strcat(server->buffer, ip);
    strcat(server->buffer, ":");
    char tmp[6];
    memset(tmp, 0, 6);
    sprintf(tmp, "%d", client->address.sin_port); // getting the clients port
    strcat(server->buffer, tmp);
    strcat(server->buffer, "</h2><br>");
    strcat(server->buffer, "<form method=\"post\"><h3>To go to the home page type: '/' in the url");
    strcat(server->buffer, "<br>To go to the index page type: '/index' in the url");
    strcat(server->buffer, "<br>To go to the color page type: '/color' in the url (example color query: ?bg=red");
    strcat(server->buffer, "<br>To go to the test page type: '/test' in the url (example query: ?TSAM=AWESOME");
    strcat(server->buffer, "<br>Enter something awesome into this field box and it will be displayed below</h3><br><input type=\"text\" name=\"value\"><br><input type=\"submit\" value=\"Submit\"></form><br>");
    //parsing the data that the client sends
    char** sub_fields = g_strsplit(server->fields, "\r\n\r\n", -1); //accesing the data fields that are accepted in the post request
    char** split_fields = g_strsplit(sub_fields[1], "&", -1); // splitting the data fields on the '='
    int start = 0;
    memset(client->data_buffer, 0, sizeof(client->data_buffer));
    while (split_fields[start] != NULL){
        char** split_data = g_strsplit(split_fields[start], "=", 2);
        if (split_data[0] != NULL && split_data[1] != NULL) {
            //adding the parsed data to a buffer
            strcat(client->data_buffer, split_data[0]);
            strcat(client->data_buffer, " ----> ");
            strcat(client->data_buffer, split_data[1]);
            strcat(client->data_buffer, "<br>");
            start++;
        }
        g_strfreev(split_data);
    }
    //adding the data to the html display buffer
    strcat(server->buffer,client->data_buffer);
    //If the variable isItColor is true then we are on the color page, if not then the server can regard all queries as tests and display them
    if(client->isItColor != true){g_hash_table_foreach(client->client_queries, add_queries, server->buffer);}

    strcat(server->buffer, "</center></body></html>");

    g_strfreev(sub_fields);
    g_strfreev(split_fields);
}
/************************************************************************************************************/
/*  A function that is used with g_hash_table_foreach to print out all the variables in the query hashtable */
/************************************************************************************************************/
void add_queries(gpointer key, gpointer val, gpointer data){
    char* buffer = (char*)data;
    strcat(buffer, (char*)key);
    strcat(buffer, "--->");
    strcat(buffer, (char*)val);
    strcat(buffer, "<br>");

}
/****************************************************************************************/
/*  Parsing function that parses everything from the header into appropriate hashtable  */
/*  Takes the first line separately and parses from it method, url and version          */
/****************************************************************************************/
void parse(server_info* server, client_info* client, int connfd){
    char** tmp = g_strsplit(server->buffer, "\r\n", -1); //getting the first line
    char** first_line = g_strsplit(tmp[0], " ", -1); //parsing the first line into method, url and version

    g_hash_table_insert(client->client_headers, g_strdup("METHOD"), g_strdup(first_line[0]));
    g_hash_table_insert(client->client_headers, g_strdup("URL"), g_strdup(first_line[1]));
    g_hash_table_insert(client->client_headers, g_strdup("VERSION"), g_strdup(first_line[2]));
    
    int index = 1;
    while(tmp[index] != NULL){
        if(g_strcmp0(tmp[index], "") == 0){
            break;
        }
        else{
            char** header = g_strsplit(tmp[index], ": ", 2);
            g_hash_table_insert(client->client_headers, g_strdup(header[0]), g_strdup(header[1]));
            index++;
            g_strfreev(header);
        }
    }
    parse_URL(server, client, connfd, first_line);

    g_strfreev(tmp);
    g_strfreev(first_line);   
}
/****************************************************************************************/
/*  Parsing function that parses everything from the URL into appropriate hashtable     */
/*  Decides what actions to take on what url                                            */
/****************************************************************************************/
void parse_URL(server_info* server, client_info* client, int connfd, char** first_line){
    char** tmp_query = g_strsplit(first_line[1], "?", -1);
    if(g_strcmp0(tmp_query[0], "/color") == 0){
        //add color
        client->isItColor = true;
        parse_query(client, tmp_query);
        set_color(client);
    }
    else if(g_strcmp0(tmp_query[0], "/") == 0){
        //normal get response
        memset(client->color, 0, sizeof(client->color));
        strcat(client->color, "white");
    }
    else if(g_strcmp0(tmp_query[0], "/test") == 0){
        //do something - show query one in each line
        client->isItColor = false;
        parse_query(client, tmp_query);    
    }
    else if(g_strcmp0(tmp_query[0], "/home") == 0){
        //funZone for get
        memset(client->color, 0, sizeof(client->color));
        strcat(client->color, "purple");
    }
    else{ //it is an invalid url send error message
        char error_version[] = {"HTTP/1.1 404"};
        char error_msg[] = {"INVALID URL!"};
        generate_error(server, client, error_version, error_msg, sizeof(error_msg));
        send_error(server, connfd);
    }

    g_strfreev(tmp_query);
}
/***************************************************************************************/
/*  Parsing function that parses everything into appropriate hashtable                 */
/*  splits all the queries up and stores them appropriatly                             */
/***************************************************************************************/
void parse_query(client_info* client, char** query){
    if(query[1] == NULL){
        return;
    }
    int start = 0;
    char** tmp_amp = g_strsplit(query[1], "&", -1);
    while(tmp_amp[start] != NULL){
        char** tmp_equal = g_strsplit(tmp_amp[start], "=", 2);
        g_hash_table_insert(client->client_queries, g_strdup(tmp_equal[0]), g_strdup(tmp_equal[1] == NULL ? "NULL" : tmp_equal[1]));
        start++;
        g_strfreev(tmp_equal);
    }
    g_strfreev(tmp_amp);
}
/***********************************************/
/*  Setting up multiple clients connections    */
/***********************************************/
void setup_multiple_clients(server_info* server){
    for(int i = 1; i < MAX_CLIENTS; i++){
        server->fds[i].fd = -1;
    }
    server->fds[0].events = POLLIN;
}
/***********************************************/
/*  Adding new clients to the server           */
/***********************************************/
void add_new_client(server_info* server, client_info* clients) {

    socklen_t len = (socklen_t) sizeof(sockaddr_in);
    int next_client = find_next_available_client(server->fds); //iterates through all the fd's until he returns a client
    if(next_client < 0){ error_handler("Accept failed");}
    memset(&clients[next_client].address, 0, sizeof(struct sockaddr_in));
    server->fds[next_client].fd = accept(server->fds[0].fd, (sockaddr*)&clients[next_client].address, &len);
    clients[next_client].timer = time(NULL);
    server->fds[next_client].events = POLLIN;
    clients[next_client].client_headers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    clients[next_client].client_queries = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

}
/*************************************************/
/*  If clients time out then close connection    */
/*************************************************/
void check_for_timeouts(client_info* clients, server_info* server){
    time_t now = time(NULL);
    for(int i = 1; i < MAX_CLIENTS; i++){
        if (server->fds[i].fd < 0) continue;
        if(difftime(now, clients[i].timer) >= MAX_TIME){
            fprintf(stdout, "Client has timedout\n"); fflush(stdout);
            //closing the port and destroying all information about the client
            close(server->fds[i].fd);
            server->fds[i].fd = -1; 
            g_hash_table_destroy (clients[i].client_headers);
            g_hash_table_destroy (clients[i].client_queries);
        }
    }
}
/***************************************************************/
/*  Running the server loop and calling necessary functions    */
/***************************************************************/
void run_server(server_info* server, client_info* clients){

    setup_multiple_clients(server);
    fprintf(stdout, "Socket set up complete. Listening for request.\n"); fflush(stdout);
    while(23) {
        int p = poll(server->fds, MAX_CLIENTS, 10 * 1000);
        if(p < 0){
            error_handler("Poll failed");
        }
        if (p == 0) {
            fprintf(stdout, "No events for  seconds\n"); fflush(stdout);
        } else {
            for(int i = 0; i < MAX_CLIENTS; i++){
                if (server->fds[i].fd != -1 && (server->fds[i].revents & POLLIN)) {
                    if(i == 0){
                        fprintf(stdout, "New CLIENT!\n"); fflush(stdout);
                        add_new_client(server, clients);
                    } else{
                        fprintf(stdout,  "A CLIENT has sent a request!\n"); fflush(stdout);
                        memset(clients[i].color, 0, sizeof(clients[i].color));
                        ssize_t recv_msg_len = recv(server->fds[i].fd, server->buffer, sizeof(server->buffer) -1, 0);
                        if(recv_msg_len < 0){ error_handler("Receive failed");}
                        clients[i].timer = time(NULL);

                        if(recv_msg_len == 0){
                            //closing the port and destroying all information about the client
                            close(server->fds[i].fd);
                            server->fds[i].fd = -1;
                            g_hash_table_destroy (clients[i].client_headers);
                            g_hash_table_destroy (clients[i].client_queries);
                        } else{
                            server->buffer[recv_msg_len] = '\0';
                            client_handle(server, &clients[i], server->fds[i].fd);
                            if(clients[i].keep_alive != 1){
                                //closing the port and destroying all information about the client
                                close(server->fds[i].fd);
                                server->fds[i].fd = -1;
                                g_hash_table_destroy (clients[i].client_headers);
                                g_hash_table_destroy (clients[i].client_queries);
                            }
                        }
                    }
                }
            }
        }
        check_for_timeouts(clients, server);
    }    
}
/************************************************************************/
/*  Starting up the server, binding and listening to appropriate port   */
/************************************************************************/
void startup_server(server_info* server, const char* port){
    init_SSL();
    server->fds[0].fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&server->address, 0, sizeof(sockaddr_in));
    server->address.sin_family = AF_INET;
    server->address.sin_addr.s_addr = htonl(INADDR_ANY);
    server->address.sin_port = htons(atoi(port));
    if(bind(server->fds[0].fd, (sockaddr *) &server->address, (socklen_t) sizeof(sockaddr_in)) < 0){error_handler("bind failed");}
    if(listen(server->fds[0].fd, QUEUED) < 0){error_handler("listen failed");}
}
/******************************************************************************/
/*  Sets up SSL and check if key matches. All error results in termination    */
/******************************************************************************/
void init_SSL()
{
    // Internal SSL init functions
    SSL_library_init();
    SSL_load_error_strings();

    // Authentication
    if ((ctx = SSL_CTX_new(TLSv1_method())) == NULL) error_handler("SSL CTX");
    if (SSL_CTX_use_certificate_file(ctx, CERTIFICATE, SSL_FILETYPE_PEM) <= 0) error_handler("certificate");
    if (SSL_CTX_use_PrivateKey_file(ctx, PRIVATE_KEY, SSL_FILETYPE_PEM) <= 0) error_handler("privatekey");
    if (SSL_CTX_check_private_key(ctx) != 1) error_handler("match");

    // Message to user
    fprintf(stdout, "Access granted\n");
    fflush(stdout);
}