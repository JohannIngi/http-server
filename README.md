
/*
 * Programming Assignment 2 – HTTP server
 *
 * Team:
 * Ármann Pétur Ævarsson [armanna16@ru.is]
 * Jóhann Ingi Bjarnason [johannb16@ru.is]
 *
*/

Out http server answers requests from one or more clients, up to 10 clients at a time. Answearing their requests with appropriate messages according to the project describtion that we received from our teacher. The server writes a timestamp for each request from clients in a log file that is generated automatically. If a client is inactive on the server for 30 seconds he is timed out and his connection closed. The server responds to requests with various messages, ranging from pure http headers to simple html pages.
Our server was tested using the browsers Microsoft Edge and Brave.

Descriptions of the functions implemented in this project
==============================================================================================
void error_handler(char* error_buffer) -> Write out the error message that are sent to this function

int find_next_available_client(pollfd* fds) -> Finding the next available client

void get_time(client_info* client) -> Setting the current date in a time buffer

void create_get_body(server_info* server, client_info* client) -> Creating a html body for the GET method

void create_header(server_info* server, client_info* client, size_t content_len) -> Creating a html header to use in all the methods

void handle_get(server_info* server, client_info* client, int connfd) -> Handling GET requests 

void create_post_body(server_info* server, client_info* client, char* fields) -> Creating a html body for the POST method 

void handle_post(server_info* server, client_info* client, int connfd, int index) -> Handling POST requests 

void handle_head(server_info* server, client_info* client, int connfd) -> Handling HEAD requests

void send_invalid(server_info* server, client_info* client, int connfd) -> Sending a 404 error if invalid requests 

int get_version(char* buffer, int* index) -> Acquiring the version of the http protocol

int is_home(char* buffer, int* index) -> Returns true if url is legal 

int get_method(char* buffer, int* index) -> Acquiring the appropriate method

void write_to_log(client_info* client, server_info* server) -> Writing to a log file in the root directory

void client_handle(server_info* server, client_info* client, int connfd) -> client handler that decides what to do based on the client request; get, post or head

void setup_multiple_clients(server_info* server) -> Setting up multiple clients connections

void add_new_client(server_info* server, client_info* clients) -> Adding new clients to the server

void check_for_timeouts(client_info* clients, server_info* server) -> If clients time out then close connection

void run_server(server_info* server, client_info* clients) -> Running the server loop and calling necessary functions

void startup_server(server_info* server, const char* port) -> Starting up the server, binding and listening to appropriate port