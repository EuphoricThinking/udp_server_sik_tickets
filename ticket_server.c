#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

#define OPTSTRING ":fpt"
#define PORT_MIN 0
#define PORT_MAX 65535
#define TIME_MIN 1
#define TIME_MAX 86400

char* read_options(int argc, char* argv, int* port_number, int* timeout) {
	int option;
	char* filename;

	while ((option = getopt(argc, argv, OPTSTRING)) != -1) {
		switch (option) {
		case 'f':
			filename = optarg;
		case 'p':
			if (!isdigit(optarg=
			*port_number = atoi(optarg);
			if (port_number < PORT_MIN || port_number > PORT_MAX) {
				printf("Port number should be included in "
					"range [%d, %d]\n", PORT_MIN, PORT_MAX);
				exit(1);
			}
		case 't':
			if (!isdigit(optarg[0])) {
				printf("Timeout should be passed as a number\n");
				exit(1);
			}

			*timeout = atoi(optarg);
			if (timeout == 1) {
				printf("Timeout should be passed as a number\n">
                                exit(1);
                        }

			if (timeout < TIME_MIN || timeout > TIME_MAX) {
				printf("Timeout should be a number specified"
					" in range [%d, %d]\n", TIME_MIN,
					TIME_MAX);
				exit(1);
