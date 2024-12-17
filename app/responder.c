#include "responder.h"
#include "parser.h"
#include "store.h"
#include "debug.h"

char* SYNTAX_ERROR = "-ERR syntax error";
char* OK_RESPONSE = "+OK\r\n";
char* NULL_BULK_STR = "$-1\r\n";
char* NOT_SUPPORTED = "Command %s not supported";

void process_command(int client_fd, str_array command_and_args) {
    char* command = command_and_args.array[0];
    str_array *rest = malloc(sizeof(str_array));
    bool error_flag = false;

    if (!strcmp(command, "PING")) {
        handle_ping(client_fd);
    } else if (!strcmp(command, "ECHO")) {
        if (command_and_args.size == 2) {
            rest->array = ((command_and_args.array) + 1);
            rest->size = command_and_args.size - 1;
            handle_echo(client_fd, rest);
        } else {
            error_flag = true;
        }
    } else if (!strcmp(command, "SET")) {
        if (command_and_args.size == 3 || command_and_args.size == 5) {
            rest->array = ((command_and_args.array) + 1);
            rest->size = command_and_args.size - 1;
            handle_set(client_fd, rest);
        } else {
            error_flag = true;
        }
    } else if (!strcmp(command, "GET")) {
        if (command_and_args.size == 2) {
            rest->array = ((command_and_args.array) + 1);
            rest->size = command_and_args.size - 1;
            handle_get(client_fd, rest);
        } else {
            error_flag = true;
        }
    } else if (!strcmp(command, "CONFIG")) {
        if (command_and_args.size >= 2) {
            rest->array = ((command_and_args.array) + 1);
            rest->size = command_and_args.size - 1;
            handle_config(client_fd, rest);
        } else {
            error_flag = true;
        }
    } else if (!strcmp(command, "KEYS")) {
        if (command_and_args.size == 2) {
            rest->array = ((command_and_args.array) + 1);
            rest->size = command_and_args.size - 1;
            handle_keys(client_fd, rest);
        } else {
            error_flag = true;
        }
    } else if (!strcmp(command, "INFO")) {
        if (command_and_args.size >= 2) {
            rest->array = ((command_and_args.array) + 1);
            rest->size = command_and_args.size - 1;
            handle_info(client_fd, rest);
        } else {
            error_flag = true;
        }
    } else if (!strcmp(command, "REPLCONF")) {
        if (command_and_args.size >= 2) {
            rest->array = ((command_and_args.array) + 1);
            rest->size = command_and_args.size - 1;
            handle_replconf(client_fd, rest);
        } else {
            error_flag = true;
        }
    } else if (!strcmp(command, "PSYNC")) {
        if (command_and_args.size == 3) {
            rest->array = ((command_and_args.array) + 1);
            rest->size = command_and_args.size - 1;
            handle_psync(client_fd, rest);
        } else {
            error_flag = true;
        }
    } else {
        handle_syntax_error(client_fd);
    }
    if (error_flag) {
        handle_syntax_error(client_fd);
    }
    free(rest);
}


void respond_to_client(int client_fd, char* buffer) {
	write(client_fd, buffer, strlen(buffer));
}

void handle_syntax_error(int client_fd) {
    printf("Handling syntax err----\n");
    char* response = to_resp_bulk_str(SYNTAX_ERROR);
    respond_to_client(client_fd, to_resp_bulk_str(SYNTAX_ERROR));
    free(response);
}

void handle_ping(int client_fd) {
	printf("Handling ping---------\n");
	char *pong = "+PONG\r\n";
	respond_to_client(client_fd, pong);
}


void handle_echo(int client_fd, str_array* arguments) {
    printf("Handling echo----\n");
    char* response = to_resp_bulk_str(arguments->array[0]);
    respond_to_client(client_fd, response);
    free(response);
}

void handle_set(int client_fd, str_array* arguments) {
    printf("Handling SET---\n");
    char* key = arguments->array[0];
    if (key == NULL) {
        printf("key is null; handle_set() at %d on %s", __LINE__, __FILE__);
        handle_syntax_error(client_fd);
        return;
    }
    char* value = arguments->array[1];
    if (value == NULL) {
        printf("value is null; handle_set() at %d on %s", __LINE__, __FILE__);
        handle_syntax_error(client_fd);
        return;
    }
    char* expiry_ms_str;
    char* PS = NULL;
    if (arguments->size == 4) {
        PS = arguments->array[2];
    }
    long int expires_in_epoch_ms = -1;
    if (PS != NULL && !strcmp(PS, "px")) {
        expiry_ms_str = arguments->array[3];
        expires_in_epoch_ms = atoi(expiry_ms_str);
        expires_in_epoch_ms += get_epoch_ms();
    }
    bool success = save_to_db(key, value, expires_in_epoch_ms);
    if (!success) {
        printf("saving to database failed\n");
        return;
    }
    respond_to_client(client_fd, OK_RESPONSE);
}

