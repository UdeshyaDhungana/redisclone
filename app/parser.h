#ifndef PARSER_H
#define PARSER_H

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include "util.h"

// will work for most of the test cases
#define BUFFER_LEN 4096

str_array* split_input_lines(char* user_input);

ssize_t sizeof_ptr_array(char** p);

int check_syntax(str_array *);

str_array* command_extraction(str_array*, int num_lines);

char* to_resp_simple_str(char* raw_response);

char* to_resp_bulk_str(char* str);

char* to_resp_integer(int);

char* to_resp_array(str_array* array);

char* stream_node_to_resp_array(StreamNode* node, int length);

#endif