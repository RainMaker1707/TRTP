#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <sys/socket.h>

#include <netinet/in.h>
#include <errno.h>

int create_socket(struct sockaddr_in6 *source_addr,int src_port,struct sockaddr_in6 *dest_addr,int dst_port){
    // create socket
    int socket_ = socket(AF_INET6, SOCK_DGRAM, 0);
    if(socket_ == -1){
        fprintf(stderr,"Create error: %s\n",strerror(errno));
        return -1;
    }
    // bind socket
    if(source_addr != NULL && src_port > 0){
        source_addr->sin6_port=htons(src_port);
        int bind_ = bind(socket_, (struct sockaddr *)source_addr, sizeof(struct sockaddr_in6));
        if (bind_ == -1) {
            fprintf(stderr,"Bind error: %s\n",strerror(errno));
            return -1;
        }
    }
    // socket connection
    if(dest_addr != NULL && dst_port>0){
        dest_addr->sin6_port=htons(dst_port);
        int connection = connect(socket_, (struct sockaddr *)dest_addr, sizeof(struct sockaddr_in6));
        if (connection == -1) {
            fprintf(stderr,"Connection error: %s\n",strerror(errno));
            return -1;
        }
    }
    return socket_;
}