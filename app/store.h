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
#include <pthread.h>
#include <limits.h>
#include "util.h"

#define C_DIR  "dir"
#define C_DBFILENAME "dbfilename"
#define INDEX "index"
#define REPLICATION "replication"
#define REPLCONF "REPLCONF"
#define GETACK "GETACK"
/* keys */
#define ROLE "role"
#define REPLICAOF "replicaof"
#define CONNECTED_SLAVES "connected_slaves"
#define MASTER_REPLID "master_replid"
#define MASTER_REPL_OFFSET "master_repl_offset"
#define SECOND_REPL_OFFSET "second_repl_offset"
#define REPL_BACKLOG_ACTIVE "repl_backlog_active"
#define REPL_BACKLOG_SIZE "repl_backlog_size"
#define REPL_BACKLOG_FIRST_BYTE_OFFSET "repl_backlog_first_byte_offset"
#define REPL_BACKLOG_HISTLEN "repl_backlog_histlen"
#define MASTER "master"
#define SLAVE "slave"
#define PORT_LITERAL "port"
/* predefined values */
#define MASTER_HOST "master_host"
#define MASTER_PORT "master_port"

#define METADATA_START ((char)0xFA)
#define DATABASE_START ((char)0xFE)
#define HASH_TABLE_START ((char)0xFB)
#define STRING_TYPE ((char)0x00)
#define EXPIRY_PAIR_MS ((char)0xFC)
#define EXPIRY_PAIR_SECONDS ((char)0xFD)
#define CHECKSUM_START ((char)0xFF)

// if you add a new option, free the variable in free_config() as well
typedef struct HostAndPort {
    char* host;
    unsigned int port;
} HostAndPort;

typedef struct ConfigOptions {
    char* dir;
    char* dbfilename;
    char* port;
    HostAndPort* replica_of;
} ConfigOptions;

typedef enum StoreType {
    STORETYPE_DB = 0,
    STORETYPE_CONFIG = 1,
    STORETYPE_METADATA = 2,
    STORETYPE_DB_INFO = 3,
    STORETYPE_REDIS_INFO = 4,
} StoreType;

typedef enum XADD_ERR {
    NONE = 0,
    SYSTEM_ERROR = 1,
    ENTRY_ID_MINIMUM = 2,
    ENTRY_ID_SMALLER = 3,
} XADD_ERR;


typedef struct Node {
    char* key;
    char* value;
    long int expiry_epoch_ms;
    struct Node* next;
} Node;

typedef struct StreamNode {
    char* key;
    char* value;
    char* ID;
    struct StreamNode* next;
} StreamNode;

typedef struct StreamHead {
    char* stream_name;
    struct StreamNode* node_ll;
    struct StreamHead* next;
} StreamHead;

typedef struct DB_Info {
    int index;
    int num_key_val;
    int num_expiry_keys;
} DB_Info;

typedef struct GlobalStore {
    Node* DB;
    StreamHead* streamDB;
    Node* config;
    Node* metadata;
    DB_Info db_info;
    str_array* command_history;
    size_t replconf;
    int_array* client_fds;
} GlobalStore;

// debug
void __debug_print_DB();
void __debug_print_config();
void __debug_print_metadata();
void __debug_print_store(Node* );
void __debug_print_stream_node(StreamNode*, int);
void __debug_print_stream_DB();

// load database from file
int init_db(ConfigOptions *);

// metadata
bool save_to_metadata(char*, char* );
Node* retrieve_from_metadata(char* );

// db info
// bool save_to_db_info(char*, char*);
// Node* retrieve_from_db_info(char*);

// init
int init_config(ConfigOptions*);
void free_config(ConfigOptions*);

// implementing the database as linked list right now; can be optimized later
Node* make_node(char* , char*, long int);
void delete_node(Node*);

// stream data structures
StreamNode* make_stream_node(char* ID, char* key, char* value, char* seq_number);
XADD_ERR verify_entry_id(char* existing, char* incoming);
XADD_ERR append_to_stream(StreamNode*, char*, char*, char*, bool);
void free_node_ll(StreamNode *n);
XADD_ERR xadd_db(char* stream_name, char* ID, char* key, char* value, bool);
StreamNode* xrange(char* stream_name, char* start_ID, char* end_ID, int* length);
int compare_entry_ID(char* first, char* second);

StreamHead* retrieve_stream(char* stream_name);

// external interfaces as store
bool save_to_store(StoreType, char*, char*, long int);
Node* retrieve_from_store(StoreType, char* key);

// db
extern pthread_mutex_t db_lock;
bool save_to_db(char*, char*, long int);
Node* retrieve_from_db(char *key);

// config
// gambling here to some extent. config file typically supports a fixed set of options
bool save_to_config(char* , char* , long int );
Node* retrieve_from_config(char*);

// keys
str_array* get_db_keys(char* pattern);

// command history
bool add_to_command_history(char[]);
str_array* get_command_history();

bool add_to_client_fds(int fd);
int_array* get_connected_client_fds();

bool add_replconf(size_t n);
size_t get_replconf();


#endif