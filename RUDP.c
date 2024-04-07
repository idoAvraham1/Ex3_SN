#include "RUDP.h"


////********************** SENDER METHODS***********************

/**
 * receiveing ACK from the src
 * return -2: timeout, -1: error, 0: disconnected, 1: Received
 */
int rudp_receiveACK(int socket,struct sockaddr_in* srcAddress){
    RUDPHeader buffer;
    memset(&buffer, 0, sizeof(RUDPHeader));

    socklen_t srcAddressLen = sizeof(*srcAddress);
    int receiveACK = recvfrom(socket, &buffer, sizeof(RUDPHeader), 0, (struct sockaddr *) &srcAddress,
                              &srcAddressLen);

    if (receiveACK == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return -2;
        }
        else{
            printf("recvfrom() failed with error code : %d", errno);
            close(socket);
            return -1;
        }
    }
    if(receiveACK == 0){
        return 0;
    }

    if(buffer.flags == ACK_FLAG){
        return 1;
    }
    return 0;
}

/**
 *  request connect and waits for ACK
 * @return -1: error, 0: disconnected, 1: received
 */
int rudp_connect(int socket,struct sockaddr_in* destAddress,struct sockaddr_in* srcAddress){
    
    // create connect header 
    RUDPHeader SYN;
    memset(&SYN,0,sizeof(RUDPHeader));
    SYN.length=0;
    strcpy(SYN.data,"");
    SYN.checksum = 0;
    SYN.flags = SYN_FLAG;

    // while didnt get ack and timeout occured send again
    while(1) {

        int sendSYN = sendto(socket, &SYN, sizeof(RUDPHeader), 0, (struct sockaddr *) destAddress, sizeof(*destAddress));
        if (sendSYN == -1) {
            printf("sendto() failed with error code  : %d", errno);
            close(socket);
            return -1;
        }

        int ACKresult = rudp_receiveACK(socket, srcAddress);
        if (ACKresult != -2) {
            return ACKresult;
        }

        printf("Timeout occurred, sending connect again\n");
    }
}
/**
 * send disconnect and wait for ack, if not sent send again
 * @param socket
 * @param destAddress
 * @param srcAddress
 * @return -1: error, 0: disconnected, 1: received
 */
int rudp_disconnect(int socket,struct sockaddr_in* destAddress,struct sockaddr_in* srcAddress){

    // Create disconnect header
    RUDPHeader FIN;
    memset(&FIN,0,sizeof(RUDPHeader));
    FIN.length=0;
    strcpy(FIN.data,"");
    FIN.checksum = 0;
    FIN.flags = FIN_FLAG;

    // while didnt get ack send again
    while (1) {

        int sendFIN = sendto(socket, &FIN, sizeof(RUDPHeader), 0, (struct sockaddr *) destAddress, sizeof(*destAddress));
        if (sendFIN == -1) {
            printf("sendto() failed with error code  : %d\n", errno);
            close(socket);
            return -1;
        }
        int ACKresult = rudp_receiveACK(socket, srcAddress);
        if (ACKresult != -2) {
            return ACKresult;
        }

        printf("Timeout occurred, sending disconnect again\n");
    }
}


/**
 * Send the data and waits for ACK, if didnt get any, send again
 * @return -1: failure, 1: successful, 0: sender closed
 */
int rudp_sendDataPacket(int socket,char* data,struct sockaddr_in* destAddress,struct sockaddr_in* srcAddress){
    // Create a data message
    RUDPHeader Data;
    memset(&Data,0,sizeof(RUDPHeader));
    Data.length = sizeof(char)* strlen(data);
    strcpy(Data.data,data);
    Data.checksum = calculate_checksum(Data.data,Data.length);
    Data.flags = DATA_FLAG;
    
    // while didnt get ack and timeout occured send again
   while(1) {

       int sendData = sendto(socket, &Data, sizeof(RUDPHeader), 0, 
       (struct sockaddr *) destAddress, sizeof(*destAddress));

       if (sendData < 0) {
           printf("sendto() failed with error code  : %d\n", errno);
           return -1;
       }

       int ACKresult = rudp_receiveACK(socket,srcAddress);

       if(ACKresult != -2) {
           return ACKresult;
       }

       printf("Timeout occurred, sending file again\n");
   }
}





