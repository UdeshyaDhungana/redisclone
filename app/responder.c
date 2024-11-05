#include "responder.h"
#include "parser.h"
#include "store.h"
#include "debug.h"
#include "util.h"

char* syntax_error = "-ERR syntax error";
char* OK_RESPONSE = "+OK\r\n";
char* NULL_BULK_STR = "$-1\r\n";
char* NOT_SUPPORTED = "Command %s not supported";

void process_command(int client_fd, char* command_and_args[]) {
    char* command = command_and_args[0];
    char** rest = command_and_args + 1; 

    if (!strcmp(command, "PING")) {
        handle_ping(client_fd);
    } else if (!strcmp(command, "ECHO")) {
        handle_echo(client_fd, rest);
    } else if (!strcmp(command, "SET")) {
        handle_set(client_fd, rest);
    } else if (!strcmp(command, "GET")) {
        handle_get(client_fd, rest);
    } else if (!strcmp(command, "CONFIG")) {
        handle_config(client_fd, rest);
    } else {
        handle_syntax_error(client_fd);
    }
}


void respond_to_client(int client_fd, char* buffer) {
	write(client_fd, buffer, strlen(buffer));
}

void handle_syntax_error(int client_fd) {
    printf("Handling syntax err----\n");
    char* response = to_resp_bulk_str(syntax_error);
    respond_to_client(client_fd, syntax_error);
    free(response);
}

void handle_ping(int client_fd) {
	printf("Handling ping---------\n");
	char *pong = "+PONG\r\n";
	respond_to_client(client_fd, pong);
}


void handle_echo(int client_fd, char* arguments[]) {
    printf("Handling echo----\n");
    int total_length = 0;
    for (int i = 0; i < total_length;i++) {
        // for space and \0 at the end
        total_length += 2 * strlen(arguments[i]);
    }
    total_length *= 2;
    
    char* raw_response = malloc(total_length);
    raw_response[0] = 0;

    strcat(raw_response, arguments[0]);
    for (int i = 1; i < total_length; i++) {
        strcat(raw_response, " ");
        strcat(raw_response, arguments[i]);
    }

    char* response = to_resp_bulk_str(raw_response);
    
    respond_to_client(client_fd, response);
    free(raw_response);
    free(response);
}

void handle_set(int client_fd, char* arguments[]) {
    printf("Handling SET---\n");
    char* key = arguments[0];
    if (key == NULL) {
        printf("key is null; handle_set() at %d on %s", __LINE__, __FILE__);
        handle_syntax_error(client_fd);
        return;
    }
    char* value = arguments[1];
    if (value == NULL) {
        printf("value is null; handle_set() at %d on %s", __LINE__, __FILE__);
        handle_syntax_error(client_fd);
        return;
    }
    char* PS = arguments[2];
    char* expiry_ms_str;
    long int expires_in_epoch_ms = -1;
    if (PS != NULL && !strcmp(PS, "px")) {
        expiry_ms_str = arguments[3];
        expires_in_epoch_ms = atoi(expiry_ms_str);
        expires_in_epoch_ms += get_epoch_ms();
    }
    bool success = save_to_db(key, value, expires_in_epoch_ms);
    __debug_print_DB();
    if (!success) {
        printf("saving to database failed\n");
        return;
    }
    respond_to_client(client_fd, OK_RESPONSE);
}

void handle_get(int client_fd, char* arguments[]) {
    printf("Handing get-----\n");
    char* key = arguments[0];
    
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


/************** Config **************/

void handle_config(int client_fd, char* command_and_args[]) {
    char* command = command_and_args[0];
    char** args = command_and_args + 1;

    if (!strcmp(command, "GET")) {
        handle_config_get(client_fd, args);
    } else {
        assert(0);
    }
}

void handle_config_get(int client_fd, char* args[]) {
    printf("Handling config get-----\n");
    char* key = args[0];

    Node* node = retrieve_from_config(key);

    if (node == NULL) {
        respond_to_client(client_fd, NULL_BULK_STR);
        return;
    }

    char* resp_array[] = { key, node->value, 0 };
    char* response = to_resp_array(resp_array);
    respond_to_client(client_fd, response);
    free(response);
}
