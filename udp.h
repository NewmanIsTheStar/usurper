#ifndef UDP_H
#define UDP_H

#define SOCKADDR_LEN sizeof(struct sockaddr)

typedef int SOCKET;
typedef struct sockaddr SOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;

SOCKET upd_establish_socket (int receive_port);
int udp_transmit (SOCKET socket, char *buffer, int buffer_len, SOCKADDR_IN destination_address);
int udp_receive (SOCKET socket, char *buffer, size_t buffer_len, SOCKADDR_IN *source_address, int usec_timeout);

#endif