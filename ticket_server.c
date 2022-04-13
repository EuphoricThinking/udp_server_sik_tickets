//#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#include <endian.h>

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
#include <time.h>

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
#define ERR_MESS_ID     0

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

#define ERR_EVENTS_TICK 1
#define ERR_RES         2

#define RES_IND_BIAS    1000000

#define DIGIT_ASCII_START   (uint8_t)48
#define LETTER_ASCII_START  (uint8_t)55 //subtracted bias
#define BASE_TICK_CONVERT   36

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

typedef struct Reservation {
    uint16_t ticket_count;
    uint64_t* ticket_ids;
    time_t expiration;
    bool has_been_completed;
    char* cookie;
    uint32_t event_id;
} Reservation;

typedef struct Reservation_array {
    Reservation* arr;
    size_t last_index;
    size_t size;
} Reservation_array;

typedef struct Client_message {
    uint8_t message_id;
    uint32_t event_id;
    uint16_t ticket_count;
    uint32_t reservation_id;
} Client_message;


/*
 * Queue
 */

typedef struct List {
    uint32_t val;

    struct List* next;
} List;

typedef struct Queue {
    List* first;
    List* last;

    int num_elements;
} Queue;

List* init_node(size_t val) {
    List* l = malloc(sizeof(List));

    l->next = NULL;
    l->val = val;

    return l;
}

void delete_node(List* l) {
    l->next = NULL;
    free(l);
}

void delete_nodes_all(List* l) {
    if (l) {
        List *child = l->next;
        List *current = l;

        while (current) {
            delete_node(current);
            current = child;
            if (child) {
                child = current->next;
            }
        }
    }
}

Queue* init_queue() {
    Queue* q = malloc(sizeof(Queue));
    q->first = NULL;
    q->last = NULL;
    q->num_elements = 0;

    return q;
}

void push(Queue* q, size_t to_insert_val) {
    if (q->num_elements == 0) {
        q->first = init_node(to_insert_val);
        q->last = q->first;
        q->num_elements++;
    }
    else {
        List* to_append = init_node(to_insert_val);
        q->last->next = to_append;
        q->last = to_append;
        q->num_elements++;
    }
}

List* top(Queue* q) {
    if (q->num_elements == 0) {
        return NULL;
    }
    else {
        return q->first;
    }
}

List* pop(Queue* q) {
    if (q->num_elements > 0) {
        List *to_return = q->first;
        List *second = q->first->next;
        q->first = second;
        q->num_elements--;

        return to_return;
    }
    else {
        return NULL;
    }
}

void delete_queue(Queue* q) {
    delete_nodes_all(q->first);
    free(q);
}

bool is_empty(Queue* q) {
    return q->num_elements == 0;
}


/*
 * End of queue
 */




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

			*timeout = atoll(optarg);
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
                                     uint32_t reservation_id) {
    Client_message filled;
    filled.message_id = message_id;
    filled.event_id = event_id;
    filled.ticket_count = ticket_count;
    filled.reservation_id = reservation_id;

    return filled;
}

Reservation create_reservation(uint16_t ticket_count, uint64_t* ticket_ids,
                               time_t expiration, char* cookie, uint32_t event_id) {
    Reservation initial_reservation;
    initial_reservation.ticket_count = ticket_count;
    initial_reservation.ticket_ids = ticket_ids;
    initial_reservation.expiration = expiration;
    initial_reservation.has_been_completed = false;
    initial_reservation.cookie = cookie;
    initial_reservation.event_id = event_id;

    return initial_reservation;
}

void delete_reservations(Reservation* arr, size_t len) {
    for (size_t i = 0; i < len; i++) {
        free(arr[i].cookie);
        free(arr[i].ticket_ids);
    }

    free(arr);
}

Reservation_array create_res_array() {
    Reservation_array result;
    result.arr = NULL;
    result.last_index = 0;
    result.size = 0;

    return result;
}

