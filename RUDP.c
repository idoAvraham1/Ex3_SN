#include "RUDP_API.h"
#define TIMEOUT_SECONDS 2
#define TIMEOUT_MICROSECONDS 0
#define SOCKADDR_IN_SIZE sizeof(struct sockaddr_in)


RUDPSocket* rudp_socket() {
    RUDPSocket* socket1 = (RUDPSocket*)malloc(sizeof(RUDPSocket));
    if (socket1 == NULL) {
        perror("Error creating RUDP socket");
        return NULL;
    }

    
    // Initialize fields
    socket1->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket1->sockfd == -1) {
        perror("Error creating UDP socket");
        free(socket1);
        return NULL;
    }

    // Initialize other fields
    memset(&(socket1->peer_addr), 0, sizeof(struct sockaddr_in)); // Initialize peer address structure
    socket1->state =  0/* Set initial state to not connected */;
    socket1->next_seq_num = 0; // Set initial sequence number
    socket1->expected_ack_num = 0; // Set initial expected acknowledgment number

    return socket1;
}
/*simple checksum algorithm like the Internet Checksum (RFC 1071) */
uint16_t calculate_checksum(const void* data, size_t length) {
    uint32_t sum = 0;
    const uint16_t* words = (const uint16_t*)data;

    // Sum all 16-bit words in the data and header
    for (size_t i = 0; i < length / 2; ++i) {
        sum += words[i];
    }

    // If there's an odd number of bytes, add the last byte to the sum
    if (length % 2 != 0) {
        sum += ((const uint8_t*)data)[length - 1];
    }

    // Fold the carry bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    // Take the one's complement
    uint16_t checksum = ~sum;

    return checksum;
}

int rudp_send(RUDPSocket* socket, const void* data, size_t length) {
    if (socket == NULL || data == NULL || length == 0) {
        return -1; // Invalid parameters
    }

    // Create an RUDPHeader 
    RUDPHeader header;
    header.flags = RUDP_ACK; 
    header.length = htons(length);
    header.checksum = 0; 
    header.ack=0;
   

    // Create a buffer to hold the entire packet (header + data)
    size_t packet_size = sizeof(RUDPHeader) + length;
    void* packet = malloc(packet_size);
    if (packet == NULL) {
        perror("Error allocating memory for RUDP packet");
        return -1;
    }
    header.checksum = calculate_checksum(packet, packet_size);

    // Copy the header and data into the packet buffer
    memcpy(packet, &header, sizeof(RUDPHeader));
    memcpy(packet + sizeof(RUDPHeader), data, length);

    int attempt = 0;
    const int max_attempts = 3; // Number of retransmission attempts

    while (attempt < max_attempts) {
        // Send the packet using the UDP socket
        ssize_t sent_bytes = sendto(socket->sockfd, packet, packet_size, 0,
                            (struct sockaddr*)&(socket->peer_addr), SOCKADDR_IN_SIZE);

        if (sent_bytes == -1) {
            perror("Error sending RUDP packet");
            return -1;
        }

        // Wait for acknowledgment with a timeout
        struct timeval timeout;
        timeout.tv_sec = TIMEOUT_SECONDS;
        timeout.tv_usec = TIMEOUT_MICROSECONDS;

        // Declare a variable of type fd_set to represent a set of file descriptors
        fd_set readfds;
        // Clear (initialize) the file descriptor set
        FD_ZERO(&readfds);
        // Add the socket file descriptor to the set
        FD_SET(socket->sockfd, &readfds);

        // Use select to wait for the socket to become ready for reading or timeout.
        int select_result = select(socket->sockfd + 1, &readfds, NULL, NULL, &timeout);

        if (select_result == -1) {
            perror("Error in select");
            return -1;
        } 
        else if (select_result == 0) {
            // Timeout occurred, acknowledgment not received
            attempt++;
            printf("Timeout occurred, retransmitting data #%d\n", attempt);
            continue; // Retry sending the packet
        } 
        else {
            // Acknowledgment received
            RUDPHeader ack_header;
            ssize_t recv_bytes = recvfrom(socket->sockfd, &ack_header, sizeof(RUDPHeader), 0, NULL, NULL);

            if (recv_bytes == sizeof(RUDPHeader) && ack_header.flags == RUDP_ACK) {
                // Handle acknowledgment processing
                if (ack_header.ack == socket->expected_ack_num) {
                    // Valid acknowledgment received
                    printf("Data transferred successfully\n");
                    free(packet);
                    return 1; // Success
                } 
                else {
                    // Invalid acknowledgment
                    attempt++;
                    printf("ACK received, but not valid,retrying #%d\n", attempt);
                    continue; // Retry sending the packet
                }
            } 
            else {
                // Invalid or unexpected acknowledgment format
                attempt++;
                printf("Invalid or unexpected acknowledgment, retrying #%d\n", attempt);
                continue; // Retry sending the packet
            }
        }
    }
    free(packet);
    // Maximum attempts reached, consider it a failure
    printf("Maximum attempts reached, data transfer failed\n");
    return 0;
}


int rudp_recv(RUDPSocket* socket, void* buffer, size_t buffer_size) {
    if (socket == NULL || buffer == NULL || buffer_size == 0) {
        return -1; // Invalid parameters
    }

    // Receive the packet using the UDP socket
    RUDPHeader received_header;
    ssize_t recv_bytes = recvfrom(socket->sockfd, &received_header, sizeof(RUDPHeader), 0, NULL, NULL);

    if (recv_bytes == -1) {
        perror("Error receiving RUDP packet");
        return -1;
    }

    // Check if the received packet is an acknowledgment
    if (received_header.flags == RUDP_ACK) {
        // Process acknowledgment
        
        // update the expected_ack_num
        socket->expected_ack_num = received_header.ack;

        printf("Received acknowledgment. Acknowledgment number: %u\n", received_header.ack);

        return 1; // Success (acknowledgment received)
    }
     else {
        // Received data packet
        size_t data_length = ntohs(received_header.length);

        // Check if the buffer size is sufficient
        if (data_length > buffer_size) {
            fprintf(stderr, "Buffer size is not sufficient for received data. Expected size: %zu\n", data_length);
            return -1;
        }

        // Receive the actual data
        recv_bytes = recvfrom(socket->sockfd, buffer, data_length, 0, NULL, NULL);

        if (recv_bytes == -1) {
            perror("Error receiving RUDP data");
            return -1;
        }

        // Verify the checksum of the received data
        uint16_t received_checksum = calculate_checksum(buffer, data_length);

        if (received_checksum != received_header.checksum) {
            fprintf(stderr, "Checksum verification failed. Discarding the received data.\n");
            return -1;
        }

        // Send acknowledgment for the received data
        RUDPHeader ack_header;
        ack_header.flags = RUDP_ACK;
        ack_header.ack = socket->next_seq_num; // Update with the appropriate acknowledgment number

        ssize_t sent_bytes = sendto(socket->sockfd, &ack_header, sizeof(RUDPHeader), 0,
                                    (struct sockaddr*)&(socket->peer_addr), sizeof(struct sockaddr_in));

        if (sent_bytes == -1) {
            perror("Error sending acknowledgment");
            return -1;
        }

        return 1; // Success (data received, checksum verified, and acknowledgment sent)
    }
}



 void rudp_close(RUDPSocket* socket) {
    if (socket == NULL) {
        return;
    }
    close(socket->sockfd);
    // Free the memory allocated for the socket
    free(socket);
}

