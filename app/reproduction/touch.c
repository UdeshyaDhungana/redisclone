#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    char* foo = malloc(5);
    strcat(foo, "foo");
    printf("%s\n", foo);
    return 0;
}