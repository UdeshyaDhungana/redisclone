#include "parser.h"
#include "debug.h"
#include <sys/types.h>
#include <assert.h>
#include "util.h"
#include "store.h"



// return a linked list, each node is a line
// caller should call free() on the returned pointer
str_array* split_input_lines(char* user_input) {
	// so we don't modify user_input
	char* user_input_copy = malloc(1 + (strlen(user_input) * sizeof(char)));
	strcpy(user_input_copy, user_input);
    const char* delim = "\r\n";
	char **lines = NULL;
	int line_count = 0;

	char *token = strtok(user_input_copy, delim);
	while (token != NULL) {
		lines = realloc(lines, (line_count + 1) * sizeof(char *));
		if (lines == NULL) {
			printf("realloc : %s", strerror(errno));
			free(user_input_copy);
			return NULL;
		}

		// allocate memory for each line and copy the token to it
		lines[line_count] = malloc(strlen(token) + 1);
		if (lines[line_count] == NULL) {
			printf("malloc : %s", strerror(errno));
			free(user_input_copy);
			return NULL;
		}
		strcpy(lines[line_count], token);
		line_count++;
		token = strtok(NULL, delim);
	}
	str_array *s = create_str_array(NULL);
	s->array = lines;
	*(s->size) = line_count;
	free(user_input_copy);
	return s;
}


ssize_t sizeof_ptr_array(char** p) {
	ssize_t count = 0;
	while (p[count] != NULL) {
		count++;
	}
	return count;
}

int check_syntax(str_array *s) {
	if (s == NULL || s->array == NULL) {
		fprintf(stderr, "Syntax error: NULL lines\n");
		return 0;
	}
	char** lines = s->array;
	if (lines[0][0] != '*') {
		return false;
	}
	int declared_elements = atoi(lines[0] + 1);
	int actual_elements = 0;
	int i = 1;
    for (;i < (*(s->size) - 1); i += 2) {
        // Parse length from the `$<length>` line
        if (lines[i][0] != '$') {
            fprintf(stderr, "Syntax error: Expected '$' at line %d.\n", i + 1);
            return 0;
        }

        int declared_length = atoi(&lines[i][1]);
        int actual_length = strlen(lines[i+1]);

        if (declared_length != actual_length) {
            fprintf(stderr, "Length mismatch at line %d: declared %d, actual %d.\n", i + 1, declared_length, actual_length);
            return 0;
        }
    }
	actual_elements = (i - 1) / 2;

    if (declared_elements != actual_elements) {
        fprintf(stderr, "Element count mismatch: declared %d, found %d.\n", declared_elements, actual_elements);
        return 0;
    }

    return actual_elements;
}


str_array* command_extraction(str_array *s_lines, int num_elements) {
	str_array* array = create_str_array(NULL);
	char** lines = s_lines->array;
	int status;
	for (int i = 0; i < num_elements; i += 1) {
		status = append_to_str_array(&array, lines[i*2+2]);
		if (status == -1) {
			__debug_printf(__LINE__, __FILE__, "failed to append to str array\n");
		}
    }
	return array;
}

// we will need to add a 'type' parameter if we intend to include all resp data types
char* to_resp_simple_str(char* raw_response) {
	// simple string cannot contain \r\n
	char *result;
	if (strstr(raw_response, "\r\n")) {
		return NULL;
	} else {
		result = malloc(strlen(raw_response) + 4);
		sprintf(result, "+%s\r\n", raw_response);
	}
	return result;
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

char* to_resp_integer(int n) {
	char* result = malloc(((n / 10) + 1) * sizeof(char));
	if (result == NULL) {
		__debug_printf(__LINE__, __FILE__, "malloc failed: %s\n", strerror(errno));
		return NULL;
	}
	sprintf(result, ":%d\r\n", n);
	return result;
}

char* to_resp_array(str_array* array) {
	if (array == NULL) {
		__debug_printf(__LINE__, __FILE__, "WARN: null pointer passed to to_resp_array\n");
		return NULL;
	}
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
	for (int i = 0; i < *(array->size) ; i++) {
		runner = array->array[i];
		arr_len += 1;
		response_length += strlen(runner);					// eg. $3\r\ndir\r\n [this line accounts for dir]
		sprintf(entry_len_str, "%d", response_length);
		response_length += 1 + strlen(entry_len_str) + 2 + 2; // eg. accounts for first $, 3, \r\n twice
	}

	sprintf(arr_len_str, "%d", arr_len);
	response_length += 1 + strlen(arr_len_str) + 2;
	response_length += 1; // additional \0 to terminate

	char *response = malloc(response_length);
	if (response == NULL) {
		__debug_printf(__LINE__, __FILE__, "malloc(): %s\n", strerror(errno));
		return NULL;
	}
	response[0] = '\0';
	strcat(response, "*");
	strcat(response, arr_len_str);
	strcat(response, "\r\n");
	for (int i = 0; i < *(array->size) ; i++) {
		runner = array->array[i];
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


char* stream_node_with_same_id_to_resp_array(StreamNode* start_node) {
	char* response = (char*)malloc(2048);
	memset(response, 0, 2048);
	// there are always 2 elements in an array, first ID, second, an array with key value pairs
	strcat(response, "*2\r\n");
	
	// insert ID
	char* ID_resp_bulk_str = to_resp_bulk_str(start_node->ID);
	strcat(response, ID_resp_bulk_str);
	free(ID_resp_bulk_str);


	StreamNode* runnerNode = start_node;
	int num_pairs = 0;
	while ((runnerNode != NULL) && !strcmp(runnerNode->ID, start_node->ID)) {
		num_pairs++;
		runnerNode = runnerNode->next;
	}

	char num_pairs_str[16];
	sprintf(num_pairs_str, "*%d\r\n", (2 * num_pairs));
	strcat(response, num_pairs_str);

	char* temp;
	runnerNode = start_node;
	// invariant: num_pairs is always even, see the line containing sprintf
	for (int i = 0; i < num_pairs; i++) {
		temp = to_resp_bulk_str(runnerNode->key);
		strcat(response, temp);
		free(temp);
		temp = to_resp_bulk_str(runnerNode->value);
		strcat(response, temp);
		free(temp);
		runnerNode = runnerNode->next;
	}
	return response;
}

// if circular include is reached, you may move this function to store.c
char* stream_node_to_resp_array(StreamNode* start_node, int length) {
	if (start_node == NULL) return NULL;
	char value_to_compare[16];
	strcpy(value_to_compare, "unmatchable");
	int num_uniq_ids = 0;
	char num_uniq_ids_placeholder[16];
	StreamNode* runner = start_node;
	for (int i = 0; i < length; i++) {
		if (strcmp(runner->ID, value_to_compare)) {
			num_uniq_ids += 1;
			strcpy(value_to_compare, runner->ID);
		}
		runner = runner->next;
	}
	sprintf(num_uniq_ids_placeholder, "*%d\r\n", num_uniq_ids);
	char* response = malloc(4096);
	memset(response, 0, 4096);
	strcpy(response, num_uniq_ids_placeholder);

	strcpy(value_to_compare, "unmatchable");
	char* individual_response;
	runner = start_node;
	for (int i = 0; i < length; i++) {
		if (strcmp(runner->ID, value_to_compare)) {
			individual_response = stream_node_with_same_id_to_resp_array(runner);
			strcat(response, individual_response);
			free(individual_response);
			strcpy(value_to_compare, runner->ID);
		}
		runner = runner->next;
	}
	return response;
}