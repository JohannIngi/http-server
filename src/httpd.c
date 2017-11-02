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

#define GET 1
#define POST 2
#define HEAD 3
#define INVALID 4

#define VERSION_ONE 0
#define VERSION_TWO 1

#define MAX_TIME 10

typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;
typedef struct pollfd pollfd;

typedef struct 
{
    sockaddr_in address;
    int keep_alive;
    time_t timer;
    char time_buffer[64];
    char method[8];
    GHashTable* client_headers;
} client_info;

typedef struct
{
    sockaddr_in address;
    pollfd fds[MAX_CLIENTS];
    char buffer[4096];
    char fields[1024];
    char header_buffer[4096];
    
}server_info;



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
void parse(server_info* server, client_info* client);
void set_method(client_info* client);
void set_keep_alive(client_info* client);
void send_error(server_info* server, client_info* client, char* error_version, char* error_msg, size_t content_len);


int main(int argc, char **argv){
    if (argc != 2) error_handler("invalid arguments");
    char* welcome_port = argv[1];
    server_info server;
    client_info clients[MAX_CLIENTS];
    fprintf(stdout, "Listening to server number: %s\n", welcome_port); fflush(stdout);  
    startup_server(&server, welcome_port);
    run_server(&server, clients);
}

void error_handler(char* error_buffer){
    perror(error_buffer);
    exit(EXIT_FAILURE);
}

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

void write_to_log(client_info* client){

    get_time(client);
    //opening a file and logging a timestamp to it
    FILE* file;
    file = fopen("timestamp.log", "a");
    if (file == NULL) {
        fprintf(stdout, "Error in opening file!");
        fflush(stdout); 
    } else{
        //writing all necessary things to the timestamp
        fprintf(file, "%s: ", client->time_buffer);
        fprintf(file, "%s:%d ", inet_ntoa(client->address.sin_addr), client->address.sin_port);
        fprintf(file, "%s : ", client->method);
        fprintf(file, "%s:", "/");
        fprintf(file, " 200 OK \n");
    }
    fclose(file);
}
void client_handle(server_info* server, client_info* client, int connfd){

    //fprintf(stdout, "%s\n", server->buffer); fflush(stdout);
    parse(server, client);

    //TODO! setja upp fall sem sækir method úr hastable, nota g_hastable_lookupð
    set_method(client);
   // fprintf(stdout, "--------------------------------------\n");fflush(stdout);
   // fprintf(stdout, "-----THIS IS THE METHOD 8====> %s\n", client->method);fflush(stdout);
    //fprintf(stdout, "----------------( . )( . )------------\n");fflush(stdout);
    set_keep_alive(client);
    
    if(g_strcmp0(client->method, "GET") == 0){
        handle_get(server, client);
        create_header(server, client, strlen(server->buffer));
        write_to_log(client);
        send(connfd, server->header_buffer, strlen(server->header_buffer), 0);
    }
    else if(g_strcmp0(client->method, "POST") == 0){
        handle_post(server, client);
        create_header(server, client, strlen(server->buffer));
        write_to_log(client);
        send(connfd, server->header_buffer, strlen(server->header_buffer), 0);
    }
    else if(g_strcmp0(client->method, "HEAD") == 0){
        create_header(server, client, 282);
        send(connfd, server->header_buffer, strlen(server->header_buffer), 0);
    }
    else{
        char error_version[] = {"HTTP/1.1 501"};
        char error_msg[] = {"Invalid method!"};
        send_error(server, client, error_version, error_msg, sizeof(error_msg));
        send(connfd, server->header_buffer, strlen(server->header_buffer), 0);
    }
}

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

void set_keep_alive(client_info* client){
    char* tmp = (char*)g_hash_table_lookup(client->client_headers, "Connection");
    if(g_strcmp0(tmp, "keep-alive") == 0){
        client->keep_alive = 1; //there is asked for a keep alive
    }else{
        client->keep_alive = 0; // no need for keep alive
    }
}

