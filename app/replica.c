#include "replica.h"
#include "responder.h"

bool extract_rdb_file(char* buffer, char* rdb_file) {
    int file_size;

    char* size_begin = strstr(buffer, "$");
    if (!size_begin) {
        return false;
    }
    char* size_end = strstr(size_begin, "\r\n");
    if (!size_end) {
        return false;
    }
    *size_end = 0;
    file_size = atoi(size_begin + 1);
    memcpy(rdb_file, size_end + 2, file_size);
    return true;
}

void extract_master_command(char* buffer, int* write_offset, int master_fd) {
    int arr_size;
    char arr_size_str[16];
    int command_size;
    char* command;
    int i;
    ssize_t remaning_length;
    while(1) {
        if (buffer[0] != '*') {
            return;
        }

        char* arr_len_start = buffer + 1;
        char* arr_len_end = strstr(buffer, "\r\n");
        for (i = 0; arr_len_start + i != arr_len_end; i++) {
            arr_size_str[i] = arr_len_start[i];
        }
        arr_size_str[i] = 0;
        arr_size = atoi(arr_size_str);
        
        char* delimeter = buffer;
        for (i = 0; i < (1 + 2 * arr_size); i++) {
            delimeter = strstr(delimeter, "\r\n");
            delimeter += 2;
        }
        delimeter -= 1;
        command_size = 1 + (delimeter - buffer) / (sizeof(char));

        command = malloc((command_size + 1)* sizeof(char));
        memcpy(command, buffer, command_size);
        command[command_size] = 0;
        handle_client_request(master_fd, command, true);
        remaning_length = strlen(delimeter + 1);
        memmove(buffer, delimeter + 1, strlen(delimeter + 1));
        memset(buffer + remaning_length, 0, BUFFER_LEN - remaning_length);
    }
}

void* process_master_command_thread(void *arg) {
    int master_fd = *(int*)arg;
    char buffer[BUFFER_LEN];
    char rdb_file[BUFFER_LEN];
    int write_offset = 0;
    ssize_t bytes_recvd;
    bool file_read = false;

    memset(buffer, 0, sizeof(buffer));
    // assuming file and commands aren't sent in a same buffer; i really need to learn network programming
    while (1) {
        bytes_recvd = recv(master_fd, buffer + write_offset, sizeof(buffer) - 1, 0);
        printf("received: %s\n", buffer);
        if (bytes_recvd > 0) {
            if (!file_read) {
                if (extract_rdb_file(buffer, rdb_file)) {
                    file_read = true;
                    write_offset = 0;
                    memset(buffer, 0, sizeof (buffer));
                } else {
                    write_offset += bytes_recvd;
                }
            } else {
                extract_master_command(buffer, &write_offset, master_fd);
            }
        } else if (bytes_recvd == 0) {
            printf("Master closed the connection\n");
            break;
        } else {
            perror("recv");
            break;
        }
    }
    close(master_fd);
    return NULL;
}

void communicate_with_master() {
    Node* master_host = retrieve_from_config(MASTER_HOST);
    Node* master_port = retrieve_from_config(MASTER_PORT);
    if (!master_host || !master_port) return;

    int master_fd;
    char master_address[16];
    struct sockaddr_in server_address;
    bool success;
    char ping_response[32];
    char ok_response[32];


    master_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (master_fd < 0) {
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
    if (connect(master_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        __debug_printf(__LINE__, __FILE__, "connecting to master failed: %s\n", strerror(errno));
        return;
    }

    str_array* ping = create_str_array("PING");
    char* ping_request = to_resp_array(ping);
    if (send(master_fd, ping_request, strlen(ping_request), 0) < 0) {
        __debug_printf(__LINE__, __FILE__, "failed to send message to server: %s\n", strerror(errno));
        return;
    }
    free(ping_request);
    free_str_array(ping);

    // receive ping from server
    memset(ping_response, 0, 32);
    int bytes_received = recv(master_fd, ping_response, 32, 0);
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
    if (send(master_fd, listening_port_request, strlen(listening_port_request), 0) < 0) {
        __debug_printf(__LINE__, __FILE__, "failed to send listening-port request: %s\n", strerror(errno));
        return;
    }
    free_str_array(listening_port);
    free(listening_port_request);

    // receive response
    memset(ok_response, 0, 32);
    bytes_received = recv(master_fd, ok_response, 32, 0);
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
    if (send(master_fd, capabilities_request, strlen(capabilities_request), 0) < 0) {
        __debug_printf(__LINE__, __FILE__, "failed to send capabilities request: %s\n", strerror(errno));
        return;
    }
    free_str_array(capabilities);
    free(capabilities_request);
    
    // ignore the response
    memset(ok_response, 0, 32);
    bytes_received = recv(master_fd, ok_response, 32, 0);
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
    if (send(master_fd, psync_request, strlen(psync_request), 0) < 0) {
        __debug_printf(__LINE__, __FILE__, "failed to send psync request: %s\n", strerror(errno));
        return;
    }
    free_str_array(psync);
    free(psync_request);

    // load the rdb and master's cached commands here 👇

    // create a separate thread to handle master's propagation
    pthread_t process_master_command_thread_id;
    if (pthread_create(&process_master_command_thread_id, NULL, process_master_command_thread, &master_fd) != 0) {
        __debug_printf(__LINE__, __FILE__, "thread creation failed: %s\n", strerror(errno));
    }
    pthread_detach(process_master_command_thread_id);
}