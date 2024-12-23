#include "store.h"
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include "parser.h"

bool extract_rdb_file(char*, char*);
void extract_master_command(char*, int*, int);
void* process_master_command_thread(void *arg);
void communicate_with_master();