#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

int changeCCAlgorithm(int socketfd, char* algo);
int socketSetup(struct sockaddr_in *serverAddress, int port, char* algo, char* ip);
int sendData(int clientSocket, void* buffer, int len);
char* readFromFile(int* size);
int authCheck(int socketfd);

char *fileName = "tosend.txt";
char *CC_reno = "reno";
char *CC_cubic = "cubic";

int main(int argc, char *argv[]) {
    if (argc != 7) {
        fprintf(stderr, "Usage: %s -a <receiver_ip> -f <file_path>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[4]);
    char *algorithm = argv[6];
    char *receiver_ip = argv[2];

    char *fileContent = NULL;
    int socketfd = -1, fileSize = 0;
    struct sockaddr_in serverAddress;

    printf("Sender starting\n");
    fileContent = readFromFile(&fileSize);

    socketfd = socketSetup(&serverAddress, port, algorithm, receiver_ip);

    if (connect(socketfd, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) == -1) {
        perror("connect");
        exit(1);
    }

    printf("Connected successfully to the Receiver\n");

    // send the file size to the receiver
    printf("Sending the size...\n");
    sendData(socketfd, &fileSize, sizeof(int));

    printf("Sending the data for the first time...\n");
    sendData(socketfd, fileContent, fileSize);

    while (true) {
    int choice = -1, ret = EOF;
    printf("Send the file again? (1 to resend, 0 to exit.) \n");
    ret = scanf("%d", &choice);

    while (ret != 1 || (choice != 1 && choice != 0)) {
        scanf("%*s");
        ret = scanf("%d", &choice);
    }

    if (!choice) {
        // Send exit command to the receiver
        char exitCommand = 'E';
        sendData(socketfd, &exitCommand, sizeof(char));

        printf("Exiting...\n");
        break;
    } else {
        // Send resend command to the receiver
        char resendCommand = 'R';
        sendData(socketfd, &resendCommand, sizeof(char));
    }

    // Continue with sending file data
    sendData(socketfd, fileContent, fileSize);
}


    close(socketfd);
    free(fileContent);
    printf("Sender exit.\n");
    return 0;
}

int sendData(int socketfd, void* buffer, int len) {
    int sentd = send(socketfd, buffer, len, 0);

    if (sentd == -1) {
        perror("send");
        exit(1);
    } else if (!sentd)
        printf("Receiver doesn't accept requests.\n");
    else if (sentd < len)
        printf("Data was only partly send (%d/%d bytes).\n", sentd, len);
    
    return sentd;
}

int socketSetup(struct sockaddr_in *serverAddress, int port, char* algo, char* ip) {
    int socketfd = -1;

    memset(serverAddress, 0, sizeof(*serverAddress));
    serverAddress->sin_family = AF_INET;
    serverAddress->sin_port = htons(port);

    if (inet_pton(AF_INET, (const char*) ip, &serverAddress->sin_addr) == -1) {
        perror("inet_pton()");
        exit(1);
    }

    if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket()");
        exit(1);
    }

    changeCCAlgorithm(socketfd, algo);

    return socketfd;
}

int changeCCAlgorithm(int socketfd, char* algo) {
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

    return 0;
}

char* readFromFile(int* size) {
    FILE *fpointer = NULL;
    char* fileContent;

    fpointer = fopen(fileName, "r");

    if (fpointer == NULL) {
        perror("fopen");
        exit(1);
    }

    // Find the file size and allocate enough memory for it.
    fseek(fpointer, 0L, SEEK_END);
    *size = (int) ftell(fpointer);
    fileContent = (char*) malloc(*size * sizeof(char));
    fseek(fpointer, 0L, SEEK_SET);

    fread(fileContent, sizeof(char), *size, fpointer);
    fclose(fpointer);

    printf("File \"%s\" total size is %d bytes.\n", fileName, *size);

    return fileContent;
}
