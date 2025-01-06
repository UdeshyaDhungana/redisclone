#ifndef REPLICA_H
#define REPLICA_H

#include "store.h"
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include "parser.h"

ssize_t parse_full_resync(char*, size_t);
ssize_t parse_rdb_file(char*, char*, size_t);

char* parse_command(char* buffer);
void execute_master_command(char*, int);
void process_master_command(char* command, int);
void communicate_with_master();
void* process_master_communication_thread(void*);

#endif