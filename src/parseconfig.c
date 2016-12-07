/*
 * parseconfig.c
 *
 * Parses a json config file for repeater.c
 *
 * Created 2015-07-29
 * Updated 2015-10-15
 *
 * Thomas Coe
 * Union Pacific Railroad
 *
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "parseconfig.h"
#include "repeater.h"

int main(int argc, char *argv[])
{
    // Check params
    if (argc != 3) {
        fprintf(stderr, "USAGE: %s rules.json repeater.log\n", argv[0]);
        return 1;
    }

    // Parse json config file
    parse_config(argv[1]);

    // Start the repeater
    if (start_repeater(argv[2]) < 0) {
        fprintf(stderr, "Couldn't start repeater.\n");
    }
}

/**
 * Opens the file at the filename provided, and runs the json parser on the
 * file data to get it into a usable form to process. Calls parse_rules() on
 * the parsed json.
 *
 * @param filename  The path to the json config file
 */
void parse_config(char *filename)
{
    FILE* fp;
    char* rules;

    // Open the rules file
    fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "ERROR (fatal): Could not open rules file\n");
        exit(1);
    }

    // Malloc size of rules file
    struct stat st;
    int rc = fstat(fileno(fp), &st);
    if (rc < 0) {
        perror("fstat: ");
        exit(1);
    }
    int filesize = st.st_size;
    rules = calloc(1, filesize + 1);
    if (rules == NULL) {
        fprintf(stderr, "ERROR (fatal): Malloc on %d bytes failed!\n", (filesize + 1));
        exit(1);
    }

    // Read in json from rules file
    if (fread(rules, filesize, 1, fp) != 1) {
        fprintf(stderr, "ERROR (fatal): Could not read from rules file\n");
        exit(1);
    }
    fclose(fp);

#ifdef DEBUG
    printf("%s\n", rules);
#endif

    // Run the json parser on the rules pointer
    json_settings settings = { 0 };
    settings.settings |= json_enable_comments; // Enable C style comments in the json
    json_value* json_rules = json_parse_ex(&settings, (json_char*)rules, filesize);
    if (json_rules == NULL) {
        fprintf(stderr, "ERROR (fatal): Couldn't parse json rules\n");
        free(rules);
        exit(1);
    }

    // Decode the rules
    parse_rules(json_rules);

    // Free data
    json_value_free(json_rules);
    free(rules);
}

/**
 * Parses the decoded json_value. Ensures that the rules have listen,
 * transmit, target, and map arrays defined. Calls other functions to parse
 * those types individually.
 *
 * @param json_rules    The json_value* obtained from json_parse()
 */
int parse_rules(json_value* json_rules)
{
    bool listen_found = false;
    bool transmit_found = false;
    bool target_found = false;
    bool map_found = false;

    bool exit_now = false;

    // Loop through each json object
    for (int i = 0; i < json_rules->u.object.length; i++) {
        char *name = json_rules->u.object.values[i].name;
        json_value *value = json_rules->u.object.values[i].value;
        int type = value->type;
        if ( strncmp(name, "listen", 6) == 0 ) {
            listen_found = true;
            if (type != json_array) {
                printf("Error: listen type is not array\n");
                exit(1);
            }
            // Loop through all listeners in the array
            for (int x = 0; x < value->u.array.length; x++) {
                parse_listener(value->u.array.values[x]);
            }
        } else if ( strncmp(name, "transmit", 8) == 0 ) {
            transmit_found = true;
            if (type != json_array) {
                printf("Error: transmit type is not array\n");
                exit(1);
            }
            // Loop through all transmitters in the array
            for (int x = 0; x < value->u.array.length; x++) {
                parse_transmitter(value->u.array.values[x]);
            }
        } else if ( strncmp(name, "target", 6) == 0 ) {
            target_found = true;
            if (type != json_array) {
                printf("Error: target type is not array\n");
                exit(1);
            }
            // Loop through all targets in the array
            for (int x = 0; x < value->u.array.length; x++) {
                parse_target(value->u.array.values[x]);
            }
        } else if ( strncmp(name, "map", 3) == 0 ) {
            map_found = true;
            if (type != json_array) {
                printf("Error: map type is not array\n");
                exit(1);
            }
            // Loop through all maps in the array
            for (int x = 0; x < value->u.array.length; x++) {
                parse_map(value->u.array.values[x]);
            }
        } else {
            printf("Unrecognized token in rules (%s)", name);
        }
    }

    // If one type of array was not found, kill the program
    if (!listen_found) {
        fprintf(stderr, "ERROR: listen config not found\n");
        exit_now = true;
    }
    if (!transmit_found) {
        fprintf(stderr, "ERROR: transmit config not found\n");
        exit_now = true;
    }
    if (!target_found) {
        fprintf(stderr, "ERROR: target config not found\n");
        exit_now = true;
    }
    if (!map_found) {
        fprintf(stderr, "ERROR: map config not found\n");
        exit_now = true;
    }
    if (exit_now) {
        exit(1);
    }

    return 0;
}

