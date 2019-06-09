/*
 * repeater.c
 *
 * UDP Packet Repeater
 *
 * Created 2015-07-29
 * Updated 2015-10-15
 *
 * Thomas Coe
 * Union Pacific Railroad
 *
 */

#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#include "repeater.h"

/* Global Variables */
static struct pollfd    poll_fds[MAX_FDS];      // Array to poll
static nfds_t           num_fds=0;              // Number of fds in the poll_fds array
static int              listener_ids[MAX_FDS];  // Mapping from socket fd-->listener id

// Hash tables
static transmitter_t    *transmitter_hash_table = NULL;
static target_t         *target_hash_table = NULL;

// Map linked list
static map_t            *map_head = NULL;
static map_t            *map_tail = NULL;

// Static method prototypes
static int verify_config();
static void recv_and_forward_packet(int fd);
static void send_packet(const void* buf, size_t len, int target_id);
static int open_socket(uint32_t address, uint16_t port);

/**
 * Starts the repeater. You should initialize all of your listeners,
 * transmitters, targets, and maps before calling this.
 *
 * Forks a new process to run in the background, then returns control to the
 * calling function.
 *
 * @param logfile   The path to the logfile to open
 * @return          0 if repeater was properly started
 */
int start_repeater(char* logfile)
{
#ifdef DEBUG
    print_transmitters();
    print_targets();
    print_maps();
#endif

    // Check config
    if (verify_config() < 0) {
        fprintf(stderr, "ERROR (Fatal): Config verification failed, repeater has not been started");
        return -1;
    }

#ifndef TESTING
    // Daemonize
    int rc;
    rc = fork();
    if (rc < 0) {
        perror("ERROR: Fork failed");
        return -1;
    } else if (rc > 0) { // We are the parent
        return 0;
    }
    setsid();

    // Open logfile
    umask(027);
    FILE *logfd = fopen(logfile, "a");
    if (logfd == NULL) {
        fprintf(stderr, "Could not open log file: %s\n", logfile);
        exit(1);
    }
    rc = setvbuf(logfd, NULL, _IOLBF, 1024); // Line buffer output up to 1024 characters
    if (rc < 0) {
        perror("Setting _IOLBF on logfile");
        exit(1);
    }
    stdout = logfd;
    stderr = logfd;

    fprintf(stdout, "Repeater started.\n");
    fflush(logfd);
#endif

    // Main loop
    int poll_rc;
    while(1) {
        poll_rc = poll(poll_fds, num_fds, -1);
        if (poll_rc < 0) { // Poll had an error
            perror("ERROR: Polling error");
            exit(1);
        } else if (poll_rc > 0) { // Data is available
            for (int i = 0; i < num_fds; i++) {
                if (poll_fds[i].revents == POLLIN) {
                    recv_and_forward_packet(poll_fds[i].fd);
                }
            }
        }
    }
}

/**
 * Receieve a packet from the fd specified. Try to match up the packet to any
 * maps, and export the packet if a match is found
 *
 * This should only be called once a fd has a packet waiting
 *
 * @param fd The file descriptor to receive the packet on
 */
static void recv_and_forward_packet(int fd)
{
    int                 n = 0;
    int                 listener_id = listener_ids[fd];
    char                buf[BUFFER_SIZE];
    struct sockaddr_in  src_addr;
    socklen_t           src_addr_size = sizeof(src_addr);
    uint32_t            src_ip;
    uint16_t            src_port;
    map_t               *map = map_head;

    memset(&buf, 0, sizeof(buf));

    // Get packet
    n = recvfrom(fd, buf, BUFFER_SIZE, 0, (struct sockaddr *)&src_addr, &src_addr_size);
    if (n < 0) {
        perror("ERROR: recvfrom");
        fprintf(stderr, "ERROR: Couldn't receive packet on listener %d\n", listener_id);
        return;
    }

    // listener_id of -1 denotes a transmitter, so we shouldn't do anything with this packet
    if (listener_id == -1) {
        return;
    }

#ifdef DEBUG
    fprintf(stderr, "Received packet on listener ID: %d from %s:%d\n",
            listener_id, inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port));
#endif

    // Get the source IP and port, in host byte order
    src_ip = ntohl(src_addr.sin_addr.s_addr);
    src_port = ntohs(src_addr.sin_port);

    // Iterate through the linked list of maps
    while (map != NULL) {
        // Check if listener and packet source match the map
        if (map->listener_id == listener_id &&
                (map->address == src_ip || map->address == 0) &&
                (map->port == src_port || map->port == 0)) {
            send_packet(buf, n, map->target_id);
        }
        map = map->next_map;
    }
}