void send_error(server_info* server, client_info* client, char* error_version, char* error_msg, size_t content_len){
    memset(server->header_buffer, 0, sizeof(server->header_buffer));
    strcat(server->header_buffer, error_version);
    strcat(server->header_buffer, "\r\nDate: ");
    get_time(client);
    strcat(server->header_buffer, client->time_buffer);
    strcat(server->header_buffer, "Content-Type: text/html\r\nContent-Length: ");
    char tmp[10];
    memset(tmp, 0, 10);
    sprintf(tmp, "%zu", content_len + 58);
    strcat(server->header_buffer, tmp);
    strcat(server->header_buffer, "\r\n\r\n");
    strcat(server->header_buffer, "<html><head><title>test</title></head><body>");
    strcat(server->header_buffer, error_msg);
    strcat(server->header_buffer, "</body></html>");
}

void create_header(server_info* server, client_info* client, size_t content_len){
    memset(server->header_buffer, 0, sizeof(server->header_buffer));

    char* tmp_version = (char*)g_hash_table_lookup(client->client_headers, "VERSION");
    if(g_strcmp0(tmp_version, "HTTP/1.1") == 0){
        strcat(server->header_buffer, "HTTP/1.1");
    }else{
        strcat(server->header_buffer, "HTTP/1.0");
    }

    strcat(server->header_buffer, " 200 OK\r\nDate: ");
    get_time(client);
    strcat(server->header_buffer, client->time_buffer);
    strcat(server->header_buffer, "\r\nContent-Type: text/html\r\nConnection: ");

    client->keep_alive == 1 ? strcat(server->header_buffer, "keep-alive") : strcat(server->header_buffer, "close");

    strcat(server->header_buffer, "\r\nContent-Length: ");
    char tmp[10];
    memset(tmp, 0, 10);
    sprintf(tmp, "%zu", content_len);
    strcat(server->header_buffer, tmp);
    fprintf(stdout, "Content length: %zu\n", content_len);fflush(stdout);
    if (content_len > 0) {
        strcat(server->header_buffer, "\r\n\r\n");
        strcat(server->header_buffer, server->buffer);
    }else{
        strcat(server->header_buffer, "\r\n\r\n");
    }
}

void handle_get(server_info* server, client_info* client){
    memset(server->buffer, 0, sizeof(server->buffer));
    strcat(server->buffer, "<html><head><title>test</title></head><body><center>Here is your ip address and port number: ");
    char *ip = inet_ntoa(client->address.sin_addr); // getting the clients ip address
    strcat(server->buffer, ip);
    strcat(server->buffer, ":");
    char tmp[6];
    memset(tmp, 0, 6);
    sprintf(tmp, "%d", client->address.sin_port); // getting the clients port
    strcat(server->buffer, tmp);
    strcat(server->buffer, "<br>");
    strcat(server->buffer, "<form method=\"post\">Enter something awesome into this field box<br><input type=\"text\" name=\"pfield\"><br><input type=\"submit\" value=\"Submit\"></form></center></body></html>");
}

void handle_post(server_info* server, client_info* client){
    memset(server->fields, 0, sizeof(server->fields));
    strcpy(server->fields, server->buffer);
    memset(server->buffer, 0, sizeof(server->buffer));
    strcat(server->buffer, "<html><head><title>test</title></head><body><center>Here is your ip address and port number: ");
    char *ip = inet_ntoa(client->address.sin_addr); // getting the clients ip address
    strcat(server->buffer, ip);
    strcat(server->buffer, ":");
    char tmp[6];
    memset(tmp, 0, 6);
    sprintf(tmp, "%d", client->address.sin_port); // getting the clients port
    strcat(server->buffer, tmp);
    strcat(server->buffer, "<br>");
    strcat(server->buffer, "<form method=\"post\">Enter something awesome into this field box<br><input type=\"text\" name=\"pfield\"><br><input type=\"submit\" value=\"Submit\"></form><br>");
    

    //fprintf(stdout, "THIS IS fields: %s\n", server->fields);fflush(stdout);
    char** sub_fields = g_strsplit(server->fields, "\r\n\r\n", -1);
    char** split_fields = g_strsplit(sub_fields[1], "=", -1);
    //fprintf(stdout, "THIS IS split_fields: %p\n", sub_fields[0]);fflush(stdout);

    //fprintf(stdout, "THIS IS split_fields: %p\n", sub_fields[1]);fflush(stdout);
    int start = 0;
    while (split_fields[start] != NULL){
        strcat(server->buffer, split_fields[start]);
        start++;
        if (split_fields[start] != NULL) {
            strcat(server->buffer, " ----> ");
            strcat(server->buffer, split_fields[start]);
            start++;
        }
        strcat(server->buffer, "<br>");
    }

    strcat(server->buffer, "</center></body></html>");


    g_strfreev(sub_fields);
    g_strfreev(split_fields);
}