/**
 * Parses a json object identified as a listener
 *
 * Iterates through the fields, and ensures the proper fields have been
 * included. If a field is missing, reports the error and kills the program.
 *
 * Uses repeater.c's create_listener() function
 */
void parse_listener(json_value *value)
{
    int id = 0;
    uint32_t address = 0;
    uint16_t port = 0;

    bool id_found = false;
    bool address_found = false;
    bool port_found = false;

    bool exit_now = false;

    // Iterate through the fields in the listener
    for (int i = 0; i < value->u.object.length; i++) {
        char *name = value->u.object.values[i].name;
        json_value *field = value->u.object.values[i].value;
        int type = field->type;
        if ( strncmp(name, "id", 2) == 0 ) {
            id_found = true;
            if (type != json_integer) {
                printf("Error: listen->id must be an integer\n");
                exit(1);
            }
            id = field->u.integer;
        } else if ( strncmp(name, "address", 7) == 0 ) {
            address_found = true;
            if (type != json_string) {
                printf("Error: listen->address must be a dotted decimal string\n");
                exit(1);
            }
            if ( strncmp(field->u.string.ptr, "*", 1) == 0 ) {
                address = 0;
            } else {
                struct in_addr addr;
                int rc = inet_pton(AF_INET, field->u.string.ptr, &addr);
                if (rc == 0) {
                    printf("Error: listen->address is not a valid IPv4 address\n");
                    exit(1);
                }
                address = ntohl(addr.s_addr);
            }
        } else if ( strncmp(name, "port", 4) == 0 ) {
            port_found = true;
            if (type != json_string) {
                printf("Error: listen->port must be a string\n");
                exit(1);
            }
            int temp = atoi(field->u.string.ptr);
            if (temp <= 1024 || temp > 65535) {
                printf("%d is an invalid port. Must be between 1024-65536 noninclusive", temp);
                exit(1);
            }
            port = temp;
        }
    }

    // Check that all parameters were included for this listener
    if (!id_found) {
        fprintf(stderr, "ERROR: listen->id not found\n");
        exit_now = true;
    }
    if (!address_found) {
        fprintf(stderr, "ERROR: listen->address not found\n");
        exit_now = true;
    }
    if (!port_found) {
        fprintf(stderr, "ERROR: listen->port not found\n");
        exit_now = true;
    }
    if (exit_now) {
        exit(1);
    }

#ifdef DEBUG
    printf("Listener- ID: %d, addr: %lu, port: %d\n", id, (long unsigned int)address, port);
#endif
    create_listener(id, address, port);
}

/**
 * Parses a json object identified as a transmitter
 *
 * Iterates through the fields, and ensures the proper fields have been
 * included. If a field is missing, reports the error and kills the program.
 *
 * Uses repeater.c's create_transmitter() function
 */
