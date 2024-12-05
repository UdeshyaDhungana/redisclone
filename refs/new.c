#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct str_array {
	char** array;
	unsigned int size;
} str_array;


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

str_array* create_str_array(const char* element) {
	str_array* result = (str_array*)malloc(sizeof(str_array));
	char** result_array;
	if (element == NULL) {
		return result;
	} else {
		result_array = malloc(sizeof(char*) * 1);
		if (result_array == NULL) {
			printf("malloc failed on str array\n");
			free_str_array(result);
			return NULL;
		}
		result_array[0] = strdup(element);
		if (result_array[0] == NULL) {
			printf("malloc failed on str array\n");
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


int main() {
    char* a = "a";
    char* b = "b";
    char* c = "c";

	str_array *s = create_str_array(NULL);
	append_to_str_array(&s, a);
	append_to_str_array(&s, b);
	append_to_str_array(&s, c);

	printf("size: %d", s->size);

	for (int i = 0; i < s->size; i++) {
		printf("%s\n", s->array[i]);
	}

	free_str_array(s);
}