/**
 * Sends a UDP packet with the data specified to the target_id specified.
 *
 * Depends on the target_id belonging to a target_t in the hash table, and the
 * target_t's transmitter_id belonging to a transmitter_t in the hash table.
 * Will print an error and return if either of these aren't true.
 *
 * @param buf       The pointer to the data to send
 * @param len       The number of bytes to send
 * @param target_id The target_id of the target_t to use for sending the packet
 */
static void send_packet(const void* buf, size_t len, int target_id)
{
    target_t            *target         = NULL;
    transmitter_t       *transmitter    = NULL;
    int                 transmitter_id  = 0;
    int                 socket          = 0;
    struct sockaddr_in  dest_addr;

    // Find the target from the hash table
    HASH_FIND_INT(target_hash_table, &target_id, target);
    if (target == NULL) {
        fprintf(stderr, "ERROR: Target %d not found in hash table.\n", target_id);
        return;
    }

    // Find the transmitter from the target
    transmitter_id = target->transmitter_id;
    HASH_FIND_INT(transmitter_hash_table, &transmitter_id, transmitter);
    if (transmitter == NULL) {
        fprintf(stderr, "ERROR: Transmitter %d not found in hash table.\n", transmitter_id);
        return;
    }

    // Set up destination address struct
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family        = AF_INET;
    dest_addr.sin_addr.s_addr   = htonl(target->address);
    dest_addr.sin_port          = htons(target->port);

    // Get socket fd
    socket = transmitter->sockfd;

    // Send packet
    if (sendto(socket, buf, len, 0, (struct sockaddr *)&dest_addr,
                sizeof(dest_addr)) != len) {
        fprintf(stderr, "ERROR: sendto failed on packet.\n");
        perror("ERROR: sendto");
    } else {
#ifdef DEBUG
        fprintf(stderr, "Sent packet to %s:%d\n", inet_ntoa(dest_addr.sin_addr), ntohs(dest_addr.sin_port));
#endif
    }
}

/**
 * Verifies everything is properly configured
 *
 * @return 0 if configuration good, -1 otherwise
 */
static int verify_config()
{
    int             rc = 0;
    int             temp = 0;
    target_t        *target = NULL;
    transmitter_t   *transmitter = NULL;
    map_t           *map = NULL;

    // Iterate through the maps, checking that targets exist
    for (map = map_head; map != NULL; map = map->next_map) {
        HASH_FIND_INT(target_hash_table, &map->target_id, target);
        if (target == NULL) {
            fprintf(stderr, "CONFIG: Target %d referenced in map but not defined.\n", map->target_id);
            rc = -1;
        }
        target = NULL;
    }

    // Iterate through targets, checking that transmitters exist
    for (target = target_hash_table; target != NULL; target = target->hh.next) {
        HASH_FIND_INT(transmitter_hash_table, &target->transmitter_id, transmitter);
        if (transmitter == NULL) {
            fprintf(stderr, "CONFIG: Transmitter %d referenced in target but not defined.\n", target->transmitter_id);
            rc = -1;
        }
        transmitter = NULL;
        // Check that the target is used in a map
        temp = 0;
        for (map = map_head; map != NULL; map = map->next_map) {
            if (map->target_id == target->id) {
                temp = 1;
            }
        }
        if (temp == 0) {
            fprintf(stderr, "CONFIG: Target %d defined, but not used in any maps.\n", target->id);
            rc = -1;
        }
    }

    // Check that all transmitters are used in a target
    for (transmitter = transmitter_hash_table; transmitter != NULL; transmitter = transmitter->hh.next) {
        temp = 0;
        for (target = target_hash_table; target != NULL; target = target->hh.next) {
            if (target->transmitter_id == transmitter->id) {
                temp = 1;
            }
        }
        if (temp == 0) {
            fprintf(stderr, "CONFIG: Transmitter %d defined, but not used in any targets.\n", transmitter->id);
            rc = -1;
        }
    }

    return rc;
}

/**
 * Opens a new socket listening on the address and port specified. Adds this
 * socket to the poll_fds array, and sets the listener_ids array to point to
 * this socket for the ID given
 *
 * All parameters should be in host byte order
 */
void create_listener(int id, uint32_t address, uint16_t port)
{
    int         socket;
    bool        exit_now = false;
    int         buffer_size = 0;
    socklen_t   optlen = sizeof(buffer_size);

    // Error checking
    if (id <= 0) {
        fprintf(stderr, "ERROR: You must define a positive ID for each listener!\n");
        exit_now = true;
    }
    if (port == 0) {
        fprintf(stderr, "ERROR: Listeners must have at least a port defined!\n");
        exit_now = true;
    }
    if (exit_now) {
        exit(1);
    }

    // Create the listener socket (adding it to poll_fds)
    socket = open_socket(address, port);

    // Log the RCVBUF size
    buffer_size = 0;
    optlen = sizeof(buffer_size);
    if (getsockopt(socket, SOL_SOCKET, SO_RCVBUF, &buffer_size, &optlen) < 0) {
        perror("Getting SO_RCVBUF");
    } else {
        struct in_addr ip_addr;
        ip_addr.s_addr = address;
        printf("Listener socket (%s:%d) receive buffer size = %d bytes\n",
                inet_ntoa(ip_addr), port, buffer_size);
    }

    // Set the listener_ids array to point to the given ID for this socket
    listener_ids[socket] = id;
}

