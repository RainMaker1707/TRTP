
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

void read_write_loop(int sfd){
    char * read_ = (char *)malloc(sizeof(char)*1024);
    char * write_ = (char *)malloc(sizeof(char)*1024);
    // node structure for poll
    struct pollfd fds[2];
    fds[0].fd = sfd;
    fds[0].events = POLLIN;
    fds[1].fd = STDIN_FILENO;
    fds[1].events=POLLIN;
    // while we receive
    while(!feof(stdin)){
        // reset memory
        memset((void *) read_, 0, sizeof(char)*1024);
        memset((void *) write_, 0, sizeof(char)*1024);
        int pol= poll(fds, 2, -1); //create poll
        if (pol == -1) fprintf(stderr,"Poll error -- %s\n", strerror(errno));
        // read poll input
        if (fds[1].revents == POLLIN){
            int read_stdin = read(STDIN_FILENO, (void *) write_, sizeof(char)*1024);
            if (read_stdin == -1) fprintf(stderr,"Read input error -- %s\n", strerror(errno));
            int write_sock = write(sfd, (void *) write_, read_stdin);
            if (write_sock == -1) fprintf(stderr,"write socket error -- %s\n", strerror(errno));
        }
        // read socket input
        if (fds[0].revents == POLLIN){
            int read_sock = read(sfd, (void *) read_, sizeof(char)*1024);
            if (read_sock == -1) fprintf(stderr,"read socket error: %s\n", strerror(errno));
            int write_stdin = write(STDOUT_FILENO, (void *) read_, read_sock);
            if (write_stdin == -1) fprintf(stderr,"write stdin error -- %s\n", strerror(errno));
        }
    }
    //garbage collector
    free(read_);
    free(write_);
}
