#include <stdlib.h>
#include <stdio.h>

#include "queue.h"

/*
 * This file is an ADT for a pkt linked list FIFO, using two struct
 * queue_t: which is a basically a linked_list with size AND a NULL pointer at the end.
 * node_t: which is a node storing a pkt_t* and its follower node_t*
 */

queue_t* queue_new(){
    queue_t* queue = (queue_t*)malloc(sizeof(queue_t));
    if(!queue) return NULL;
    queue->size = 0;
    queue->maxSize = 1;
    queue->head = NULL;
    queue->tail = NULL;
    return queue;
}

node_t* node_new(){
    node_t* node = (node_t*)malloc(sizeof(node_t));
    if(!node) return NULL;
    node->pkt = NULL;
    node->next = NULL;
    return node;
}

void setup_queue(queue_t* queue, int maxSize){
    queue->maxSize = maxSize;
}
void setup_node(node_t* node, pkt_t* pkt, node_t* next){
    node->pkt = pkt;
    node->next = next;
}

int queue_get_size(queue_t* queue){
    return queue->size;
}
int queue_get_max_size(queue_t* queue){
    return queue->maxSize;
}

node_t* queue_get_head(queue_t* queue){
    return queue->head;
}
node_t* queue_get_tail(queue_t* queue){
    return queue->tail;
}

void queue_push(queue_t* queue, node_t* to_push){
    if(!queue || !to_push) return;
    if(queue->size == queue->maxSize) return;
    if(queue->size == 0){
        queue->head = to_push;
        queue->tail = to_push;
        queue->size = 1;

    }else {
        queue->tail->next = to_push;
        queue->tail = to_push;
        queue->size += 1;
        fprintf(stderr, "INQUEUE");
    }
}

void queue_push_pkt(queue_t* queue, pkt_t* pkt){
    node_t* to_push = node_new();
    setup_node(to_push, pkt, NULL);
    queue_push(queue, to_push);
}

node_t* queue_pop(queue_t* queue){
    if(queue->size == 0) return NULL;
    node_t* to_return = queue->head;
    queue->head = queue->head->next;
    queue->size -= 1;
    return to_return; // TODO NOT FORGET TO FREE NODE
}
int queue_insert_at(queue_t* queue, node_t* to_push, int index);
node_t* queue_remove_at(queue_t* queue, int index);

/*
int main(int argc, char* argv[]){
    queue_t* queue = queue_new();
    setup_queue(queue, 10);
    printf("queue size: %d\n", queue_get_size(queue));
    queue_push(queue, node_new());
    printf("queue size: %d\n", queue_get_size(queue));
    while(queue_pop(queue)) printf("POP\n");
    return EXIT_SUCCESS;
}
 */