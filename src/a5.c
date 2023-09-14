//Developed by Kiryl Baravikou, UIC aka Wondamonstaa
//Credit: Luis Pina, UIC

#include "a5-pthread.h"
#include "a5.h"
#include<stdbool.h>

static const char ping_request[] = "GET /ping HTTP/1.1\r\n\r\n";
static const char write_request[] = "POST /write HTTP/1.1\r\n";
static const char read_request[] = "GET /read HTTP/1.1\r\n";
static const char file_request[] = "GET /%s HTTP/1.1\r\n";
static const char stats_request[] = "GET /stats HTTP/1.1\r\n";
static const char echo_request[] = "GET /echo HTTP/1.1\r\n";
//New
//static const char echo_request[] = "GET /echo HTTP/1.1\r\n\r\n";
static const char stats_response_body[] = "Requests: %d\nHeader bytes %d\nBody bytes %d\nErrors: %d\n Error bytes: %d";

static const char ok200_response[] = "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n";
static const char err404_response[] = "HTTP/1.1 404 Not Found";
static const char err400_response[] = "HTTP/1.1 400 Bad Request";
static const char content_len_header[] = "Content-Length: %d";
const char error404[1000] = "HTTP/1.1 404 Not Found";
const char bad_request[1000] = "HTTP/1.1 400 Bad Request";


//Test 5
static char written[1024] = "<empty>";
static int written_size = 7;

//Test 4
static const char ping_header[] = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\n";
static const char ping_body[] = "pong";

static char request[2048];
static char head[1024];
static int head_size = 0;
static char body[1024];
static int body_size = 0;

static int reqs = 0;
static int head_bytes = 0;
static int body_bytes = 0;
static int errs = 0;
static int err_bytes = 0;

//Mutex initialization
static pthread_mutex_t echo_mutex = PTHREAD_MUTEX_INITIALIZER;

//Function definitions for further use
static void handle_client_request(int sockfd);
//static void send_response(int sockfd);
static void send_response(int sockfd, char head[], int head_size, char body[], int body_size);


//The struct used to store the statistics for the test case 8
struct statistics{

    int requests;
    size_t header_bytes;
    size_t body_bytes;
    int errors;
    size_t error_bytes;

};

//The sctruct used to store the variables I use across the program check
struct storage{

    //Buffers below used to store specific for them strings
    char buffer[5000]; 
    char ping[3000]; //Used to store the ping
    char pong[3000]; //Used to store the pong
    char container[3000]; //Store the string which is constant and must be present in the output
    char* start; //The head of the string
    char* end; //The tail of the string
    size_t length; //Difference between end and start
    socklen_t address_length;
    char* result;
    char content[3000]; //Used for the test case 4 to store the output of the buffer
    long data;
    int data_sent; //Used to count how much data was actually sent to the server
    size_t chunk_size;
    char metadata[3000];
    char* tail;
    char display_string[3000];
};

//Helper function that allows to check if the current string starts with the part we need, i.e. prefix
bool start(char* target, char* prefix) {

    //True or false
    return strncmp(target, prefix, strlen(target)) == 0;
}


//Helper function that allows to extract the target string from the REQUEST
char* string_builder(char target[]){
    
    //The buffer to store the REQUEST string
    char REQUEST[3000];

    //Copy the target string inside the buffer, size 3000
    strncpy(REQUEST, target, 3000);

    //This is the start of the string => we need everything from "tests" or ./tests
    //char *start = strstr(REQUEST, "tests");
    char *start = strstr(REQUEST, ".");

    //This is the end of the string
    char *end = strstr(start, ".html");
    
    //Sanity check => returns NULL if the targets were not found
    //return (start == NULL || end == NULL) ? (printf("Substring not found\n"), NULL) : result;

    char result[7000] = "./";

    // Find the first occurrence of '/' character in Object.buffer
    char* str = strchr(target, '/');

    if(str != NULL) {

        //Allows to copy the substring after the first occurrence of '/' into target variable. Reference: https://en.cppreference.com/w/c/experimental/dynamic/strdup
        char* target = strdup(str + 1);

        //Find the next occurrence of ' ' character in the target substring
        char* space = strchr(target, ' ');

        //Sanity check if the occurence was not found
        (space != NULL) ? (*space = '\0') : 0;

        // Append the target substring to the result string
        strcat(result, target);

        //Need to free the memory that I have just allocated (TA)
        free(target);
    }

    //To get the .html, we need to add 5 bytes
    int len = end - start + 5; 

    //Result string = length + 1 to handle the terminating symbol
    //char *result = malloc(len + 1); 
    //strncpy(result, start, len);

    //The null terminator handling case
    //result[len] = '\0'; 

    //printf("%s", result);

    //Sanity check => returns NULL if the targets were not found
    //return (start == NULL || end == NULL) ? (printf("Substring not found\n"), NULL) : result;
    return strdup(result);
}