void insert_new_reservation(Reservation_array* reservations, uint16_t ticket_count,
                            uint64_t* ticket_ids, time_t expiration, char* cookie,
                            uint32_t event_id) {
    if (reservations->last_index >= reservations->size) {
        size_t new_size = (reservations->size)*2 + sizeof(Reservation);
        Reservation* new_array = realloc(reservations->arr, new_size);
        check_err_perror((new_array != NULL), "Error in extending reservations array");

        reservations->arr = new_array;
        reservations->size = new_size;
    }

    Reservation new_reservation = create_reservation(ticket_count, ticket_ids,
                                                     expiration, cookie,
                                                     event_id);
    reservations->arr[(reservations->last_index)++] = new_reservation;
}

char* generate_cookie() {
    char* sweet_cookie = malloc(COOKIE_OCT);

    for (int i = 0; i < COOKIE_OCT; i++) {
        sweet_cookie[i] = (char)(rand()%(COOKIE_ASCII_MAX + 1 - COOKIE_ASCII_MIN)
                            + COOKIE_ASCII_MIN);
    }

    return sweet_cookie;
}

void print_cookies(char* cookies) {
    for (int i = 0; i < COOKIE_OCT; i++) {
        printf("%c", cookies[i]);
    }

    printf("\n");
}

