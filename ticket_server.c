#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/stat.h>
#include <stdint.h>
#include <string.h>

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

#define ROUNDUP_8(x)    (((x) + 7) >> 3)

#define DESC_LEN 80
#define TICK_LEN 5

typedef struct Event {
    char* description;
    uint8_t text_length;
    uint8_t description_octets;
    uint16_t available_tickets;
} Event;

typedef struct Event_array {
    Event* arr;
    size_t len;
} Event_array;

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
Event create_event(char* description, int num_tickets, int read_length) {
    Event single_event;

    single_event.description = malloc(read_length + 1);
    single_event.description = strcpy(single_event.description, description);

    single_event.text_length = read_length;

    single_event.available_tickets = 0;
    single_event.available_tickets |= num_tickets;

    single_event.description_octets = 0;
    single_event.description_octets |= ROUNDUP_8(read_length);

    return single_event;
}

void print_single_event(Event e) {
    printf("desc:\n%s\ntext_length: %d\noctets: %d\ntickets: %d\n\n",
           e.description, e.text_length, e.description_octets, e.available_tickets);
}

void print_events(Event_array e) {
    for (size_t i = 0; i < e.len; i++) {
        print_single_event(e.arr[i]);
    }
}

void delete_event_array(Event* arr, size_t len) {
    for (size_t i = 0; i < len; i++) {
        free(arr[i].description);
//        arr[i].description = NULL;
    }

    free(arr);
}

Event_array read_process_save_file_content(char* path) {
//    struct stat buffer;
//    if (!stat(path, &buffer)) {
//        perror("Error with opening the file");
//
//        exit(1);
//    }
    printf("ROUNDUP %d\n", ROUNDUP_8(65));
    FILE* opened = fopen(path, "r");
    if (!opened) {
        perror(path);
    }

    char read_descr[DESC_LEN + 2];
    char read_ticket_num[TICK_LEN + 2];

    size_t max_index = 1;
    Event_array read_events;
    read_events.arr = malloc(sizeof(Event)*max_index);
    Event* temp = NULL;

    size_t index = 0;
    char* ret;
    while(fgets(read_descr, DESC_LEN + 1, opened)) {
        ret = fgets(read_ticket_num, TICK_LEN + 1, opened);
        if (!ret) {
            printf("Error while reading file\n");

            exit(1);
        }

        size_t length = strlen(read_descr);
        if (read_descr[length - 1] == '\n') read_descr[length - 1] = '\0';

        Event single_event = create_event(read_descr, atoi(read_ticket_num),
                                          length);
        read_events.arr[index++] = single_event;

        if (index >= max_index) {
            max_index *= 2;
            temp = realloc(read_events.arr, max_index* sizeof(Event));
            if (!temp) {
                printf("Internal allocation memory error\n");
                delete_event_array(read_events.arr, index);

                exit(1);
            }
            else {
                read_events.arr = temp;
            }
        }
    }

    read_events.len = index;
    fclose(opened);

    return read_events;
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
    Event_array read_events = read_process_save_file_content(filename);
    print_events(read_events);
    delete_event_array(read_events.arr, read_events.len);

	return 0;
}
