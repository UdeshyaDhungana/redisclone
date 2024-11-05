#ifndef RESPONDER_H
#define RESPONDER_H

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <assert.h>

void process_command(int client_fd, char* command_and_args[]);

void respond_to_client(int fd, char* buffer);
void handle_syntax_error(int client_fd);
void handle_ping(int);
void handle_echo(int, char*[]);
void handle_set(int client_fd, char* arguments[]);
void handle_get(int client_fd, char* arguments[]);

/* Config */
void handle_config(int, char* command_and_args[]);
void handle_config_get(int client_fd, char* args[]);

#endif