//Helper function used to send the responce to the server after all the previous conditions were met
void respond(int new_clientfd, char* response, char* data, char* Object_start, size_t size){

    //If the condition inside () is true, then response[strlen(response)-1] = '\0' will be executed. Otherwise => false (TA)
    (response[strlen(response)-1] == '\n') ? response[strlen(response)-1] = '\0' : false;

    //Set all bytes in the arrays to 0 => need to use it to refresh the array (TA)
    memset(data, 0, size);
    memset(Object_start, 0, strlen(Object_start));

    //Send the request to the server
    send(new_clientfd, response, strlen(response), 0);
    close(new_clientfd);
}





//Helper function which allows to open the provided FILE* filename, obtain its length and then read the file again using rewind()
void reader(char* filename, FILE** file, long* length, int new_clientfd) {
    
    //Open the file using the buffer and read it afterwards
    *file = fopen(filename, "r");
    
    //Sanity check
    if (!file) {
        printf("The file was not opened. Check the reader() function!\n");
        return;
    }

    if(file == NULL){
        send(new_clientfd, error404, strlen(error404), 0);
        close(new_clientfd);
        return;
    }

    if(*file != NULL){

        //Allows to get the length of the file => probably, the simplest way to do this. Reference: https://en.cppreference.com/w/c/io/fseek
        fseek(*file, 0, SEEK_END);

        //The actual length will be updated since I passed the address of the variable to the function call (TA)
        *length = ftell(*file);

        //Allows to read the same file again => jumps back to the beginning of the same file. Reference: https://cplusplus.com/reference/cstdio/rewind/
        rewind(*file);
    }
    else{
        printf("THE FILE IS INVALID. Check the reader() function!\n");
        return;
    }
    

    return;
}



void check_buffer(int new_clientfd, char *buffer) {

    //Check if the fist chars of the buffer are the target values
    //if ((strncmp(buffer, "GET/", 5) == 0) || (strncmp(buffer, "GETGET", 6) == 0)) {
      
        send(new_clientfd, bad_request, strlen(bad_request), 0);
        close(new_clientfd);
    //}
}


void handle_write(int sockfd){

    //Create an object using the struct
    struct storage Object;

    struct statistics st;

    char* file_comp = "GET /";
    char* read_comp = "GET /read";
    int file_length = strlen(file_comp);

    if (strncmp(Object.buffer, file_comp, file_length) == 0) {

            //DELETE
            printf("THIS IS THE FIRST IF FROM THE BUFFER OBJECT: %s\n", Object.buffer);

            //printf("THIS IS THE LINE 651, STRNCMP READING");


            //char* file_comp = "GET /";
            //int file_length = strlen(file_comp);
            
            char start[2050] = "HTTP/1.1 200 OK\r\nContent-Length: ";
            char file[3000];
            char buffer[7000];
            long length;
            long rec_content;
            rec_content = 0;
            char empty[100] = "";
            Object.data = 0;
            char result[7000];

            //Copy the result of the string_builder() call into the result array => this is our filename => WORKS
            strcpy(result, string_builder(Object.buffer));

            //Store the name of the file inside the filename
            int fd = open(result, O_RDONLY);

            //Sanity check to see if the file was opened successfully
            if (fd == -1) {
                send(sockfd, error404, strlen(error404), 0);
                close(sockfd);
                //exit(0);
            }
            else{

            

            //Allows to get the length of the file
            struct stat st;
            if (fstat(fd, &st) == -1) {
                close(fd);
                send(sockfd, error404, strlen(error404), 0);
                close(sockfd);
                exit(0);
                //return 0;
            }
        

                length = st.st_size;

                //Send the length to display
                sprintf(empty, "%ld", length);
                strcat(start, empty);
                strcat(start, "\r\n\r\n");
                send(sockfd, start, strlen(start), 0);

                //Send the data in chunks => read the data in chunks with the size of 100 bytes, type char, from the filename and store it inside the buffer
                do{
                    rec_content = read(fd, buffer, 100);

                    //Sanity check
                    if (rec_content == 0) {
                        break;
                    }

                    //The current data sent is 0, so need to use it to start sending the chunks 
                    Object.data_sent = 0;

                    //Start sending the data in chunks
                    while (Object.data_sent < rec_content) {
                        
                        int counter = send(sockfd, buffer + Object.data_sent, rec_content - Object.data_sent, 0);

                        //Sanity check
                        if (counter == -1) {
                            //printf("The file could not be read!\n");
                            send(sockfd, error404, sizeof(error404), 0);
                            close(fd);
                            close(sockfd);
                            //exit(0);
                            //return 0;
                        }

                    //Update the current data sent to the server
                    Object.data_sent += counter;
                    }

            }while (rec_content != 0);

            //Close the file
            close(fd);

            //Close the connection
            close(sockfd);
        }
    }
}


