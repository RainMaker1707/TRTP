#include <string.h>
#include <stdint.h>

#include <netdb.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include "real_address.h"

const char * real_address(const char *address, struct sockaddr_in6 *rval){
    // protocol setup
    struct addrinfo head;
    head.ai_flags = 0;
    head.ai_family =  AF_INET6;
    head.ai_socktype = SOCK_DGRAM;
    head.ai_protocol = IPPROTO_UDP;
    // result waiting for init
    struct addrinfo *result = NULL;
    // setup result
    int s = getaddrinfo(address, NULL, &head, &result);
    if (s != 0) {
        const char *error = gai_strerror(s);
        return error;
    }
    // setup return pointer to ipv6 addr struct
    struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *) result->ai_addr;
    *rval = *ipv6;
    freeaddrinfo(result);
    return NULL;
}
