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

#define BUFFER_SIZE 17  // MSS size
#define DATA_SIZE 2     // Data portion size
#define SEQ_NUM_SIZE 11 // Sequence number size
#define MAXWAITTIME 2   // Maximum wait time for ACKs in seconds
#define WINDOW_SIZE 10  // Window size

int main(int argc, char *argv[])
{
    int sockfd;
    int i;
    int j;
    int portNumber;
    struct sockaddr_in server_address;
    char serverIP[29];
    char send_buffer[BUFFER_SIZE];
    char recv_buffer[100]; // Buffer for reading ACK, increased size for safety
    struct timeval timeout;
    socklen_t addrlen = sizeof(server_address);
    int seq_num = 0;
    portNumber = strtol(argv[2], NULL, 10);
    ssize_t bytes_sent;
    ssize_t rc; // For storing the result of recvfrom
    int msg_length = 0;
    char *data;
    time_t timeSent, currentTime;
    int data_offset = 0;

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <ServerIP> <PortNumber>\n", argv[0]);
        return 1;
    }

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
    int net_msg_length = htonl(msg_length); // Convert message length to network byte order

    // Send the length of the message to the server
    bytes_sent = sendto(sockfd, &net_msg_length, sizeof(net_msg_length), 0,
                        (struct sockaddr *)&server_address, addrlen);
    if (bytes_sent < 0)
    {
        perror("Send to server failed");
        close(sockfd);
        return 1;
    }

    // Make sure the server received the message length
    printf("Sent message length %d to server\n", msg_length);

    // 存储数据包的序列号
    int *acks_received = calloc(msg_length, sizeof(int));
    if (acks_received == NULL)
    {
        perror("Memory allocation for acks_received failed");
        close(sockfd);
        return 1;
    }

    // 使用循环初始化数组
    for (i = 0; i < msg_length; ++i)
    {
        acks_received[i] = 0; // 初始化数组元素为0
    }
    int window_start = 0;             // 窗口的左端
    int window_end = WINDOW_SIZE - 1; // 窗口的右端
    // Step 4: Loop until all data is sent
    int remaining_data = msg_length; // Remaining data to be sent

    while (remaining_data > 0)
    {
        for (i = window_start; i <= window_end && i < msg_length;)
        {
            if (!acks_received[i])
            {
                data_offset = i / 2 * DATA_SIZE;
                int data_to_send = (remaining_data >= DATA_SIZE) ? DATA_SIZE : 1;

                sprintf(send_buffer, "%11d%4d%s", i, data_to_send, user_input + data_offset);

                // a) send 2 bytes of data
                bytes_sent = sendto(sockfd, send_buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_address, addrlen);
                if (bytes_sent < 0)
                {
                    perror("Send to server failed");
                    break;
                }
                printf("sent %ld bytes, seqNumber %d, bottomOfwindow %d\n", bytes_sent, i, window_start);
            }
            i += 2;
        }
        timeSent = time(NULL);
        for (i = window_start; i <= window_end;)
        {
            if (!acks_received[i])
            {
                memset(recv_buffer, 0, sizeof(recv_buffer)); // Clear the buffer
                rc = recvfrom(sockfd, &recv_buffer, 100, MSG_DONTWAIT, (struct sockaddr *)&server_address, &addrlen);
                int ack_num;
                sscanf(recv_buffer, "%11d", &ack_num);
                if (ack_num == i)
                {
                    printf("ACK received %d\n", ack_num);
                    remaining_data -= DATA_SIZE;
                    acks_received[i] = 1;                        // 标记ACK已经收到
                    window_start = i + 2;                        // 移动窗口的左端
                    window_end = window_start + WINDOW_SIZE - 1; // 移动窗口的右端
                }
                else
                {
                    // printf("received ack is %d\n", ack_num);
                    // printf("expected ack is %d\n", i);
                    currentTime = time(NULL);
                    if (currentTime - timeSent > MAXWAITTIME)
                    {
                        printf("Timeout, sequence number = %d\n", i);

                        // Resend all packets in the window
                        for (j = i; j < WINDOW_SIZE + i - 1;)
                        {
                            acks_received[j] = 0;
                            j += 2;
                        }
                        break; // Exit the loop to resend packets
                    }
                }
            }
            i += 2;
        }
    }
    free(acks_received);
    close(sockfd);
    return 0;
}
