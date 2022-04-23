#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <poll.h>
#include <string.h>

#include "log.h"
#include "queue.h"
#include "real_address.h"
#include "create_socket.h"
#include "packet_interface.h"

// TODO FEC

uint32_t timestamp;
queue_t* queue;
uint8_t window_size = 1;

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
// stats[10]: min_rtt
// stats[11]: max_rtt
// stats[12]: packet_retransmitted
uint32_t stats[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, INT32_MAX, 0, 0};

int print_usage(char *prog_name) {
    ERROR("Usage:\n\t%s [-f filename] [-s stats_filename] [-c] receiver_ip receiver_port", prog_name);
    return EXIT_FAILURE;
}

uint32_t get_timestamp(){
    struct timeval now;
    gettimeofday(&now, NULL);
    return (uint32_t) (now.tv_sec*1000 + now.tv_usec/1000);;
}

bool pkt_send(int sock, pkt_t* pkt){
    /// SEND packet and UPDATE timestamp
    size_t len = (size_t) 16 + pkt_get_length(pkt);
    char buff[len];
    pkt_set_timestamp(pkt, get_timestamp()); // UPDATE TIMESTAMP ON LAUNCH
    if(pkt_encode(pkt, buff, &len) != PKT_OK) return false; // ERROR ON PACKET ENCODING
    ssize_t error = write(sock, buff, len);  // SEND PACKET
    if(error < 0) return false;
    return true;
}

void pkt_set_data(pkt_t* pkt, size_t red_len, uint8_t seqnum, char* buff){
    pkt_set_type(pkt, PTYPE_DATA); // SETTING TYPE ON DATA
    pkt_set_tr(pkt, 0); // NOT TRUNCATED
    pkt_set_window(pkt, window_size);  // NO WINDOW, UPDATED FROM ACK or NACK
    pkt_set_length(pkt, red_len); // LEN GET FROM fread()
    pkt_set_seqnum(pkt, seqnum); // SETTING UP SEQ_NUM
    pkt_set_timestamp(pkt, 0); // NEED TO BE UPDATED
    pkt_set_payload(pkt, buff, red_len); // SETTING UP BUFFER TO PAYLOAD
}

void ack_nack_dispatch(int sock){
    pkt_t* pkt = pkt_new();
    char buff[10];
    ssize_t len = read(sock, buff, 10);
    if(len < 0 || pkt_decode(buff, len, pkt) !=  PKT_OK) {
        fprintf(stderr, "Packet ignored, len: %zd\n", len);
        stats[9]++; /// STAT: packet ignored
    }else if(pkt_get_type(pkt) != PTYPE_ACK && pkt_get_type(pkt) != PTYPE_NACK){
        if(pkt_get_type(pkt) == PTYPE_DATA && pkt_get_tr(pkt)==0) stats[1]++; /// STAT: data received
        if(pkt_get_type(pkt) == PTYPE_DATA && pkt_get_tr(pkt)==1) stats[2]++; /// STAT: truncated data received
        if(pkt_get_type(pkt) == PTYPE_FEC) stats[4]++; /// STAT: fec received
    }else{
        /// IT s a ACK or a NACK
        timestamp = get_timestamp();
        if(pkt_get_type(pkt) == PTYPE_ACK){
            stats[6]++; /// STAT: ACK received
            window_size = pkt_get_window(pkt);
            fprintf(stderr, "ACK received -> %d -- New Window -> %d\n", pkt_get_seqnum(pkt), window_size);
            /// Delete useless packet in queue
            node_t* current = queue_get_head(queue);
            while(current){
                if(pkt_get_seqnum(current->pkt) == pkt_get_seqnum(pkt)) break;
                node_t* to_free = queue_pop(queue);
                current = queue_get_head(queue);
                pkt_del(to_free->pkt);
                free(to_free);
            }
            fprintf(stderr, "Queue size after ACK %d -> %d\n", pkt_get_seqnum(pkt), queue_get_size(queue));
            /// Update rtt
            uint32_t rtt = get_timestamp() - pkt_get_timestamp(pkt);
            if(stats[10] > rtt) stats[10] = rtt;
            if(stats[11] < rtt) stats[11] = rtt;
            timestamp = get_timestamp();
            if(pkt_get_window(pkt) > 0) setup_queue(queue, pkt_get_window(pkt));
        }else{
            stats[8]++; /// STAT: NACK received
            fprintf(stderr, "NACK received -> %d\n", pkt_get_seqnum(pkt));
            /// Retransmit NACK packet
            node_t* current = queue_get_head(queue);
            while(current){
                if(pkt_get_seqnum(pkt) == pkt_get_seqnum(current->pkt)){
                    pkt_send(sock, current->pkt);
                    fprintf(stderr, "Packet retransmitted -> %d\n", pkt_get_seqnum(current->pkt));
                    break;
                }
            }
        }
    }
    pkt_del(pkt);
}

