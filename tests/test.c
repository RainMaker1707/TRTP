#include "packet_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(int argc, char *argv[]){
    pkt_t packet = malloc(sizeof(pkt_t));
    char *payload = "Hello, i'm the payload !";
    pkt_set_type(packet, PTYPE_DATA);
    pkt_set_tr(packet, 0);
    pkt_set_window(packet, 31);
    pkt_set_length(packet, sizeof(payload));
    pkt_set_payload(packet, payload);
    pkt_set_seqnum(packet, 123);
    char *buffer = malloc(sizeof(char) * 1024);
    pkt_encode(packet, buffer, 1024);
    printf("%s\n", buffer);
    return  EXIT_SUCCESS;
}