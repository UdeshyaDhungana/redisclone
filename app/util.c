#include "util.h"

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
