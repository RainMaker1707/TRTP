
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdbool.h>
#include <string.h>

#include "log.h"
#include "create_socket.h"
#include "real_address.h"
#include "packet_interface.h"
#include "wait_for_client.h"
#include "queue.h"
// TODO statistics

int window_size = 1;
int last_seq = -1;
uint32_t timestamp;
int next_seq(){return (last_seq+1)%256;} /// %256 because seq num is on 8 bits
queue_t* queue;

typedef enum {
    AOK = 0, /// All ok
    IGN = 1, /// Ignored
    END = 2, /// Reached EOF
    //ERR = 3, /// Other error
}SYM;

void pkt_set_ack_nack(pkt_t* ans, pkt_t* pkt){
    pkt_set_tr(ans, 0);
    pkt_set_window(ans, window_size);
    pkt_set_timestamp(ans, pkt_get_timestamp(pkt));
}

void pkt_set_nack(pkt_t* ans, pkt_t* pkt){
    pkt_set_type(ans, PTYPE_NACK);
    pkt_set_seqnum(ans, pkt_get_seqnum(pkt));
    pkt_set_ack_nack(ans, pkt);
}

void pkt_set_ack(pkt_t* ans, pkt_t* pkt){
    pkt_set_type(ans, PTYPE_ACK);
    pkt_set_seqnum(ans, next_seq());
    pkt_set_ack_nack(ans, pkt);
}

int print_usage(char *prog_name) {
    ERROR("Usage:\n\t%s [-s stats_filename] listen_ip listen_port", prog_name);
    return EXIT_FAILURE;
}

bool checker(uint8_t seq){
    /// Check if seqnum is in sequence
    if(seq == last_seq) return false;
    if(seq > last_seq || seq <= (last_seq + window_size)%256) return true;
    return false;
}

bool pkt_send(int sock, pkt_t* pkt){
    /// SEND packet
    size_t len = 10;
    char buff[len-1];
    if(pkt_encode(pkt, buff, &len) != PKT_OK) return false; // ERROR ON PACKET ENCODING
    ssize_t error = write(sock, buff, len);  // SEND PACKET
    if(error < 0) return false;
    return true;
}

SYM answer(int sock, pkt_t* pkt){
    if(pkt_get_type(pkt) != PTYPE_DATA){
        fprintf(stderr, "Packet is not a data packet\n");
        return IGN;
    }
    if(pkt_get_length(pkt) > MAX_PAYLOAD_SIZE){
        fprintf(stderr,"Packet too long\n");
        return IGN;
    }
    fprintf(stderr, "Answering.\n");
    timestamp = pkt_get_timestamp(pkt);
    pkt_t* ans = pkt_new();
    fprintf(stderr, "Answering...\n");
    fprintf(stderr, "pkt len = %d\n", pkt_get_length(pkt));
    if(pkt_get_tr(pkt)){ /// TRUNCATED PACKET
        if(checker(pkt_get_seqnum(pkt))){
            /// NEED TO NACK
            if(window_size > 1) window_size /= 2;
            pkt_set_nack(ans, pkt);
            if(!pkt_send(sock, ans)) {
                pkt_del(ans);
                fprintf(stderr, "NACK send with  seq: %d\n", pkt_get_seqnum(ans));
                return IGN;
            }
        }else {
            pkt_del(ans);
            return IGN;
        }
    }else{ /// NOT TRUNCATED PACKET
        if(checker(pkt_get_seqnum(pkt))){
            node_t* to_push = node_new();
            to_push->pkt = pkt;
            queue_insert(queue, to_push);
            fprintf(stderr, "In sequence packet %d - %d\n", pkt_get_seqnum(pkt), last_seq);
            if(pkt_get_seqnum(pkt) == next_seq()) {
                if (pkt_get_length(pkt) == 0) {
                    /// LAST ACK
                    fprintf(stderr, "EOF ACK setup...\n");
                    pkt_set_ack(ans, pkt);
                    fprintf(stderr, "EOF ACK set...\n");
                    if (pkt_send(sock, ans)) {
                        fprintf(stderr, "EOF ACK send with seq: %d\n", pkt_get_seqnum(ans));
                    }
                    pkt_del(ans);
                    return END;
                }else {
                    /// WRITE sequence payload
                    for(int _ = 0; _ < queue->size; _++){
                        if(queue_get_head(queue) && pkt_get_seqnum(queue_get_head(queue)->pkt) == next_seq()){
                            node_t* to_print = queue_pop(queue);
                            printf("%s", pkt_get_payload(to_print->pkt));
                            pkt_del(to_print->pkt);
                            free(to_print);
                            last_seq++;
                            last_seq %= 256;
                            fprintf(stderr, "print payload seq %d\n", last_seq);
                        }else break;
                    }

                }
                // todo correct segfault here
                pkt_set_ack(ans, pkt);
                if(pkt_send(sock, ans)){
                    fprintf(stderr, "ACK sent with seq: %d\n", pkt_get_seqnum(ans));
                }else {
                    pkt_del(ans);
                    return IGN;
                }
            }
        }else{ /// IGNORE packet not in sequence
            pkt_set_ack(ans, pkt);
            if(pkt_send(sock, ans)){
                fprintf(stderr, "IGN ACK sent with seq: %d\n", pkt_get_seqnum(ans));
            }
            pkt_del(ans);
            return IGN;
        }
    }
    pkt_del(ans);
    return AOK;
}

