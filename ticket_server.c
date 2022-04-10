#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <getopt.h>

#define OPTSTRING ":fpt"
#define PORT_MIN 0
#define PORT_MAX 65535
#define TIME_MIN 1
#define TIME_MAX 86400

#define PORT_DEF 2022
#define TIME_DEF 5

bool is_number(char* arg_opt) {
	int index = 0;
	while (arg_opt[index] != '\0') {
		if (!isdigit(arg_opt[index])) {

			return false;
		}
	}

	return true;
}

char* read_options(int argc, char* argv[], int* port_number, int* timeout) {
	int option;
	char* filename = NULL;

	while ((option = getopt(argc, argv, OPTSTRING)) != -1) {
		switch (option) {
		case 'f':
			filename = optarg;
		case 'p':
			if (!is_number(optarg)) {
				printf("Port number should be passed as a"
					" numeric code\n");
                                exit(1);
                        }

			*port_number = atoi(optarg);
			if (*port_number < PORT_MIN || *port_number > PORT_MAX) {
				printf("Port number should be included in "
					"range [%d, %d]\n", PORT_MIN, PORT_MAX);
				exit(1);
			}

		case 't':
			if (!is_number(optarg)) {
				printf("Timeout should be passed as a "
					"numeric code\n");
				exit(1);
			}

			*timeout = atoi(optarg);
			if (*timeout < TIME_MIN || *timeout > TIME_MAX) {
				printf("Timeout should be a number specified"
					" in range [%d, %d]\n", TIME_MIN,
					TIME_MAX);
				exit(1);
			}

		default: /* ? */
			printf("Unrecognized argument\n");
			exit(1);
		}
	}

	return filename;
}


int main(int argc, char* argv[]) {
	int timeout = TIME_DEF;
	int port = PORT_DEF;

	char* filename = read_options(argc, argv, &port, &timeout);

	return 0;
}