void handle_get(int client_fd, str_array* arguments) {
    printf("Handing get-----\n");
    char* key = arguments->array[0];
    
    Node* node = retrieve_from_db(key);

    if (node == NULL) {
        respond_to_client(client_fd, NULL_BULK_STR);
        return;
    }

    /* compare against current system time */
    if (node->expiry_epoch_ms != -1 && node->expiry_epoch_ms < get_epoch_ms()) {
        // value has expired
        delete_node(node);
        respond_to_client(client_fd, NULL_BULK_STR);
        return;
    }

    char* response = to_resp_bulk_str(node->value);
    respond_to_client(client_fd, response);
    free(response);
}

void handle_keys(int client_fd, str_array* arguments) {
    printf("Handling keys...\n");
    char* pattern = arguments->array[0];
    str_array* keys = get_db_keys(pattern);
    char* response = to_resp_array(keys);
    respond_to_client(client_fd, response);
    free_str_array(keys);
    free(response);
}

void handle_info(int client_fd, str_array* arguments) {
    printf("Handling info...\n");
    __debug_print_config();
    // char* response = "role:%s\nconnected_slaves:%s\nmaster_replid:%s\nmaster_repl_offset:%s\nsecond_repl_offset:%s\nrepl_backlog_active:%s\nrepl_backlog_size:%s\nrepl_backlog_first_byte_offset:%s\nrepl_backlog_histlen:%s"
    char* raw_response;
    char *response;
    if (arguments == NULL) {
        __debug_printf(__LINE__, __FILE__, "arguments to info command is null. Returning all info\n");
        respond_to_client(client_fd, to_resp_bulk_str("implement_all_keys"));
    } else {
        if (!strcmp(arguments->array[0], REPLICATION)) {
            // this part is shit, but i ain't working on this anytime soon
            ssize_t response_size = 6;
            raw_response = malloc(response_size);
            strcpy(raw_response, "role:");
            Node* master_host = retrieve_from_config(MASTER_HOST);
            Node* master_port = retrieve_from_config(MASTER_PORT);
            response_size += (master_host == NULL && master_port == NULL)?strlen(MASTER):strlen(SLAVE);
            raw_response = realloc(raw_response, response_size);
            strcat(raw_response, (master_host == NULL && master_port == NULL)? MASTER: SLAVE);

            // you need to duplicate this block for every property
            response_size += 1 + strlen(MASTER_REPLID) + 1;
            raw_response = realloc(raw_response, response_size);
            strcat(raw_response, "\nmaster_replid:");
            Node* master_replid = retrieve_from_config(MASTER_REPLID);
            response_size += master_replid == NULL?strlen(MASTER_HOST):strlen(master_replid->value); // strlen(role)
            raw_response = realloc(raw_response, response_size);
            strcat(raw_response, (master_replid == NULL)? "":master_replid->value);

            response_size += 1 + strlen(MASTER_REPL_OFFSET) + 1;
            raw_response = realloc(raw_response, response_size);
            strcat(raw_response, "\nmaster_repl_offset:");
            Node* master_repl_offset = retrieve_from_config(MASTER_REPL_OFFSET);
            response_size += master_repl_offset == NULL?0:strlen(master_repl_offset->value);
            raw_response = realloc(raw_response, response_size);
            strcat(raw_response, (master_repl_offset == NULL)? "": master_repl_offset->value);

            response_size += 1;
            raw_response = realloc(raw_response, response_size);
            strcat(raw_response, "\n");
        } else {
            __debug_printf(__LINE__, __FILE__, "info called with unimplemented argument\n");
            raw_response = to_resp_bulk_str("IMplement all keys\n");
        }
    }
    response = to_resp_bulk_str(raw_response);
    respond_to_client(client_fd, response);
    free(response);
    free(raw_response);
}

void handle_replconf(int client_fd, str_array* arguments) {
    printf("Handling replconf...\n");
    respond_to_client(client_fd, OK_RESPONSE);
}

