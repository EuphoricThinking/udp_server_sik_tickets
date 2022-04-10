#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/stat.h>

#define OPTSTRING "f:p:t:"
#define PORT_MIN 0
#define PORT_MAX 65535
#define TIME_MIN 1
#define TIME_MAX 86400

#define PORT_DEF 2022
#define TIME_DEF 5

#define TIME_ARG "timeout"
#define PORT_ARG "port number"
#define FILE_ARG "filename"

#define TIME_OPT 't'
#define PORT_OPT 'p'
#define FILE_OPT 'f'

bool is_number(char* arg_opt) {
	int index = arg_opt[0] == '-' ? 1 : 0;

	while (arg_opt[index] != '\0') {
		if (!isdigit(arg_opt[index])) {

			return false;
		}
		index++;
	}

	return true;
}

void test_whether_arg_is_passed(char* arg_opt, char* output) {
    if (!arg_opt) {
        printf("No argument passed to %s\n", output);

        exit(1);
    }
}

void test_optional_initial_correct(char* arg_opt, char mode) {
	char* output = mode == TIME_OPT ? TIME_ARG : PORT_ARG;

    test_whether_arg_is_passed(arg_opt, output);

	if (!is_number(arg_opt)) {
		printf("%s should be passed as a numeric code\n", output);
                exit(1);
	}
}

void test_optional_range(int* value, char mode) {
    char *output = mode == TIME_OPT ? TIME_ARG : PORT_ARG;
    int max_range = mode == TIME_OPT ? TIME_MAX : PORT_MAX;
    int min_range = mode == TIME_OPT ? TIME_MIN : PORT_MIN;

    if (*value < min_range || *value > max_range) {
        printf("%s should be included in "
               "range [%d, %d]\n", output, max_range, min_range);
        exit(1);
    }
}

char* read_options(int argc, char* argv[], int* port_number, int* timeout) {
	int option;
	char* filename = NULL;

	while ((option = getopt(argc, argv, OPTSTRING)) != -1) {
		printf("READ: %c\n", option);
		switch (option) {
		case FILE_OPT:
            test_whether_arg_is_passed(optarg, FILE_ARG);
			filename = optarg;

			break;
		case PORT_OPT:
            test_optional_initial_correct(optarg, PORT_OPT);

			*port_number = atoi(optarg);
            test_optional_range(port_number, PORT_OPT);

			break;

		case TIME_OPT:
            test_optional_initial_correct(optarg, TIME_OPT);

			*timeout = atoi(optarg);
            test_optional_range(timeout, TIME_OPT);

			break;

		default: /* ? */
			printf("Unrecognized argument or no parameter passed"
				"\nUsage: ./test_service -f filename\n"
                "Optional: -p port_number -t timeout\n");
			exit(1);
		}
	}

	return filename;
}

void check_if_file_exists(char* path) {
    struct stat buffer;
    if (!stat(path, &buffer)) {
        perror("Error with opening the file");

        exit(1);
    }
}

int main(int argc, char* argv[]) {
	if (argc == 1) {
		printf("Required arguments: -f filename\n"
               "Optional: -p port_number -t timeout\n");
		exit(1);
	}

	int timeout = TIME_DEF;
	int port = PORT_DEF;

	char* filename = read_options(argc, argv, &port, &timeout);
	printf("FILENAME %s\n", filename);
    check_if_file_exists(filename);

	return 0;
}
