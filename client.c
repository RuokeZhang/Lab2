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

#define BUFFER_SIZE 17
#define DATA_SIZE 2
#define SEQ_NUM_SIZE 11
#define MAXWAITTIME 1
#define WINDOW_SIZE 10

int main(int argc, char *argv[])
{
    int sd;
    int i;
    int j;
    int portNumber;
    struct sockaddr_in server_address;
    char serverIP[29];
    char send_buffer[BUFFER_SIZE];
    char recv_buffer[11]; // Buffer for reading ACK, increased size for safety
    struct timeval timeout;
    socklen_t addrlen = sizeof(server_address);
    int seq_num = 0;
    portNumber = strtol(argv[2], NULL, 10);
    ssize_t bytes_sent;
    ssize_t rc; // For storing the result of recvfrom
    int msg_length = 0;
    char *data;
    time_t timeSent, currentTime;
    int remainingBytes;

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <ServerIP> <PortNumber>\n", argv[0]);
        return 1;
    }

    // Create a UDP socket
    sd = socket(AF_INET, SOCK_DGRAM, 0);
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
    remainingBytes = msg_length;
    // Step 3: Send that length to the server
    sprintf(send_buffer, "%11d%4d", seq_num, msg_length);
    int net_msg_length = htonl(msg_length); // Convert message length to network byte order

    // Send the length of the message to the server
    bytes_sent = sendto(sd, &net_msg_length, sizeof(net_msg_length), 0,
                        (struct sockaddr *)&server_address, addrlen);
    if (bytes_sent < 0)
    {
        perror("Send to server failed");
        close(sd);
        return 1;
    }

    printf("Sent message length %d to server\n", msg_length);

    int window_start = 0;
    int window_end = WINDOW_SIZE;
    if (window_end >= msg_length)
    {
        window_end = msg_length;
    }
    int acc_ack = 0, wish_ack;
    while (window_start < msg_length)
    {
        wish_ack = 0;
        for (i = window_start; i < window_end; i += 2)
        {
            int data_to_send = (msg_length - i >= DATA_SIZE) ? DATA_SIZE : msg_length - i;
            memset(send_buffer, 0, BUFFER_SIZE);
            sprintf(send_buffer, "%11d%4d", i, data_to_send);
            memcpy(send_buffer + 15, user_input + i, data_to_send);

            bytes_sent = sendto(sd, send_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_address, addrlen);
            if (bytes_sent < 0)
            {
                perror("Send to server failed");
                break;
            }
            printf("sent %ld bytes, seqNumber %d, bottomOfwindow %d, end of the window %d, data: '%.*s'\n",
                   bytes_sent, i, window_start, window_end, data_to_send, send_buffer + SEQ_NUM_SIZE + 4);
            wish_ack++;
        }

        timeSent = time(NULL);
        acc_ack = 0;

        while (acc_ack < wish_ack)
        {
            currentTime = time(NULL);
            if (currentTime - timeSent > MAXWAITTIME)
            {
                printf("Timeout, sequence number = %d, doinig resending\n", window_start);
                break;
            }

            memset(recv_buffer, 0, sizeof(recv_buffer));
            rc = recvfrom(sd, &recv_buffer, 11, MSG_DONTWAIT, (struct sockaddr *)&server_address, &addrlen);
            if (rc > 0)
            {
                int ack_num;
                sscanf(recv_buffer, "%11d", &ack_num);
                if (ack_num == window_start)
                {
                    printf("received ACK: %d\n", ack_num);
                    acc_ack++;
                    remainingBytes -= 2;
                    if (remainingBytes < 0)
                    {
                        remainingBytes = 0;
                    }
                    window_start += 2;
                    window_end = window_start + WINDOW_SIZE;
                    if (window_end > msg_length)
                    {
                        window_end = msg_length;
                    }
                    if (window_start == window_end)
                    {
                        break;
                    }
                }
            }
        }
    }
    close(sd);
    return 0;
}
