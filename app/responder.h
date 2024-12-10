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
#include "util.h"

void process_command(int client_fd, str_array);

void respond_to_client(int fd, char* buffer);
void handle_syntax_error(int client_fd);

/* commands */
void handle_ping(int);
void handle_echo(int, str_array*);
void handle_set(int client_fd, str_array*);
void handle_get(int client_fd, str_array*);
void handle_keys(int client_fd, str_array*);
void handle_info(int client_fd, str_array*);

/* Config */
void handle_config(int, str_array*);
void handle_config_get(int client_fd, str_array*);

#endif