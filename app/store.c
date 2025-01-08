#include "store.h"
#include "debug.h"
#include "util.h"

/* TODO: Test reading from file, with and with out expiry */
/*
Keys "*"
GET (with and without expiry)
*/

pthread_mutex_t db_lock;


GlobalStore GS = {
    .DB = NULL,
    .config = NULL,
    .metadata = NULL,
    .db_info = {
        .index = 0,
        // we are ignoring other things like number of keys with expriy and not you can add that by reading
        // save_to_db_info and retrieve_from_db_info
    },
    .command_history = NULL,
    .replconf = 0,
    .client_fds = NULL
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

void __debug_print_stream_node(StreamNode* node) {
    StreamNode* runner = node;
    while (runner != NULL) {
        printf("ID: %s\n", runner->ID);
        printf("Key: %s\n", runner->key);
        printf("Value: %s\n", runner->value);
        printf("--------------\n");
        runner = runner->next;
    }
}

void __debug_print_stream_DB() {
    StreamHead* head_runner;
    head_runner = GS.streamDB;
    if (!head_runner) {
        printf("StreamDB is empty\n");
        return;
    }
    while (head_runner != NULL) {
        printf("=====================\n");
        printf("Stream: [%s]\n", head_runner->stream_name);
        __debug_print_stream_node(head_runner->node_ll);
        head_runner = head_runner->next;
    }
}
/* end debug */


/* at any point during initializing db, if we come across any errors in the file, we start with null config */
/* this function is *very very* poorly written. I need to do a project on string manipulation and shit to improve my skills */
int init_db(ConfigOptions* c) {
    pthread_mutex_init(&db_lock, NULL);
    unsigned int num_keys = 0;
    if (c == NULL || c->dir == NULL || c->dbfilename == NULL) {
        __debug_printf( __LINE__, __FILE__, "empty config\n");
        return -1;
    }
    char* full_rdb_path = malloc(sizeof(char) * (strlen(c->dir) + strlen(c->dbfilename) + 2));
    strcpy(full_rdb_path, c->dir);
    strcat(full_rdb_path, "/");
    strcat(full_rdb_path, c->dbfilename);
    // we now read from the rdb file
    file_content *f = read_entire_file(full_rdb_path);
    if (f == NULL) {
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
    file_offset = copy_from_file_contents(header, f->content, file_offset, read_count, true);
    if (strcmp(header, "REDIS0011") && strcmp(header, "REDIS0010")) {
        __debug_printf(__LINE__, __FILE__, "%s", "RDB file version unsupported :(\n");
        free_file_content(f);
        return -1;
    }

    // metadata
    char identifier;
    char* key;
    char* value;
    // printf("Parsing Metadata...\n");
    file_offset = copy_from_file_contents(&identifier, f->content, file_offset, 1, false);
    bool saved;
    int value_length;
    while (identifier == METADATA_START) {
        // read key size
        file_offset = copy_from_file_contents(&identifier, f->content, file_offset, 1, false);
        key = (char*)malloc((identifier + 1) * sizeof(char));
        if (key == NULL) {
            __debug_printf( __LINE__, __FILE__,  strerror(errno));
            free_file_content(f);
            return -1;
        }
        file_offset = copy_from_file_contents(key, f->content, file_offset, (unsigned int)identifier, true);
        // some keys do not have string encoded values
        if (!(strcmp(key, "redis-bits") && strcmp(key, "ctime") && strcmp(key, "used-mem") && strcmp(key, "aof-base"))) {
            for (value_length = 0; (f->content[file_offset + value_length] != METADATA_START && f->content[file_offset + value_length] != DATABASE_START && f->content[file_offset + value_length] != CHECKSUM_START); value_length++); 
        } else {
            file_offset = copy_from_file_contents(&identifier, f->content, file_offset, 1, false);
            value_length = identifier;
        }
        value = malloc((value_length + 1) * sizeof(char)) ;
        if (value == NULL) {
            __debug_printf( __LINE__, __FILE__, strerror(errno));
            free(key);
            free_file_content(f);
            return -1;
        }
        file_offset = copy_from_file_contents(value, f->content, file_offset, value_length, true);

        saved = save_to_metadata(key, value);
        if (!saved) {
            __debug_printf(__LINE__, __FILE__, "Failed to save %s = %s\n", key, value);
        }
        printf("Saved metadata: %s = %s\n", key, value);
        free(key);
        free(value);
        file_offset = copy_from_file_contents(&identifier, f->content, file_offset, 1, false);
    }

    if (identifier == CHECKSUM_START) {
        // call checksum early
        return num_keys;
    }
    

    printf("Parsing section between metadata and db\n");
    // database, we have encountered the 0xFE byte by now, we may need to change this implementation in case 'size-encoded' means something different
    if (identifier != DATABASE_START) {
        __debug_printf(__LINE__, __FILE__, "Error reading start of database subsection\n");
        free_file_content(f);
        return -1;
    }
    // database index size
    file_offset = copy_from_file_contents(&identifier, f->content, file_offset, 1, false);
    GS.db_info.index = identifier;

    // 0xFB [we skipped reading this part :)]
    file_offset += 1;

    // num of key value pairs without expiry
    file_offset = copy_from_file_contents(&identifier, f->content, file_offset, 1, false);
    GS.db_info.num_key_val = identifier;

    // num of key value pairs with expiry
    file_offset = copy_from_file_contents(&identifier, f->content, file_offset, 1, false);
    GS.db_info.num_expiry_keys = identifier;

    // start reading data
    char type_identifier;
    char size_identifier;
    printf("Parsing DB...\n");
    file_offset = copy_from_file_contents(&type_identifier, f->content, file_offset, 1, false);
    unsigned long long expiry_ms = 00;
    unsigned int expiry_s = 0;
    bool save_success;
    while (type_identifier != CHECKSUM_START) {
        if (type_identifier == EXPIRY_PAIR_MS) {
            // the 8 byte that follows is timestamp in ms
            file_offset = copy_ms_from_file_contents(&expiry_ms, f->content, file_offset);
        } else if (type_identifier == EXPIRY_PAIR_SECONDS) {
            // the 4 bytes that follows is timestamp in s
            // can we make better abstractions? yes. am i willing to do it rn? no!
            file_offset = copy_seconds_from_file_contents(&expiry_s, f->content, file_offset);
        }
        if (type_identifier == EXPIRY_PAIR_MS || type_identifier == EXPIRY_PAIR_SECONDS) {
            // since it was an expiring data, determine its type, otherwise type_identifier already contains it
            file_offset = copy_from_file_contents(&type_identifier, f->content, file_offset, 1, false);
        }
        file_offset = copy_from_file_contents(&size_identifier, f->content, file_offset, 1, false);
        // __debug_printf(__LINE__, __FILE__, "size of key is %x\n", size_identifier);
        key = malloc((size_identifier + 1) * sizeof(char));
        if (key == NULL) {
            __debug_printf(__LINE__, __FILE__, strerror(errno));
            free_file_content(f);
            return -1;
        }
        file_offset = copy_from_file_contents(key, f->content, file_offset, (unsigned int)size_identifier, true);
        // __debug_printf(__LINE__, __FILE__, "key is %s\n", key);
        file_offset = copy_from_file_contents(&size_identifier, f->content, file_offset, 1, false);
        // __debug_printf(__LINE__, __FILE__, "size of value is %x\n", size_identifier);
        value = malloc((size_identifier + 1) * sizeof(char));
        if (key == NULL) {
            __debug_printf(__LINE__, __FILE__, strerror(errno));
            free(key);
            free_file_content(f);
            return -1;
        }
        file_offset = copy_from_file_contents(value, f->content, file_offset, (unsigned int)size_identifier, true);
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
        file_offset = copy_from_file_contents(&type_identifier, f->content, file_offset, 1, false);
    }
    printf("Parsing checksum...\n");
    // printf("%x\n", file_contents[file_offset]);
    // checksum part
    // now you have to refactor the code, and read the entire file into a string
    // skip the checksum part and let's test the code
    free_file_content(f);
    __debug_print_DB();
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
    } else {
        save_to_config(MASTER_REPLID, "8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb", -1);
        save_to_config(MASTER_REPL_OFFSET, "0", -1);
        num_config += 2;
    }
    if (c->port != NULL) {
        save_to_config(PORT_LITERAL, c->port, -1);
        num_config++;
    }
    // initialize redis info related things
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

StreamNode* make_stream_node(char* ID, char* key, char* value, char* prev_id) {
    char* generated_id = (char*)malloc(16 * sizeof(char));
    if (!generated_id) {
        __debug_printf(__LINE__, __FILE__, "malloc failed: %s\n", strerror(errno));
        return NULL;
    }
    if (!strcmp(ID, "*")) {
        sprintf(generated_id, "%ld-%d", get_epoch_ms(), 0);
    } else {
        char* dash = strstr(ID, "-");
        if (!dash) return NULL;
        if (strcmp(dash + 1, "*") != 0) {
            strcpy(generated_id, ID);
        } else {
            // first one in the stream
            long int incoming_ts = get_timestamp_from_entry_id(ID);
            if (prev_id == NULL) {
                sprintf(generated_id, "%ld-%d", incoming_ts, (incoming_ts == 0) ? 1:0);
            } else {
                int previous_seq = get_sequence_number_from_entry_id(prev_id);
                long int previous_ts = get_timestamp_from_entry_id(prev_id);
                if (incoming_ts == previous_ts) {
                    sprintf(generated_id, "%ld-%d", incoming_ts, (previous_seq + 1));
                } else {
                    sprintf(generated_id, "%ld-%d", incoming_ts, 0, (incoming_ts == 0) ? 1:0);
                }
            }
        }
    }
    StreamNode* result = malloc(sizeof(StreamNode));
    if (!result) {
        __debug_printf(__LINE__, __FILE__, "malloc failed: %s\n", strerror(errno));
        return NULL;
    }
    /* If ID is of format *, fully generate */
    result->next = NULL;
    result->ID = strdup(generated_id);
    result->key = strdup(key);
    result->value = strdup(value);
    strcpy(ID, generated_id);
    return result;
}

XADD_ERR verify_entry_id(char* existing, char* incoming) {
    bool is_full_id = true;
    if (!strcmp(incoming, "*")) {
        return NONE;
    } else {
        char* dash = strstr(incoming, "-");
        if (!strcmp(dash + 1, "*")) {
            is_full_id = false;
        }
    }
    long int existing_ts, incoming_ts;
    int existing_seq, incoming_seq;
    existing_ts = get_timestamp_from_entry_id(existing);
    incoming_ts = get_timestamp_from_entry_id(incoming);
    if (is_full_id) {
        existing_seq = get_sequence_number_from_entry_id(existing);
        incoming_seq = get_sequence_number_from_entry_id(incoming);
        // verify minimum
        if (incoming_ts == 0) {
            if (incoming_seq < 1) {
                return ENTRY_ID_MINIMUM;
            }
        }
        if (incoming_ts < existing_ts) return ENTRY_ID_SMALLER;
        if (incoming_ts == existing_ts) {
            if (incoming_seq > existing_seq) {
                return NONE;
            }
            return ENTRY_ID_SMALLER;
        }
        return NONE;
    } else {
        if (incoming_ts < existing_ts) return ENTRY_ID_SMALLER;
        return NONE;
    }     
}

XADD_ERR append_to_stream(StreamNode* target, char* ID, char* key, char* value, bool ignore_ts_check) {
    XADD_ERR verify_result = NONE;
    if (!target) {
        return SYSTEM_ERROR;
    }
    StreamNode* runner = target;
    while (runner->next != NULL) {
        runner = runner->next;
    }
    if (!ignore_ts_check) {
        verify_result = verify_entry_id(runner->ID, ID);
    }
    if (verify_result != NONE) {
        return verify_result;
    }
    StreamNode* new_node = make_stream_node(ID, key, value, runner->ID);
    if (!new_node) {
        return SYSTEM_ERROR;
    }
    runner->next = new_node;
    return NONE;
}

// only frees a single node
void free_node_ll(StreamNode* n) {
    free(n->ID);
    free(n->key);
    free(n->value);
    free(n);
}

XADD_ERR xadd_db(char* stream_name, char* ID, char* key, char* value, bool ignore_timestamp_check) {
    if (GS.streamDB == NULL) {
        StreamNode *s = make_stream_node(ID, key, value, NULL);
        if (!s) {
            __debug_printf(__LINE__, __FILE__, "make_stream_node failed\n");
            free(s);
            return SYSTEM_ERROR;
        }
        GS.streamDB = malloc(sizeof(StreamHead));
        GS.streamDB->next = NULL;
        GS.streamDB->stream_name = strdup(stream_name);
        GS.streamDB->node_ll = s;
        return NONE;
    } else {
        StreamHead* runner = GS.streamDB;
        StreamNode* new_node_ll;
        while (runner->next != NULL) {
            runner = runner->next;
        }
        if (!strcmp(stream_name, runner->stream_name)) {
            return append_to_stream(runner->node_ll, ID, key, value, ignore_timestamp_check);
        } else {
            // create a new stream head
            new_node_ll = make_stream_node(ID, key, value, NULL);
            if (!new_node_ll) {
                __debug_printf(__LINE__, __FILE__, "make_stream_node failed\n");
                return SYSTEM_ERROR;
            }
            StreamHead* new_head = malloc(sizeof(StreamHead));
            if (!new_head) {
                __debug_printf(__LINE__, __FILE__, "malloc failed: %s\n", strerror(errno));
                free_node_ll(new_node_ll);
            }
            new_head->next = NULL;
            runner->next = new_head;
            new_head->stream_name = strdup(stream_name);
            new_head->node_ll = new_node_ll;
            return NONE;
        }
    }
}

StreamHead* retrieve_stream(char* stream_name) {
    StreamHead* runner = GS.streamDB;
    while (runner != NULL) {
        if (!strcmp(stream_name, runner->stream_name)) {
            return runner;
        }
        runner = runner->next;
    }
    return NULL;
}

// > be me 
// > pack data for conciseness
// > facing the cost of packing data 
// > mfw
StreamNode* xrange(char* stream_name, char* start_ID, char* end_ID, int* length) {
    StreamHead* head = retrieve_stream(stream_name);
    if (!head) return NULL;
    long int start_node_ts, end_node_ts, start_id_ts, end_id_ts;
    int start_id_seq, end_id_seq, start_node_seq, end_node_seq;
    start_id_ts = get_timestamp_from_entry_id(start_ID);
    end_id_ts = get_timestamp_from_entry_id(end_ID);
    start_id_seq = get_sequence_number_from_entry_id(start_ID);
    end_id_seq = get_sequence_number_from_entry_id(end_ID);
    // find first node
    StreamNode* start_node = head->node_ll;
    while (start_node != NULL) {
        start_node_ts = get_timestamp_from_entry_id(start_node->ID);
        start_node_seq = get_sequence_number_from_entry_id(start_node->ID);
        if (start_node_ts > start_id_ts) break;
        if (start_node_ts < start_id_ts) {
            start_node = start_node->next;
        } else {
            if (start_id_seq == -1) break;
            if (start_id_seq < start_node_seq) {
                start_node = start_node->next
            } else {
                break;
            }
        }
    }
    if (start_node == NULL) {
        return NULL;
    }
    StreamNode* end_node = start_node;
    StreamNode* end_node_next = start_node->next;
    int counter = 1;
    while (end_node_next != NULL) {
        counter++;
        end_node_ts = get_timestamp_from_entry_id(end_node_next->ID);
        end_node_seq = get_sequence_number_from_entry_id(end_node->ID);
        if (end_node_ts > end_id_ts) break;
        if (end_node_ts < end_id_ts) {
            end_node = end_node_next;
            end_node_next = end_node_next->next;
        } else {
            if (end_id_seq == -1) {
                end_node = end_node_next;
                end_node_next = end_node_next->next;
            } else if (end_id_seq > end_node_seq) {
                break;
            } else if (end_id_seq <= end_node_seq) {
                end_node = end_node_next;
                end_node_next = end_node_next->next;
            }
        }
    }

    *length = counter;
    return start_node;
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
    pthread_mutex_lock(&db_lock);
    bool result = save_to_store(STORETYPE_DB, key, value, expiry);
    pthread_mutex_unlock(&db_lock);
    return result;
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

/* Command history */
bool add_to_command_history(char command[]) {
    if (GS.command_history == NULL) {
        GS.command_history = create_str_array(command);
        return true;
    }
    return (!append_to_str_array(&GS.command_history, command));
}

str_array* get_command_history() {
    return GS.command_history;
}

bool add_to_client_fds(int fd) {
    set_non_blocking(fd);
    int res = append_to_int_array(&(GS.client_fds), fd);
    if (res == 0) {
        return true;
    }
    return false;
}

int_array* get_connected_client_fds() {
    return GS.client_fds;
}

// replconf
bool add_replconf(size_t n) {
    GS.replconf += n;
    return true;
}

size_t get_replconf() {
    return GS.replconf;
}
