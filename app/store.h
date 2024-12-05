#ifndef STORE_H
#define STORE_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include "util.h"

#define C_DIR  "dir"
#define C_DBFILENAME "dbfilename"
#define INDEX "index"

#define METADATA_START ((char)0xFA)
#define DATABASE_START ((char)0xFE)
#define HASH_TABLE_START ((char)0xFB)
#define STRING_TYPE ((char)0x00)
#define EXPIRY_PAIR_MS ((char)0xFC)
#define EXPIRY_PAIR_SECONDS ((char)0xFD)
#define CHECKSUM_START ((char)0xFF)

// if you add a new option, free the variable in free_config() as well
typedef struct ConfigOptions {
    char* dir;
    char* dbfilename;
} ConfigOptions;

typedef enum StoreType {
    STORETYPE_DB = 0,
    STORETYPE_CONFIG = 1,
    STORETYPE_METADATA = 2,
    STORETYPE_DB_INFO = 3
} StoreType;


typedef struct Node {
    char* key;
    char* value;
    long int expiry_epoch_ms;
    struct Node* next;
} Node;

typedef struct DB_Info {
    int index;
    int num_key_val;
    int num_expiry_keys;
} DB_Info;

typedef struct GlobalStore {
    Node* DB;
    Node* config;
    Node* metadata;
    DB_Info db_info;
} GlobalStore;

// debug
void __debug_print_DB();
void __debug_print_config();
void __debug_print_metadata();
void __debug_print_store(Node* );

// 

// load database from file
int init_db(ConfigOptions*);

// metadata
bool save_to_metadata(char*, char* );
Node* retrieve_from_metadata(char* );

// db info
bool save_to_db_info(char*, char*);
Node* retrieve_from_db_info(char*);

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
// gambling here to some extent. config file typically supports a fixed set of options
bool save_to_config(char* , char* , long int );
Node* retrieve_from_config(char*);

// keys
str_array* get_db_keys(char* pattern);

#endif