#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

struct ll_node {
    char* data;
    struct ll_node* next;  // Pointer to the next node
};

struct ll_node* decode_resp_bulk(char*);

void encode_to_resp_bulk(char* source, char* destination);

void deallocate_ll(struct ll_node* node);

void __print_ll(struct ll_node*);
