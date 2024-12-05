#ifndef PARSER_H
#define PARSER_H

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include "util.h"



str_array* split_input_lines(char* user_input);

ssize_t sizeof_ptr_array(char** p);

int check_syntax(str_array *);

str_array* command_extraction(str_array*, int num_lines);

char* to_resp_bulk_str(char* str);

// deprecated
char* to_resp_array(str_array* array);

// use this one to convert str array to resp array

#endif