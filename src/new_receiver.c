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

// stats[0]: data_sent
// stats[1]: data_received
// stats[2]: data_truncated_received
// stats[3]: fec_sent
// stats[4]: fec_received
// stats[5]: ack_sent
// stats[6]: ack_received
// stats[7]: nack_sent
// stats[8]: nack_received
// stats[9]: packet_ignored
// stats[10]: packet_duplicated
// stats[11]: packet_recovered
int stats[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

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
    pkt_set_payload(ans, "", 0);
    pkt_set_window(ans, window_size);
    pkt_set_length(ans, 0);
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

bool pkt_send(int sock, pkt_t* pkt){
    /// SEND packet
    size_t len = 10;
    char buff[len];
    if(pkt_encode(pkt, buff, &len) != PKT_OK) {
        fprintf(stderr, "Error on encoding packet -> %d\n", pkt_get_seqnum(pkt));
        return false; // ERROR ON PACKET ENCODING
    }
    ssize_t error = write(sock, buff, len);  // SEND PACKET
    if(error < 0) {
        fprintf(stderr, "Error on sending packet -> %d\n", pkt_get_seqnum(pkt));
        return false;
    }
    return true;
}

int max(int a, int b){
    if(a>=b)return a;
    else return b;
}

bool in_window(pkt_t* pkt){
    if(last_seq > pkt_get_seqnum(pkt)) return false;
    if(last_seq <= pkt_get_seqnum(pkt)) return max(256, window_size + last_seq) > pkt_get_seqnum(pkt);
    else return (window_size + last_seq) % 256 > pkt_get_seqnum(pkt);
}

void print_queue(pkt_t* pkt){
    queue_insert_pkt(queue, pkt);
    node_t* current = queue_get_head(queue);
    while(current){
        if(pkt_get_seqnum(current->pkt) == next_seq()) {
            current = current->next;
            node_t* to_free = queue_pop(queue);
            printf("%s", pkt_get_payload(to_free->pkt));
            fprintf(stderr, "Printed packet %d payload\n", pkt_get_seqnum(to_free->pkt));
            pkt_del(to_free->pkt);
            free(to_free);
            last_seq = next_seq();
        }
        else break;
    }
}

void receiver_agent(int sock){
    bool finish = false;
    char buff[MAX_PAYLOAD_SIZE+16];
    struct pollfd poll_fd[1];
    poll_fd[0].fd = sock;
    poll_fd[0].events = POLLIN;
    if(sock == 0) fprintf(stderr, "testing...");
    while(!finish){
        int poll_fdd = poll(poll_fd, 1, 200);
        if(poll_fdd > 0){
            if(poll_fd[0].revents == POLLIN){
                fprintf(stderr, "Packet received\n");
                ssize_t red_len = read(sock, buff, MAX_PAYLOAD_SIZE+16);
                pkt_t* pkt = pkt_new();
                if(red_len > 0 && pkt_decode(buff, red_len, pkt) == PKT_OK){
                    if(pkt_get_type(pkt) == PTYPE_DATA) {
                        stats[1]++;
                        if (pkt_get_length(pkt) > 0) {
                            fprintf(stderr, "Processing ... \n");
                            if (in_window(pkt)) {
                                fprintf(stderr, "In window! Answering ... \n");
                                if (pkt_get_seqnum(pkt) == next_seq()) { /// We wait for this packet
                                    setup_queue(queue, ++window_size);
                                    print_queue(pkt);
                                    pkt_t *ack = pkt_new();
                                    pkt_set_ack(ack, pkt);
                                    pkt_send(sock, ack);
                                    fprintf(stderr, "ACK sent -> %d\n", pkt_get_seqnum(ack));
                                    pkt_del(ack);
                                } else {
                                    fprintf(stderr, "Not directly waited");
                                    if(queue_insert_pkt(queue, pkt)){
                                        pkt_t *ack = pkt_new();
                                        pkt_set_ack(ack, pkt);
                                        pkt_send(sock, ack);
                                        fprintf(stderr, "ACK sent -> %d\n", pkt_get_seqnum(ack));
                                        stats[5]++; /// STAT: ACK sent
                                        pkt_del(ack);
                                        fprintf(stderr, "Queue size after ack -> %d\n", queue_get_size(queue));
                                    }else {
                                        fprintf(stderr, "Duplicated packet, not add to queue\n");
                                        stats[10]++; /// STAT: packet duplicated
                                        stats[9]++; /// STAT: packet ignored
                                    }
                                }
                            } else {
                                /// TODO prepare NACK or Ignore packet
                                fprintf(stderr, "Not in window");
                                stats[7]++; /// STAT: NACK sent
                                stats[9]++; /// STAT: packet ignored
                            }
                        } else if (pkt_get_length(pkt) == 0) finish = true;
                    }
                    memset(buff, 0, red_len);
                }else stats[9]++; /// STAT: packet ignored
            }
        }
    }
    fprintf(stderr, "Final packet received correctly\n");
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
    setup_queue(queue, window_size);
    receiver_agent(sock);
    // TODO print statistics
    return EXIT_SUCCESS;
}
