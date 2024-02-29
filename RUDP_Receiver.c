#include "RUDP_API.h"
#include <stdio.h>

#define MAX_RUNS 50

struct RunStatistics {
    double time;    // Time taken for the run in milliseconds
    double speed;   // Data transfer speed in MB/s
};

void printStatistics(struct RunStatistics* statistics, int numRuns);
void calcTime(int fileSize, struct timeval start, struct RunStatistics* runStatistics, int numRuns);

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s -p <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Parse command line arguments
    int port = atoi(argv[2]);

    // array for the times
    struct RunStatistics runStatistics[MAX_RUNS];
    int numRuns = 0;

    // Create a RUDP receiver
    RUDP_Socket* receiver_socket = rudp_socket(true, port); // Server socket
    if (receiver_socket == NULL) {
        perror("Error creating RUDP socket for receiver");
        return EXIT_FAILURE;
    }

    printf("Starting Receiver...\n");

    struct timeval start;
    gettimeofday(&start, NULL);  // Initialize start time

    // Accept incoming connection
    if (rudp_accept(receiver_socket) == 0) {
        perror("Error accepting connection");
        rudp_close(receiver_socket);
        return EXIT_FAILURE;
    }

    int fileSize;
    char* buffer = getBuffer(&fileSize, receiver_socket);

    // Main loop to receive data and sender response
    while (1) {
        // Receive the packet using the RUDP socket
        int recvResult = rudp_recv(receiver_socket, buffer, sizeof(buffer));

        if (recvResult == -1) {
            fprintf(stderr, "Error receiving RUDP packet\n");
            break;
        }

        // calc the time for the packet
        calcTime(fileSize, start, runStatistics, numRuns);
        numRuns++;

        // Check for sender's decision
        char senderDecision;
        int decisionResult = rudp_recv(receiver_socket, &senderDecision, sizeof(char));

        if (decisionResult == -1) {
            fprintf(stderr, "Error receiving sender's decision\n");
            break;
        }

        // Process sender's decision
        if (senderDecision == 'E') {
            printf("Sender wants to exit\n");
            break;  // Exit the loop and finish
        } else if (senderDecision == 'R') {
            printf("Sender is sending again\n");
        }
    }

    printf("Receiver End\n");

    // Print statistics after receiving the exit message
    printf("----------------------------------\n");
    printf("- * Statistics * -\n");
    printStatistics(runStatistics, numRuns);
    printf("----------------------------------\n");

    // Cleanup
    free(buffer);
    rudp_close(receiver_socket);

    return 0;
}

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

// Function to dynamically allocate a buffer for the data
char* getBuffer(int* fileSize, RUDP_Socket* receiverSocket) {
    // Receive the file size from the sender
    int recvSizeResult = rudp_recv(receiverSocket, fileSize, sizeof(int));

    if (recvSizeResult == -1) {
        fprintf(stderr, "Error receiving file size\n");
        return NULL;
    }

    // Dynamically allocate a buffer for the data
    char* buffer = (char*)malloc(*fileSize);
    if (buffer == NULL) {
        perror("Error allocating memory for buffer");
        return NULL;
    }

    return buffer;
}

// Function to calculate and store statistics
void calcTime(int fileSize, struct timeval start, struct RunStatistics* runStatistics, int numRuns) {
    struct timeval end;
    gettimeofday(&end, NULL);
    double elapsedTime = (end.tv_sec - start.tv_sec) * 1000.0;  // Convert to milliseconds
    elapsedTime += (end.tv_usec - start.tv_usec) / 1000.0;

    double speed = (fileSize / elapsedTime) * 1000.0 / (1024 * 1024);  // Speed in MB/s

    runStatistics[numRuns].time = elapsedTime;
    runStatistics[numRuns].speed = speed;
}
