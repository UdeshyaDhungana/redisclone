#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include <stdarg.h>

void __debug_printf(int line, char* file, const char* format, ...);

void __debug_print_hex(char* str, int count);

void __debug_printf_strptr(char** ptr);


#endif