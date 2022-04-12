#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/stat.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


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

#define ROUNDUP_8(x)    (((x) + 255) >> 8)

#define DESC_LEN 80
#define TICK_LEN 5

#define UDP_MAX  65507

#define GET_EVENTS      1
#define GET_RESERVATION 3
#define GET_TICKETS     5

#define EVENTS          2
#define RESERVATION     4
#define TICKETS         6
#define BAD_REQUEST     255
#define ERR_MESS_ID     7

#define MESS_ID_OCT     1
#define DESC_LEN_OCT    1
#define TICK_COU_OCT    2
#define EVENT_ID_OCT    4
#define RES_ID_OCT      4
#define COOKIE_OCT      48
#define EXP_TIME_OCT    8
#define TICKET_OCT      7

#define EVENT_ID_MIN    0
#define EVENT_ID_MAX    999999
#define RES_ID_MIN      999999
#define COOKIE_ASCII_MIN    33
#define COOKIE_ASCII_MAX    126

typedef struct Event {
    char* description;
//    uint8_t text_length;
    uint8_t description_octets;
    uint16_t available_tickets;
} Event;

typedef struct Event_array {
    Event* arr;
    size_t len;
} Event_array;

typedef struct Client_message {
    uint8_t message_id;
    uint32_t event_id;
    uint16_t ticket_count;
    uint32_t reservation_id;
    char* cookie;
} Client_message;

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

//    single_event.text_length = read_length;

    single_event.available_tickets = 0;
    single_event.available_tickets |= num_tickets;

//    single_event.description_octets = 0;
//    single_event.description_octets |= ROUNDUP_8(read_length);
//    if ((read_length >> 8) == 0) {
//        single_event.description_octets = 1;
//    } else {
//        single_event.description_octets = 2;
//    }
    single_event.description_octets = read_length;

    return single_event;
}

void print_single_event(Event e) {
    printf("desc:\n%s\noctets: %d\ntickets: %d\n\n",
           e.description, e.description_octets, e.available_tickets);
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
    printf("ROUNDUP %d\n", ROUNDUP_8(65535));
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
        if (read_descr[length - 1] == '\n') read_descr[--length] = '\0';

        Event single_event = create_event(read_descr, atoi(read_ticket_num),
                                          length); //TODO length
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

void check_err_perror(bool statement, const char* mess) {
    if (!statement) {
        perror(mess);

        exit(1);
    }
}

int bind_socket(uint16_t port) {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0); // creating IPv4 UDP socket
//    ENSURE(socket_fd > 0);
    // after socket() call; we should close(sock) on any execution path;
    check_err_perror((socket_fd >= 0), "Determining socket file descriptor");

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
    server_address.sin_port = htons(port);

    // bind the socket to a concrete address
    int err = bind(socket_fd, (struct sockaddr *) &server_address,
                     (socklen_t) sizeof(server_address));
    check_err_perror((err == 0), "Socket bindng");

    return socket_fd;
}

size_t read_message(int socket_fd, struct sockaddr_in *client_address,
                    char *buffer, size_t max_length) {
    socklen_t address_length = (socklen_t) sizeof(*client_address);
    int flags = 0; // we do not request anything special
//    errno = 0;
    ssize_t len = recvfrom(socket_fd, buffer, max_length, flags,
                           (struct sockaddr *) client_address, &address_length);
//    if (len < 0) {
//        PRINT_ERRNO();
//    }
    check_err_perror((len >= 0), "Receiving a message from a client");

    return (size_t) len;
}

Client_message create_client_message(uint8_t message_id, uint32_t event_id,
                                     uint16_t ticket_count,
                                     uint32_t reservation_id, char* cookie) {
    Client_message filled;
    filled.message_id = message_id;
    filled.event_id = event_id;
    filled.ticket_count = ticket_count;
    filled.reservation_id = reservation_id;
    filled.cookie;

    return filled;
}

void print_client_message(Client_message clm) {
    printf("mess : %d | event: %d | tick_count: %d | res: %d | cookie:"
           " %s\n", clm.message_id, clm.event_id, clm.ticket_count, clm.reservation_id,
           clm.cookie);
}

uint32_t bitshift_to_retrieve_message(int begining, int end, char* message) {
    uint32_t result = 0;
    for (int i = begining; i < end; i++) {
        result |= ((uint32_t) message[i] << 8*(i - begining));
    }

    return result;
}

/*
 * uint32_t event_id = 0;
            for (int i = 1; i <= EVENT_ID_OCT; i++) {
                event_id |= ((uint32_t) message[i] << 8*(i - 1));
            }

            uint16_t ticket_count = 0;
            for (int i = 2 + EVENT_ID_OCT; i <= 1 + EVENT_ID_OCT + TICK_COU_OCT;
                i++) {
                ticket_count |= ((uint32_t))
            }
 */

Client_message interpret_client_message(char* message, int received_length,
                                        Event_array events) { //TODO check if reservation has been made
    Client_message error_message = create_client_message(ERR_MESS_ID, 0, 0, 0, "");
    if (received_length == 0) return error_message;

    Client_message full_data;
    uint32_t message_id_ntohl = ntohl(message[0]);
    uint8_t message_id = 0;

    if (message_id_ntohl != GET_EVENTS && message_id_ntohl != GET_RESERVATION
        && message_id_ntohl != GET_TICKETS) return error_message;

    switch (message_id_ntohl) {
        case GET_EVENTS:
            if (received_length > 1) return error_message;

            return create_client_message(message_id | message_id_ntohl, 0, 0,
                                         0, "");
        case GET_RESERVATION:
            if (received_length != (MESS_ID_OCT + EVENT_ID_OCT + TICK_COU_OCT))
                return error_message;

            uint32_t event_id = bitshift_to_retrieve_message(1,
                                                             EVENT_ID_OCT + 1,
                                                             message);
            if (event_id > events.len) {
                error_message.event_id = event_id;

                return error_message;
            }

            uint16_t tickets_count = 0;
            int ticket_begin = 1 + EVENT_ID_OCT;
            tickets_count |= bitshift_to_retrieve_message(ticket_begin,
                                                    ticket_begin + TICK_COU_OCT,
                                                    message);
            if (tickets_count == 0 || events.arr[event_id].available_tickets
                < tickets_count)
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
    Event_array read_events = read_process_save_file_content(filename);
    print_events(read_events);
    printf("Listening on port %d\n", port);

    char message_buffer[UDP_MAX + 1];
    memset(message_buffer, 0, sizeof(message_buffer));

    size_t read_length;

    uint16_t port_converted = 0;
    port_converted |= port;

    int socket_fd = bind_socket(port_converted);
    struct sockaddr_in client_address;

    do {
        read_length = read_message(socket_fd, &client_address,
                                   message_buffer, sizeof(message_buffer));
        char* client_ip = inet_ntoa(client_address.sin_addr);
        uint16_t client_port = ntohs(client_address.sin_port);
        printf("received %zd bytes from client %s:%u: '%.*s' |%d|\n",
               read_length, client_ip, client_port,
               (int) read_length, message_buffer, (int) message_buffer[0]); // note: we specify the length of the printed string

    } while (read_length > 0);

    delete_event_array(read_events.arr, read_events.len);

	return 0;
}
