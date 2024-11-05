#include "parser.h"
#include <sys/types.h>
#include <assert.h>



// return a linked list, each node is a line
// caller should call free() on the returned pointer
char** split_input_lines(char* user_input) {
    const char* delim = "\r\n";
	char **lines = NULL;
	int line_count = 0;

	char *token = strtok(user_input, delim);
	while (token != NULL) {
		lines = realloc(lines, (line_count + 1) * sizeof(char *));
		if (lines == NULL) {
			printf("realloc : %s", strerror(errno));
			return NULL;
		}

		// allocate memory for each line and copy the token to it
		lines[line_count] = malloc(strlen(token) + 1);
		if (lines[line_count] == NULL) {
			printf("malloc : %s", strerror(errno));
			return NULL;
		}
		strcpy(lines[line_count], token);
		line_count++;
		token = strtok(NULL, delim);
	}
	lines = realloc(lines, (line_count + 1) * sizeof(char *));
	if (lines == NULL) {
		printf("realloc : %s", strerror(errno));
	}
	lines[line_count] = NULL;

	return lines;
}


ssize_t sizeof_ptr_array(char** p) {
	ssize_t count = 0;
	while (p[count] != NULL) {
		count++;
	}
	return count;
}

int check_syntax(char **lines) {
	if (lines[0][0] != '*') {
		return false;
	}
	int declared_elements = atoi(lines[0] + 1);
	int actual_elements = 0;
	int i = 1;

    while (lines[i] != NULL && lines[i + 1] != NULL) {
        // Parse length from the `$<length>` line
        if (lines[i][0] != '$') {
            fprintf(stderr, "Syntax error: Expected '$' at line %d.\n", i + 1);
            return 0;
        }

        int declared_length = atoi(&lines[i][1]);
        int actual_length = strlen(lines[i + 1]);

        if (declared_length != actual_length) {
            fprintf(stderr, "Length mismatch at line %d: declared %d, actual %d.\n", i + 1, declared_length, actual_length);
            return 0;
        }

        actual_elements++;
        i += 2;
    }

    if (declared_elements != actual_elements) {
        fprintf(stderr, "Element count mismatch: declared %d, found %d.\n", declared_elements, actual_elements);
        return 0;
    }

    return actual_elements;
}


char** command_extraction(char* lines[], int num_elements) {
	
	char** input = malloc((num_elements+1) * sizeof(char*));
	if (input == NULL) {
		printf("malloc: %s", strerror(errno));
	}


	for (int i = 0; i < num_elements; i += 1) {
		input[i] = malloc(strlen(lines[i*2 + 1]));
		if (input[i] == NULL) {
			printf("malloc() at %d on %s:  %s", __LINE__, __FILE__, strerror(errno));
		}

		strcpy(input[i], lines[i*2+1]);
    }
	input[num_elements] = NULL;
	
	return input;
}


char* to_resp_bulk_str(char* raw_reponse) {
    size_t length = strlen(raw_reponse);

    size_t resp_length = 1 + snprintf(NULL, 0, "%zu", length) + 2 + length + 2;
    char* resp_string = malloc(resp_length);
    if (resp_string == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    sprintf(resp_string, "$%zu\r\n%s\r\n", length, raw_reponse);

    return resp_string;	
}

char* to_resp_array(char* str_array[]) {
	assert(str_array != NULL);
	// assumes that the array is not null
	// 1. length of array
	// for each item
		// length of that item
		// item
	int arr_len = 0;
	char arr_len_str[11];
	int entry_len;
	char entry_len_str[11];
	int response_length = 0;
	char* runner;

	// response length calculation for malloc
	for (int i = 0; str_array[i] != NULL; i++) {
		runner = str_array[i];
		arr_len += 1;
		response_length += strlen(runner);					// eg. $3\r\ndir\r\n [this line accounts for dir]
		sprintf(entry_len_str, "%d", response_length);
		response_length += 1 + strlen(entry_len_str) + 2 + 2; // eg. accounts for first $, 3, \r\n twice
	}
	sprintf(arr_len_str, "%d", arr_len);
	response_length += 1 + strlen(arr_len_str) + 2;
	response_length += 1; // additional \0 to terminate

	char *response = malloc(response_length);
	response = 0;
	strcat(response, "*");
	strcat(response, arr_len_str);
	strcat(response, "\r\n");
	for (int i = 0; str_array[i] != NULL; i++) {
		runner = str_array[i];
		strcat(response, "$");
		entry_len = strlen(runner);
		sprintf(entry_len_str, "%d", entry_len);
		strcat(response, entry_len_str);
		strcat(response, "\r\n");
		strcat(response, runner);
		strcat(response, "\r\n");
	}
	return response;
}