void sender_agent(int sock, char* filename){
    bool finish = false;
    bool file_red = false;
    struct pollfd poll_fd[2];
    poll_fd[0].fd = sock;
    poll_fd[0].events = POLLIN;
    char buff[MAX_PAYLOAD_SIZE];
    FILE* file;
    uint8_t seq_num = 0;
    if(filename) file = fopen(filename, "r");
    fprintf(stderr, "%s\n", filename);
    while(!finish){
        while(!file_red && queue_get_size(queue) < queue_get_max_size(queue)){
            size_t red_len;
            if(filename) red_len = fread(buff, sizeof(char), sizeof(buff), file);
            else red_len = read(STDIN_FILENO, buff, sizeof(buff)); // read on stdin if no filename
            if(red_len == 0 || feof(stdin)) {
                fprintf(stderr, "File entirely red\n");
                file_red = true;
            }
            else{
                pkt_t* pkt = pkt_new();
                pkt_set_data(pkt, red_len, seq_num, buff);
                queue_push_pkt(queue, pkt);
                pkt_send(sock, pkt);
                fprintf(stderr, "Send packet -> %d\n", pkt_get_seqnum(pkt));
                stats[0]++; /// STAT: packet sent
                seq_num = (seq_num + 1) % 256; // UPDATE SEQNUM 256 because 8 bits count at 255 max.
                memset(buff, 0, red_len);
            }
        }
        if(file_red && queue_get_size(queue)==0) finish = true;
        else{
            /// Handle ACK and NACK
            int poll_fdd = poll(poll_fd, 1, 2000); // 0 == no timeout
            if(poll_fdd >= 1) ack_nack_dispatch(sock);
            /// Resent TO packets
            node_t* current = queue_get_head(queue);
            while(current){
                // Resent after 5s without ACK
                if(get_timestamp() - pkt_get_timestamp(current->pkt) >= 5000) {
                    pkt_send(sock, current->pkt);
                    fprintf(stderr, "Resent packet TO -> %d\n", pkt_get_seqnum(current->pkt));
                    //stats[0]++; /// STAT: data re-sent ?
                    stats[12]++; /// STAT: packet retransmitted
                }
                current = current->next;
            }
            /// GLOBAL TO
            if(get_timestamp() - timestamp >= 30000){ // 30s without message in
                while(queue_get_size(queue) != 0) {
                    node_t* garbage = queue_pop(queue);
                    pkt_del(garbage->pkt);
                    free(garbage);
                }
                free(queue);
                fprintf(stderr, "\nGlobal TO\n");
            }
        }
    }
    /// SEND FINAL PACKET
    pkt_t* pkt = pkt_new();
    pkt_set_data(pkt, 0, seq_num, "");
    pkt_send(sock, pkt);
    stats[0]++; /// STAT: packet sent
    if(filename) fclose(file);
    fprintf(stderr, "Final packet sent with seqnum: %d\n", pkt_get_seqnum(pkt));
    pkt_del(pkt);
}

int main(int argc, char **argv) {
    int opt;

    char *filename = NULL;
    char *stats_filename = NULL;
    char *receiver_ip = NULL;
    char *receiver_port_err;
    bool fec_enabled = false;
    uint16_t receiver_port;
    while ((opt = getopt(argc, argv, "f:s:hc")) != -1) {
        switch (opt) {
            case 'f':
                filename = optarg;
                break;
            case 'h':
                return print_usage(argv[0]);
            case 's':
                stats_filename = optarg;
                break;
            case 'c':
                fec_enabled = true;
                break;
            default:
                return print_usage(argv[0]);
        }
    }
    if (optind + 2 != argc) {
        ERROR("Unexpected number of positional arguments");
        return print_usage(argv[0]);
    }
    receiver_ip = argv[optind];
    receiver_port = (uint16_t) strtol(argv[optind + 1], &receiver_port_err, 10);
    if (*receiver_port_err != '\0') {
        ERROR("Receiver port parameter is not a number");
        return print_usage(argv[0]);
    }

    ASSERT(1 == 1); // Try to change it to see what happens when it fails
    DEBUG_DUMP("Some bytes", 11); // You can use it with any pointer type
    // This is not an error per-se.
    ERROR("Sender has following arguments: filename is %s, stats_filename is %s, fec_enabled is %d, receiver_ip is %s, receiver_port is %u",
          filename, stats_filename, fec_enabled, receiver_ip, receiver_port);
    DEBUG("You can only see me if %s", "you built me using `make debug`");
    ERROR("This is not an error, %s", "now let's code!");

    // Now let's code!
    struct  sockaddr_in6 ip;
    if(real_address(receiver_ip, &ip)) return  EXIT_FAILURE;
    int  sock = create_socket(NULL, -1, &ip, receiver_port);
    if(sock < 0) {
        fprintf(stderr, "Error on socket creation");
        return EXIT_FAILURE;
    }
    timestamp = get_timestamp();
    queue = queue_new();
    setup_queue(queue, 1);
    sender_agent(sock, filename);
    /// Print statistics
    FILE* stat_file;
    if(stats_filename) stat_file = fopen(stats_filename, "w");
    else stat_file = stderr;
    fprintf(stat_file, "data_sent: %d\n", stats[0]);
    fprintf(stat_file, "data_received: %d\n", stats[1]);
    fprintf(stat_file, "data_truncated: %d\n", stats[2]);
    fprintf(stat_file, "fec_sent: %d\n", stats[3]);
    fprintf(stat_file, "fec_received: %d\n", stats[4]);
    fprintf(stat_file, "ack_sent: %d\n", stats[5]);
    fprintf(stat_file, "ack_received: %d\n", stats[6]);
    fprintf(stat_file, "nack_sent: %d\n", stats[7]);
    fprintf(stat_file, "nack_received: %d\n", stats[8]);
    fprintf(stat_file, "packet_ignored: %d\n", stats[9]);
    fprintf(stat_file, "min_rtt: %u\n", stats[10]);
    fprintf(stat_file, "max_rtt: %u\n", stats[11]);
    fprintf(stat_file, "packet_retransmitted: %d\n", stats[12]);
    if(stats_filename) fclose(stat_file);
    close(sock);
    return EXIT_SUCCESS;
}