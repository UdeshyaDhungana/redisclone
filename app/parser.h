#ifndef PARSER_H
#define PARSER_H

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>



char** split_input_lines(char* user_input);

ssize_t sizeof_ptr_array(char** p);

int check_syntax(char** lines);

char** command_extraction(char* lines[], int num_lines);

char* to_resp_bulk_str(char* str);
char* to_resp_array(char* str_array[]);

#endif