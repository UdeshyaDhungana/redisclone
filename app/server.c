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
#include <stdbool.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <getopt.h>

#include "parser.h"
#include "responder.h"
#include "store.h"
#include "debug.h"
#include "util.h"

#define MAX_EVENTS 10
#define PORT 6379
#define BUFFER_SIZE 1024

void handle_client_request(int, char[]);
void set_non_blocking(int);
void set_reuse(int);
void print_help();

int main(int argc, char** argv) {
	int opt;
	int server_fd, client_fd, epoll_fd;
	struct sockaddr_in client_addr;
	struct epoll_event event, events[MAX_EVENTS];
	int port = PORT;
	ConfigOptions c = { .dir = NULL, .dbfilename = NULL, .replica_of = NULL  };
	/* Argparse */
	static struct option long_options[] = {
        {"dir", required_argument, 0, 'd'},
		{"dbfilename", required_argument, 0, 'f'},
		{"port", required_argument, 0, 'p'},
		{"help", no_argument, 0, 'h'},
		{"replicaof", required_argument, 0, 'r'},
		{NULL, 0, NULL, 0}
	};
	char* master_host;
	unsigned int master_port;

	while ((opt = getopt_long(argc, argv, "d:f:p:h:r", long_options, NULL)) != -1) {
		switch (opt) {
			case 'd':
				c.dir = malloc((strlen(optarg) + 1) * sizeof(char));
				strcpy(c.dir, optarg);
				break;
			case 'f':
				c.dbfilename = malloc((strlen(optarg) + 1) * sizeof(char));
				strcpy(c.dbfilename, optarg);
				break;
			case 'p':
				port = atoi(optarg);
				// since we will be listening on port > 1000 anyway, 0 is unused
				if (!port) {
					__debug_printf(__LINE__, __FILE__, "not a valid port\n");
					exit(1);
				}
				break;
			case 'h':
				print_help();
				exit(0);
			case 'r':
				// replica of
				if (parse_master_host_and_port(optarg, &master_host, &master_port) == -1) {
					__debug_printf(__LINE__, __FILE__, "Could not initialize redis as a replica\n");
					exit(1);
				}
				c.replica_of = malloc(sizeof(HostAndPort));
				if (!c.replica_of) {
					__debug_printf(__LINE__, __FILE__, "malloc failed\n");
				} else {
					c.replica_of->host = master_host;
					c.replica_of->port = master_port;
				}
				break;
			default:
				break;
		}
	}
	// initialize config and db
	// int num_configs, num_keys;
	// num_configs = _
	init_config(&c);
	// num_keys =
	init_db(&c);
	// __debug_printf(__LINE__, __FILE__, "%d configs read\n", num_configs);
	free_config(&c);

	/* Logging */
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	printf("Logs from your program will appear here!\n");

	// creating socket
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("socket() at %d on %s:  %s", __LINE__, __FILE__, strerror(errno));
		return 1;
	}

	// bind socket
	set_reuse(server_fd);
	set_non_blocking(server_fd);
	struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
		.sin_port = htons(port),
		.sin_addr = { (INADDR_ANY) },
		
	};

	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("bind() at %d on %s:  %s", __LINE__, __FILE__, strerror(errno));
		return 1;
	}

	// listen on socket
	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("listen() at %d on %s:  %s", __LINE__, __FILE__, strerror(errno));
		return 1;
	}

	/* Add server sock to epoll */
	epoll_fd = epoll_create1(0);
	event.data.fd = server_fd;
	event.events = EPOLLIN;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);

	// Event loop
	// printf("Waiting for a client to connect on %d\n", port);
	while (1) {
		int event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		for (int i = 0; i < event_count; i++) {
			if (events[i].data.fd == server_fd) {
				socklen_t client_len = sizeof(client_addr);
				client_fd = accept(server_fd, (struct sockaddr*) &client_addr, &client_len);
				if (client_fd == -1) {
					printf("accept() at %d on %s:  %s", __LINE__, __FILE__, strerror(errno));
					continue;
				}
				set_non_blocking(client_fd);
				event.data.fd = client_fd;
				event.events = EPOLLIN | EPOLLET;
				epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);

				printf("Client connected!\n");
			} else {
				// Handle client request
				client_fd = events[i].data.fd;
				char buffer[BUFFER_SIZE] = {0};
				int bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
				if (bytes_read <= 0) {
					// client disconnected
					// check if empty data
					if (errno == EAGAIN || errno == EWOULDBLOCK) {
						continue;
					} else {
						close(client_fd);
						epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
						printf("Client disconnected...\n");
					}	
				} else {
					buffer[bytes_read] = '\0';
					handle_client_request(client_fd, buffer);
				}
			}
		}
	}
	return 0;
}

void handle_client_request(int client_fd, char buffer[]) {
	str_array* lines = split_input_lines(buffer);
	int num_elements;
	num_elements = check_syntax(lines);
	/* syntax error */
	if (!num_elements) {
			handle_syntax_error(client_fd);
			return;
	}
	str_array* command_and_args = command_extraction(lines, num_elements);
	process_command(client_fd, *command_and_args);
	free_str_array(command_and_args);
	free_str_array(lines);
}


void set_non_blocking(int sockfd) {
	int flags = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

void set_reuse(int sockfd) {
	int reuse = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("setsocketopt() at %d on %s:  %s", __LINE__, __FILE__, strerror(errno));
	}
}

void print_help() {
	printf("help message\n");
}