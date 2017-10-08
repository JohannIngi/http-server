
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

// TODO: fake_job is just to check if some function need handling to remove warnings - remove at end product
void fake_job() {
    printf("_");
}

void debug(const char* msg) {
    fprintf(stdout, msg); fflush(stdout);
    putchar('\n');
}

typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;
typedef struct pollfd pollfd;

typedef struct 
{
    sockaddr_in address;
    socklen_t len;
    char time_buffer[20];
    char header_buffer[1024];
    char url_buffer[1024];
    char html[4096];
    char* ip_address;
    unsigned short port;
} client_info;

typedef struct
{
    int sockfd;
    int timeout;
    sockaddr_in address;
    //making an array of pollfd's to handle multiple clients
    pollfd fds[MAX_CLIENTS];
    char buffer[4096];
    char url_for_all[1024];
    char data_buffer[2048];
    
}server_info;

void startup_server(server_info* server, const char* port);
void setup_multiple_clients(server_info* server);
void run_server(server_info* server, client_info* clients);
int find_next_available_client(pollfd* fds);
int accept_request(client_info* client, server_info* server);
void get_client_information(client_info* client);
void get_header_type(server_info* server, client_info* client);
void get_time(client_info* client);
void generate_html_header(client_info* client, int response_code);
void generate_html_get(client_info* client);
void generate_html_post(client_info* client, server_info* server);
void generate_404_error(client_info* client);
void write_to_log(client_info* client);
void get_url(server_info* server);
void get_post_data(server_info* server);
void client_handle(server_info* server, client_info* client, int connfd);
void error_handler(char* error_buffer);


/*********************************************************************************************************/
/*  main function starts here and declares instances of the structs that are used thougout the server    */
/*********************************************************************************************************/
int main(int argc, char **argv)
{
    if (argc != 2) error_handler("invalid arguments");
    char* welcome_port = argv[1];
    server_info server;
    client_info clients[MAX_CLIENTS];
    fprintf(stdout, "Listening to server number: %s\n", welcome_port); fflush(stdout);  
    startup_server(&server, welcome_port);
    run_server(&server, clients);
}

