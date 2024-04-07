#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include "stdio.h"
#include <sys/time.h>

#define BUFFER_SIZE 4096
#define MESSAGE_SIZE 2048
#define DEFAULT_IP "127.0.0.1"

// flags
#define SYN_FLAG 'S'
#define FIN_FLAG 'F'
#define ACK_FLAG 'A'
#define DATA_FLAG 'D'


typedef struct RUDPHeader{
    char data[BUFFER_SIZE];
    unsigned short length; // length of data
    unsigned short checksum; // checksum of data
    char flags;
}RUDPHeader;


// Sender Functions

int rudp_receiveACK(int socket,struct sockaddr_in* srcAddress);

int rudp_connect(int socket,struct sockaddr_in* destAddress,struct sockaddr_in* srcAddress);

int rudp_disconnect(int socket,struct sockaddr_in* destAddress,struct sockaddr_in* srcAddress);

int rudp_sendDataPacket(int socket,char* data,struct sockaddr_in* destAddress,struct sockaddr_in* srcAddress);

// Receiver Functions

int rudp_sendACK(int socket,struct sockaddr_in* destAddress);

int rudp_receive(int socket,struct sockaddr_in* senderAddress);

// Other Functions

unsigned short int calculate_checksum(void *data, unsigned int bytes);