void print_client_message(Client_message clm) {
    printf("mess : %d | event: %d | tick_count: %d | res: %d | cookie: "
           , clm.message_id, clm.event_id, clm.ticket_count, clm.reservation_id);
//           clm.cookie);
//    if (if_cookies) print_cookies(clm.cookie);
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
void send_message(int socket_fd, const struct sockaddr_in *client_address, const char *message, size_t length) {
    socklen_t address_length = (socklen_t) sizeof(*client_address);
    int flags = 0;
    ssize_t sent_length = sendto(socket_fd, message, length, flags,
                                 (struct sockaddr *) client_address, address_length);
    check_err_perror((sent_length == (ssize_t) length),
                     "Error while sending from server");
}

void fill_buffer(int start, int end, char* buffer, uint32_t converted_htonl) {
    uint8_t to_be_written = 0;

    for (int i = start; i < end; i++) {
        to_be_written |= converted_htonl >> 8*(i - start);
        buffer[i] = to_be_written;
        to_be_written = 0;
    }
}

void free_expired(Event_array* events, Queue* expiring, const Reservation_array *reservs) {
    time_t expiration;
    List* reservation;
    while (!is_empty(expiring)) {
        expiration = reservs->arr[top(expiring)->val].expiration;

        if (expiration >= time(NULL)) {
            break;
        }
        else {
            reservation = pop(expiring);
            Reservation cancelled = reservs->arr[reservation->val];

            if (!cancelled.has_been_completed) {
                Event* event_to_add_tickets = &(events->arr[cancelled.event_id]);
                event_to_add_tickets->available_tickets += cancelled.ticket_count;
            }

            delete_node(reservation);
        }
    }
}

void overwrite_buffer(char* cookie, char* message, int limit) {
    for (int i = 0; i < limit; i++) {
        message[i] = cookie[i];
    }
}

uint8_t determine_char(uint8_t byte) {
    if (byte < 10) {
        return byte + DIGIT_ASCII_START;
    }
    else {
        return byte + LETTER_ASCII_START;
    }
}

void fill_buffer_with_a_single_ticket(char* buffer, uint8_t ticket[]) {
    for (int i = 0; i < TICKET_OCT; i++ ){
        buffer[i] = determine_char(ticket[i]);
    }
}

void resolve_a_single_ticket(uint8_t storage[], uint64_t single_ticket) {
    int index = TICKET_OCT - 1;
    while (index >= 0) {
        if (single_ticket != 0) {
            storage[index--] = (uint8_t) (single_ticket % BASE_TICK_CONVERT);
            single_ticket /= BASE_TICK_CONVERT;
        }
        else {
            storage[index--] = single_ticket;
        }
    }
}

void fill_buffer_with_tickets(uint16_t ticket_count, uint64_t* tickets,
                              char* buffer) {
    uint8_t single_ticket[TICKET_OCT];
    char* current_pointer = buffer;
    for (uint16_t i = 0; i < ticket_count; i++) {
        resolve_a_single_ticket(single_ticket, tickets[i]);
        fill_buffer_with_a_single_ticket(current_pointer, single_ticket);
        current_pointer += TICKET_OCT;
    }
}

void handle_client_message(Client_message from_client, char* message,
                           int socket_fd,
                           const struct sockaddr_in *client_address,
                           Reservation_array* reservs, time_t timeout,
                           uint64_t* ticket_seed, Event_array* events,
                           Queue* expiring) {
    size_t length_to_send = 0;

    bool to_be_ignored = false;
    char* cookie;
    Reservation* requested_reservation;
    int left_space;

    printf("ID: %d ERR: %d \n", from_client.message_id, ERR_MESS_ID);
    uint8_t meesage_id = from_client.message_id;
    char* current_pointer = message;

    switch (meesage_id) {
        case ERR_MESS_ID:
            if (from_client.ticket_count == ERR_EVENTS_TICK) {
                length_to_send = MESS_ID_OCT + EVENT_ID_OCT;
                printf("EVENTS_TICK\n");

                current_pointer[0] = BAD_REQUEST;
                current_pointer++;

                uint32_t event_id_to_send = htonl(from_client.event_id);
                *(uint32_t*)current_pointer = event_id_to_send;
//                fill_buffer(1, 1 + EVENT_ID_OCT, message, event_id_to_send);
//                *(uint32_t*)(message + 1) = event_id_to_send; //TODO does it work
                //send_message(socket_fd, client_address, message, length_to_send);
            }
            else if (from_client.ticket_count == ERR_RES) {
                printf("RESE_ERR TICk %d ERR_res %d \n", from_client.ticket_count, ERR_RES);
                length_to_send = MESS_ID_OCT + RES_ID_OCT;

                current_pointer[0] = BAD_REQUEST;
                current_pointer++;

                uint32_t reservation_id_to_send = htonl(from_client.reservation_id);
                *(uint32_t*)current_pointer = reservation_id_to_send;
//                fill_buffer(1, 1 + RES_ID_OCT, message, reservation_id_to_send);
//                *(uint32_t*)(message + 1) = reservation_id_to_send;

                //send_message(socket_fd, client_address, message, length_to_send);
            }
            else {
                to_be_ignored = true;
            }

            break; //incorrect length etc. - simply skip

        case GET_RESERVATION:
            cookie = generate_cookie();
            length_to_send = MESS_ID_OCT + EVENT_ID_OCT + TICK_COU_OCT
                    + COOKIE_OCT + EXP_TIME_OCT;

            current_pointer[0] = RESERVATION;
            current_pointer++;

            insert_new_reservation(reservs, from_client.ticket_count,
                                   NULL, 0, cookie,
                                   from_client.event_id);

            size_t requested_index = (reservs->last_index) - 1;
            requested_reservation = &(reservs->arr[requested_index]);

            Event* requested_event = &(events->arr[from_client.event_id]);
            requested_event->available_tickets -= from_client.ticket_count;

            to_be_ignored = true;

            *(uint32_t*)current_pointer = htonl(requested_index + RES_IND_BIAS);
            current_pointer += 4;

            *(uint32_t*)current_pointer = htonl(from_client.event_id);
            current_pointer += 4;

            *(uint16_t*)current_pointer = htons(from_client.ticket_count);
            current_pointer += 2;

            overwrite_buffer(cookie, current_pointer, COOKIE_OCT);
            current_pointer += COOKIE_OCT;

            time_t expiration_time = time(NULL) + timeout;
            *(uint64_t*)current_pointer = htobe64((uint64_t)expiration_time);

            send_message(socket_fd, client_address, message, length_to_send);

            requested_reservation->expiration = expiration_time;
            push(expiring, requested_index);

            break;

        case GET_TICKETS:
            requested_reservation =
                    &(reservs->arr[from_client.reservation_id - RES_IND_BIAS]);

            if (!(requested_reservation->ticket_ids)) {
                requested_reservation->ticket_ids =
                    malloc((requested_reservation->ticket_count)*sizeof(uint64_t));

                check_err_perror((requested_reservation->ticket_ids != NULL),
                                 "Error while assigning the tickets");

                for (int i = 0; i < requested_reservation->ticket_count; i++) {
                    requested_reservation->ticket_ids[i] = *(ticket_seed)++;
                }
            }

            current_pointer[0] = TICKETS;
            current_pointer++;

            *(uint32_t*)current_pointer = htonl(from_client.reservation_id); //+ RES_IND_BIAS);
            current_pointer += 4;

            uint16_t ticket_count = requested_reservation->ticket_count;
            *(uint16_t*)current_pointer = htons(ticket_count);
            current_pointer += 2;

            fill_buffer_with_tickets(ticket_count,
                                     requested_reservation->ticket_ids,
                                     current_pointer);

            length_to_send = MESS_ID_OCT + RES_ID_OCT + TICK_COU_OCT*ticket_count;

            break;

        case GET_EVENTS:
            left_space = UDP_MAX - MESS_ID_OCT;
            Event current_event;
            int constant_block = EVENT_ID_OCT + TICK_COU_OCT + DESC_LEN_OCT;
            int one_event_block_to_send;
            int desc_length;

            printf("before mess %d curr %d\n", message[0], current_pointer[0]);
            current_pointer[0] = EVENTS;
            current_pointer++;
            printf("after mess %d curr %d\n", message[0], current_pointer[0]);

            for (uint32_t i = 0; i < events->len; i++) {
                current_event = events->arr[i];
                one_event_block_to_send = constant_block
                                        + current_event.description_octets;

                if (0 > (left_space - one_event_block_to_send)) {
                    break;
                }
                else {
                    *(uint32_t*)current_pointer = htonl(i);
                    current_pointer += 4;
                    printf("inside mess %d curr %d\n", message[0], current_pointer[0]);

                    *(uint16_t*)current_pointer = htons(current_event.available_tickets);
                    current_pointer += 2;

                    desc_length = current_event.description_octets;
                    *current_pointer = desc_length;
                    current_pointer++;

                    overwrite_buffer(current_event.description,
                                     current_pointer, desc_length);
                    current_pointer += desc_length;
                    left_space -= one_event_block_to_send;
                }
            }

            length_to_send = UDP_MAX - left_space;
    }

    printf("TO SEND\nmess_id %d\nlength %ld\nignore %d\n", message[0], length_to_send, to_be_ignored);
    if (!to_be_ignored) {
        printf("SENDING\n");
        send_message(socket_fd, client_address, message,
                     length_to_send);
    }
}

bool are_cookies_identical(char* original_cookie, char* received_cookie) {
    for (int i = 0; i < COOKIE_OCT; i++) {
        if (original_cookie[i] != received_cookie[i]) {
            return false;
        }
    }

    return true;
}

Client_message interpret_client_message(char* message, size_t received_length,
                                        Event_array* events, Queue* expiring,
                                        const Reservation_array* reservs) { //TODO check if reservation has been made
    Client_message result_message = create_client_message(ERR_MESS_ID, 0, 0, 0);
    if (received_length == 0) return result_message;

    uint8_t message_id = message[0];
//    printf("ntohl: %d\n", message_id_ntohl);
    if (message_id != GET_EVENTS && message_id != GET_RESERVATION
        && message_id != GET_TICKETS) return result_message;
    printf("nopassed\n");
    char* current_pointer = message;
    current_pointer++;

    switch (message_id) {
        case GET_EVENTS:
            if (received_length > 1) return result_message;

//            result_message.message_id |= message_id_ntohl;
            result_message.message_id = message_id;

            return result_message;

        case GET_RESERVATION:
            printf("received length: %ld\n", received_length);
            if (received_length != (MESS_ID_OCT + EVENT_ID_OCT + TICK_COU_OCT))
                return result_message;

//            uint32_t event_id = ntohl(bitshift_to_retrieve_message(1,
//                                                             EVENT_ID_OCT + 1,
//                                                             message));
//            uint32_t event_id = ntohl(*(uint32_t*)(message + 1));
            uint32_t event_id = ntohl(*(uint32_t*)current_pointer);
            current_pointer += 4;

            result_message.event_id = event_id;

//            char* ptr = message + 1;
//            uint32_t converted = *(uint32_t*)ptr;
//            printf("CONVERTED %d\n", converted);

            if (event_id > (events->len - 1)) {
                result_message.ticket_count = ERR_EVENTS_TICK;

                return result_message;
            }

//            uint16_t tickets_count = 0;
//            int ticket_begin = 1 + EVENT_ID_OCT;
//            tickets_count = ntohs(tickets_count |= bitshift_to_retrieve_message(ticket_begin,
//                                                    ticket_begin + TICK_COU_OCT,
//                                                    message));
            uint16_t tickets_count = ntohs(*(uint16_t*)current_pointer);
            current_pointer += 2;

            printf("TICKETS %d\n", tickets_count);
            free_expired(events, expiring, reservs);
            if (tickets_count == 0 || events->arr[event_id].available_tickets
                < tickets_count || UDP_MAX < (TICKET_OCT*tickets_count +
                MESS_ID_OCT + RES_ID_OCT + TICK_COU_OCT)) {
                    result_message.ticket_count = ERR_EVENTS_TICK;

                    return result_message;
            }

//            result_message.message_id |= message_id_ntohl;
            result_message.message_id = message_id;
//            result_message.ticket_count |= tickets_count;
            result_message.ticket_count = tickets_count;

            return result_message;

        case GET_TICKETS: //TODO error checking
            if (received_length != (MESS_ID_OCT + RES_ID_OCT + COOKIE_OCT))
                return result_message;

//            uint32_t reservation_id = ntohl(bitshift_to_retrieve_message(
//                    1, 1 + RES_ID_OCT, message));
//            uint32_t reservation_id = ntohl(*(uint32_t*)(message + 1));
            uint32_t reservation_id = ntohl(*(uint32_t*)current_pointer);
            current_pointer += 4;
            result_message.reservation_id = reservation_id;

            Reservation* requested_reservation = &(reservs->arr[reservation_id
                                                             - RES_IND_BIAS]);
            if (reservation_id >= reservs->last_index
                || (!requested_reservation->has_been_completed
                && requested_reservation->expiration < time(NULL))
                || !are_cookies_identical(requested_reservation->cookie,
                                           message + MESS_ID_OCT
                                           + RES_ID_OCT)) {
                    result_message.ticket_count = ERR_RES;

                    return result_message;
            }

//            message[received_length] = '\0';
//            char* cookie = message + (1 + RES_ID_OCT); //does it work?

            result_message.message_id = message_id;
//            result_message.reservation_id = reservation_id;
//            result_message.cookie = cookie;
//            result_message.cookie = message;
            if (!requested_reservation->has_been_completed)
                requested_reservation->has_been_completed = true;

            return result_message;
    }

    return result_message;
}

void print_to_send(char* buffer) {
    uint8_t mess_id = *buffer;
    char* current_pointer = buffer;
    current_pointer++;

    printf("TO SEND:\nmess_id: %d\n", mess_id);
    switch (mess_id) {
        case BAD_REQUEST:
            printf("EV/RES ERR %d\n", *(uint32_t*)current_pointer);

            break;
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
//    print_events(read_events);
    printf("Listening on port %d\n", port);

    char message_buffer[UDP_MAX + 1];
    memset(message_buffer, 0, sizeof(message_buffer));

    size_t read_length;

    uint16_t port_converted = 0;
    port_converted |= port;

    int socket_fd = bind_socket(port_converted);
    struct sockaddr_in client_address;
//    printf("BYTE ORDER%d\n'", __BYTE_ORDER == __LITTLE_ENDIAN);
    Queue* reservation_expiring = init_queue();
    Reservation_array reservations = create_res_array();
    srand(time(NULL));
    uint64_t ticket_seed = 0;

    do {
        read_length = read_message(socket_fd, &client_address,
                                   message_buffer, sizeof(message_buffer));
        char* client_ip = inet_ntoa(client_address.sin_addr);
        uint16_t client_port = ntohs(client_address.sin_port);
//        message_buffer[read_length] = '\0';
        printf("received %zd bytes from client %s:%u: '%.*s' |%c|\n",
               read_length, client_ip, client_port,
               (int) read_length, message_buffer, message_buffer[1 + RES_ID_OCT]); // note: we specify the length of the printed string

        Client_message received_message = interpret_client_message(message_buffer,
                                                                   read_length,
                                                                   &read_events,
                                                                   reservation_expiring,
                                                                   &reservations);
        print_client_message(received_message);
        printf("\n");

        handle_client_message(received_message, message_buffer, socket_fd,
                              &client_address, &reservations, (time_t)timeout,
                              &ticket_seed, &read_events, reservation_expiring);

    } while (true); //(read_length > 0);

    delete_event_array(read_events.arr, read_events.len);
    delete_queue(reservation_expiring);
    delete_reservations(reservations.arr, reservations.last_index);

	return 0;
}
