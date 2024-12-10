#include "store.h"
#include "debug.h"
#include "util.h"

/* TODO: Test reading from file, with and with out expiry */
/*
Keys "*"
GET (with and without expiry)
*/

GlobalStore GS = {
    .DB = NULL,
    .config = NULL,
    .metadata = NULL,
    .db_info = {
        .index = 0,
        // we are ignoring other things like number of keys with expriy and not you can add that by reading
        // save_to_db_info and retrieve_from_db_info
    }
};

/* debug */

void __debug_print_Node(Node* n) {
    Node* runner = n;
    if (runner == NULL) {
        printf("<NIL>\n");
        return;
    }
    while (runner != NULL) {
        printf("%s = %s on %ld\t", runner->key, runner->value, runner->expiry_epoch_ms);
        runner = runner -> next;
    }
    printf("\n");
}

void __debug_print_DB() {
    __debug_print_Node(GS.DB);
}

void __debug_print_config() {
    __debug_print_Node(GS.config);
}

void __debug_print_metadata() {
    __debug_print_Node(GS.metadata);
}
/* end debug */

// you have to de-allocate the char ptr returned by this function
char* read_entire_file(const char *filename) {
    if (access(filename, F_OK & R_OK & W_OK) == -1) {
        __debug_printf(__LINE__, __FILE__, "RDB file does not exist at %s\n", filename);
        return NULL;
    }
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        __debug_printf(__LINE__, __FILE__, "%s\n", strerror(errno));
        return NULL;
    }
    struct stat file_stat;
    if (fstat(fd, &file_stat) < 0) {
        __debug_print_config(__LINE__, __FILE__, "%s\n", strerror(errno));
        close(fd);
        return NULL;
    }
    size_t file_size = file_stat.st_size;
    char *content = malloc(file_size + 1);
    if (!content) {
        __debug_printf(__LINE__, __FILE__, "%s\n", strerror(errno));
        close(fd);
        return NULL;
    }
    ssize_t bytes_read = read(fd, content, file_size);
    if (bytes_read != file_size) {
        __debug_printf(__LINE__, __FILE__, "%s", "Could not read the entire file\n");
        close(fd);
        return NULL;
    }
    content[bytes_read] = 0x00;
    close(fd);
    return content;
}