/*************************************************************************/
/*  Starting up the server, binding and listening to appropriate port    */
/*************************************************************************/
void startup_server(server_info* server, const char* port){
    // Create and bind a TCP socket.
    server->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    //If sockfd is some bullshit throw error!
    memset(&server->address, 0, sizeof(sockaddr_in));
    server->address.sin_family = AF_INET;
    //htonl and htons convert the bytes to be used by the network functions
    server->address.sin_addr.s_addr = htonl(INADDR_ANY);
    server->address.sin_port = htons(atoi(port));
    //bind to port
    //if bind returns less then 0 then an error has occured
    if(bind(server->sockfd, (sockaddr *) &server->address, (socklen_t) sizeof(sockaddr_in)) < 0){error_handler("bind failed");}
    //Server is listening to the port in order to accept messages. A backlog of five connections is allowed
    //if listen returns less then 0 then an error has occured
    if(listen(server->sockfd, QUEUED) < 0){error_handler("listen failed");}
}
/*************************************************************************/
/*  Setting up multiple clients connections                              */
/*************************************************************************/
void setup_multiple_clients(server_info* server){

    memset(&server->fds, 0, sizeof(pollfd));
    for(int i = 0; i < MAX_CLIENTS; i++){
        server->fds[i].fd = -1; //initializing all the objects in the array to -1, we only handle cases where the object is 1
        server->fds[i].events = POLLIN;
    }
    server->fds[0].fd = server->sockfd; //giving the first object in the array the value of the file descriptor in poll()

    server->timeout = 3 * 1000; //time duration for timeout set to 3 sec   
}
/*************************************************************************/
/*  Running the server loop and calling necessary functions              */
/*************************************************************************/
void run_server(server_info* server, client_info* clients){

    setup_multiple_clients(server);
    fprintf(stdout, "Socket set up complete. Listening for request.\n"); fflush(stdout);
    
    while(23) {
        // saving the outcome in a variable
        int rc = poll(server->fds, MAX_CLIENTS, server->timeout);//int poll(struct pollfd *fds, nfds_t nfds, int timeout);
        if(rc < 0){
            error_handler("something went wrong!");
        }
        else if(rc == 0){
            debug("Nothing has happened for 3 sec");
        } else{
            debug("!!!!!!something is in my client!!!!!!");

            if(server->fds[0].revents & POLLIN){ //our new client is here
                debug("!!!NEW client baby!!!");
                //TODO error handle
                int next_client = find_next_available_client(server->fds); //iterates through all the fd's until he returns a clien
                fprintf(stdout, "NEXT CLIENT %d\n", next_client); fflush(stdout);
                //server->fds[next_client].events = POLLIN;

                //setting the buffer to zero
                memset(&clients[next_client].address, 0, sizeof(sockaddr_in));
                clients[next_client].len = (socklen_t) sizeof(sockaddr_in);

                int connfd = accept(server->fds[0].fd, (sockaddr *) &clients[next_client].address, &clients[next_client].len);

                //if -1 then the client is inactive
                if (connfd < 0){error_handler("accepting message failed");}
                server->fds[next_client].fd = connfd; // give the client the fd value that

                //server->fds[0].events = 0; //ekki viss með þetta
            }

            for(int i = 1; i < MAX_CLIENTS; i++){

                if(server->fds[i].fd != -1 && server->fds[i].revents & POLLIN){
                    debug("!!!same OLD client!!!");
                    //setting the buffer to zero
                    memset(server->buffer, 0, sizeof(server->buffer));
                    // Receive from clients fd connfd, not sockfd.
                    ssize_t recv_msg_len = recv(server->fds[i].fd, server->buffer, sizeof(server->buffer)-1, 0);
                    fprintf(stdout, "receive msg len: %zu\n", recv_msg_len);
                    server->buffer[recv_msg_len] = '\0';
                    fprintf(stdout, "Received message from client in array pos:  %d:\n\n--------------------\n%s\n", i, server->buffer);fflush(stdout);


                    //sending in information about the specific client to be handled
                    client_handle(server, &clients[i], server->fds[i].fd);
                    write_to_log(&clients[i]);

                    server->fds[i].events = POLLIN;

                    close(server->fds[i].fd);
                    server->fds[i].fd = -1;
                }
            }
        }

        /*

        // Close the connection.
        shutdown(connfd, SHUT_RDWR);
        close(connfd);
        */


        // TODO: check for timed out clients...
    }    
}

/****************************************************/
/*  Finding the next available client               */
/****************************************************/
int find_next_available_client(pollfd* fds){

    int i;
    for(i = 1; i < MAX_CLIENTS; i++){
        if(fds[i].fd == -1){
            return i; //skila staðsetningunni sem er með laust pláss
        }
    }
    return -1;
}
/*************************************/
/*  Accepting a request from client  */
/*************************************/
int accept_request(client_info* client, server_info* server){
    //setting the buffer to zero
    memset(&client->address, 0, sizeof(sockaddr_in));
    client->len = (socklen_t) sizeof(sockaddr_in);
    int connfd = accept(server->sockfd, (sockaddr *) &client->address, &client->len);
    //if -1 then the client is inactive
    if (connfd < 0){error_handler("accepting message failed");}
    return connfd;
}
/*************************************/
/*  Receiving message from someone   */
/*************************************/
void get_client_information(client_info* client){
    ////acquiring the ip address from the client in the form of a string
    client->ip_address = inet_ntoa(client->address.sin_addr);
    //storing information about the port from client
    client->port = client->address.sin_port;
}

//*******************************************************************************/
//  Getting the header type from the clients message. Is it a get, post or head */
//*******************************************************************************/
void get_header_type(server_info* server, client_info* client){
    //setting the buffer to zero
    memset(client->header_buffer, 0, sizeof(client->header_buffer));
    //make a pointer to a pointer or in other words a string array and storing the first word from the message buffer
    char** header_type = g_strsplit(server->buffer, " ", -1);
    //adding the header type to the appropriate buffer
    strcat(client->header_buffer, *header_type);
    //releasing the memory that g_strsplit was using when splittin the string
    g_strfreev(header_type);
}
/*************************************************/
/*  Setting the current date in a time buffer    */
/*************************************************/
void get_time(client_info* client){
    //setting ISO time
    time_t t;
    time(&t);
    struct tm* t_info;
    //setting the buffer to zero
    memset(client->time_buffer, 0, sizeof(client->time_buffer));
    t_info = localtime(&t);
    strftime(client->time_buffer, sizeof(client->time_buffer), "%a, %d %b %Y %X GMT", t_info);  
}

