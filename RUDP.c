#include "RUDP_API.h"
#define TIMEOUT_SECONDS 2
#define TIMEOUT_MICROSECONDS 0
#define SOCKADDR_IN_SIZE sizeof(struct sockaddr_in)



RUDP_Socket* rudp_socket(bool isServer, unsigned short int listen_port) {
    RUDP_Socket* socket1 = (RUDP_Socket*)malloc(sizeof(RUDP_Socket));
    if (socket1 == NULL) {
        perror("Error creating RUDP socket");
        return NULL;
    }

    // Initialize fields
    socket1->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket1->socket_fd == -1) {
        perror("Error creating UDP socket");
        free(socket1);
        return NULL;
    }

    // Initialize other fields
    memset(&(socket1->dest_addr), 0, sizeof(struct sockaddr_in)); // Initialize destination address structure
    socket1->isServer = isServer; // Set state based on the isServer parameter
    socket1->isConnected = false; // Set initial connection state

    if (isServer) {
        // If it's a server socket, bind to the specified port
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons(listen_port);

        if (bind(socket1->socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            perror("Error binding UDP socket");
            close(socket1->socket_fd);
            free(socket1);
            return NULL;
        }
    }

    // Additional initialization as needed

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

 int rudp_send(RUDP_Socket* socket, const void* data, size_t length) {
    if (socket == NULL || data == NULL || length == 0) {
        return -1; // Invalid parameters
    }

    // Create an RUDPHeader 
    RUDPHeader header;
    header.flags = RUDP_ACK; 
    header.length = htons(length);
    header.checksum = 0; 
    header.ack = 0;

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
        // Send the packet using the RUDP socket
        ssize_t sent_bytes = sendto(socket->socket_fd, packet, packet_size, 0,
                            (struct sockaddr*)&(socket->dest_addr), sizeof(struct sockaddr_in));

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
        FD_SET(socket->socket_fd, &readfds);

        // Use select to wait for the socket to become ready for reading or timeout.
        int select_result = select(socket->socket_fd + 1, &readfds, NULL, NULL, &timeout);

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
            ssize_t recv_bytes = recvfrom(socket->socket_fd, &ack_header, sizeof(RUDPHeader), 0, NULL, NULL);

            if (recv_bytes == sizeof(RUDPHeader) && ack_header.flags == RUDP_ACK) {
                // Handle acknowledgment processing
                if (ack_header.ack == header.ack) {
                    // Valid acknowledgment received
                    printf("Data transferred successfully\n");
                    free(packet);
                    return 1; // Success
                } 
                else {
                    // Invalid acknowledgment
                    attempt++;
                    printf("ACK received, but not valid, retrying #%d\n", attempt);
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




 int rudp_recv(RUDP_Socket* socket, void* buffer, size_t buffer_size) {
    if (socket == NULL || buffer == NULL || buffer_size == 0) {
        return -1; // Invalid parameters
    }

    // Receive the packet using the RUDP socket
    RUDPHeader received_header;
    ssize_t recv_bytes = recvfrom(socket->socket_fd, &received_header, sizeof(RUDPHeader), 0, NULL, NULL);

    if (recv_bytes == -1) {
        perror("Error receiving RUDP packet");
        return -1;
    }

    // Check if the received packet is an acknowledgment
    if (received_header.flags == RUDP_ACK) {

        return 1; // Success (acknowledgment received)
    } 
    else if (received_header.flags == RUDP_DATA) {
        // Received data packet
        size_t data_length = ntohs(received_header.length);

        // Check if the buffer size is sufficient
        if (data_length > buffer_size) {
            fprintf(stderr, "Buffer size is not sufficient for received data. Expected size: %zu\n", data_length);
            return -1;
        }

        // Receive the actual data
        recv_bytes = recvfrom(socket->socket_fd, buffer, data_length, 0, NULL, NULL);

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
        ack_header.ack = received_header.ack; // Use received acknowledgment number as the acknowledgment for the data packet
        ack_header.length = 0;

        ssize_t sent_bytes = sendto(socket->socket_fd, &ack_header, sizeof(RUDPHeader), 0,
                                    (struct sockaddr*)&(socket->dest_addr), sizeof(struct sockaddr_in));

        if (sent_bytes == -1) {
            perror("Error sending acknowledgment");
            return -1;
        }

        return 1; // Success (data received, checksum verified, and acknowledgment sent)
    } 
    else {
        // Invalid or unexpected packet format
        fprintf(stderr, "Invalid or unexpected packet format\n");
        return -1;
    }
}




 void rudp_close(RUDP_Socket* socket) {
    if (socket == NULL) {
        // Handle invalid socket
        return;
    }
    // Close the socket
    close(socket->socket_fd);
    // Free the memory allocated for the socket
    free(socket);
}



int rudp_connect(RUDP_Socket *sockfd, const char *dest_ip, unsigned short int dest_port) {
    if (sockfd == NULL || dest_ip == NULL) {
        return 0; // Invalid parameters
    }

    if (sockfd->isConnected || sockfd->isServer) {
        return 0; // Socket is already connected or set to server
    }

    // Initialize the destination address
    memset(&(sockfd->dest_addr), 0, sizeof(struct sockaddr_in));
    sockfd->dest_addr.sin_family = AF_INET;
    sockfd->dest_addr.sin_port = htons(dest_port);

    // Convert IP address to binary form
    if (inet_pton(AF_INET, dest_ip, &(sockfd->dest_addr.sin_addr)) <= 0) {
        perror("Invalid IP address");
        return 0; // Failure
    }

    sockfd->isConnected = true;
    return 1; // Success
}

int rudp_accept(RUDP_Socket *sockfd) {
    if (sockfd == NULL) {
        return 0; // Invalid parameter
    }

    if (sockfd->isConnected || !sockfd->isServer) {
        return 0; // Socket is already connected or not set to server
    }

    // Accept the incoming connection
    sockfd->isConnected = true;
    return 1; // Success
}