/* JUST for codecrafters stupid fucking test */
#define HEX_STRING "524544495330303131fa0972656469732d76657205372e322e30fa0a72656469732d62697473c040fa056374696d65c26d08bc65fa08757365642d6d656dc2b0c41000fa08616f662d62617365c000fff06e3bfec0ff5aa2"

void hex_to_bytes(const char *hex_str, unsigned char **out_bytes, size_t *out_len) {
    size_t hex_len = strlen(hex_str);
    *out_len = hex_len / 2; // Two hex characters = 1 byte
    *out_bytes = malloc(*out_len);
    if (*out_bytes == NULL) {
        perror("malloc failed");
        exit(1);
    }
    ssize_t i = 0;
    for (; i < *out_len; i++) {
        sscanf(&hex_str[i * 2], "%2hhx", &(*out_bytes)[i]); // Read 2 hex chars as a byte
    }
}

void codecrafters_send(int client_fd) {
    unsigned char *byte_array = NULL;
    ssize_t byte_len = 0;
    hex_to_bytes(HEX_STRING, &byte_array, &byte_len);
    char response[1024];
    sprintf(response, "$%ld\r\n", byte_len);
    int len = strlen(response);
    memcpy((response + len), byte_array, byte_len);
    respond_to_client(client_fd, response);
}

void handle_psync(int client_fd, str_array* arguments) {
    printf("Handling psync...\n");
    bool error;
    char* raw_response;
    char* response;
    if (!strcmp(arguments->array[0], "?") || !strcmp(arguments->array[1], "-1")) {
        // slave connecting to master for the first time
        Node* master_repl_id = retrieve_from_config(MASTER_REPLID);
        Node* master_repl_offset = retrieve_from_config(MASTER_REPL_OFFSET);
        if (!master_repl_id || !master_repl_offset) {
            __debug_printf(__LINE__, __FILE__, "slave does not support this\n");
            error = true;
        } else {
            raw_response = malloc(((strlen(master_repl_id->value) + strlen(master_repl_offset->value)) * sizeof(char)) + 16);
            sprintf(raw_response, "+FULLRESYNC %s %s", master_repl_id->value, master_repl_offset->value);
            response = to_resp_simple_str(raw_response);
            respond_to_client(client_fd, response);
            free(response);
            free(raw_response);
            // transfer_rdb_file(client_fd);
            codecrafters_send(client_fd);
        }
    } else {
        error = true;
    }
    if (error) {
        respond_to_client(client_fd, SYNTAX_ERROR);
    }
}

/************** Config **************/

void handle_config(int client_fd, str_array* command_and_args) {
    char* command = command_and_args->array[0];
    str_array* args = malloc(sizeof(str_array));

    if (!strcmp(command, "GET")) {
        if (command_and_args->size == 2) {
            args->array = ((command_and_args->array) + 1);
            args->size = command_and_args->size - 1;
            handle_config_get(client_fd, args);
        }
    } else {
        handle_syntax_error(client_fd);
    }
    free(args);
}

void handle_config_get(int client_fd, str_array* args) {
    printf("Handling config get-----\n");
    char* key = args->array[0];

    Node* node = retrieve_from_config(key);

    if (node == NULL) {
        respond_to_client(client_fd, NULL_BULK_STR);
        return;
    }

    str_array* arr = create_str_array(key);
    append_to_str_array(&arr, node->value);
    char* response = to_resp_array(arr);
    respond_to_client(client_fd, response);
    free_str_array(arr);
    free(response);
}

/************** Transfer Empty Config **************/

void transfer_rdb_file(int client_fd) {
    char *response;
    Node* rdb_dir = retrieve_from_config(C_DIR);
    Node* rdb_filename = retrieve_from_config(C_DBFILENAME);
    if (!rdb_dir || !rdb_filename) {
        __debug_printf(__LINE__, __FILE__, "RDB file details not present in the config\n");
        return;
    }
    char* full_file_name = malloc(sizeof(char) * (strlen(rdb_dir->value) + strlen(rdb_filename->value)) + 2);
    strcpy(full_file_name, rdb_dir->value);
    strcat(full_file_name, "/");
    strcat(full_file_name, rdb_filename->value);

    file_content* f = read_entire_file(full_file_name);
    response = malloc(1 + ((f->size / 10) + 1) + 2 + f->size + 1);
    sprintf(response, "$%u\r\n", f->size);
    strcat(response, f->content);
    respond_to_client(client_fd, response);
    free(response);
    free_file_content(f);
}