/*************************************/
/*  Hard-coded html for the client   */
/*************************************/
void generate_html_header(client_info* client, int response_code){

    // TODO: check if u can ask for head on a specific sub-url and if so - 404 on invalids

    char tmp_response_code[4];
    //memsetting the response code buffer to zero
    memset(tmp_response_code, 0, sizeof(tmp_response_code));
    //converting the response_code from int to string
    sprintf(tmp_response_code, "%d", response_code);

    //memsetting the html buffer to zero
    memset(client->html, 0, sizeof(client->html));
    //appending all the html code that is needed to generate a page
    strcat(client->html, "HTTP/1.1 ");
    strcat(client->html, tmp_response_code);
    if(strcmp(tmp_response_code, "404")){
        strcat(client->html, "\r\nDate: ");
    }else{
        strcat(client->html, "OK\r\nDate: ");
    }

    get_time(client);
    strcat(client->html, client->time_buffer);
    strcat(client->html, "Content-Type: text/html\r\nserver: Awesome_Server 1.0\r\ncharset=UTF-8\r\n\r\n");
    
}
/*****************************************/
/*  Hard-coded html for the get request  */
/*****************************************/
void generate_html_get(client_info* client){

    generate_html_header(client, 200);
    //appending all the html code that is needed to generate a page
    strcat(client->html, "<!DOCTYPE html>\r\n<html><head><title>HTTPServer</title></head>\r\n");
    strcat(client->html, "<body>http://foo.com/");
    //adding the url to the buffer
    strcat(client->html, client->url_buffer);
    strcat(client->html, " ");
    //adding the ip address to the budder
    strcat(client->html, client->ip_address);
    strcat(client->html, ":");
    //a buffer for the port number
    char port_fix[16];
    //manipulating the port number to be useable as a char pointer
    sprintf(port_fix, "%d", client->port);
    //adding the port number to the buffer
    strcat(client->html, port_fix);
    //adding a form to receive a post request
    strcat(client->html, "<form method=\"post\">\r\n<label for=\"name1\">Name1:</label>\r\n<input type=\"name\" name=\"name\">\r\n<label for=\"name2\">Name2:</label>\r\n<input type=\"name\" name=\"name\">\r\n<button>Submit</button>\r\n</form>");
    //closing the html page
    strcat(client->html, "\r\n</body></html>\r\n");
}

