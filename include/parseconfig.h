/*
 * parseconfig.h
 *
 * Parses a json config file for repeater.c
 *
 * Created 2015-07-29
 * Updated 2015-10-06
 *
 * Thomas Coe
 * Union Pacific Railroad
 *
 */

#ifndef PARSECONFIG_H
#define PARSECONFIG_H

#include "json.h"

// Prototypes
void parse_config(char *filename);
int parse_rules(json_value *json_rules);
void parse_listener(json_value *value);
void parse_transmitter(json_value *value);
void parse_target(json_value *value);
void parse_map(json_value *value);
#endif
