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


uint32_t timestamp;
bool file_red = false;
queue_t* queue;
uint8_t last_ack = 0;
//TODO statistics

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

bool checker(uint8_t seq, int window_size){
    /// Check if seqnum is in sequence
    if(seq == last_ack) return false;
    if(seq > last_ack || seq <= (last_ack + window_size)%256) return true;
    return false;
}

bool ack_nack_dispatch(int sock){
    pkt_t* pkt = pkt_new();
    char buff[10];
    ssize_t len = read(sock, buff, 10);
    if(len < 0){
        pkt_del(pkt);
        return false;
    }
    if(pkt_decode(buff, len, pkt) != PKT_OK){
        pkt_del(pkt);
        return false;
    }
    if(pkt_get_tr(pkt)){
        pkt_del(pkt);
        return false;
    }
    if(pkt_get_type(pkt) != PTYPE_ACK && pkt_get_type(pkt) != PTYPE_NACK){
        fprintf(stderr, "Not a ACK or NACK, %d\n", pkt_get_type(pkt));
        pkt_del(pkt);
        return false;
    }
    queue->maxSize = pkt_get_window(pkt);
    uint8_t seq_num = pkt_get_seqnum(pkt);
    timestamp = pkt_get_timestamp(pkt);
    /// Free useless stored packet in queue
    if(pkt_get_type(pkt) == PTYPE_ACK){
        fprintf(stderr, "ACK received\n");
        if(checker(seq_num, MAX_WINDOW_SIZE)){
            fprintf(stderr, "Waited ACK -- head seq %d\n", pkt_get_seqnum(queue->head->pkt));
            while(queue->head != NULL && seq_num > pkt_get_seqnum(queue->head->pkt)) {
                free(queue_pop(queue));
                seq_num += 0; /// ONLY TO AVOID WARNING NOT UPDATED
                fprintf(stderr, "ACK compute, queue size: %d\n", queue_get_size(queue));
            }
            last_ack = pkt_get_seqnum(pkt);
            fprintf(stderr, "Last ack: %d\n", last_ack);
        }
    }if(pkt_get_type(pkt) == PTYPE_NACK){
        /// RESEND PACKET WITH NACK
        fprintf(stderr, "NACK received\n");
        if(!checker((seq_num + 1) % 256, queue->maxSize)){
            pkt_del(pkt);
            return false;
        }
        node_t* current = queue->head;
        while(pkt_get_seqnum(current->pkt) != seq_num) current = current->next;
        pkt_send(sock, current->pkt);
        fprintf(stderr, "Resent packet n: %d\n", pkt_get_seqnum(current->pkt));
    }
    pkt_del(pkt);
    return true;
}

int sender_agent(int sock, char* filename){
    bool finished = false;
    struct pollfd poll_fd[2];
    poll_fd[0].fd = sock;
    poll_fd[0].events = POLLIN;
    char buff[MAX_PAYLOAD_SIZE];
    FILE* file;
    uint8_t seq_num = 0;
    if(filename) file = fopen(filename, "r");
    fprintf(stderr, "%s\n", filename);
    while(!finished  || queue_get_size(queue) != 0){
        /// READ file part by part
        while(!file_red && queue->size < queue->maxSize){
            size_t red_len;
            if(!filename && feof(stdin)) file_red = true;
            if(filename) red_len = fread(buff, sizeof(char), sizeof(buff)-1, file);
            else red_len = read(STDIN_FILENO, buff, sizeof(buff)-1); // read on stdin if no filename
            if(!red_len) file_red = true;
            else{
                /// SETUP and SEND packet
                pkt_t* pkt = pkt_new();
                pkt_set_type(pkt, PTYPE_DATA); // SETTING TYPE ON DATA
                pkt_set_tr(pkt, 0); // NOT TRUNCATED
                pkt_set_window(pkt, 0);  // NO WINDOW, UPDATED FROM ACK or NACK
                pkt_set_length(pkt, red_len); // LEN GET FROM FREAD()
                pkt_set_seqnum(pkt, seq_num); // SETTING UP SEQ_NUM
                pkt_set_timestamp(pkt, 0); // NEED TO BE UPDATED
                pkt_set_payload(pkt, buff, red_len); // SETTING UP BUFFER TO PAYLOAD
                seq_num = (seq_num + 1) % 256; // UPDATE SEQNUM 256 because 8 bits count at 255 max.
                fprintf(stderr, "Send packet ->%d\n", pkt_get_seqnum(pkt));
                if(pkt_send(sock, pkt)) {
                    queue_push_pkt(queue, pkt);
                    fprintf(stderr, "Queue size (on push): %d\n", queue_get_size(queue));
                }
                memset(buff, 0, red_len);
            }
        }
        /// WAIT for ACK and NACK
        int poll_fdd = poll(poll_fd, 1, 0); // 0 == no timeout
        if(poll_fdd >= 1) ack_nack_dispatch(sock);
        /// SEND packet expired
        node_t* current = queue->head;
        while(current != NULL){
            if(get_timestamp() - pkt_get_timestamp(current->pkt) > 5000) {
                pkt_send(sock, current->pkt);
                fprintf(stderr, "\nResent packet TO -> %d\n", pkt_get_seqnum(current->pkt));
            }
            current = current->next;
        }
        if(file_red && queue->size == 0) finished = true;
        if(get_timestamp() - timestamp >= 30000) { /// FORCE STOP after 30s
            while(queue->size > 0) {
                fprintf(stderr, "Queue size (on TO): %d\n", queue_get_size(queue));
                free(queue_pop(queue));
            }
            finished = true;
        }
    }
    /// FINAL SEND
    fprintf(stderr, "Send final packet EOF reached with seq: %d\n", last_ack);
    pkt_t *pkt = pkt_new();
    pkt_set_type(pkt, PTYPE_DATA);
    pkt_set_tr(pkt, 0);
    pkt_set_window(pkt, 0);
    pkt_set_length(pkt, 0);
    pkt_set_seqnum(pkt, seq_num);
    pkt_set_timestamp(pkt,0);
    pkt_set_payload(pkt,NULL,0);
    pkt_send(sock, pkt);
    pkt_del(pkt);
    if(filename)fclose(file);
    node_t* current = queue_get_head(queue);
    while(current){
        node_t* temp = current;
        current = current->next;
        free(temp);
    }
    free(queue);
    return EXIT_SUCCESS;
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
    setup_queue(queue, MAX_WINDOW_SIZE);
    return sender_agent(sock, filename);
}
