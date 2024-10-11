#include "parser.h"

// return a linked list, each node is a line
// caller should call free() on the returned pointer
struct ll_node* decode_resp_bulk(char* encoded_req) {
	if (encoded_req[0] != '*') {
		return NULL;
	}
	// skip the first *
	char *fraction = encoded_req + 1 * sizeof(char);
	const char *delimeter = "\r\n";
	char* token = strstr(fraction, delimeter);

	ssize_t token_len;
	struct ll_node* last_ptr = NULL;
	struct ll_node* first_ptr = NULL;
	struct ll_node* node;
	while (token != NULL) {
		node = (struct ll_node*)malloc(sizeof(struct ll_node));
		if (node == NULL) {
			printf("malloc failed: %s \n", strerror(errno));
			deallocate_ll(first_ptr);
			return NULL;
		}
		token_len = (token - fraction) / sizeof(char);
		node->data = (char*)malloc(sizeof(char) * (token_len + 1));
		if (node->data == NULL) {
			printf("malloc failed: %s \n", strerror(errno));
			deallocate_ll(first_ptr);
			return NULL;
		}
		// can't do: should only store the string
		strncpy(node->data, fraction, token_len);
		node->data[token_len] = '\0';
		node->next = NULL;
		if (first_ptr == NULL) {
			first_ptr = node;
		} else {
			last_ptr->next = node;
		}
		last_ptr = node;
		fraction += 3 * sizeof(char);
		token = strstr(fraction, delimeter);
		printf("token is: -%s-\n", last_ptr->data);
		
	}
	return first_ptr;
}

void deallocate_ll(struct ll_node* node) {
	struct ll_node* temp;
	while (node != NULL) {
		temp = node;
		node = temp->next;

		free(temp->data);
		free(temp);
	}
}


void __print_ll(struct ll_node *head) {
	struct ll_node* temp;
	printf("[");
	while (head != NULL) {
		temp = head;
		head = head->next;
		printf("%s, ", temp->data);
	}
	printf("]\n");
}
