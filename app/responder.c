#include "responder.h"
#include "parser.h"
#include "store.h"
#include "debug.h"

char* syntax_error = "-ERR syntax error";
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
    char* response = to_resp_bulk_str(syntax_error);
    respond_to_client(client_fd, to_resp_bulk_str(syntax_error));
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
