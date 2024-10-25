#include "parser.h"


void __debug_print_hex(char* str, int count) {
	if (str == NULL) {
		printf("<NULL>");
	}
	for (int i = 0; i < count; i++) {
		printf("%02x ", (unsigned char)str[i]);
	}
	printf("00\n");
}

// return a linked list, each node is a line
// caller should call free() on the returned pointer
char** split_input_lines(char* user_input) {
	char **lines = NULL;
	int line_count = 0;

	char *token = strtok(user_input, "\n");
	while (token != NULL) {
		lines = realloc(lines, (line_count + 1) * sizeof(char *));
		if (lines == NULL) {
			printf("realloc failed: %s", strerror(errno));
			return NULL;
		}

		// allocate memory for each line and copy the token to it
		lines[line_count] = malloc(strlen(token) + 1);
		if (lines[line_count] == NULL) {
			printf("malloc failed: %s", strerror(errno));
			return NULL;
		}
		strcpy(lines[line_count], token);
		line_count++;
		token = strtok(NULL, "\n");
	}
	lines = realloc(lines, (line_count + 1) * sizeof(char *));
	if (lines == NULL) {
		printf("realloc failed: %s", strerror(errno));
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