void for_each_func(gpointer key, gpointer val, gpointer data)
{
    printf("%s -> %s\n", ((char*)key), ((char*)val));
}

void parse(server_info* server, client_info* client){


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

    g_hash_table_foreach(client->client_headers, for_each_func, NULL);

    g_strfreev(tmp);
    g_strfreev(first_line);

    
}

void setup_multiple_clients(server_info* server){
    for(int i = 1; i < MAX_CLIENTS; i++){
        server->fds[i].fd = -1;
    }
    server->fds[0].events = POLLIN;
}

void add_new_client(server_info* server, client_info* clients) {

    socklen_t len = (socklen_t) sizeof(sockaddr_in);
    int next_client = find_next_available_client(server->fds); //iterates through all the fd's until he returns a client
    if(next_client < 0){ error_handler("Accept failed");}
    memset(&clients[next_client].address, 0, sizeof(struct sockaddr_in));
    server->fds[next_client].fd = accept(server->fds[0].fd, (sockaddr*)&clients[next_client].address, &len);
    clients[next_client].timer = time(NULL);
    server->fds[next_client].events = POLLIN;
    clients[next_client].client_headers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

}

void check_for_timeouts(client_info* clients, server_info* server){
    time_t now = time(NULL);
    for(int i = 1; i < MAX_CLIENTS; i++){
    	if (server->fds[i].fd < 0) continue;
        if(difftime(now, clients[i].timer) >= MAX_TIME){
            fprintf(stdout, "Client has timedout\n"); fflush(stdout);

            close(server->fds[i].fd);
            server->fds[i].fd = -1; //TODO sýna jonna timeout pælingar
        }
    }
}

void run_server(server_info* server, client_info* clients){

    setup_multiple_clients(server);
    fprintf(stdout, "Socket set up complete. Listening for request.\n"); fflush(stdout);
    while(23) {
        int p = poll(server->fds, MAX_CLIENTS, 10 * 1000);
        if(p < 0){
            error_handler("Poll failed");
        }
        if (p == 0) {
			fprintf(stdout, "No events for 10 seconds\n"); fflush(stdout);
        } else {
            for(int i = 0; i < MAX_CLIENTS; i++){
                if (server->fds[i].fd != -1 && (server->fds[i].revents & POLLIN)) {
                    if(i == 0){
                    	fprintf(stdout, "New CLIENT!\n"); fflush(stdout);
                        add_new_client(server, clients);
                    } else{
						fprintf(stdout,  "Some CLIENT with a request!\n"); fflush(stdout);

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

void startup_server(server_info* server, const char* port){
    server->fds[0].fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&server->address, 0, sizeof(sockaddr_in));
    server->address.sin_family = AF_INET;
    server->address.sin_addr.s_addr = htonl(INADDR_ANY);
    server->address.sin_port = htons(atoi(port));
    if(bind(server->fds[0].fd, (sockaddr *) &server->address, (socklen_t) sizeof(sockaddr_in)) < 0){error_handler("bind failed");}
    if(listen(server->fds[0].fd, QUEUED) < 0){error_handler("listen failed");}
}