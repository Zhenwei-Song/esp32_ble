#ifndef _ROUTING_TABLE_H_
#define _ROUTING_TABLE_H_

#define ROUTING_TAG "ROUTING_TABLE"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct routing_note {
    uint8_t mac[6];
    struct routing_note *next;
} routing_note, *p_routing_note;

typedef struct routing_table {
    p_routing_note head;
} routing_table, *p_routing_table;

void init_routing_table(p_routing_table table);

// int insert_routing_node(p_routing_table table, p_routing_note new_routing);
int insert_routing_node(p_routing_table table, uint8_t *new_mac);

void remove_routing_node_from_node(p_routing_table table, p_routing_note old_routing);
void remove_routing_node(p_routing_table table, uint8_t *old_mac);

bool is_routing_table_empty(p_routing_table table);

bool check_routing_table(p_routing_table table, p_routing_note routing_note);

void print_routing_table(p_routing_table table);

void destroy_routing_table(p_routing_table table);

#endif // _ROUTING_TABLE_H_