void parse_transmitter(json_value *value)
{
    int id = 0;
    uint32_t address = 0;
    uint16_t port = 0;

    bool id_found = false;
    bool address_found = false;
    bool port_found = false;

    bool exit_now = false;

    // Iterate through the fields in the listener
    for (int i = 0; i < value->u.object.length; i++) {
        char *name = value->u.object.values[i].name;
        json_value *field = value->u.object.values[i].value;
        int type = field->type;
        if ( strncmp(name, "id", 2) == 0 ) {
            id_found = true;
            if (type != json_integer) {
                printf("Error: transmit->id must be an integer\n");
                exit(1);
            }
            id = field->u.integer;
        } else if ( strncmp(name, "address", 7) == 0 ) {
            address_found = true;
            if (type != json_string) {
                printf("Error: transmit->address must be a dotted decimal string\n");
                exit(1);
            }
            if ( strncmp(field->u.string.ptr, "*", 1) == 0 ) {
                address = 0;
            } else {
                //address = ntohl(inet_addr(field->u.string.ptr));
                struct in_addr addr;
                int rc = inet_pton(AF_INET, field->u.string.ptr, &addr);
                if (rc == 0) {
                    printf("Error: transmit->address is not a valid IPv4 address\n");
                    exit(1);
                }
                address = ntohl(addr.s_addr);
            }
        } else if ( strncmp(name, "port", 4) == 0 ) {
            port_found = true;
            if (type != json_string) {
                printf("Error: transmit->port must be a string\n");
                exit(1);
            }
            if ( strncmp(field->u.string.ptr, "*", 1) == 0 ) {
                port = 0;
            } else {
                int temp = atoi(field->u.string.ptr);
                if (temp <= 1024 || temp > 65535) {
                    printf("%d is an invalid port. Must be between 1024-65536 noninclusive", temp);
                    exit(1);
                }
                port = temp;
            }
        }
    }

    // Check that all parameters were included for this listener
    if (!id_found) {
        fprintf(stderr, "ERROR: transmit->id not found\n");
        exit_now = true;
    }
    if (!address_found) {
        fprintf(stderr, "ERROR: transmit->address not found\n");
        exit_now = true;
    }
    if (!port_found) {
        fprintf(stderr, "ERROR: transmit->port not found\n");
        exit_now = true;
    }
    if (exit_now) {
        exit(1);
    }

#ifdef DEBUG
    printf("Transmitter- ID: %d, addr: %lu, port: %d\n", id, (long unsigned int)address, port);
#endif
    create_transmitter(id, address, port);
}

/**
 * Parses a json object identified as a target
 *
 * Iterates through the fields, and ensures the proper fields have been
 * included. If a field is missing, reports the error and kills the program.
 *
 * Uses repeater.c's create_target() function
 */
void parse_target(json_value *value)
{
    int id = 0;
    uint32_t address = 0;
    uint16_t port = 0;
    int transmit_id = 0;

    bool id_found = false;
    bool address_found = false;
    bool port_found = false;
    bool transmit_found = false;

    bool exit_now = false;

    // Iterate through the fields in the target
    for (int i = 0; i < value->u.object.length; i++) {
        char *name = value->u.object.values[i].name;
        json_value *field = value->u.object.values[i].value;
        int type = field->type;
        if ( strncmp(name, "id", 2) == 0 ) {
            id_found = true;
            if (type != json_integer) {
                printf("Error: target->id must be an integer\n");
                exit(1);
            }
            id = field->u.integer;
        } else if ( strncmp(name, "address", 7) == 0 ) {
            address_found = true;
            if (type != json_string) {
                printf("Error: target->address must be a dotted decimal string\n");
                exit(1);
            }
            //address = ntohl(inet_addr(field->u.string.ptr));
            struct in_addr addr;
            int rc = inet_pton(AF_INET, field->u.string.ptr, &addr);
            if (rc == 0) {
                printf("Error: target->address is not a valid IPv4 address\n");
                exit(1);
            }
            address = ntohl(addr.s_addr);
        } else if ( strncmp(name, "port", 4) == 0 ) {
            port_found = true;
            if (type != json_string) {
                printf("Error: target->port must be a string\n");
                exit(1);
            }
            int temp = atoi(field->u.string.ptr);
            if (temp <= 1024 || temp > 65535) {
                printf("%d is an invalid port. Must be between 1024-65536 noninclusive", temp);
                exit(1);
            }
            port = temp;
        } else if ( strncmp(name, "transmitter", 11) == 0 ) {
            transmit_found = true;
            if (type != json_integer) {
                printf("Error: target->transmitter must be an integer\n");
                exit(1);
            }
            transmit_id = field->u.integer;
        }
    }

    // Check that all parameters were included for this target
    if (!id_found) {
        fprintf(stderr, "ERROR: target->id not found\n");
        exit_now = true;
    }
    if (!address_found) {
        fprintf(stderr, "ERROR: target->address not found\n");
        exit_now = true;
    }
    if (!port_found) {
        fprintf(stderr, "ERROR: target->port not found\n");
        exit_now = true;
    }
    if (!transmit_found) {
        fprintf(stderr, "ERROR: target->transmitter not found\n");
        exit_now = true;
    }
    if (exit_now) {
        exit(1);
    }

#ifdef DEBUG
    printf("Target- ID: %d, addr: %lu, port: %d transmitter: %d\n", id, (long unsigned int)address, port, transmit_id);
#endif
    create_target(id, address, port, transmit_id);
}

