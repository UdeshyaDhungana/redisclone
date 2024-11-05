#include "debug.h"

void __debug_printf(int line, char* file, const char *format, ...) {
	printf("At line %d in file %s: ", line, file);
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

void __debug_print_hex(char* str, int count) {
	if (str == NULL) {
		printf("<NULL>");
	}
	for (int i = 0; i < count; i++) {
		printf("%02x ", (unsigned char)str[i]);
	}
	printf("00\n");
}

void __debug_printf_strptr(char** ptr) {
	for (int i = 0; ptr[i] != NULL; i++) {
		printf("%s-\n",ptr[i]);
	}
}
