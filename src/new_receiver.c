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
                    if(pkt_get_length(pkt) > 0) {
                        if(pkt_get_seqnum(pkt) == next_seq()){
                            last_seq = next_seq();
                            printf("%s", pkt_get_payload(pkt));
                            pkt_t* ack = pkt_new();
                            pkt_set_ack(ack, pkt);
                            pkt_send(sock, ack);
                            fprintf(stderr, "ACK sent -> %d\n", pkt_get_seqnum(ack));
                            pkt_del(ack);
                            pkt_del(pkt);
                        }
                    }
                    else if(pkt_get_length(pkt) == 0) finish = true;
                    memset(buff, 0, red_len);
                }
            }
        }else fprintf(stderr, "Error on poll %d\n", poll_fdd);
    }
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
    receiver_agent(sock);
    // TODO print statistics
    return EXIT_SUCCESS;
}
