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

int main(int argc, char *argv[]) {
    if (argc != 4 || strcmp(argv[1], "-a") != 0 || strcmp(argv[3], "-f") != 0) {
        fprintf(stderr, "Usage: %s -a <receiver_ip> -f <file_path>\n", argv[0]);
        exit(1);
    }

    char *receiver_ip = argv[2];
    char *file_path = argv[4];

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[6])); // Assuming -p is used for port

    if (inet_pton(AF_INET, receiver_ip, &server_addr.sin_addr) <= 0)
        error("ERROR converting IP address");

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        error("ERROR connecting");

    printf("Connected to Receiver. Starting file transmission...\n");

    FILE *file = fopen(file_path, "rb");
    if (file == NULL)
        error("ERROR opening file");

    char buffer[MAX_BUFFER_SIZE];
    size_t bytesRead;

    // Send file
    do {
        bytesRead = fread(buffer, 1, sizeof(buffer), file);
        if (bytesRead < 0)
            error("ERROR reading from file");

        if (send(sockfd, buffer, bytesRead, 0) < 0)
            error("ERROR writing to socket");

    } while (bytesRead > 0);

    printf("File transmission complete.\n");

    // User decision
    char decision;
    printf("Do you want to send the file again? (a for yes, any other key for no): ");
    scanf(" %c", &decision);

    while (decision == 'a') {
        // Send 'a' to request additional runs
        send(sockfd, &decision, sizeof(decision), 0);

        // Send file again
        rewind(file);  // Reset file pointer to the beginning
        do {
            bytesRead = fread(buffer, 1, sizeof(buffer), file);
            if (bytesRead < 0)
                error("ERROR reading from file");

            if (send(sockfd, buffer, bytesRead, 0) < 0)
                error("ERROR writing to socket");

        } while (bytesRead > 0);

        printf("File transmission complete.\n");

        // User decision
        printf("Do you want to send the file again? (a for yes, any other key for no): ");
        scanf(" %c", &decision);
    }

    // Send exit message
    decision = 'e';
    send(sockfd, &decision, sizeof(decision), 0);

    // Close the TCP connection
    fclose(file);
    close(sockfd);

    printf("Sender end.\n");

    return 0;
}
