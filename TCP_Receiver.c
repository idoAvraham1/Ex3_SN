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

#define MAX_RUNS 50

// Structure to store statistics for each run
struct RunStatistics {
    double time;    // Time taken for the run in milliseconds
    double speed;   // Data transfer speed in MB/s
};

// Function to set the congestion control algorithm for the socket
void SetCCAlgorithm(int socketfd, char* algo);

// Function to set up the socket for communication
int socketSetup(struct sockaddr_in *serverAddress, int port, char* algo);

// Function to receive data from the client
int getDataFromClient(int clientSocket, void *buffer, int len);

// Function to send data to the client
int sendData(int clientSocket, void* buffer, int len);

// Function to print statistics for each run
void printStatistics(struct RunStatistics* statistics, int numRuns);

// Function to calculate time and speed for a run
void calcTime(int fileSize, struct timeval start, struct RunStatistics* runStatistics, int numRuns);

// Main function
int main(int argc, char *argv[]) {
    // Check command line arguments
    if (argc != 5) {
        fprintf(stderr, "Usage: %s -p <port> -algo <algorithm>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // time statistics variables
    struct RunStatistics runStatistics[MAX_RUNS];
    int numRuns = 0;

    // Parse command line arguments
    int port = atoi(argv[2]);
    char *algorithm = argv[4];

    // Socket and address variables
    int clientSocket = -1;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    char clientAddress[INET_ADDRSTRLEN];
    char* buffer = NULL;

    //data releated variables
    int fileSize,
        totalReceived = 0,
        socketfd = -1;

    printf("Starting Receiver...\n");

    // Set up the socket
    socketfd = socketSetup(&serverAddr, port, algorithm);

    // Accept connection from the sender
    memset(&clientAddress, 0, sizeof(clientAddress));
    clientSocket = accept(socketfd, (struct sockaddr *) &clientAddr, &clientAddrLen);

    if (clientSocket == -1) {
        perror("accept");
        exit(1);
    }
    printf("Sender connected, beginning to receive file...\n");

    // Get sender's IP address
    inet_ntop(AF_INET, &(clientAddr.sin_addr), clientAddress, INET_ADDRSTRLEN);

    // Receive expected file size from the sender
    getDataFromClient(clientSocket, &fileSize, sizeof(int));

    printf("Expected file size is %d bytes.\n", fileSize);

    // Allocate memory for the file data
    buffer = malloc(fileSize * sizeof(char));

    if (buffer == NULL) {
        perror("malloc");
        exit(1);
    }

    _Bool continueReceiving = true;
    
    while (continueReceiving) {
        int BytesReceived;

        if (!totalReceived) {
            memset(buffer, 0, fileSize);
        }

        struct timeval start;
        gettimeofday(&start, NULL);  // Initialize start time

        // Receive data from the sender
        BytesReceived = getDataFromClient(clientSocket, buffer + totalReceived, fileSize - totalReceived);
        totalReceived += BytesReceived;

        if (!BytesReceived) {
            break;
        } else if (totalReceived == fileSize) {
            printf("File transfer completed, Received total %d bytes.\n", totalReceived);
            printf("Waiting for sender decision...\n");

            // Calculate the time for the packet
            calcTime(fileSize, start, runStatistics, numRuns);
            numRuns++;
            char exitCommand;

            // Get the sender's response
            getDataFromClient(clientSocket, &exitCommand, sizeof(char));

            if (exitCommand == 'E') {
                printf("Sender wants to exit\n");
                continueReceiving = false;
                break;
            } else if (exitCommand == 'R') {
                printf("Sender is sending again\n");
                totalReceived = 0;
                gettimeofday(&start, NULL);  // Reset start time for the new run
            }
        }
    }

    // Print statistics after receiving the exit message
    printf("----------------------------------\n");
    printf("- * Statistics * -\n");

    for (int i = 0; i < numRuns; i++) {
        printf("- Run #%d Data: Time=%.2fms; Speed=%.2fMB/s\n", i + 1, runStatistics[i].time, runStatistics[i].speed);
    }

    // Calculate and print averages
    printStatistics(runStatistics, numRuns);

    printf("----------------------------------\n");
    close(clientSocket);
    close(socketfd);
    free(buffer);
    printf("Receiver end.\n");
    return 0;
}

// Function to send data to the client
int sendData(int clientSocket, void* buffer, int len) {
    int sentd = send(clientSocket, buffer, len, 0);

    if (sentd == 0) {
        printf("Sender doesn't accept requests.\n");
    } else if (sentd < len) {
        printf("Data was only partly send (%d/%d bytes).\n", sentd, len);
    } else {
        printf("Total bytes sent is %d.\n", sentd);
    }

    return sentd;
}

// Function to receive data from the client
int getDataFromClient(int clientSocket, void *buffer, int len) {
    int recvb = recv(clientSocket, buffer, len, 0);

    if (recvb == -1) {
        perror("recv");
        exit(1);
    } else if (!recvb) {
        return 0;
    }

    return recvb;
}

// Function to set up the socket for communication
int socketSetup(struct sockaddr_in *serverAddress, int port, char* algo) {
    int socketfd = -1, canReused = 1;

    // Initialize server address structure
    memset(serverAddress, 0, sizeof(*serverAddress));
    serverAddress->sin_family = AF_INET;
    serverAddress->sin_addr.s_addr = INADDR_ANY;
    serverAddress->sin_port = htons(port);

    // Create a socket
    if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    // Set socket options
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &canReused, sizeof(canReused)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    // Bind the socket
    if (bind(socketfd, (struct sockaddr *)serverAddress, sizeof(*serverAddress)) == -1) {
        perror("bind");
        exit(1);
    }

    // Listen for connections
    if (listen(socketfd, 1) == -1) {
        perror("listen");
        exit(1);
    }

    printf("waiting for TCP connection \n");

    // Set congestion control algorithm
    SetCCAlgorithm(socketfd, algo);

    return socketfd;
}

// Function to set the congestion control algorithm for the socket
void SetCCAlgorithm(int socketfd, char* algo) {
    if (strcmp(algo, "reno") == 0) {
        socklen_t CC_reno_len = strlen("reno");

        if (setsockopt(socketfd, IPPROTO_TCP, 13, "reno", CC_reno_len) != 0) {
            perror("setsockopt");
            exit(1);
        }
    } else {
        socklen_t CC_cubic_len = strlen("cubic");

        if (setsockopt(socketfd, IPPROTO_TCP, 13, "cubic", CC_cubic_len) != 0) {
            perror("setsockopt");
            exit(1);
        }
    }
}

// Function to print statistics for each run
void printStatistics(struct RunStatistics* statistics, int numRuns) {
    double totalTime = 0.0;
    double totalSpeed = 0.0;

    for (int i = 0; i < numRuns; i++) {
        totalTime += statistics[i].time;
        totalSpeed += statistics[i].speed;
    }

    double avgTime = totalTime / numRuns;
    double avgSpeed = totalSpeed / numRuns;

    printf("- Average time: %.2fms\n", avgTime);
    printf("- Average bandwidth: %.2fMB/s\n", avgSpeed);
}

// Function to calculate time and speed for a run
void calcTime(int fileSize, struct timeval start, struct RunStatistics* runStatistics, int numRuns) {
    struct timeval end;
    gettimeofday(&end, NULL);
    double elapsedTime = (end.tv_sec - start.tv_sec) * 1000.0;  // Convert to milliseconds
    elapsedTime += (end.tv_usec - start.tv_usec) / 1000.0;

    double speed = (fileSize / elapsedTime) * 1000.0 / (1024 * 1024);  // Speed in MB/s

    runStatistics[numRuns].time = elapsedTime;
    runStatistics[numRuns].speed = speed;
}