/**
 * Creates a new transmitter_t, adding it to the Hash Table. The socket for the
 * transmitter will be bound to the address and port specified (or unbound if
 * the address and port are 0)
 *
 * Creating the socket also adds it to the poll_fds array. This function also
 * sets the listener_ids[socket] to -1 so that we know to throw away any data
 * received by a transmitter.
 *
 * All parameters should be in host byte order
 */
void create_transmitter(int id, uint32_t address, uint16_t port)
{
    transmitter_t   *transmitter = NULL;
    int             socket;
    int             buffer_size = SOCKET_SEND_BUFFER;
    socklen_t       optlen = sizeof(buffer_size);
    bool            exit_now = false;

    // Error checking
    HASH_FIND_INT(transmitter_hash_table, &id, transmitter);
    if (transmitter != NULL) {
        fprintf(stderr, "ERROR: Duplicate transmitter ID\n");
        exit_now = true;
    }
    if (id <= 0) {
        fprintf(stderr, "ERROR: You must define a positive ID for each transmitter!\n");
        exit_now = true;
    }
    if (exit_now) {
        exit(1);
    }

    // Create the transmitter socket (and add to poll_fds)
    socket = open_socket(address, port);
    // Increase the sockets send buffer
    if (setsockopt(socket, SOL_SOCKET, SO_SNDBUF, &buffer_size, optlen) < 0) {
        perror("Setting SO_SNDBUF");
        exit(1);
    }

    // Log the SNDBUF size
    buffer_size = 0;
    optlen = sizeof(buffer_size);
    if (getsockopt(socket, SOL_SOCKET, SO_SNDBUF, &buffer_size, &optlen) < 0) {
        perror("Getting SO_RCVBUF");
    } else {
        struct in_addr ip_addr;
        ip_addr.s_addr = address;
        printf("Transmitter socket (%s:%d) send buffer size = %d bytes\n",
                inet_ntoa(ip_addr), port, buffer_size);
    }

    // listener ID -1 indicates this socket is for a transmitter
    listener_ids[socket] = -1;

    // Create new transmitter
    transmitter = malloc(sizeof(transmitter_t));
    if (transmitter == NULL) {
        fprintf(stderr, "ERROR: malloc failed\n");
        exit(1);
    }
    transmitter->id = id;
    transmitter->sockfd = socket;

    // Add transmitter to the hash table
    HASH_ADD_INT(transmitter_hash_table, id, transmitter);
}

/**
 * Creates a new target_t, adding it to the Hash Table
 *
 * A target is where a packet may be forwarded. The address and port are the
 * destination IP address and destination UDP port for the sent packet, and
 * the transmitter_id corresponds to the transmitter to be used to send the
 * packet.
 *
 * All parameters should be in host byte order
 */
void create_target(int id, uint32_t address, uint16_t port, int transmitter_id)
{
    target_t    *target = NULL;
    bool        exit_now = false;

    // Error checking
    HASH_FIND_INT(target_hash_table, &id, target);
    if (target != NULL) {
        fprintf(stderr, "ERROR: Duplicate target ID: %d\n", id);
        exit_now = true;
    }
    if (id <= 0) {
        fprintf(stderr, "ERROR: You must define a positive ID for each Target!\n");
        exit_now = true;
    }
    if (address == 0) {
        fprintf(stderr, "ERROR: Target %d must have an address defined!\n", id);
        exit_now = true;
    }
    if (port == 0) {
        fprintf(stderr, "ERROR: Target %d must have a port defined!\n", id);
        exit_now = true;
    }
    if (transmitter_id <= 0) {
        fprintf(stderr, "ERROR: Target %d must have a transmitter defined!\n", id);
        exit_now = true;
    }
    if (exit_now) {
        exit(1);
    }

    // Create new target
    target = malloc(sizeof(target_t));
    if (target == NULL) {
        fprintf(stderr, "ERROR: malloc failed\n");
        exit(1);
    }
    target->id = id;
    target->address = address;
    target->port = port;
    target->transmitter_id = transmitter_id;

    // Add target to the hash table
    HASH_ADD_INT(target_hash_table, id, target);
}