/* at any point during initializing db, if we come across any errors in the file, we start with null config */
// TODO: change this to use read_entire_file()
int init_db(ConfigOptions* c) {
    if (c == NULL || c->dir == NULL || c->dbfilename == NULL) {
        __debug_printf( __LINE__, __FILE__, "empty config\n");
        return -1;
    }
    char* full_rdb_path = malloc(sizeof(char) * (strlen(c->dir) + strlen(c->dbfilename) + 1));
    strcpy(full_rdb_path, c->dir);
    strcat(full_rdb_path, "/");
    strcat(full_rdb_path, c->dbfilename);
    // we now read from the rdb file
    char* file_contents = read_entire_file(full_rdb_path);
    if (file_contents == NULL) {
        free(full_rdb_path);
        return -1;
    }
    free(full_rdb_path);
    unsigned int file_offset = 0;
    unsigned int read_count = 0;
    // for every copy_substring operation, operate in this fashion:
    // 1. load the number of bytes to be read to `read_count`
    // 2. check the length returned by copy_substring and `read_count`
    
    // header section: contains magic string and version number
    char header[10];

    read_count = 9;
    // printf("Parsing Magic constant...");
    file_offset = copy_from_file_contents(header, file_contents, file_offset, read_count, true);
    if (strcmp(header, "REDIS0011") && strcmp(header, "REDIS0010")) {
        __debug_printf(__LINE__, __FILE__, "%s", "RDB file version unsupported :(\n");
        free(file_contents);
        return -1;
    }

    // metadata
    char identifier;
    char* key;
    char* value;
    // printf("Parsing Metadata...\n");
    file_offset = copy_from_file_contents(&identifier, file_contents, file_offset, 1, false);
    bool saved;
    int value_length;
    while (identifier == METADATA_START) {
        // read key size
        file_offset = copy_from_file_contents(&identifier, file_contents, file_offset, 1, false);
        key = (char*)malloc((identifier + 1) * sizeof(char));
        if (key == NULL) {
            __debug_printf( __LINE__, __FILE__,  strerror(errno));
            free(file_contents);
            return -1;
        }
        file_offset = copy_from_file_contents(key, file_contents, file_offset, (unsigned int)identifier, true);
        // some keys do not have string encoded values
        if (!(strcmp(key, "redis-bits") && strcmp(key, "ctime") && strcmp(key, "used-mem") && strcmp(key, "aof-base"))) {
            for (value_length = 0; (file_contents[file_offset + value_length] != METADATA_START && file_contents[file_offset + value_length] != DATABASE_START); value_length++); 
        } else {
            file_offset = copy_from_file_contents(&identifier, file_contents, file_offset, 1, false);
            value_length = identifier;
        }
        value = malloc((value_length + 1) * sizeof(char)) ;
        if (value == NULL) {
            __debug_printf( __LINE__, __FILE__, strerror(errno));
            free(key);
            free(file_contents);
            return -1;
        }
        file_offset = copy_from_file_contents(value, file_contents, file_offset, value_length, true);

        saved = save_to_metadata(key, value);
        if (!saved) {
            __debug_printf(__LINE__, __FILE__, "Failed to save %s = %s\n", key, value);
        }
        printf("Saved metadata: %s = %s\n", key, value);
        free(key);
        free(value);
        file_offset = copy_from_file_contents(&identifier, file_contents, file_offset, 1, false);
    }
    
    printf("Parsing section between metadata and db\n");
    // database, we have encountered the 0xFE byte by now, we may need to change this implementation in case 'size-encoded' means something different
    if (identifier != DATABASE_START) {
        __debug_printf(__LINE__, __FILE__, "Error reading start of database subsection\n");
        free(file_contents);
        return -1;
    }
    // database index size
    file_offset = copy_from_file_contents(&identifier, file_contents, file_offset, 1, false);
    GS.db_info.index = identifier;

    // 0xFB [we skipped reading this part :)]
    file_offset += 1;

    // num of key value pairs without expiry
    file_offset = copy_from_file_contents(&identifier, file_contents, file_offset, 1, false);
    GS.db_info.num_key_val = identifier;

    // num of key value pairs with expiry
    file_offset = copy_from_file_contents(&identifier, file_contents, file_offset, 1, false);
    GS.db_info.num_expiry_keys = identifier;

    // start reading data
    char type_identifier;
    char size_identifier;
    printf("Parsing DB...\n");
    file_offset = copy_from_file_contents(&type_identifier, file_contents, file_offset, 1, false);
    unsigned long long expiry_ms = 00;
    unsigned int expiry_s = 0;
    unsigned int num_keys = 0;
    bool save_success;
    printf("type id: %x\n", type_identifier);
    while (type_identifier != CHECKSUM_START) {
        if (type_identifier == EXPIRY_PAIR_MS) {
            // the 8 byte that follows is timestamp in ms
            file_offset = copy_ms_from_file_contents(&expiry_ms, file_contents, file_offset);
        } else if (type_identifier == EXPIRY_PAIR_SECONDS) {
            // the 4 bytes that follows is timestamp in s
            // can we make better abstractions? yes. am i willing to do it rn? no!
            file_offset = copy_seconds_from_file_contents(&expiry_s, file_contents, file_offset);
        }
        if (type_identifier == EXPIRY_PAIR_MS || type_identifier == EXPIRY_PAIR_SECONDS) {
            // since it was an expiring data, determine its type, otherwise type_identifier already contains it
            file_offset = copy_from_file_contents(&type_identifier, file_contents, file_offset, 1, false);
        }
        file_offset = copy_from_file_contents(&size_identifier, file_contents, file_offset, 1, false);
        // __debug_printf(__LINE__, __FILE__, "size of key is %x\n", size_identifier);
        key = malloc((size_identifier + 1) * sizeof(char));
        if (key == NULL) {
            __debug_printf(__LINE__, __FILE__, strerror(errno));
            free(file_contents);
            return -1;
        }
        file_offset = copy_from_file_contents(key, file_contents, file_offset, (unsigned int)size_identifier, true);
        // __debug_printf(__LINE__, __FILE__, "key is %s\n", key);
        file_offset = copy_from_file_contents(&size_identifier, file_contents, file_offset, 1, false);
        // __debug_printf(__LINE__, __FILE__, "size of value is %x\n", size_identifier);
        value = malloc((size_identifier + 1) * sizeof(char));
        if (key == NULL) {
            __debug_printf(__LINE__, __FILE__, strerror(errno));
            free(key);
            free(file_contents);
            return -1;
        }
        file_offset = copy_from_file_contents(value, file_contents, file_offset, (unsigned int)size_identifier, true);
        __debug_printf(__LINE__, __FILE__, "%s = %s @ %ul\n", key, value, choose_between_expiries(expiry_ms, expiry_s));
        // convert endianness of expiry timestamp
        save_success = save_to_db(key, value, choose_between_expiries(expiry_ms, expiry_s));
        if (!save_success) {
            __debug_printf(__LINE__, __FILE__, "Failed to save data to db\n");
        }
        num_keys += 1;
        expiry_ms = expiry_s = 0;
        free(key);
        free(value);
        file_offset = copy_from_file_contents(&type_identifier, file_contents, file_offset, 1, false);
    }
    printf("Parsing checksum...\n");
    // printf("%x\n", file_contents[file_offset]);
    // checksum part
    // now you have to refactor the code, and read the entire file into a string
    // skip the checksum part and let's test the code
    return num_keys;
}

/* Init */
int init_config(ConfigOptions* c) {
    int num_config = 0;
    char port_str[6];

    if (c->dir != NULL) {
        save_to_config(C_DIR, c->dir, -1);
        num_config++;
    }
    if (c->dbfilename != NULL) {
        save_to_config(C_DBFILENAME, c->dbfilename, -1);
        num_config++;
    }
    if (c->replica_of != NULL) {
        save_to_config(MASTER_HOST, c->replica_of->host, -1);
        sprintf(port_str, "%u", c->replica_of->port);
        save_to_config(MASTER_PORT, port_str, -1);
        num_config++;
    }
    return num_config;
}