//Professor's handle_ping(int sockfd) function
static void handle_ping(int sockfd) {
    
    //Test 4
    /*static char head[1024];
    static int head_size = 0;
    static char body[1024];
    static int body_size = 0;*/

    head_size = strlen(ping_header);
    memcpy(head, ping_header, head_size);

    body_size = strlen(ping_body);
    memcpy(body, ping_body, body_size);

    //send_response(sockfd);
    send_response(sockfd, head, head_size, body, body_size);
}


//Professor's handle_echo() function
static void handle_echo(int sockfd, char request[]){

    //static char request[2048];
    static char head[1024];
    static int head_size = 0;

    static char body[1024];
    static int body_size = 0;

    char* end = strstr(request, "\r\n\r\n");
    assert(end != NULL);
    *end = '\0';

    char* start = strstr(request, "\r\n");
    assert(start != NULL);

    start += 2;

    //head_size = strlen(ping_header);
    //memcpy(head, ping_header, head_size);

    //body_size = strlen(ping_body);
    //memcpy(body, ping_body, body_size);

    //First, lock the mutex
    pthread_mutex_lock(&echo_mutex);

    body_size = strlen(start);
    memcpy(body, start, body_size);
    
    head_size = snprintf(head, sizeof(head), ok200_response, body_size);

    //Unlock the created mutex 
    pthread_mutex_unlock(&echo_mutex);

    //send_response(sockfd);
    send_response(sockfd, head, head_size, body, body_size);
}


//Tests 2-3: handles incoming client requests on a socket connection
void* request_handler(void* new_client) {

    int socket_connect = *(int*)(&new_client);

    /*socket_connect < 0 -> if error in the socket communication was detected -> return NULL immediately. 
    Otherwise we execute handle_client_request() and pass the socket_connect as an argument to it, and return NULL.
    */
    return (socket_connect < 0) ? NULL : (handle_client_request(socket_connect), NULL);
}



static int prepare_socket(int port) {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket < 0) {
        perror("Error creating socket");
        exit(1);
    }

    static struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &(server.sin_addr));

    int optval = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)); // maybe incomplete

    if (bind(server_socket, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Error on bind");
        exit(1);
    }

    if(listen(server_socket, 10) < 0) {
        perror("Error on listen");
        exit(1);
    }

    return server_socket;
}


int create_server_socket(int port, int threads) {
     int server_socket = prepare_socket(port);

     // Initialize any global variables here

     // Tests 9 and 10: Create threads here

     return server_socket;
}


static void send_response(int sockfd, char head[], int head_size, char body[], int body_size){
    
    int head_sent = send_fully(sockfd, head, head_size, 0);
    int body_sent = 0;

    while (body_sent != body_size) {
        body_sent += send_fully(sockfd, body+body_sent, body_size-body_sent, 0);
    }

    assert(head_sent == head_size);
    assert(body_sent == body_size);

    reqs += 1;
    head_bytes += head_size;
    body_bytes += body_size;
}


static void handle_client_request(int sockfd) {

    
    int len = recv_http_request(sockfd, request, sizeof(request), 0);

    if(len == 0) {
        // No request
        return;
    }

    assert(len > 0);

    if(!strncmp(request, ping_request, strlen(ping_request))) {
        handle_ping(sockfd);
        close(sockfd);
        return;
    } 
    else if (!strncmp(request, echo_request, strlen(echo_request))) {
        handle_echo(sockfd, request);
        close(sockfd);
        return;
    } 
    else if (!strncmp(request, write_request, strlen(write_request))) {
         handle_write(sockfd);
         close(sockfd);
         return;
    } 
    //else if (!strncmp(request, read_request, strlen(read_request))) {
    //     handle_read(sockfd);
    //     close(sockfd);
    //     return;
    // } else if (!strncmp(request, stats_request, strlen(stats_request))) {
    //     handle_stats(sockfd);
    //     close(sockfd);
    //     return;
    // } else if (!strncmp(request, "GET ", 4)) {
    //     handle_file(sockfd);
    //     return;
    //}

    //  send_error(sockfd, err400_response);
    close(sockfd);
}


void accept_client(int server_socket) {

    pthread_t threads;
    long ret1; // created it to save the return value

    static struct sockaddr_in client;
    static socklen_t client_size;

    memset(&client, 0, sizeof(client));
    memset(&client_size, 0, sizeof(client_size));

    int client_socket = accept(server_socket, (struct sockaddr *)&client, &client_size);
    if (client_socket < 0) {
        perror("Error on accept");
        exit(1);
    }


    /*
    pthread_create() creates a new thread using the POSIX thread library:
    a) &threads - ptr to the pthread_t variable that will be used to reference the new thread;
    b) NULL - sets the thread attributes to default values;
    c) request_handler - ptr to the function that will be executed by the new thread;
    d) (void*)(long) client_socket - void ptr that can be used to pass data to the thread function -> first client_socker must be casted to long,
    then to a void* (OH)
    */
    pthread_create(&threads, NULL, request_handler, (void *)(long) client_socket);
}
