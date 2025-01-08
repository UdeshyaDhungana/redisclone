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

// this program will be 
int get_timestamp_from_entry_id(char* entry_id) {
	int result;
	char* entry_id_dup = strdup(entry_id);
	char* dash = strstr(entry_id_dup, "-");
	*dash = '\0';
	result = atoi(entry_id_dup);
	free(entry_id_dup);
	return result;
}

int get_sequence_number_from_entry_id(char* entry_id) {
	char* dash = strstr(entry_id, "-");
	return atoi(dash + 1);
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
		result->size = malloc(sizeof(unsigned int));
		if (!result->size) {
			free(result);
			return NULL;
		}
		*(result->size) = 0;
		return result;
	} else {
		result_array = malloc(sizeof(char*));
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
	if (element != NULL) {
		result->size = malloc(sizeof (unsigned int));
	}
	*(result->size) = 1;
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
    if ((*array)->array == NULL && (*((*array)->size)) == 0) {
        *array = create_str_array(element);
        return 0;
    }

    char** new_array = (char**)realloc((*array)->array, (*((*array)->size) + 1) * sizeof(char *));
    if (!new_array) {
        return -1;
    }

    (*array)->array = new_array;
    (*array)->array[*((*array)->size)] = strdup(element);

    if (!((*array)->array[*((*array)->size)])) {
        return -1;
    }
    *((*array)->size) += 1;
    return 0;
}

void free_str_array(str_array *array) {
	if (array == NULL) {
		return;
	}
	if (array->array == NULL) {
		return;
	}
	for (int i = 0; i < *(array->size); i++) {
		free(array->array[i]);
	}
	free(array->array);
	free(array->size);
}

str_array* dup_str_array(str_array *s) {
	if (s == NULL) {
		return NULL;
	}
	str_array *new = create_str_array(NULL);
	*(new->size) = *(s->size);
	new->array = malloc(*(s->size) * sizeof(char*));
	for (int i = 0; i < *(s->size); i++) {
		new->array[i] = malloc((sizeof (char)) * (strlen(s->array[i]) + 1));
		strcpy(new->array[i], s->array[i]);
	}
	return new;
}

void print_str_array(str_array *s, char delim) {
	if (s == NULL || s->array == NULL) {
		__debug_printf(__LINE__, __FILE__, "NULL\n");
		return;
	}
	printf("Size: %d[ ", *(s->size));
	for (int i = 0; i < *(s->size); i++) {
		printf("%s,%c", s->array[i], delim);
	}
	printf("]\n");
}

int_array* create_int_array(int element) {
	int_array* res = malloc(1 * sizeof(int_array));
	if (res == NULL) {
		__debug_printf(__LINE__, __FILE__, "malloc failed: %s\n", strerror(errno));
		return NULL;
	}
	res->array = malloc(1 * sizeof(int));
	if (!res->array) {
		__debug_printf(__LINE__, __FILE__, "malloc failed: %s\n", strerror(errno));
		free(res);
		return NULL;
	}
	(res->array)[0] = element;
	(res->size) = 1;
	return res;
}

int append_to_int_array(int_array** arr, int element) {
	if (*arr == NULL) {
		*arr = create_int_array(element);
		if (arr != NULL) {
			return 0;
		}
		return -1;
	} else {
		(*arr)->array = (int*) realloc((*arr)->array, (1 + (*arr)->size) * sizeof(int));
		(*arr)->array[(*arr)->size] = element;
		(*arr)->size += 1;
	}
	return 0;
}

ssize_t index_of_element(int_array* arr, int element) {
	if (arr == NULL) {
		return -1;
	}
	for (int i = 0; i < arr->size; i++) {
		if (element == arr->array[i]) {
			return i;
		}
	}
	return -1;
}

void free_int_array(int_array* arr) {
	if (arr == NULL) {
		return;
	}
	free(arr->array);
	free(arr);
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

bool is_valid_ipv4(char* ip_address) {
	struct sockaddr_in sa;
	int result = inet_pton(AF_INET, ip_address, &(sa.sin_addr));
	return result != 0;
}

// only supports ipv4 at the moment
bool hostname_to_ip(char* hostname, char ip[16]) {
	// strcpy(ip, "127.0.0.1");
	struct addrinfo hints, *res, *p;
	int status;
	void *addr;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((status = getaddrinfo(hostname, NULL, &hints, &res)) != 0) {
		return false;
	}
	for (p = res; p != NULL; p = p->ai_next) {
		if (p->ai_family == AF_INET) {
			struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
			addr = &(ipv4->sin_addr);
			inet_ntop(p->ai_family, addr, ip, 16);
			break;
		}
	}
	freeaddrinfo(res);
	return true;
}

// you have to de-allocate the char ptr returned by this function
file_content* read_entire_file(const char *filename) {
    if (access(filename, F_OK & R_OK & W_OK) == -1) {
        __debug_printf(__LINE__, __FILE__, "RDB file does not exist at %s\n", filename);
        return NULL;
    }
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        __debug_printf(__LINE__, __FILE__, "%s\n", strerror(errno));
        return NULL;
    }
    struct stat file_stat;
    if (fstat(fd, &file_stat) < 0) {
        __debug_printf(__LINE__, __FILE__, "%s\n", strerror(errno));
        close(fd);
        return NULL;
    }
    size_t file_size = file_stat.st_size;
    char *content = malloc(file_size + 1);
    if (!content) {
        __debug_printf(__LINE__, __FILE__, "%s\n", strerror(errno));
        close(fd);
        return NULL;
    }
    ssize_t bytes_read = read(fd, content, file_size);
    if (bytes_read != file_size) {
        __debug_printf(__LINE__, __FILE__, "%s", "Could not read the entire file\n");
        close(fd);
        return NULL;
    }
    content[bytes_read] = 0x00;
    close(fd);
	file_content* f = malloc(sizeof(file_content));
	f->content = content;
	f->size = bytes_read;
    return f;
}

void free_file_content(file_content *f) {
	if (f == NULL) {
		return;
	}
	if (f->content == NULL) {
		return;
	}
	free(f->content);
	free(f);
}

void hex_to_bytes(const char *hex_str, unsigned char **out_bytes, size_t *out_len) {
    size_t hex_len = strlen(hex_str);
    *out_len = hex_len / 2; // Two hex characters = 1 byte
    *out_bytes = malloc(*out_len);
    if (*out_bytes == NULL) {
        perror("malloc failed");
        exit(1);
    }
    ssize_t i = 0;
    for (; i < *out_len; i++) {
        sscanf(&hex_str[i * 2], "%2hhx", &(*out_bytes)[i]); // Read 2 hex chars as a byte
    }
}

// makes a fd non blocking
void set_non_blocking(int sockfd) {
	int flags = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}