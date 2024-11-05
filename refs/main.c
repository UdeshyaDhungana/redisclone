#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define PORT 12345
#define MAX_EVENTS 10
#define BUFFER_SIZE 1024

// Simple in-memory key-value store structure
typedef struct {
    char key[BUFFER_SIZE];
    char value[BUFFER_SIZE];
} KVStore;

KVStore store[100]; // Simplified in-memory store
int store_count = 0;

// Set non-blocking mode for a socket
void set_non_blocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

// Handle client commands
void handle_command(int client_sock, const char *command) {
    char response[BUFFER_SIZE] = {0};

    if (command[0] == 'S') { // SET command
        char key[BUFFER_SIZE], value[BUFFER_SIZE];
        sscanf(command, "S %s %s", key, value);
        strncpy(store[store_count].key, key, BUFFER_SIZE);
        strncpy(store[store_count].value, value, BUFFER_SIZE);
        store_count++;
        snprintf(response, BUFFER_SIZE, "OK\n");

    } else if (command[0] == 'G') { // GET command
        char key[BUFFER_SIZE];
        sscanf(command, "G %s", key);
        int found = 0;
        for (int i = 0; i < store_count; i++) {
            if (strcmp(store[i].key, key) == 0) {
                snprintf(response, BUFFER_SIZE, "%s\n", store[i].value);
                found = 1;
                break;
            }
        }
        if (!found) {
            snprintf(response, BUFFER_SIZE, "(nil)\n");
        }

    } else if (command[0] == 'E') { // ECHO command
        char message[BUFFER_SIZE];
        sscanf(command, "E %[^\n]", message);
        snprintf(response, BUFFER_SIZE, "%s\n", message);

    } else if (command[0] == 'P') { // PING command
        snprintf(response, BUFFER_SIZE, "PONG\n");

    } else { // Unknown command
        snprintf(response, BUFFER_SIZE, "ERROR: Unknown command\n");
    }

    // Send the response to the client
    send(client_sock, response, strlen(response), 0);
}

// Main server loop
int main() {
    int server_sock, client_sock, epoll_fd;
    struct sockaddr_in server_addr, client_addr;
    struct epoll_event event, events[MAX_EVENTS];

    // Create server socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set server socket options and non-blocking mode
    set_non_blocking(server_sock);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, 10) < 0) {
        perror("Listen failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // Set up epoll
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("Epoll creation failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // Add server socket to epoll
    event.data.fd = server_sock;
    event.events = EPOLLIN;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_sock, &event);

    printf("Server listening on port %d...\n", PORT);

    // Main event loop
    while (1) {
        int event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < event_count; i++) {
            if (events[i].data.fd == server_sock) { // New client connection
                socklen_t client_len = sizeof(client_addr);
                client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
                if (client_sock < 0) {
                    perror("Client accept failed");
                    continue;
                }
                set_non_blocking(client_sock);

                event.data.fd = client_sock;
                event.events = EPOLLIN | EPOLLET;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_sock, &event);

                printf("New client connected.\n");

            } else { // Handle client request
                client_sock = events[i].data.fd;
                char buffer[BUFFER_SIZE] = {0};
                int bytes_read = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);

                if (bytes_read <= 0) { // Client disconnected
                    close(client_sock);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_sock, NULL);
                    printf("Client disconnected.\n");
                } else { // Process command
                    buffer[bytes_read] = '\0';
                    printf("Received command: %s\n", buffer);
                    handle_command(client_sock, buffer);
                }
            }
        }
    }

    // Cleanup
    close(server_sock);
    close(epoll_fd);

    return 0;
}
