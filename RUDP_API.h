#ifndef RUDP_H
#define RUDP_H
#define RUDP_STATE_NOT_CONNECTED 0
#define RUDP_STATE_CONNECTED 1
#define RUDP_STATE_CLOSED 2
#define RUDP_STATE_HANDSHAKE_IN_PROGRESS 3

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#define RUDP_SYN 0x01
#define RUDP_ACK 0x02



typedef struct {
    uint8_t flags;      // Flags indicating the type of packet (e.g., RUDP_SYN, RUDP_ACK)
    uint16_t length;     // Length of the data payload
    uint16_t checksum;   // Checksum for error detection
    uint32_t ack;        // Acknowledgment number
} RUDPHeader;

typedef struct {
    int sockfd; // Socket descriptor
    struct sockaddr_in peer_addr; // Peer's address information
    int state; // Connection state (e.g., CONNECTED, CLOSED)
    uint32_t next_seq_num; // Next sequence number to use
    uint32_t expected_ack_num; // Expected acknowledgment number
} RUDPSocket;

RUDPSocket* rudp_socket();

int rudp_send(RUDPSocket* socket, const void* data, size_t length);

int rudp_recv(RUDPSocket* socket, void* buffer, size_t buffer_size);

void rudp_close(RUDPSocket* socket);


#endif
