#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <getopt.h>

#define OPTSTRING "f:pt"
#define PORT_MIN 0
#define PORT_MAX 65535
#define TIME_MIN 1
#define TIME_MAX 86400

#define PORT_DEF 2022
#define TIME_DEF 5

#define TIME_ARG "timeout"
#define PORT_ARG "port number"

#define TIME_OPT 't'
#define PORT_OPT 'p'
#define FILE_OPT 'f'

bool is_number(char* arg_opt) {
	int index = 0;
	while (arg_opt[index] != '\0') {
		if (!isdigit(arg_opt[index])) {

			return false;
		}
		index++;
	}

	return true;
}

void test_optional(char* arg_opt, char mode) {
	char[] output = mode == TIME_OPT ? TIME_ARG : PORT_ARG;

	if (!arg_opt) {
		printf("No argument passed to %s\n", output);

		exit(1);
	}

	if (!is_number(arg_opt)) {
		printf("%s should be passed as a numeric code\n", output);
                exit(1);
                }
	}
}

char* read_options(int argc, char* argv[], int* port_number, int* timeout) {
	int option;
	char* filename = NULL;

	while ((option = getopt(argc, argv, OPTSTRING)) != -1) {
		printf("READ: %c\n", option);
		switch (option) {
		case FILE_OPT:
			filename = optarg;

			break;
		case PORT_OPT:
			printf("|%s|\n", optarg);
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

			break;

		case TIME_OPT:
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

			break;

		default: /* ? */
			printf("Unrecognized argument or no parameter passed"
				"\nUsage: ./test_service -f filename\n");
			exit(1);
		}
	}

	return filename;
}


int main(int argc, char* argv[]) {
	if (argc == 1) {
		printf("Required arguments: -f filename\n");
		exit(1);
	}

	int timeout = TIME_DEF;
	int port = PORT_DEF;

	char* filename = read_options(argc, argv, &port, &timeout);
	printf("FILENAME %s\n", filename);
	return 0;
}
