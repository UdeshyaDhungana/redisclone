#ifndef DATABASE_H
#define DATABASE_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#define C_DIR "dir"
#define C_DBFILENAME "dbfilename"

// if you add a new option, free the variable in free_config() as well
typedef struct ConfigOptions {
    char* dir;
    char* dbfilename;
} ConfigOptions;

typedef enum StoreType {
    DB = 0,
    CONFIG = 1,
} StoreType;

typedef struct Node {
    char* key;
    char* value;
    long int expiry_epoch_ms;
    struct Node* next;
} Node;

typedef struct GlobalStore {
    Node* DB;
    Node* config;
} GlobalStore;

// init
int init_config(ConfigOptions*);
void free_config(ConfigOptions*);

// implementing the database as linked list right now; can be optimized later
Node* make_node(char* , char*, long int);
void delete_node(Node*);

// external interfaces as store
bool save_to_store(StoreType, char*, char*, long int);
Node* retrieve_from_store(StoreType, char* key);

// db
bool save_to_db(char*, char*, long int);
Node* retrieve_from_db(char *key);

// config
bool save_to_config(char* , char* , long int );
Node* retrieve_from_config(char*);

void __debug_print_DB();
void __debug_print_config();
void __debug_print_store(Node* );

#endif