/**
 * Creates a new map_t with the parameters provided, adding it to the map_head
 * linked list. Packets received that match a map are forwarded to a target.
 *
 * Packets received on "listener_id" from "src_address:src_port" will be sent
 * using "target_id"
 *
 * All parameters should be in host byte order
 */
void create_map(int listener_id, uint32_t src_address, uint16_t src_port, int target_id)
{
    map_t *map;

    // Create new map
    map = malloc(sizeof(map_t));
    if (map == NULL) {
        fprintf(stderr, "ERROR: malloc failed\n");
        exit(1);
    }
    map->listener_id    = listener_id;
    map->address        = src_address;
    map->port           = src_port;
    map->target_id      = target_id;
    map->next_map       = NULL;

    // Add map to the linked list
    if (map_head == NULL) {
        map_head = map;
    } else {
        if (map_tail == NULL) {
            fprintf(stderr, "ERROR: map_tail should never be null here!\n");
            exit(1);
        }
        map_tail->next_map = map;
    }
    map_tail = map;
}

/*
 * Open a new UDP socket on a port specified. Also sets the socket option
 * SO_REUSEADDR and sets the O_NONBLOCK file descriptor flag. Binds the fd to
 * the port before returning, assuming the port is specified
 *
 * Also adds the socket to the poll_fds array, incrementing num_fds.
 *
 * Both parameters should be in host byte order
 *
 * @param address   The 32-bit IP address to bind the socket to (or 0 for any)
 * @param port      The port number to bind the socket to (or 0 for any)
 * @return          The file descriptor for the new socket
 */
static int open_socket(uint32_t address, uint16_t port)
{
    int                 sock;
    int                 flags;
    struct sockaddr_in  addr;
    int                 enable = 1;
    int                 buffer_size = SOCKET_RECV_BUFFER;
    short               events = POLLIN;

    // Error checking
    if (num_fds >= MAX_FDS) {
        fprintf(stderr, "ERROR: num_fds (%d) exceeded MAX_FDS\n", (int)num_fds);
        exit(1);
    }

    // Attempt to initialize socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Opening socket");
        exit(1);
    } else if (sock >= MAX_FDS) {
        fprintf(stderr, "ERROR: Socket (%d) exceeded MAX_FDS", sock);
        exit(1);
    }

    // Add socket to the poll_fds array
    poll_fds[num_fds].fd = sock;
    poll_fds[num_fds].events = events;
    num_fds++;

    // Set SO_REUSEADDR and SO_RCVBUF
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        perror("Setting SO_REUSEADDR");
        exit(1);
    }
    if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size)) < 0) {
        perror("Setting SO_RCVBUF");
        exit(1);
    }

    // Set O_NONBLOCK flag
    flags = fcntl(sock, F_GETFL, 0);
    flags = flags | O_NONBLOCK;
    if (fcntl(sock, F_SETFL, flags) < 0) {
        perror("Setting non-blocking");
        exit(1);
    }

    // Return socket now if binding is not needed
    if (address == 0 && port == 0) {
        return sock;
    }

    // Initialize address structure
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    if (address == 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        addr.sin_addr.s_addr = htonl(address);
    }
    addr.sin_port = htons(port);

    // Attempt to bind socket to the port/address specified
    if (bind(sock,(struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Binding address: %lu port: %d\n", (long unsigned int)address, port);
        perror("Binding");
        exit(1);
    }

    return sock;
}

/**
 * Iterates through the transmitter hash table, printing the contents
 */
void print_transmitters()
{
    transmitter_t   *cur = transmitter_hash_table;
    while (cur != NULL) {
        printf("Transmitter: %d\n", cur->id);
        printf(" sockfd:%d\n", cur->sockfd);
        cur = cur->hh.next;
    }
}

/**
 * Iterates through the taget hash table, printing the contents
 */
void print_targets()
{
    target_t        *cur = target_hash_table;
    while (cur != NULL) {
        printf("Target: %d\n", cur->id);
        printf(" address: %lu\n", (long unsigned int)cur->address);
        printf(" port: %d\n", cur->port);
        printf(" transmitter_id: %d\n", cur->transmitter_id);
        cur = cur->hh.next;
    }
}

/**
 * Iterates through the map linked list, printing the contents
 */
void print_maps()
{
    map_t   *map = map_head;
    int     i = 1;
    while (map != NULL) {
        printf("Map: %d(%p)\n", i, (void *)map);
        printf(" listener_id: %d\n",map->listener_id);
        printf(" address: %lu\n", (long unsigned int)map->address);
        printf(" port: %d\n", map->port);
        printf(" target_id: %d\n", map->target_id);
        printf(" next_map: %p\n", (void *)map->next_map);
        map = map->next_map;
        i++;
    }
}
