#include <stdlib.h>
#include <stdbool.h>

#include "queue.h"

/*
 * This file is an ADT for a pkt linked list FIFO, using two structs
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
    return queue; /// DO NOT FORGET  TO FREE
}

node_t* node_new(){
    node_t* node = (node_t*)malloc(sizeof(node_t));
    if(!node) return NULL;
    node->pkt = NULL;
    node->next = NULL;
    return node; /// DO NOT FORGET  TO FREE
}

void setup_queue(queue_t* queue, int maxSize){
    queue->maxSize = maxSize;
}
void setup_node(node_t* node, pkt_t* pkt, node_t* next){
    node->pkt = pkt;
    node->next = next;
}

bool queue_insert(queue_t* queue, node_t* to_insert){
    if(queue_get_size(queue) == 0) return queue_push(queue, to_insert);
    node_t* current = queue_get_head(queue);
    node_t* pred = NULL;
    while(current && pkt_get_seqnum(current->pkt) < pkt_get_seqnum(to_insert->pkt)) {
        pred = current;
        current = current->next;
    }
    if(current && pkt_get_seqnum(current->pkt) == pkt_get_seqnum(to_insert->pkt)) return false; /// Ignore packets already stored
    if(pred){
        pred->next = to_insert;
        to_insert->next = current;
        queue->size++;
    }else{
        to_insert->next = queue_get_head(queue);
        queue->head = to_insert;
        queue->size++;
    }
    return true;
}

bool queue_insert_pkt(queue_t* queue, pkt_t* pkt){
    node_t* node = node_new();
    setup_node(node, pkt, NULL);
    return queue_insert(queue, node);
}

int queue_get_size(queue_t* queue){
    return (int)queue->size;
}
int queue_get_max_size(queue_t* queue){
    return (int)queue->maxSize;
}

node_t* queue_get_head(queue_t* queue){
    return queue->head;
}
node_t* queue_get_tail(queue_t* queue){
    return queue->tail;
}

bool queue_push(queue_t* queue, node_t* to_push){
    if(!queue || !to_push) return false;
    if(queue->size == queue->maxSize) return false;
    if(queue->size == 0){
        queue->head = to_push;
        queue->tail = to_push;
        queue->size = 1;

    }else {
        queue->tail->next = to_push;
        queue->tail = to_push;
        queue->size += 1;
    }
    return true;
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
    return to_return; /// DO NOT FORGET TO FREE NODE
}

/*
int main(int argc, char* argv[]){
    printf("Running\n");
    queue_t* queue = queue_new();
    setup_queue(queue, 10);

    pkt_t* pkt = pkt_new();
    pkt_set_seqnum(pkt, 1);
    pkt_t* pkt2 = pkt_new();
    pkt_set_seqnum(pkt2, 3);
    pkt_t* pkt3 = pkt_new();
    pkt_set_seqnum(pkt3, 0);
    printf("pkt created\n");
    node_t* node = node_new();
    node_t* node2 = node_new();
    node_t* node3 = node_new();
    printf("node created\n");
    setup_node(node, pkt, NULL);
    queue_insert(queue, node);
    printf("First insert -- qs: %d\n", queue_get_size(queue));
    setup_node(node2, pkt2, NULL);
    queue_insert(queue, node2);
    printf("2nd insert -- qs: %d\n", queue_get_size(queue));
    setup_node(node3, pkt3, NULL);
    queue_insert(queue, node3);
    printf("Queue Size after inserts (3) -> %d\n", queue_get_size(queue));
    node_t* current = queue_get_head(queue);
    while(current){
        printf("Seq: %d\n", pkt_get_seqnum(current->pkt));
        current = current->next;
    }

    node_t* cu = node_new();
    pkt_t* pt = pkt_new();
    pkt_set_seqnum(pt, 2);
    setup_node(cu, pt, NULL);
    queue_insert(queue, cu);
    printf("Queue size: %d (4)\n", queue_get_size(queue));

    current = queue_get_head(queue);
    while(current){
        printf("Seq: %d\n", pkt_get_seqnum(current->pkt));
        current = current->next;
    }

    /// Garbage
    pkt_del(pkt);
    pkt_del(pkt2);
    pkt_del(pkt3);
    current = queue_get_head(queue);
    while(current){
        current=current->next;
        free(queue_pop(queue));
    }
    free(queue);
    return EXIT_SUCCESS;
}
 */
/// gcc queue.c packet_implem.c -o test -lz && ./test