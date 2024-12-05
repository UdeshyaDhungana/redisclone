#ifndef UTIL_H
#define UTIL_H

#include "debug.h"
#include <stdlib.h>
#include <sys/time.h>
#include <stdbool.h>
#include <string.h>

void free_ptr_to_char_ptr(char**);

long int get_epoch_ms();

long long choose_between_expiries(unsigned long long, unsigned int);

unsigned int copy_from_file_contents(char* dst, char* src, unsigned int offset, unsigned int num, bool null_terminate);

typedef struct str_array {
	char** array;
	unsigned int size;
} str_array;

str_array* create_str_array(const char* element);
int append_to_str_array(str_array** array, const char* element);
void free_str_array(str_array *s);
void print_str_array(str_array *s, char delim);

bool match_str(char* pattern, char* string);

#endif