#include<stdio.h>
#include<string.h>
#include<unistd.h>
#include<stdlib.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<pthread.h>
#include"server.h"

// Mutex to limit concurrent clients
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int active_clients = 0;
const int MAX_CLIENTS = 3;  // Max number of concurrent clients

void* handle_client(void *arg) {
    char buffer[3000];
    char response[4000];
    int new_socket = *(int*)arg;
    free(arg);

    // HTML page for file upload
    const char *html_page =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n\r\n"
        "<!DOCTYPE html>"
        "<html>"
        "<head><title>File Upload</title></head>"
        "<body>"
        "<h2>Upload a File</h2>"
        "<form action=\"/upload\" method=\"POST\" enctype=\"multipart/form-data\">"
        "  <input type=\"file\" name=\"file\">"
        "  <input type=\"submit\" value=\"Upload\">"
        "</form>"
        "</body>"
        "</html>";

    // Directory to save uploads
    const char *upload_dir = "upload";
    mkdir(upload_dir, 0777);

    // Client request handling
    memset(buffer, 0, sizeof(buffer));
    read(new_socket, buffer, sizeof(buffer) - 1);

    // Check if the client is allowed to connect (max 3 clients)
    pthread_mutex_lock(&mutex);
    if (active_clients >= MAX_CLIENTS) {
        snprintf(response, sizeof(response),
                 "HTTP/1.1 503 Service Unavailable\r\n"
                 "Content-Type: text/html\r\n\r\n"
                 "<!DOCTYPE html>"
                 "<html>"
                 "<head><title>Service Unavailable</title></head>"
                 "<body>"
                 "<h2>Sorry, the server is full. Try again later.</h2>"
                 "</body>"
                 "</html>");
        write(new_socket, response, strlen(response));
        close(new_socket);
        pthread_mutex_unlock(&mutex);
        return NULL;
    }
    active_clients++;
    pthread_mutex_unlock(&mutex);

    // Serve the HTML page or handle file upload
    if (strncmp(buffer, "GET / ", 6) == 0) {
        write(new_socket, html_page, strlen(html_page));
    } else if (strncmp(buffer, "POST /upload", 12) == 0) {
        char *file_start = strstr(buffer, "\r\n\r\n") + 4;
        char *boundary = strstr(buffer, "boundary=");
        if (boundary) {
            boundary += 9;  // Move past "boundary="
            char boundary_str[100];
            sscanf(boundary, "%s", boundary_str);

            char *file_data = strstr(file_start, "\r\n\r\n") + 4;
            char *file_end = strstr(file_data, boundary_str) - 4;

            if (file_data && file_end && file_end > file_data) {
                // Save file to upload directory
                char filepath[1024];
                /*TO DO
                	1. make sure th file uploaded in .o format
                	2. the file name should be same 
                */
                snprintf(filepath, sizeof(filepath), "%s/uploaded_file", upload_dir);
                int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd >= 0) {
                    write(fd, file_data, file_end - file_data);
                    close(fd);

                    snprintf(response, sizeof(response),
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: text/html\r\n\r\n"
                             "<!DOCTYPE html>"
                             "<html>"
                             "<head><title>Upload Successful</title></head>"
                             "<body>"
                             "<h2>File uploaded successfully!</h2>"
                             "<a href=\"/\">Upload Another File</a><br>"
                             "<a href=\"/exit\">Exit</a>"
                             "</body>"
                             "</html>");
                    write(new_socket, response, strlen(response));
                } else {
                    snprintf(response, sizeof(response),
                             "HTTP/1.1 500 Internal Server Error\r\n"
                             "Content-Type: text/plain\r\n\r\n"
                             "Failed to save file\n");
                    write(new_socket, response, strlen(response));
                }
            } else {
                snprintf(response, sizeof(response),
                         "HTTP/1.1 400 Bad Request\r\n"
                         "Content-Type: text/plain\r\n\r\n"
                         "Invalid file upload\n");
                write(new_socket, response, strlen(response));
            }
        } else {
            snprintf(response, sizeof(response),
                     "HTTP/1.1 400 Bad Request\r\n"
                     "Content-Type: text/plain\r\n\r\n"
                     "Invalid request format\n");
            write(new_socket, response, strlen(response));
        }
    } else if (strncmp(buffer, "GET /exit", 9) == 0) {
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/html\r\n\r\n"
                 "<!DOCTYPE html>"
                 "<html>"
                 "<head><title>Goodbye</title></head>"
                 "<body>"
                 "<h2>Goodbye!</h2>"
                 "</body>"
                 "</html>");
        write(new_socket, response, strlen(response));
        close(new_socket);
        pthread_mutex_lock(&mutex);
        active_clients--;
        pthread_mutex_unlock(&mutex);
        return NULL;
    } else {
        snprintf(response, sizeof(response),
                 "HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/plain\r\n\r\n"
                 "Resource not found\n");
        write(new_socket, response, strlen(response));
    }

    close(new_socket);
    pthread_mutex_lock(&mutex);
    active_clients--;
    pthread_mutex_unlock(&mutex);

    return NULL;
}

void launch(struct Server *server) {
    int new_socket;
    int address_length = sizeof(server->address);

    while (1) {
        printf(".... WAITING FOR THE CONNECTION ....\n enter in http://localhost:8080 for uploading\n");
        new_socket = accept(server->socket, (struct sockaddr *)&server->address, (socklen_t *)&address_length);

        if (new_socket < 0) {
            perror("Failed to accept connection");
            continue;
        }

        // Create a new thread to handle the client
        pthread_t thread_id;
        int *socket_ptr = malloc(sizeof(int));
        *socket_ptr = new_socket;
        if (pthread_create(&thread_id, NULL, handle_client, socket_ptr) != 0) {
            perror("Failed to create thread");
            close(new_socket);
        } else {
            pthread_detach(thread_id);  // Automatically reclaim resources when thread ends
        }
    }
}

int main() {
    struct Server server = server_constructor(AF_INET, SOCK_STREAM, 0, INADDR_ANY, 8080, 3, launch);
    server.launch(&server);
}

