#include "replica.h"
#include "responder.h"

// responsible for parsing full resync command from master, clears the full_resync_command_from_buffer
// returns index of the end of designated section
ssize_t parse_full_resync(char* buffer, size_t content_size) {
    for (ssize_t i = 0; i < content_size; i++) {
        // find the beginning of rdb file stream
        if (buffer[i] == '$') {
            return i;
        }
    }
    return -1;
}

ssize_t parse_rdb_file(char* buffer, char* rdb_file, size_t write_offset) {
    int file_size;
    char* checksum_start;
    char* size_begin = strstr(buffer, "$");
    if (!size_begin) {
        return -1;
    }
    char* size_end = strstr(size_begin, "\r\n");
    if (!size_end) {
        return -1;
    }
    *size_end = 0;
    file_size = atoi(size_begin + 1);
    // check if the entire file is contained in the buffer
    if (((buffer + write_offset) - (size_end + 1)) / sizeof(char) < file_size) {
        return -1;
    }
    checksum_start = (size_end + 1 + file_size) - (8 * sizeof(char));
    if (!((*checksum_start) == CHECKSUM_START)) {
        return -1;
    }
    memcpy(rdb_file, size_end + 2, file_size);
    return (size_end + 2 + file_size - buffer);
}

// should return true if can parse and syntax also true
// should also fix the buffer content and remove the command from the buffer
char* parse_command(char* buffer) {
    // parse
    int i;
    int array_size;
    size_t command_size;
    char* command, *array_len_start, *array_len_end, *delimiter;
    char array_size_str[16];
    str_array* lines;
    size_t remaining_length;

    array_len_start = buffer + 1;
    array_len_end = strstr(buffer, "\r\n");
    if (array_len_end == NULL) {
        return NULL;
    }
    for (i = 0; array_len_start + i != array_len_end; i++) {
        array_size_str[i] = array_len_start[i];
    }
    array_size_str[i] = 0;
    array_size = atoi(array_size_str);

    delimiter = buffer;
    for (i = 0; i < (1 + 2 * array_size); i++) {
        delimiter = strstr(delimiter, "\r\n");
        if (delimiter == NULL) {
            return NULL;
        }
        delimiter += 2;
    }

    command_size = (delimiter - buffer) / (sizeof(char));
    command = malloc((command_size + 1) * sizeof(char));
    memcpy(command, buffer, command_size);
    command[command_size] = 0;

    lines = split_input_lines(command);
    printf("Command is: %s\n", command);
    if (!check_syntax(lines)) {
        free(command);
        free_str_array(lines);
        return NULL;
    }
    // fix the buffer
    remaining_length = strlen(delimiter);
    memmove(buffer, delimiter, remaining_length);
    memset(buffer + remaining_length, 0, command_size);
    // return command
    free_str_array(lines);
    return command;
}

void execute_master_command(char* command, int master_fd) {
    handle_client_request(master_fd, command, true);
    add_replconf(strlen(command));
} 

void process_master_command(char* buffer, int master_fd) {
    ssize_t recv_length = strlen(buffer);
    ssize_t write_offset = strlen(buffer);
    char* command;
    bool should_receive = (strlen(buffer) == 0);
    while (true) {
        if (should_receive) {
            recv_length = recv(master_fd, buffer + write_offset, BUFFER_LEN - 1 - write_offset, 0);
            if (recv_length < 1) {
                break;
            }
        }
        should_receive = true;
        command = parse_command(buffer);
        while (command != NULL) {
            execute_master_command(command, master_fd);
            free(command);
            command = parse_command(buffer);
        }
        write_offset = strlen(buffer);
    }
    if (recv_length == -1) {
        __debug_printf(__LINE__, __FILE__, "recv: %s\n", strerror(errno));
    } else {
        __debug_printf(__LINE__, __FILE__, "master closed connection :(\n");
    }

}

void* process_master_communication_thread(void* arg) {
    int master_fd = *((int*)arg);
    char buffer[BUFFER_LEN];
    // right now, i'm assuming that rdb file will not be more than 4096 bytes; if so, i'll prolly have to malloc some shit
    char rdb_file[BUFFER_LEN];
    int write_offset = 0;
    ssize_t bytes_recvd;
    bool file_parsed = false;
    bool full_resync_parsed = false;
    memset(buffer, 0, sizeof(buffer));
    ssize_t parse_fullresync_result;
    ssize_t parse_rdb_file_result;
    // assuming file and commands aren't sent in a same buffer; i really need to learn network programming
    while (!file_parsed || !full_resync_parsed) {
        bytes_recvd = recv(master_fd, buffer + write_offset, sizeof(buffer) - 1, 0);
        if (bytes_recvd > 0) {
            write_offset += bytes_recvd;
            if (!full_resync_parsed) {
                parse_fullresync_result = parse_full_resync(buffer, write_offset);
                if (parse_fullresync_result > -1) {
                    full_resync_parsed = true;
                    memmove(buffer, buffer + parse_fullresync_result, write_offset - parse_fullresync_result);
                    memset(buffer + (write_offset - parse_fullresync_result), 0, parse_fullresync_result);
                    write_offset -= parse_fullresync_result;
                } else continue;
            }
            if (!file_parsed) {
                parse_rdb_file_result = parse_rdb_file(buffer, rdb_file, write_offset);
                if (parse_rdb_file_result > -1) {
                    file_parsed = true;
                    memmove(buffer, buffer + parse_rdb_file_result, write_offset - parse_rdb_file_result);
                    memset(buffer + (write_offset - parse_rdb_file_result), 0, parse_rdb_file_result);
                } else continue;
            } 
        } else if (bytes_recvd == 0) {
            printf("Master closed the connection\n");
            close(master_fd);
            break;
        } else {
            perror("recv");
        }
    }
    process_master_command(buffer, master_fd);
    close(master_fd);
    return NULL;
}

