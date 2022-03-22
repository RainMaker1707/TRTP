#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdbool.h>

#include "log.h"
#include "create_socket.h"
#include "real_address.h"
#include "packet_interface.h"
#include "wait_for_client.h"
#include "queue.h"

// TODO statistics
int window_size = 1;


int print_usage(char *prog_name) {
    ERROR("Usage:\n\t%s [-s stats_filename] listen_ip listen_port", prog_name);
    return EXIT_FAILURE;
}

void pkt_set_ack(pkt_t* ans, pkt_t* pkt){
    pkt_set_type(ans, PTYPE_ACK);
    pkt_set_tr(ans, 0);
    pkt_set_seqnum(ans, pkt_get_seqnum(pkt));
    pkt_set_window(ans, window_size);
    pkt_set_timestamp(ans, pkt_get_timestamp(pkt));
}

bool pkt_send(int sock, pkt_t* pkt){
    /// SEND packet
    size_t len = 10;
    char buff[len];
    fprintf(stderr, "here\n");
    if(pkt_encode(pkt, buff, &len) != PKT_OK) {
        fprintf(stderr, "error here\n");
        return false; // ERROR ON PACKET ENCODING
    }
    fprintf(stderr, "there\n");
    ssize_t error = write(sock, buff, len);  // SEND PACKET
    if(error < 0) return false;
    return true;
}

int receiver_agent(int sock){//TODO add int sock as param
    bool finished = false;
    char buff[MAX_PAYLOAD_SIZE+16];
    struct pollfd poll_fd[1];
    poll_fd[0].fd = sock;
    poll_fd[0].events = POLLIN;
    while(!finished){
        int poll_fdd = poll(poll_fd, 1, 5000);
        if(poll_fdd == 0) return EXIT_FAILURE;
        if(poll_fd[0].revents == POLLIN){
            ssize_t read_len = read(sock, buff, MAX_PAYLOAD_SIZE+16);
            pkt_t* pkt = pkt_new();
            if(pkt_decode(buff, read_len, pkt) != PKT_OK){
                fprintf(stderr, "Packet ignored, decode error");
                pkt_del(pkt);
                /// IGNORE
            }else{
                fprintf(stderr, "PKT_OK -> %d\n", pkt_get_seqnum(pkt));
                finished = true;
                //TODO ACK and NACK
                pkt_t* ans = pkt_new();
                pkt_set_ack(ans, pkt);
                fprintf(stderr, "send: %d\n", pkt_send(sock, ans));
                fprintf(stderr, "ok\n");
                //pkt_del(pkt);
                //pkt_del(ans);
            }
        }
    }
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

    // TODO Now let's code!
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
    return receiver_agent(sock);//TODO add sock as param
}