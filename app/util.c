#include "util.h"

int split_string(char* source, char delim, char** left, char** right) {
	char *delim_pos = strchr(source, delim);
    if (!delim_pos) {
        return -1;
    }
	
	size_t left_len = delim_pos - source;
	size_t right_len = strlen(delim_pos + 1);

	*left = (char *)malloc(left_len + 1);
    *right = (char *)malloc(right_len + 1);
    if (!*left || !*right) {
        free(*left);
        free(*right);
        return -1; // Memory allocation failed
    }

	strncpy(*left, source, left_len);
	(*left)[left_len] = 0;
	strcpy(*right, (delim_pos+1));
	return 0;
}

void free_ptr_to_char_ptr(char** arg) {
    for (int i = 0; arg[i] != NULL; i++) {
		free(arg[i]);
	}
	free(arg);
}

long int get_epoch_ms() {
	struct timeval tp;
	struct timezone tz;
	gettimeofday(&tp, &tz);
	return tp.tv_sec * 1000 + (tp.tv_usec / 1000);
}

long long choose_between_expiries(unsigned long long expiry_ms, unsigned int expiry_s) {
	if (expiry_ms == 0 && expiry_s == 0) {
		return -1;
	}
	if (expiry_ms == -1) {
		return expiry_s * 1000;
	}
	return expiry_ms;
}

/*
i came across a problem here
i had two choices either to let the caller pass an offset, or 
(since this function is called on only one src pointer (contents_of_file))
to maintain an static variable for offset. Weighing the pros and cons,

1. Passing offset
	advantages
		- The caller will know about the offset, will be useful in cases (e.g. in metadata section, the caller will need to seek the next 'fa' 
		  identifier and allocate the size accordingly)
		- can be used in other cases as well (though unlikely, but we can open the possibility)
	disadvantages
	- The caller should maintain an offset variable. may not be as clean

2. not passing offset
	advantage:
		- caller code looks clean
	disadvantages 
	- highly coupled code
	- caller cannot do any operations like lseek() (if you were here, this function was a replacement of a series of reads()s)

> eventually i decided to pass offset
*/
unsigned int copy_from_file_contents(char* dst, char* src, unsigned int offset, unsigned int num, bool null_terminate) {
	// as dumb as it can get
	char* result;
	unsigned int copied_count = 1;
	dst[0] = 0;
	char* start = src + offset;
	result = memcpy(dst, start, num);
	if (null_terminate) {
		dst[num] = 0;
		copied_count = num;
	}
	if (result != dst) {
		__debug_printf(__LINE__, __FILE__, "FATAL: copy_from_file_contents; something went terribly wrong\n");
	}
	return offset + copied_count;
}

// i had to make two extras for timestamps (seconds and milliseconds) -> we have to account for little endianness and shit like that so
//(&expiry_ms, file_contents, file_offset);
unsigned int copy_ms_from_file_contents(long long unsigned int* expiry_ms, char* file_contents, unsigned int offset) {
	for (int i = 0; i < 8; i++) {
        *expiry_ms |= (unsigned long)((unsigned char)file_contents[offset + i]) << (i * 8);
    }; 
    return offset + 8;
}

unsigned int copy_seconds_from_file_contents(unsigned int* expiry_s, char* file_contents, unsigned int offset) {
	for (int i = 0; i < 4; i++) {
        *expiry_s |= (unsigned long)((unsigned char)file_contents[i + offset]) << (i * 8);
    }
    return offset + 4;
}

// array of strings

str_array* create_str_array(const char* element) {
	str_array* result = (str_array*)malloc(sizeof(str_array));
	char** result_array;
	if (element == NULL) {
		result->array = NULL;
		result->size = 0;
		return result;
	} else {
		result_array = malloc(sizeof(char*) * 1);
		if (result_array == NULL) {
			__debug_printf(__LINE__, __FILE__, "malloc failed on str array\n");
			free_str_array(result);
			return NULL;
		}
		result_array[0] = strdup(element);
		if (result_array[0] == NULL) {
			__debug_printf(__LINE__, __FILE__, "malloc failed on str array\n");
			free(result_array);
			free(result);
			return NULL;
		}
	}
	result->array = result_array;
	result->size = 1;
	return result;
}

int append_to_str_array(str_array **array, const char* element) {
    if (element == NULL) {
        printf("trying to append null ptr to array\n");
        return -1;
    }
    if (array == NULL || *array == NULL) {
        printf("trying to append to NULL pointer\n");
        return -1;
    }
    if ((*array)->array == NULL && (*array)->size == 0) {
        *array = create_str_array(element);
        return 0;
    }

    char** new_array = (char**)realloc((*array)->array, (((*array)->size) + 1) * sizeof(char *));
    if (!new_array) {
        return -1;
    }

    (*array)->array = new_array;
    (*array)->array[(*array)->size] = strdup(element);

    if (!((*array)->array[(*array)->size])) {
        return -1;
    }
    (*array)->size += 1;
    return 0;
}

void free_str_array(str_array *array) {
	if (array == NULL) {
		return;
	}
	if (array->array == NULL) {
		return;
	}
	for (int i = 0; i < array->size; i++) {
		free(array->array[i]);
	}
	free(array->array);
}

void print_str_array(str_array *s, char delim) {
	if (s == NULL || s->array == NULL) {
		__debug_printf(__LINE__, __FILE__, "NULL\n");
		return;
	}
	printf("Size: %d[ ", s->size);
	for (int i = 0; i < s->size; i++) {
		printf("%s,%c", s->array[i], delim);
	}
	printf("]\n");
}

bool match_str(char* pattern, char* string) {
	return true;
}

int parse_master_host_and_port(char* host_and_port_string, char** host, unsigned int *port) {
	char* port_str;
	int success = split_string(host_and_port_string, ' ', host, &port_str);
	if (success == -1) {
		free(port_str);
		return -1;
	}
	// convert that to unsigned int
	*port = (unsigned int)atoi(port_str);
	if (*port > 65535) {
		free(port_str);
		return -1;
	}
	free(port_str);
	return 0;
}