//********************** RECEIVER METHODS***********************



/**
 * function that sends ACK to destAddress
 * @param socket
 * @param destAddress
 * @return -1: failed\n 1: successful
 */
int rudp_sendACK(int socket,struct sockaddr_in* destAddress){
    // Create a ACK message and send it
    RUDPHeader ACK;
    memset(&ACK,0,sizeof(RUDPHeader));
    ACK.length=0;
    strcpy(ACK.data,"");
    ACK.checksum = 0;
    ACK.flags = ACK_FLAG;
    int sendACK = sendto(socket, &ACK, sizeof(RUDPHeader), 0, (struct sockaddr *)destAddress, sizeof(*destAddress));
    if (sendACK == -1) {
        printf("sendto() failed with error code  : %d\n", errno);
        return -1;
    }
    return 1;
}

/**
 * recieve the data from the sender and sends ACK
 * @return -1: failure, 0: exit message, -2: EOF, -3:bad packet >0:Data
 */
int rudp_receive(int socket,struct sockaddr_in* senderAddress){
    RUDPHeader buffer;
    memset(&buffer,0,sizeof(RUDPHeader));

    // Recieve Data from sender
    socklen_t senderAddressLen = sizeof(*senderAddress);
    int recvData = recvfrom(socket, &buffer, sizeof(buffer), 0, (struct sockaddr *)senderAddress,&senderAddressLen);
    if (recvData < 0){
        printf("recvfrom() failed with error code : %d", errno);
        close(socket);
        return -1;
    }

    int ACKResult;
    //Analyze data from sender
    switch (buffer.flags) {
        
        // if SYN save client IP and send ACK
        case SYN_FLAG:
            printf("Connection request received, sending ACK.\n");
            char clientIPAddrReadable[32] = {'\0'};
            inet_ntop(AF_INET, &senderAddress->sin_addr, clientIPAddrReadable, sizeof(clientIPAddrReadable));
            ACKResult = rudp_sendACK(socket,senderAddress);
            if(ACKResult < 0){return -1;}
            return 1;

        // if FIN send ACK
        case FIN_FLAG:
            ACKResult = rudp_sendACK(socket,senderAddress);
            if(ACKResult < 0){return -1;}
            printf("ACK Sent. Exiting...\n");
            return 0;

        // if Message check checksum, return ACK if checksum is not OK dont send ack,
        // return -2 if got EOF 
        case DATA_FLAG:
            if(buffer.checksum == calculate_checksum(buffer.data,buffer.length)){
                ACKResult = rudp_sendACK(socket,senderAddress);
                if(ACKResult < 0){return -1;}
                if(buffer.data[0]==EOF){
                    printf("File transfer completed.\n");
                    printf("ACK Sent.\n");
                    return -2;
                }
                // if not EOF send the bytes received
                return buffer.length;
            }
            else{
                printf(" something wrong with the packet, sending ACK\n");
                return -3;
            }
    }
    return -1;
}

/* 
*   A checksum function that returns 16 bit checksum for data.
*   This function is taken from RFC1071, can be found here:
*   https://tools.ietf.org/html/rfc1071
*
*/
unsigned short int calculate_checksum(void *data, unsigned int bytes) {
    unsigned short int *data_pointer = (unsigned short int *)data;
    unsigned int total_sum = 0;
// Main summing loop
    while (bytes > 1) {
        total_sum += *data_pointer++;
        bytes -= 2;
    }
// Add left-over byte, if any
    if (bytes > 0)
        total_sum += *((unsigned char *)data_pointer);
// Fold 32-bit sum to 16 bits
    while (total_sum >> 16)
        total_sum = (total_sum & 0xFFFF) + (total_sum >> 16);
    return (~((unsigned short int)total_sum));
}