int receiver_agent(int sock){
    bool finished = false;
    char buff[MAX_PAYLOAD_SIZE+16];
    struct pollfd poll_fd[1];
    poll_fd[0].fd = sock;
    poll_fd[0].events = POLLIN;
    while(!finished){
        int poll_fdd = poll(poll_fd, 1, 5000);
        if(poll_fdd == 0) {
            fprintf(stderr, "ERROR poll\n");
            return EXIT_FAILURE;
        }
        if(poll_fd[0].revents == POLLIN){
            ssize_t read_len = read(sock, buff, MAX_PAYLOAD_SIZE+16);
            pkt_t* pkt = pkt_new();
            if(pkt_decode(buff, read_len, pkt) != PKT_OK){
                fprintf(stderr, "Packet ignored, decode error");
                pkt_del(pkt);
                /// IGNORE packet
            }else{
                fprintf(stderr, "PKT_OK we wait for -> %d\n", next_seq());
                /// ACK and NACK
                SYM flag = answer(sock, pkt);
                if(flag == IGN){
                    fprintf(stderr, "Packet %d ignored\n", pkt_get_seqnum(pkt));
                    pkt_del(pkt);
                }else if(flag == END){
                    fprintf(stderr, "Packet %d reached end\n", pkt_get_seqnum(pkt));
                    pkt_del(pkt);
                    finished = true;
                }else fprintf(stderr, "Packet %d ACK\n", pkt_get_seqnum(pkt));
            }
        }
        memset(buff, 0, MAX_PAYLOAD_SIZE);
    }
    node_t* current = queue_get_head(queue);
    fprintf(stderr, "Queue size at end: %d\n", queue_get_size(queue));
    printf("\n\n%s", pkt_get_payload(current->pkt));
    while(current !=  NULL){
        node_t* temp = current;
        current = current->next;
        free(temp);
    }
    free(queue);
    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    int opt;
    char *stats_filename = NULL;
    char *listen_ip = NULL;
    char *listen_port_err;
    uint16_t listen_port;

    while ((opt = getopt(argc, argv, "s:h")) != -1) {
        switch (opt) {
            case 'h':
                return print_usage(argv[0]);
            case 's':
                stats_filename = optarg;
                break;
            default:
                return print_usage(argv[0]);
        }
    }

    if (optind + 2 != argc) {
        ERROR("Unexpected number of positional arguments");
        return print_usage(argv[0]);
    }

    listen_ip = argv[optind];
    listen_port = (uint16_t) strtol(argv[optind + 1], &listen_port_err, 10);
    if (*listen_port_err != '\0') {
        ERROR("Receiver port parameter is not a number");
        return print_usage(argv[0]);
    }

    ASSERT(1 == 1); // Try to change it to see what happens when it fails
    DEBUG_DUMP("Some bytes", 11); // You can use it with any pointer type

    // This is not an error per-se.
    ERROR("Receiver has following arguments: stats_filename is %s, listen_ip is %s, listen_port is %u",
          stats_filename, listen_ip, listen_port);

    DEBUG("You can only see me if %s", "you built me using `make debug`");
    ERROR("This is not an error, %s", "now let's code!");

    /// START Now let's code!
    struct sockaddr_in6 addr;
    if(real_address(listen_ip, &addr)){
        fprintf(stderr, "Host cannot be resolved.");
        return EXIT_FAILURE;
    }
    int sock = create_socket(&addr, listen_port, NULL, -1);
    if(sock < 0) {
        fprintf(stderr, "Error on socket creation.");
        return EXIT_FAILURE;
    }
    int waiter = wait_for_client(sock);
    if(waiter < 0){
        fprintf(stderr, "Connection lost.");
        return EXIT_FAILURE;
    }
    queue = queue_new();
    return receiver_agent(sock);
}