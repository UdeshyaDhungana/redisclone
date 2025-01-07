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
#include <time.h>
#include <pthread.h>
#include "util.h"

#define EMPTY_RDB_HEX "524544495330303131fa0972656469732d76657205372e322e30fa0a72656469732d62697473c040fa056374696d65c26d08bc65fa08757365642d6d656dc2b0c41000fa08616f662d62617365c000fff06e3bfec0ff5aa2"

enum State_modification {
    RESULT_ERROR = 0,
    DONT_SAVE_TO_STATE,
    SAVE_TO_STATE,
};

void handle_client_request(int client_fd, char command[], bool);
enum State_modification process_command(int client_fd, str_array*, bool from_master);

void respond_str_to_client(int fd, char* buffer);
void respond_bytes_to_client(int fd, char* buffer, ssize_t);
void handle_syntax_error(int client_fd);

/* commands */
int handle_ping(int);
int handle_echo(int, str_array*);
int handle_set(int client_fd, str_array*);
int handle_get(int client_fd, str_array*);
int handle_keys(int client_fd, str_array*);
int handle_info(int client_fd, str_array*);

int handle_replconf(int client_fd, str_array*);
int handle_psync(int client_fd, str_array*);

typedef struct thread_handle_wait_args {
    int client_fd;
    str_array* arguments;
} thread_handle_wait_args;

// number of clients that have acknowledged replconf getack *
extern int acked_clients;
extern pthread_mutex_t acked_clients_lock;
void set_acked_clients(int);
void* thread_handle_wait(void*);
int handle_wait(int client_fd, str_array*);

/* Config */
int handle_config(int, str_array*);
int handle_config_get(int client_fd, str_array*);

/* Transfer rdb */
void transfer_empty_rdb(int client_fd);
void transfer_rdb_file(int);
void transfer_command_history(int);

bool propagate_to_replicas(char *command);

#endif