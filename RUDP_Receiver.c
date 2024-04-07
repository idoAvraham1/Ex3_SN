#include "RUDP.h"
#include <stdio.h>

#define MAX_RUNS 50



// Structure to store statistics for each run
struct RunStatistics {
    double time;    // Time taken for the run in milliseconds
    double speed;   // Data transfer speed in MB/s
};

void printStatistics(struct RunStatistics* statistics, int numRuns);
void calcTime(int fileSize, struct timeval start, struct RunStatistics* runStatistics, int numRuns);

int main(int argc,char** argv) {

   // Check command line arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s -p <port> -algo <algorithm>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // time statistics variables
    struct RunStatistics runStatistics[MAX_RUNS];
    int numRuns = 0;
    struct timeval start;

    // Parse command line arguments
    int port = atoi(argv[2]);

    
    // Create a UDP connection between the Receiver and the Sender.
    int receiver_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (receiver_socket == -1) {
        printf("Could not create socket\n");
        return -1;
    }

    struct sockaddr_in receiverAddress;
    memset((char *)&receiverAddress, 0, sizeof(receiverAddress));
    receiverAddress.sin_family = AF_INET;
    receiverAddress.sin_port = htons(port);
    int ret = inet_pton(AF_INET, (const char *)DEFAULT_IP, &(receiverAddress.sin_addr));
    if (ret <= 0) {
        printf("inet_pton() failed\n");
        return -1;
    }

    struct sockaddr_in senderAddress;
    memset((char *)&senderAddress, 0, sizeof(senderAddress));

    printf("Starting Receiver...\n");

    int bindResult = bind(receiver_socket, (struct sockaddr *)&receiverAddress, sizeof(receiverAddress));
    if (bindResult == -1) {
        close(receiver_socket);
        return -1;
    }

    //Get a connection from the sender
    printf("Waiting for RUDP Connection...\n");
    int recvResult = rudp_receive(receiver_socket, &senderAddress);
    if(recvResult<=0){return -1;}
    printf("Sender connected, beginning to receive file...\n");

    // Receive the file.
    int keepReceiving = 1;
    int measureTime=1;
    while(keepReceiving) {

        // count the total of bytes received
        int totalReceived = 0;

        // start measriung time
        gettimeofday(&start,NULL);
        while (1) {
            
            int receiveResult = rudp_receive(receiver_socket, &senderAddress);

            // add the received data to the total sent
            if(receiveResult > 0){totalReceived+=receiveResult;}
            
            // if failed return -1,
            if (receiveResult == -1) { return -1; }
            

            // if got exitMessage break
            else if (receiveResult == 0) {
                keepReceiving =0;
                break; }

            //if got EOF break
            else if (receiveResult == -2) {
                break; }
        }
        
            if (measureTime) {
                  // data sent. calc the time it took 
            calcTime(totalReceived, start, runStatistics, numRuns);
            numRuns++;

            // Wait for Sender response:
            printf("Waiting for Sender response...\n");
            int receiveChoice = rudp_receive(receiver_socket,&senderAddress);

            // if no respone, exit
            if(receiveChoice == -1){return -1;}


            // handle case where sender wants to exit
            if(receiveChoice == 2) {
                printf("Sender sent exit message...\n");
                measureTime=0;
            }
            // handle case where sender is sending again
            else {
                printf("Sender sending  again...\n");
                totalReceived=0;
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
    //printStatistics(runStatistics, numRuns);

    printf("----------------------------------\n");

    // Exit and close connections
    close(receiver_socket);
    return 0;
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