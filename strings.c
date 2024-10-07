#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>


// returns the index at which the command is supposed to end
void extract_command(char* user_request, ssize_t request_length, char command[]) {
	const char *end = strpbrk(user_request, " \n");
	size_t length = (end != NULL) ? (end - user_request) : request_length;
	strncpy(command, user_request, length);
	command[length] = '\0';
}
