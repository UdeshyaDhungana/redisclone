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
void communicate_with_master();
void* process_master_communication_thread(void*);

#endif