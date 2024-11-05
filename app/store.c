#include "store.h"
#include "debug.h"
#include <assert.h>

GlobalStore GS = {
    .DB = NULL,
    .config = NULL,
};

/* debug */

void __debug_print_Node(Node* n) {
    Node* runner = n;
    if (runner == NULL) {
        printf("<NIL>\n");
        return;
    }
    while (runner != NULL) {
        printf("%s = %s on %ld\t", runner->key, runner->value, runner->expiry_epoch_ms);
        runner = runner -> next;
    }
    printf("\n");
}

void __debug_print_DB() {
    __debug_print_Node(GS.DB);
}

void __debug_print_config() {
    __debug_print_Node(GS.config);
}
/* end debug */

/* Init */
int init_config(ConfigOptions* c) {
    int num_config = 0;

    if (c->dir != NULL) {
        save_to_config(C_DIR, c->dir, -1);
        num_config++;
    }
    if (c->dbfilename != NULL) {
        save_to_config(C_DBFILENAME, c->dbfilename, -1);
        num_config++;
    }
    return num_config;
}

/* Free config options */
void free_config(ConfigOptions* c) {
    if (c->dir != NULL) {
        free(c->dir);
    }
    if (c->dbfilename != NULL) {
        free(c->dbfilename);
    }
}


Node* make_node(char* key, char* value, long int expiry_ms) {
    Node* n = (Node*) malloc(sizeof(Node));
    if (n == NULL) {
        printf("malloc() at %d on %s:  %s", __LINE__, __FILE__, strerror(errno));
        return NULL;
    }
    n->key = malloc((1 + strlen(key)) * sizeof(char));
    if (n->key == NULL) {
        printf("malloc() at %d on %s:  %s", __LINE__, __FILE__, strerror(errno));
        free(n);
        return NULL;
    }
    n->value = malloc((1 + strlen(value)) * sizeof(char));
    if (n->value == NULL) {
        printf("malloc() at %d on %s:  %s", __LINE__, __FILE__, strerror(errno));
        free(n->key);
        free(n);
        return NULL;
    }
    n->expiry_epoch_ms = expiry_ms;
    n->next = NULL;
    strcpy(n->key, key);
    strcpy(n->value, value);
    return n;
}

void free_node(Node* n) {
    // free up the memory allocated for a node
    if (!n) {
        __printf("Can't free NULL node\n");
        return;
    }
    free(n->key);
    free(n->value);
    free(n);
}

void delete_node(Node* n) {
    if (!n || !GS.DB) {
        return;
    }
    Node* runner = GS.DB;
    if (n == runner) {
        GS.DB = runner->next;
        free_node(n);
        return;
    }
    while (runner->next != n) {
        runner = runner->next;
    }
    Node* to_delete = runner->next;
    runner->next = to_delete->next;
    free_node(to_delete);
}

bool save_to_store(StoreType st, char* key, char* value, long int expiry_ms) {
    bool result;
    Node** store_ptr;
    if (!store_ptr) {
        __printf("malloc(): %s\n", strerror(errno));
        return false;
    }
    Node* tmp;
    switch (st) {
        case STORETYPE_DB:
            store_ptr = &GS.DB;
            break;
        case STORETYPE_CONFIG:
            store_ptr = &GS.config;
            break;
        default:
            assert(false);
    } 
    if (*store_ptr == NULL) {
        tmp = make_node(key, value, expiry_ms);
        if (!tmp) {
            result = false;
        } else {
            *store_ptr = tmp;
            result = true;
        }
    } else {
        Node* runner = *store_ptr;
        while (runner->next != NULL) {
            runner = runner -> next;
        }
        Node* new_node = make_node(key, value, expiry_ms);
        runner->next = new_node;
        result = true;
    }
    return result;
}

Node* retrieve_from_store(StoreType st, char* key) {
    Node* store;
    switch(st) {
        case STORETYPE_DB:
            store = GS.DB;
            break;
        case STORETYPE_CONFIG:
            store = GS.config;
            break;
        default:
            assert(false);
    }
    char* result;
    Node* runner = store;
    while (runner != NULL) {
        if (!strcmp(key, runner->key)) {
            return runner;
        }
        runner = runner -> next;
    }
    return NULL;
}

bool save_to_db(char* key, char* value, long int expiry) {
    return save_to_store(STORETYPE_DB, key, value, expiry);
}

Node* retrieve_from_db(char* key) {
    return retrieve_from_store(STORETYPE_DB, key);
}

bool save_to_config(char* key, char* value, long int expiry) {
    return save_to_store(STORETYPE_CONFIG, key, value, -1);
}

Node* retrieve_from_config(char* key) {
    return retrieve_from_store(STORETYPE_CONFIG, key);
}