/**
 * Parses a json object identified as a map
 *
 * Iterates through the fields, and ensures the proper fields have been
 * included. If a field is missing, reports the error and kills the program.
 *
 * Uses repeater.c's create_map() function
 */
void parse_map(json_value *value)
{
    int source = 0;
    int target = 0;
    int i = 0;
    json_value *targets = NULL;
    uint32_t address = 0;
    uint16_t port = 0;

    bool source_found = false;
    bool target_found = false;
    bool address_found = false;
    bool port_found = false;

    bool exit_now = false;

    // Iterate through the fields in the listener
    for (int i = 0; i < value->u.object.length; i++) {
        char *name = value->u.object.values[i].name;
        json_value *field = value->u.object.values[i].value;
        int type = field->type;
        if ( strncmp(name, "source", 6) == 0 ) {
            source_found = true;
            if (type != json_integer) {
                printf("Error: map->source must be an integer\n");
                exit(1);
            }
            source = field->u.integer;
        } else if ( strncmp(name, "target", 6) == 0 ) {
            target_found = true;
            if (type != json_array) {
                printf("Error: map->target must be an array of integers\n");
                exit(1);
            }
            targets = field;
        } else if ( strncmp(name, "address", 7) == 0 ) {
            address_found = true;
            if (type != json_string) {
                printf("Error: map->address must be a dotted decimal string\n");
                exit(1);
            }
            if ( strncmp(field->u.string.ptr, "*", 1) == 0 ) {
                address = 0;
            } else {
                //address = ntohl(inet_addr(field->u.string.ptr));
                struct in_addr addr;
                int rc = inet_pton(AF_INET, field->u.string.ptr, &addr);
                if (rc == 0) {
                    printf("Error: map->address is not a valid IPv4 address\n");
                    exit(1);
                }
                address = ntohl(addr.s_addr);
            }
        } else if ( strncmp(name, "port", 4) == 0 ) {
            port_found = true;
            if (type != json_string) {
                printf("Error: map->port must be a string\n");
                exit(1);
            }
            if ( strncmp(field->u.string.ptr, "*", 1) == 0 ) {
                port = 0;
            } else {
                int temp = atoi(field->u.string.ptr);
                if (temp <= 1024 || temp > 65535) {
                    printf("%d is an invalid port. Must be between 1024-65536 noninclusive", temp);
                    exit(1);
                }
                port = temp;
            }
        }
    }

    // Check that all parameters were included for this listener
    if (!source_found) {
        fprintf(stderr, "ERROR: map->source not found\n");
        exit_now = true;
    }
    if (!target_found) {
        fprintf(stderr, "ERROR: map->target not found\n");
        exit_now = true;
    }
    if (!address_found) {
        fprintf(stderr, "ERROR: map->address not found\n");
        exit_now = true;
    }
    if (!port_found) {
        fprintf(stderr, "ERROR: map->port not found\n");
        exit_now = true;
    }
    if (exit_now) {
        exit(1);
    }

    // Loop through all of the targets, creating map for each
    if (targets == NULL) {
        fprintf(stderr, "ERROR: targets is NULL\n");
        exit(1);
    }
    for (i = 0; i < targets->u.array.length; i++) {
        json_value *value = targets->u.array.values[i];
        if (value->type != json_integer) {
            printf("Error: map->target must be an array of integers\n");
            exit(1);
        }
        target = value->u.integer;
#ifdef DEBUG
        printf("Map- source: %d, target: %d addr: %lu, port: %d\n", source, target, (long unsigned int)address, port);
#endif
        create_map(source, address, port, target);
    }

}

