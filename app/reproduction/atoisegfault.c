#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>

/* Lesson : Learnt the meaning of required_argument, no_argument and optional_argument */
#define PORT 12344

int main(int argc, char** argv) {
    int opt;
	int server_fd, client_fd, epoll_fd;
	int child_pid;
	int port = PORT;
    char* file = NULL;

	/* Argparse */
	static struct option long_options[] = {
		{"port", required_argument, NULL, 'a'},
		{"file", required_argument, NULL, 'b'},
		{"help", no_argument, NULL, 'c'},
		{NULL, 0, NULL, 0}
	};


	while ((opt = getopt_long(argc, argv, "pfh", long_options, NULL)) != -1) {
		switch (opt) {
			case 'a':
                printf("port is %s\n", optarg);
				break;
			case 'b':
				printf("file is: %s\n", optarg);
				break;
			case 'c':
				printf("help message\n");
				exit(0);
			default:
				break;
		}
	}

    printf("end\n");
}

