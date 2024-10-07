#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

void handle_client(int);
void handle_send_usage(int);
void handle_ping(int);
void handle_echo(int, char*);
void handle_err_command(int, char*);

int main() {
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	printf("Logs from your program will appear here!\n");

	int server_fd, client_addr_len;
	struct sockaddr_in client_addr;
	int client_fd;
	int child_pid;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}

	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}

	struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
		.sin_port = htons(6379),
		.sin_addr = { htonl(INADDR_ANY) },
	};

	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}

	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}

	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);

	while (1) {
		client_fd =  accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);

		if (client_fd == -1) {
			printf("Accept failed: %s \n", strerror(errno));
			return 1;
		}

		child_pid = fork();

		switch (child_pid) {
			case -1:
				printf("Fork failed: %s \n", strerror(errno));
				return 1;
			case 0:
				// child process
				close(server_fd);
				handle_client(client_fd);
				close(client_fd);
				return 0;
			default:
				close(client_fd);
				int stat_loc;
				while (waitpid(child_pid, &stat_loc, WNOHANG) > 0);
		}
	}
	return 0;
}

void handle_client(int client_fd) {
	// read from client
	ssize_t bytes_read;
	char client_request[1024];
	char *command;

	bytes_read = read(client_fd, client_request, sizeof(client_request));
	if (bytes_read == -1) {
		printf("Read failed: %s \n", strerror(errno));
		return;
	}
	if (bytes_read < 2) {
		handle_send_usage(client_fd);
		return;
	}
	printf("Client request: %s- and read %d", client_request, bytes_read);
	raise(SIGTRAP);
	command = strtok(client_request, " ");
	printf("%x-len is %d\n", *(unsigned long long *)command, strlen(command));
	printf("%d", strcmp(command, "PING"));
	if (strcmp(command, "PING") == 0) {
		handle_ping(client_fd);
	}
	else if (strcmp(command, "ECHO") == 0) {
		handle_echo(client_fd, client_request);
	} else {
		handle_err_command(client_fd, client_request);
	}
}

void respond_to_client(int client_fd, char* buffer) {
	ssize_t bytes_written;
	bytes_written = write(client_fd, buffer, strlen(buffer));
	if (bytes_written == -1) {
		printf("write failed: %s \n", strerror(errno));
		return;
	}
}

void handle_send_usage(int client_fd) {
	char *buffer = "Send a command to get started....\n";
	respond_to_client(client_fd, buffer);
	printf("Empty request from client\n");
}


void handle_ping(int client_fd) {
	printf("Handling ping---------\n");
	char *pong = "PONG\r\n";
	respond_to_client(client_fd, pong);
	printf("Successfully ponged!");
}

void handle_echo(int client_fd, char* client_request) {
	printf("Handling echo---------\n");
	char *rest;
	rest = strchr(client_request, ' ');
	if (rest == NULL) {
		rest = malloc(8);
		strcpy(rest, "<NIL>\n");
	}
	respond_to_client(client_fd, rest);
	free(rest);
	printf("Successfully echoed\n");
}

void handle_err_command(int client_fd, char* client_request) {
	printf("Handling err---------\n");
	char *error_command = "Invalid command\r\n";
	respond_to_client(client_fd, error_command);
	printf("Invalid command from client: %s", client_request);
}