void communicate_with_master() {
    Node* master_host = retrieve_from_config(MASTER_HOST);
    Node* master_port = retrieve_from_config(MASTER_PORT);
    if (!master_host || !master_port) return;

    int* master_fd = malloc(sizeof(int));
    char master_address[16];
    struct sockaddr_in server_address;
    bool success;
    char ping_response[32];
    char ok_response[32];


    *master_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*master_fd < 0) {
        __debug_printf(__LINE__, __FILE__, "failed to create socket for master: %s\n", strerror(errno));
        return;
    }

    server_address.sin_family = AF_INET;
    if (!is_valid_ipv4(master_host->value)) {
        // convert that to ip address using hostname lookup
        success = hostname_to_ip(master_host->value, master_address);
        if (!success) {
            __debug_printf(__LINE__, __FILE__, "could not determine master address: %s", strerror(errno));
            return;
        }
    } else {
        strcpy(master_address, master_host->value);
    }
    // inet_pton
    if (inet_pton(AF_INET, master_address, &server_address.sin_addr) <= 0) {
        __debug_printf(__LINE__, __FILE__, "could not convert ip address to number: %s\n", strerror(errno));
        return;
    }
    server_address.sin_port = htons(atoi(master_port->value));
    if (connect(*master_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        __debug_printf(__LINE__, __FILE__, "connecting to master failed: %s\n", strerror(errno));
        return;
    }

    str_array* ping = create_str_array("PING");
    char* ping_request = to_resp_array(ping);
    if (send(*master_fd, ping_request, strlen(ping_request), 0) < 0) {
        __debug_printf(__LINE__, __FILE__, "failed to send message to server: %s\n", strerror(errno));
        return;
    }
    free(ping_request);
    free_str_array(ping);

    // receive ping from server
    memset(ping_response, 0, 32);
    int bytes_received = recv(*master_fd, ping_response, 32, 0);
    if (bytes_received < 0) {
        __debug_printf(__LINE__, __FILE__, "failed to get master response: %s\n", strerror(errno));
        return;
    } else if (bytes_received == 0) {
        __debug_printf(__LINE__, __FILE__, "serve unexpectedly closed connection: %s\n");
        return;
    }


    // ignore the ping response for now
    Node* port = retrieve_from_config(PORT_LITERAL);
    // i miss the chain builder pattern here :(
    str_array* listening_port = create_str_array("REPLCONF");
    append_to_str_array(&listening_port, "listening-port");
    append_to_str_array(&listening_port, port->value);
    char* listening_port_request = to_resp_array(listening_port);
    if (send(*master_fd, listening_port_request, strlen(listening_port_request), 0) < 0) {
        __debug_printf(__LINE__, __FILE__, "failed to send listening-port request: %s\n", strerror(errno));
        return;
    }
    free_str_array(listening_port);
    free(listening_port_request);

    // receive response
    memset(ok_response, 0, 32);
    bytes_received = recv(*master_fd, ok_response, 32, 0);
    if (bytes_received < 0) {
        __debug_printf(__LINE__, __FILE__, "failed to receive listening-port response: %s\n", strerror(errno));
        return;
    } if (bytes_received == 0) {
        __debug_printf(__LINE__, __FILE__, "serve unexpectedly closed connection: %s\n");
        return;
    }

    // ignore the response

    str_array* capabilities = create_str_array("REPLCONF");
    append_to_str_array(&capabilities, "capa");
    append_to_str_array(&capabilities, "psync2");
    char* capabilities_request = to_resp_array(capabilities);
    if (send(*master_fd, capabilities_request, strlen(capabilities_request), 0) < 0) {
        __debug_printf(__LINE__, __FILE__, "failed to send capabilities request: %s\n", strerror(errno));
        return;
    }
    free_str_array(capabilities);
    free(capabilities_request);
    
    // ignore the response
    memset(ok_response, 0, 32);
    bytes_received = recv(*master_fd, ok_response, 32, 0);
    if (bytes_received < 0) {
        __debug_printf(__LINE__, __FILE__, "failed to receive capabilities response: %s\n", strerror(errno));
        return;
    } if (bytes_received == 0) {
        __debug_printf(__LINE__, __FILE__, "serve unexpectedly closed connection:\n");
        return;
    }

    // again ignore the response and send "PSYNC ? -1"
    str_array* psync = create_str_array("PSYNC");
    append_to_str_array(&psync, "?");
    append_to_str_array(&psync, "-1");
    char* psync_request = to_resp_array(psync);
    if (send(*master_fd, psync_request, strlen(psync_request), 0) < 0) {
        __debug_printf(__LINE__, __FILE__, "failed to send psync request: %s\n", strerror(errno));
        return;
    }
    free_str_array(psync);
    free(psync_request);

    // load the rdb and master's cached commands here 👇

    // create a separate thread to handle master's propagation
    pthread_t process_master_communication_thread_id;
    if (pthread_create(&process_master_communication_thread_id, NULL, process_master_communication_thread, master_fd) != 0) {
        __debug_printf(__LINE__, __FILE__, "thread creation failed: %s\n", strerror(errno));
    }

    pthread_detach(process_master_communication_thread_id);
}