/*********************************************/
/*  Hard-coded html for the post request     */
/*********************************************/
void generate_html_post(client_info* client, server_info* server){

    generate_html_header(client, 200);
    //appending all the html code that is needed to generate a page
    strcat(client->html, "<!DOCTYPE html>\r\n<html><head><title>HTTPServer</title></head>\r\n");
    strcat(client->html, "<body>http://foo.com/");
    //adding the url to the buffer
    strcat(client->html, client->url_buffer);
    strcat(client->html, " ");
    //adding the ip address to the budder
    strcat(client->html, client->ip_address);
    strcat(client->html, ":");
    //a buffer for the port number
    char port_fix[16];
    //manipulating the port number to be useable as a char pointer
    snprintf(port_fix, 16, "%d", client->port); // TODO: does snpritnf null terminate if smaller than n?
    //adding the port number to the buffer
    strcat(client->html, port_fix);
    //adding a form to receive a post request
    strcat(client->html, "<form method=\"post\">\r\n<label for=\"name1\">Name1:</label>\r\n<input type=\"name\" name=\"name\">\r\n<label for=\"name2\">Name2:</label>\r\n<input type=\"name\" name=\"name\">\r\n<button>Submit</button>\r\n</form>");
    strcat(client->html, "<center><table><tr><th>Data</th><th>something</th></tr><tr><td>");
    get_post_data(server);

    strcat(client->html, server->data_buffer);
    strcat(client->html, "</td><td>something2</td></tr></table></center>");
    
    
    //closing the html page
    strcat(client->html, "\r\n</body></html>\r\n");
}
/*************************************************/
/*  Writing to a log file in the root directory  */
/*************************************************/
void generate_404_error(client_info* client){

    generate_html_header(client, 404);
    strcat(client->html, "<!DOCTYPE html>\r\n<html><head><title>HTTPServer</title></head>\r\n");
    strcat(client->html, "<body>ERROR 404");
    //closing the html page
    strcat(client->html, "\r\n</body></html>\r\n");

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
        fprintf(file, "%s:%hu ", client->ip_address, client->port);
        fprintf(file, "%s : ", client->header_buffer);
        fprintf(file, "%s:", client->url_buffer);
        fprintf(file, " 200 OK \n");
    }
    fclose(file);
}
/*************************************************/
/*  Acquiring the url from a specific client     */
/*************************************************/
void get_url(server_info* server){
    //setting the buffer to zero
    memset(server->url_for_all, 0, sizeof(server->url_for_all));
    //splitting the client buffer where HTTP begins
    char** url_keeper = g_strsplit(server->buffer, "HTTP", -1);
    //adding the content to the appropriate buffer
    strcat(server->url_for_all, url_keeper[0]);
    //releasing the memory that g_strsplit was using when splittin the string
    g_strfreev(url_keeper);
}
/*************************************************/
/*  Acquiring the data from the post response    */
/*************************************************/
void get_post_data(server_info* server){
    //setting the buffer to zero
    memset(server->data_buffer, 0, sizeof(server->data_buffer));
    //splitting the client buffer where HTTP begins
    char** data = g_strsplit(server->buffer, "\r\n\r\n", -1);
    //adding the content to the appropriate buffer
    strcat(server->data_buffer, data[1]);
    //releasing the memory that g_strsplit was using when splittin the string
    g_strfreev(data);
}
/*********************************************************************************************/
/*  client handler that decides what to do based on the client request; get, post or head    */
/*********************************************************************************************/
void client_handle(server_info* server, client_info* client, int connfd){

    get_client_information(client);
    //fprintf(stdout, "From ip address: %s. Port number: %hu\n", client.ip_address, client.port); fflush(stdout);

    get_header_type(server, client);
    get_url(server);

    fprintf(stdout, "HEADER TYPE: %s\n", client->header_buffer);fflush(stdout);

    //checking if the header_buffer matches a GET request
    if(g_str_match_string("GET", client->header_buffer, 1)){
        //copying the server url buffer from array index 5 into the client url buffer
        strcpy(client->url_buffer, &server->url_for_all[5]);
        //if the buffer for the url contains more than a whitespace it should send 404 error
        if(server->url_for_all[5] == ' '){
            generate_html_get(client);
            //sending the page
            send(connfd, client->html, strlen(client->html), 0);
        }
        else if(strcmp(client->url_buffer, "favicon.ico")){
            debug("RECIEVED A FAV ICON - SENDING THAT FAGGOT SOME ERROR - COCK");
            generate_404_error(client);
            send(connfd, client->html, strlen(client->html), 0);
        }
        //sending 404 error
        else{
            generate_404_error(client);
            send(connfd, client->html, strlen(client->html), 0);
        }
    }
    
    //checking if the header_buffer matches a POST request
    else if(g_str_match_string("POST", client->header_buffer, 1)){
        //copying the server url buffer from array index 6 into the client url buffer
        strcpy(client->url_buffer, &server->url_for_all[6]);
        generate_html_post(client, server);
        //fprintf(stdout, "DATA FOR POST: %s\n", server->data_buffer);fflush(stdout);
        send(connfd, client->html, strlen(client->html), 0);
 
    }
    //checking if the header_buffer matches a HEAD request
    else if(g_str_match_string("HEAD", client->header_buffer, 1)){
        //copying the server url buffer from array index 6 into the client url buffer
        //strcpy(client->url_buffer, &server->url_for_all[6]);
        generate_html_header(client, 200);
        //closing the html page
        strcat(client->html, "\r\n</html>\r\n");
        send(connfd, client->html, strlen(client->html), 0);

    }
    else{error_handler("No match to header-type!");}
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
