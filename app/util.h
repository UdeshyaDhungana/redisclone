#ifndef UTIL_H
#define UTIL_H

#include "debug.h"
#include <stdlib.h>
#include <sys/time.h>
#include <stdbool.h>
#include <string.h>

int split_string(char* source, char delim, char** left, char** right);
void free_ptr_to_char_ptr(char**);

long int get_epoch_ms();

long long choose_between_expiries(unsigned long long, unsigned int);

unsigned int copy_from_file_contents(char* dst, char* src, unsigned int offset, unsigned int num, bool null_terminate);
unsigned int copy_ms_from_file_contents(long long unsigned int* expiry_ms, char* file_contents, unsigned int offset);
unsigned int copy_seconds_from_file_contents(unsigned int* expiry_s, char* file_contents, unsigned int offset);


typedef struct str_array {
	char** array;
	unsigned int size;
} str_array;

str_array* create_str_array(const char* element);
int append_to_str_array(str_array** array, const char* element);
void free_str_array(str_array *s);
void print_str_array(str_array *s, char delim);
int parse_master_host_and_port(char* host_and_port_string, char** host, unsigned int *port);

bool match_str(char* pattern, char* string);

#endif