
/*
 * Programming Assignment 3 – HTTP server and SSL
 *
 * Team:
 * Ármann Pétur Ævarsson [armanna16@ru.is]
 * Jóhann Ingi Bjarnason [johannb16@ru.is]
 *
*/

***

Answer to question 9. (5 points (bonus)) Fairness in PA2
One definition of fairness is that all clients that send a request to the server will eventually
receive a reply. Ensure that this is the case for your server and carefully explain why your
server ensures this property.
- Our server ensures fairness because it iterates through all clients that are currently connected. Every client gets to finish all his actions in every iteration. 
If on the other hand the server would e.g. choose a client randomly or always choose the first client that connects, it would not be ensuring fairness.

***

Answer to question 9.3 (5 points (bonus)) in PA3
Why is HTTP basic authentication not secure? Explain why it is necessary
to allow secure authentication only over SSL.
- When sending e.g. username and password over the wire using HTTP basic authentication it will be sent as a plain-text for the whole world to see. It is necessary to use secure authentication because then the entire message you send is encrypted based on keys and SSL certificate. HTTPS also provides ways for authentication and not only encryption.

***


**PA2:
Our http server answers requests from one or more clients, up to 10 clients at a time. Answering their requests with appropriate messages according to the project describtion that we received from our teacher. The server writes a timestamp for each request from clients in a log file that is generated automatically. If a client is inactive on the server for 30 seconds he is timed out and his connection closed. The server responds to requests with various messages, ranging from pure http headers to simple html pages.
Our server was tested using the browsers Microsoft Edge, Google Chrome and Brave.



**PA3:
Our server was upgraded quite a bit when we implemented PA3. Now the server can start up two ports to listen to. We added all the subpages that was asked for, i.e. '/color','/test'. We also made '/ ' our home page and added '/index' just for fun. If our client sends us a cookie about a certain color that he/she whises to have on the '/color' page our server will display the wanted color. We have also implemented key management on the server. This was done by following the directions in the OpenSSL cookbook at https://www.feistyduck.com/books/openssl-cookbook/ adding the keys and the necessary certificates.

The passphrase to run the server is 'russelcrow'

Unfortunatly we could not complete Questions 8 and 9 regarding the openSSL initialization and the Authentication, but we did start on them. Although we had a pretty decent idea how to complete them. We would do something along the lines of beginning with starting the second port up on a server as we did with the first one and when adding new clients into the server we would check which port they used. If it was the secure port we would initialize them with special SSL recv() and send() if it was the original port we would deal with them normally.


**Descriptions of the functions implemented in this project**
==============================================================================================
void error_handler(char* error_buffer) -> Write out the error message that are sent to this function

int find_next_available_client(pollfd* fds) -> Finding the next available client

void get_time(client_info* client) -> Setting the current date in a time buffer

void write_to_log(client_info* client, server_info* server) -> Writing to a log file in the root directory

void client_handle(server_info* server, client_info* client, int connfd) -> client handler that decides what to do based on the client request; get, post, head or error

void check_content(server_info* server) -> checking if there is some html content inside the server buffer to be displayed

void set_method(client_info* client) -> Looks for the 'method' key in the headers-hastable and sets the current method if it finds one

void set_color(client_info* client) -> Checks if there has been a color determined with a query or a cookie, by looking in both client hashtables

void set_keep_alive(client_info* client); -> initializes the keep-alive parameter from the client

void generate_error(server_info* server, client_info* client, char* error_version, char* error_msg, size_t content_len) -> Creates the html needed to send an error

void send_error() -> Sends the error to the client

void create_header(server_info* server, client_info* client, size_t content_len) -> Creating a html header to use in all the methods

void handle_get(server_info* server, client_info* client, int connfd) -> Creating a html body for GET requests 

void handle_post(server_info* server, client_info* client, int connfd, int index) -> Creating a html body for POST requests 

void add_queries(gpointer key, gpointer val, gpointer data) -> A function that is used with g_hash_table_foreach to print out all the variables in the query hashtable

void parse(server_info* server, client_info* client, int connfd) -> Parsing function that parses everything from the header into appropriate hashtable

void parse_URL(server_info* server, client_info* client, int connfd, char** first_line) -> Parsing function that parses everything from the URL into appropriate hashtable

void parse_query(client_info* client, char** query) -> Parsing function that parses everything into appropriate hashtable  

void setup_multiple_clients(server_info* server) -> Setting up multiple clients connections

void add_new_client(server_info* server, client_info* clients) -> Adding new clients to the server

void check_for_timeouts(client_info* clients, server_info* server) -> If clients time out then close connection

void run_server(server_info* server, client_info* clients) -> Running the server loop and calling necessary functions

void startup_server(server_info* server, const char* port) -> Starting up the server, binding and listening to appropriate port

void init_SSL() -> Sets up SSL and check if key matches. All error results in termination 
