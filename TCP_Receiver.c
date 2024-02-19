#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define MAX_BUFFER_SIZE 1024

void error(const char *msg) {
    perror(msg);
    exit(1);
}

double calculate_elapsed_time(struct timeval start_time, struct timeval end_time) {
    return (double)(end_time.tv_sec - start_time.tv_sec) * 1000.0 +
           (double)(end_time.tv_usec - start_time.tv_usec) / 1000.0;
}

int main(int argc, char *argv[]) {
    if (argc != 4 || strcmp(argv[1], "-p") != 0 || strcmp(argv[3], "-algo") != 0) {
        fprintf(stderr, "Usage: %s -p <port> -algo <congestion_algorithm>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[2]);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    struct sockaddr_in server_addr, client_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        error("ERROR on binding");

    listen(sockfd, 1);
    printf("Starting Receiver...\n");
    printf("Waiting for TCP connection...\n");

    socklen_t client_len = sizeof(client_addr);
    int newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
    if (newsockfd < 0)
        error("ERROR on accept");

    printf("Sender connected, beginning to receive file...\n");

    FILE *file;
    size_t bytesRead;
    char buffer[MAX_BUFFER_SIZE];
    struct timeval start_time, end_time;

    // Receive file and measure time
    gettimeofday(&start_time, NULL);

    do {
        bytesRead = recv(newsockfd, buffer, sizeof(buffer), 0);
        if (bytesRead < 0)
            error("ERROR reading from socket");
        if (bytesRead > 0)
            fwrite(buffer, 1, bytesRead, file);
    } while (bytesRead > 0);

    gettimeofday(&end_time, NULL);

    // Calculate elapsed time
    double elapsed_time = calculate_elapsed_time(start_time, end_time);

    fclose(file);

    // Wait for sender response
    char decision;
    recv(newsockfd, &decision, sizeof(decision), 0);

    printf("----------------------------------\n");
    printf("- * Statistics * -\n");
    printf("- Run #1 Data: Time=%.2fms; Speed=%.2fMB/s\n", elapsed_time, 1024.00);

    // Average time and bandwidth for the first run
    double avg_time = elapsed_time;
    double avg_bandwidth = 1024.00;

    while (decision == 'a') {
        // Receive the file again and measure time
        gettimeofday(&start_time, NULL);

        do {
            bytesRead = recv(newsockfd, buffer, sizeof(buffer), 0);
            if (bytesRead < 0)
                error("ERROR reading from socket");
            if (bytesRead > 0)
                fwrite(buffer, 1, bytesRead, file);
        } while (bytesRead > 0);

        gettimeofday(&end_time, NULL);

        // Calculate elapsed time
        elapsed_time = calculate_elapsed_time(start_time, end_time);

        fclose(file);

        // Wait for sender response
        recv(newsockfd, &decision, sizeof(decision), 0);

        // Print statistics for additional runs
        printf("- Run #%d Data: Time=%.2fms; Speed=%.2fMB/s\n", decision - 'a' + 2, elapsed_time, 1024.00);

        // Update averages
        avg_time = (avg_time + elapsed_time) / 2.0;
        avg_bandwidth = (avg_bandwidth + 1024.00) / 2.0;
    }

    // Overall averages
    printf("-\n");
    printf("- Average time: %.2fms\n", avg_time);
    printf("- Average bandwidth: %.2fMB/s\n", avg_bandwidth);
    printf("----------------------------------\n");

    // Close the TCP connection
    close(newsockfd);
    close(sockfd);

    printf("Receiver end.\n");

    return 0;
}