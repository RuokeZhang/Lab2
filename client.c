#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>

#define SERVER_IP       // Define the server IP address here
#define BUFFER_SIZE 17  // MSS size
#define DATA_SIZE 2     // Data portion size
#define SEQ_NUM_SIZE 11 // Sequence number size
#define MAXWAITTIME 2   // Maximum wait time for ACKs in seconds

int main(int argc, char *argv[]) {
    int sockfd;
    int portNumber;
    struct sockaddr_in server_address;
    char serverIP[29];
    char send_buffer[BUFFER_SIZE];
    char recv_buffer[100];  // Buffer for reading ACK, increased size for safety
    struct timeval timeout;
    socklen_t addrlen = sizeof(server_address);
    int seq_num = 0;
    portNumber = strtol(argv[2], NULL, 10);
    ssize_t bytes_sent;
    ssize_t rc;  // For storing the result of recvfrom
    int msg_length;
    char *data;
    time_t timeSent, currentTime;

    // Create a UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    portNumber = strtol(argv[2], NULL, 10);
    strcpy(serverIP, argv[1]);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(portNumber);
    server_address.sin_addr.s_addr = inet_addr(serverIP);

    // Step 1: Ask the user for a string
    printf("Please enter a string: ");
    char user_input[1024]; // Assuming the input won't be longer than 1024 characters
    fgets(user_input, sizeof(user_input), stdin);
    user_input[strcspn(user_input, "\n")] = 0; // Remove newline character

    // Step 2: Determine the length of the string
    msg_length = strlen(user_input);

    // Step 3: Send that length to the server
    sprintf(send_buffer, "%11d%4d", seq_num, msg_length);
    bytes_sent = sendto(sockfd, send_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_address, addrlen);
    if (bytes_sent < 0) {
        perror("Send to server failed");
        close(sockfd);
        return 1;
    }

    // Make sure the server received the message length
    printf("Sent message length %d to server\n", msg_length);

    // Step 4: Loop until all data is sent
    data = user_input; // Pointer to the current position in the user input string
    while (msg_length > 0) {
        int data_to_send = (msg_length >= DATA_SIZE) ? DATA_SIZE : msg_length;
        sprintf(send_buffer, "%11d%4d%s", seq_num, data_to_send, data);

        // a) send 2 bytes of data
        bytes_sent = sendto(sockfd, send_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_address, addrlen);
        if (bytes_sent < 0) {
            perror("Send to server failed");
            break;
        }
        timeSent = time(NULL); // Capture the time when the data was sent

        // b) wait for an ACK or timeout
        memset(recv_buffer, 0, sizeof(recv_buffer)); // Clear the buffer
        rc = recvfrom(sockfd, &recv_buffer, 100, MSG_DONTWAIT, (struct sockaddr *)&server_address, &addrlen);

        // Check if ACK was received
        if (rc > 0) {
            int ack_num;
            sscanf(recv_buffer, "%11d", &ack_num);
            if (ack_num == seq_num) {
                printf("ACK received for seq# %d\n", seq_num);
                seq_num += 2; // Increment sequence number
                data += data_to_send; // Move the pointer in the input string
                msg_length -= data_to_send; // Decrease the remaining message length
            }
        } else {
            currentTime = time(NULL);
            if (currentTime - timeSent > MAXWAITTIME) {
                printf("Timeout occurred, resending data...\n");
                // No need to resend here because it will automatically resend at the next loop iteration
                timeSent = currentTime; // Reset the timeSent to the current time
            }
        }
    }

    close(sockfd);
    return 0;
}
