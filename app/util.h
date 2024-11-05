#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <sys/time.h>

void free_ptr_to_char_ptr(char**);

long int get_epoch_ms();

#endif