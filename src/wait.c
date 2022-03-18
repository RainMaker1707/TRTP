#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <netdb.h>

#include <sys/time.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <arpa/inet.h>

#include <poll.h>

#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>

#include "wait_for_client.h"

int wait_for_client(int sfd){
    char *buffer = (char*) malloc(sizeof(char*) * 528);
    if(!buffer) return -1;
    socklen_t addr_len = sizeof(struct sockaddr_in6);
    struct sockaddr_in6 *src_addr = (struct sockaddr_in6 *)malloc(sizeof(struct sockaddr_in6));
    // receive error
    if(recvfrom(sfd, (void *)buffer, 528, MSG_PEEK, (struct sockaddr*) src_addr,&addr_len )<0){
        //garbage
        free(buffer);
        free(src_addr);
        return -1;
    }
    // connection error
    if(connect(sfd,(struct sockaddr*) src_addr,addr_len)<0){
        //garbage
        free(buffer);
        free(src_addr);
        return -1;
    }
    // garbage
    free(buffer);
    free(src_addr);
    return 0;
}