/* Free config options */
void free_config(ConfigOptions* c) {
    if (c->dir != NULL) {
        free(c->dir);
    }
    if (c->dbfilename != NULL) {
        free(c->dbfilename);
    }
}


Node* make_node(char* key, char* value, long int expiry_ms) {
    Node* n = (Node*) malloc(sizeof(Node));
    if (n == NULL) {
        printf("malloc() at %d on %s:  %s", __LINE__, __FILE__, strerror(errno));
        return NULL;
    }
    n->key = malloc((1 + strlen(key)) * sizeof(char));
    if (n->key == NULL) {
        printf("malloc() at %d on %s:  %s", __LINE__, __FILE__, strerror(errno));
        free(n);
        return NULL;
    }
    n->value = malloc((1 + strlen(value)) * sizeof(char));
    if (n->value == NULL) {
        printf("malloc() at %d on %s:  %s", __LINE__, __FILE__, strerror(errno));
        free(n->key);
        free(n);
        return NULL;
    }
    n->expiry_epoch_ms = expiry_ms;
    n->next = NULL;
    strcpy(n->key, key);
    strcpy(n->value, value);
    return n;
}

void free_node(Node* n) {
    // free up the memory allocated for a node
    if (!n) {
        __debug_printf( __LINE__, __FILE__, "Can't free NULL node\n");
        return;
    }
    free(n->key);
    free(n->value);
    free(n);
}

void delete_node(Node* n) {
    if (!n || !GS.DB) {
        return;
    }
    Node* runner = GS.DB;
    if (n == runner) {
        GS.DB = runner->next;
        free_node(n);
        return;
    }
    while (runner->next != n) {
        runner = runner->next;
    }
    Node* to_delete = runner->next;
    runner->next = to_delete->next;
    free_node(to_delete);
}

bool save_to_store(StoreType st, char* key, char* value, long int expiry_ms) {
    bool result;
    Node** store_ptr;
    Node* tmp;
    ssize_t valuelen;
    int same_strings;
    switch (st) {
        case STORETYPE_DB:
            store_ptr = &GS.DB;
            break;
        case STORETYPE_CONFIG:
            store_ptr = &GS.config;
            break;
        case STORETYPE_METADATA:
            store_ptr = &GS.metadata;
            break;
        default:
            assert(false);
    } 
    if (*store_ptr == NULL) {
        tmp = make_node(key, value, expiry_ms);
        if (!tmp) {
            result = false;
        } else {
            *store_ptr = tmp;
            result = true;
        }
    } else {
        Node* runner = *store_ptr;
        same_strings = (!strcmp(runner->key, key));
        while (!same_strings) {
            if (runner->next == NULL) {
                break;
            }
            runner = runner -> next;
            same_strings = (!strcmp(runner->key, key));
        }
        if (same_strings) {
            valuelen = strlen(value);
            if (valuelen > strlen(runner->value)) {
                runner->value = realloc(runner->value, (sizeof (char)) * (valuelen + 1));
            }
            runner->value[0] = 0;
            strcpy(runner->value, value);
        } else {
            Node* new_node = make_node(key, value, expiry_ms);
            runner->next = new_node;
        }
        result = true;
    }
    return result;
}

Node* retrieve_from_store(StoreType st, char* key) {
    Node* store;
    switch(st) {
        case STORETYPE_DB:
            store = GS.DB;
            break;
        case STORETYPE_CONFIG:
            store = GS.config;
            break;
        default:
            assert(false);
    }
    Node* runner = store;
    while (runner != NULL) {
        if (!strcmp(key, runner->key)) {
            return runner;
        }
        runner = runner -> next;
    }
    return NULL;
}

bool save_to_db(char* key, char* value, long int expiry) {
    return save_to_store(STORETYPE_DB, key, value, expiry);
}

Node* retrieve_from_db(char* key) {
    return retrieve_from_store(STORETYPE_DB, key);
}

bool save_to_config(char* key, char* value, long int expiry) {
    return save_to_store(STORETYPE_CONFIG, key, value, -1);
}

Node* retrieve_from_config(char* key) {
    return retrieve_from_store(STORETYPE_CONFIG, key);
}

bool save_to_metadata(char* key, char* value) {
    return save_to_store(STORETYPE_METADATA, key, value, -1);
}

Node* retrieve_from_metadata(char* key) {
    return retrieve_from_store(STORETYPE_METADATA, key);
}

str_array* get_db_keys(char* pattern) {
    Node* store = GS.DB;
    char* key;
    str_array* result = create_str_array(NULL);
    while (store != NULL) {
        key = store->key;
        if (match_str(pattern, key)) {
            append_to_str_array(&result, key);
        }
        store = store->next;
    }
    print_str_array(result, '\n');
    return result;
}