#ifndef UTIL_H
#define UTIL_H

#include "debug.h"
#include <stdlib.h>
#include <sys/time.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>

#define POLY 0xC96C5795D7870F42ULL // ECMA polynomial

int split_string(char* source, char delim, char** left, char** right);
void free_ptr_to_char_ptr(char**);

long int get_epoch_ms();

long long choose_between_expiries(unsigned long long, unsigned int);

int get_timestamp_from_entry_id(char* entry_id);
int get_sequence_number_from_entry_id(char* entry_id);

unsigned int copy_from_file_contents(char* dst, char* src, unsigned int offset, unsigned int num, bool null_terminate);
unsigned int copy_ms_from_file_contents(long long unsigned int* expiry_ms, char* file_contents, unsigned int offset);
unsigned int copy_seconds_from_file_contents(unsigned int* expiry_s, char* file_contents, unsigned int offset);

typedef struct str_array {
	char** array;
	unsigned int *size;
} str_array;

typedef struct int_array {
	int* array;
	unsigned int size;
} int_array;

typedef struct file_content {
	char* content;
	unsigned int size;
} file_content;

str_array* create_str_array(const char* element);
int append_to_str_array(str_array** array, const char* element);
void free_str_array(str_array *s);
void print_str_array(str_array *s, char delim);
str_array* dup_str_array(str_array *s);

int_array* create_int_array(int);
int append_to_int_array(int_array**, int);
ssize_t index_of_element(int_array* arr, int element);
void free_int_array(int_array*);


int parse_master_host_and_port(char* host_and_port_string, char** host, unsigned int *port);

bool match_str(char* pattern, char* string);

bool is_valid_ipv4(char* ip_address);
bool hostname_to_ip(char* hostname, char ip[16]);

file_content* read_entire_file(const char*);
void free_file_content(file_content*);

void hex_to_bytes(const char *hex_str, unsigned char **out_bytes, size_t *out_len);

void set_non_blocking(int);

#endif