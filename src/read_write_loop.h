#ifndef __READ_WRITE_LOOP_H_
#define __READ_WRITE_LOOP_H_

/* Loop reading a socket and printing to stdout,
 * while reading stdin and writing to the socket
 * @sfd: The socket file descriptor. It is both bound and connected.
 * @return: as soon as stdin signals EOF
 */
void read_write_loop(const int sfd);

#endif
