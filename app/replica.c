#include "store.h"
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "parser.h"

void communicate_with_master() {
    Node* master_host = retrieve_from_config(MASTER_HOST);
    Node* master_port = retrieve_from_config(MASTER_PORT);
    if (!master_host || !master_port) return;

    int sock;
    char master_address[16];
    struct sockaddr_in server_address;
    bool success;
    char ping_response[32];
    char ok_response[32];


    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
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
    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        __debug_printf(__LINE__, __FILE__, "connecting to master failed: %s\n", strerror(errno));
        return;
    }

    str_array* ping = create_str_array("PING");
    char* ping_request = to_resp_array(ping);
    if (send(sock, ping_request, strlen(ping_request), 0) < 0) {
        __debug_printf(__LINE__, __FILE__, "failed to send message to server: %s\n", strerror(errno));
        return;
    }
    free(ping_request);
    free_str_array(ping);

    // receive ping from server
    memset(ping_response, 0, 32);
    int bytes_received = recv(sock, ping_response, 32, 0);
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
    if (send(sock, listening_port_request, strlen(listening_port_request), 0) < 0) {
        __debug_printf(__LINE__, __FILE__, "failed to send listening-port request: %s\n", strerror(errno));
        return;
    }
    free_str_array(listening_port);
    free(listening_port_request);

    // receive response
    memset(ok_response, 0, 32);
    bytes_received = recv(sock, ok_response, 32, 0);
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
    if (send(sock, capabilities_request, strlen(capabilities_request), 0) < 0) {
        __debug_printf(__LINE__, __FILE__, "failed to send capabilities request: %s\n", strerror(errno));
        return;
    }
    free_str_array(capabilities);
    free(capabilities_request);
    
    // ignore the response
    memset(ok_response, 0, 32);
    bytes_received = recv(sock, ok_response, 32, 0);
    if (bytes_received < 0) {
        __debug_printf(__LINE__, __FILE__, "failed to receive listening-port response: %s\n", strerror(errno));
        return;
    } if (bytes_received == 0) {
        __debug_printf(__LINE__, __FILE__, "serve unexpectedly closed connection: %s\n");
        return;
    }

    // again ignore the response and send "PSYNC ? -1"
    str_array* psync = create_str_array("PSYNC");
    append_to_str_array(&psync, "?");
    append_to_str_array(&psync, "-1");
    char* psync_request = to_resp_array(psync);
    if (send(sock, psync_request, strlen(psync_request), 0) < 0) {
        __debug_printf(__LINE__, __FILE__, "failed to send psync request: %s\n", strerror(errno));
        return;
    }
    free_str_array(psync);
    free(psync_request);

    /* Remove on further implement */
    char buffer[4096];
    int recvsize = recv(sock, buffer, 4096, 0);

    for (int i = 0; i < recvsize; i++) {
        printf("%.2x\t", buffer[i]);
        if (i % 16 == 0) {
            printf("\n");
        }
    }
    /* end */

    close(sock);
}