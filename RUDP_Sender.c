#include "RUDP.h"


// Function to read content from a file and return it along with its size
char* readFromFile(int* size);

// Global variables
char *fileName = "tosend.txt";

int main(int argc,char** argv) {

     // Check command line arguments
    if (argc != 5) {
        fprintf(stderr, "Usage: %s -a <receiver_ip> -f <file_path>\n", argv[0]);
        exit(1);
    }

    // Parse command line arguments
    int port = atoi(argv[4]);
    char *receiver_ip = argv[2];

    // File-related variables
    char *fileContent =NULL;
    int fileSize = 0;


    //Create a UDP socket between the Sender and the Receiver.
    int sender_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sender_socket < 0){
        return -1;
    }

    // Create a timeout of receive in sender socket
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 1000;

    setsockopt(sender_socket,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));

    struct sockaddr_in receiverAddress;
    memset(&receiverAddress, 0, sizeof(receiverAddress));
    receiverAddress.sin_family = AF_INET;
    receiverAddress.sin_port = htons(port);
    int rval = inet_pton(AF_INET, (const char *)receiver_ip, &receiverAddress.sin_addr);
    if (rval <= 0) {
        printf("inet_pton() failed");
        return -1;
    }
    struct sockaddr_in fromAddress;
    memset((char *)&fromAddress, 0, sizeof(fromAddress));

    // Connecet to receiver
    printf("Sending connect message to receiver\n");
    int connectionResult = rudp_connect(sender_socket,&receiverAddress,&fromAddress);
    if(connectionResult <= 0){
        printf("Connction to Receiver Failed\n");
        return -1;
    }
    printf("got ACK connection successful, sending file\n");

    // read the file
    fileContent = readFromFile(&fileSize);

    //Send the file to the receiver
    int userChoice = 1;

    while(userChoice) {
        printf("Sending file...\n");
        
        // Send data in chunks
        char data[MESSAGE_SIZE];
        int i;
        for(i=0;i+MESSAGE_SIZE<fileSize;i+=MESSAGE_SIZE) {
            strncpy(data,fileContent+i,MESSAGE_SIZE);
            rudp_sendDataPacket(sender_socket, data, &receiverAddress, &fromAddress);
        }
        char* lastData = (char*) malloc(sizeof (char)*fileSize-i);
        strncpy(lastData, fileContent+i,(fileSize-i));
        rudp_sendDataPacket(sender_socket, lastData, &receiverAddress, &fromAddress);
        free(lastData);

        // Send the EOF
        char endOfFile[1] = {EOF};
        rudp_sendDataPacket(sender_socket, endOfFile, &receiverAddress, &fromAddress);

        // waiting for user descision
        printf("Resend the file? 1 for resend, 0 for exit \n");
        scanf("%d",&userChoice);
        
        // send the data agagin
        if(userChoice == 1){
            int sendChoice = rudp_sendDataPacket(sender_socket,"yes",&receiverAddress, &fromAddress);
            if(sendChoice < 0){
                printf("send() failed\n");
                return -1;
            }
        }
        // send to the receiver exit 
        if(userChoice == 0){
            int sendChoice = rudp_sendDataPacket(sender_socket,"no",&receiverAddress, &fromAddress);
            if(sendChoice < 0){
                printf("send() failed\n");
                return -1;
            }
        }
    }


     //Send an exit message to the receiver.
    printf("Sending disconnect message to receiver\n");
    int diconnectionResult = rudp_disconnect(sender_socket,&receiverAddress,&fromAddress);
    if(diconnectionResult<=0){
        printf("Disconnction to Receiver Failed\n");
        return -1;
    }
    printf("Got Ack from receiver, sender Exit...\n");


    //Close the connection and exit 
    close(sender_socket);
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
