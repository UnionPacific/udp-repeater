/*
 * repeater.h
 *
 * UDP Packet Repeater
 *
 * Created 2015-07-29
 * Updated 2015-10-06
 *
 * Thomas Coe
 * Union Pacific Railroad
 *
 */

#ifndef REPEATER_H
#define REPEATER_H

#include "uthash.h"

#define MAX_FDS             256                 // Upper bound for socket fd value
#define SOCKET_RECV_BUFFER  5 * 1024 * 1024     // 5MB Receive Buffer
#define SOCKET_SEND_BUFFER  5 * 1024 * 1024     // 5MB Send Buffer
#define BUFFER_SIZE         65507               // Size of the max UDP payload to receive (bytes) (65535-20(IP)-8(UDP))

typedef enum {false, true} bool;

/*
 * A transmitter_t is used to map the arbitrary transmitter ID from the rules
 * file to a socket file descriptor.
 *
 * Transmitters are stored in a hash table by their ID (which muct be unique)
 */
typedef struct transmitter_s
{
    int             id;         // ID (key for hash), must be unique
    int             sockfd;     // Socket file descriptor
    UT_hash_handle  hh;         // Used for storing in hash table
} transmitter_t;

/*
 * Target used to store the destination address and port information for where
 * packets should be forwarded, as well as a reference to the transmitter
 * (socket) that should be used to send these packets
 *
 * Targets are stored in a hash table by their ID (which must be unique)
 */
typedef struct target_s
{
    int             id;             // ID (key for hash), must be unique
    uint32_t        address;        // dst IP of the forwarded packet
    uint16_t        port;           // dst port of the forwarded packet
    int             transmitter_id; // ID of the transmitter_t to use for the export
    UT_hash_handle  hh;             // Used for storing in hash table
} target_t;

/*
 * Map used to match incoming packets and determine where to forward them.
 *
 * Packets recevied will be matched on listener_id (incoming socket),
 * src address, and src port (with 0 as wildcard). Packets are forwarded using
 * the socket and destination address and port determined by the target_t
 *
 * Maps are stored in a linked list that is traversed for every packet
 * received. Packets can match more than one map.
 */
typedef struct map_s
{
    int             listener_id;    // ID of the listener the packet arrived on
    uint32_t        address;        // src address of packet
    uint16_t        port;           // src port of packet (0 = wildcard)
    int             target_id;      // The target to use to send a matching packet
    struct map_s    *next_map;      // Used for storing maps in linked list
} map_t;

// Starts the repeater
int start_repeater(char* logfile);

// Functions for setting up the repeater
void create_listener(int id, uint32_t address, uint16_t port);
void create_transmitter(int id, uint32_t address, uint16_t port);
void create_target(int id, uint32_t address, uint16_t port, int transmitter_id);
void create_map(int listener_id, uint32_t src_address, uint16_t src_port, int target_id);

// Functions for printing the internal data structures
void print_maps(void);
void print_transmitters(void);
void print_targets(void);

#endif
