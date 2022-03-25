#ifndef RESEAU_QUEUE_H
#define RESEAU_QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include "packet_interface.h"

typedef struct node{
    pkt_t *pkt;
    struct node* next;
}node_t;

typedef struct queue{
    ssize_t size;
    node_t* head;
    node_t* tail;
    node_t* current;
    ssize_t maxSize;
}queue_t;

queue_t* queue_new();
node_t* node_new();

void setup_queue(queue_t* queue, int maxSize);
void setup_node(node_t* node, pkt_t* pkt, node_t* next);

int queue_get_size(queue_t* queue);
int queue_get_max_size(queue_t* queue);

node_t* queue_get_head(queue_t* queue);
node_t* queue_get_tail(queue_t* queue);
node_t* queue_get_current(queue_t* queue);

bool queue_push(queue_t* queue, node_t* to_push);
node_t* queue_pop(queue_t* queue);
bool queue_insert(queue_t* queue, node_t* to_insert);
bool queue_insert_pkt(queue_t* queue, pkt_t* pkt);
void queue_push_pkt(queue_t* queue, pkt_t* pkt);

int queue_insert_at(queue_t* queue, node_t* to_push, int index); // TODO
node_t* queue_remove_at(queue_t* queue, int index);  // TODO


#endif //RESEAU_QUEUE_H
