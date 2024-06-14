#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define BUFFER_SIZE 2048

int client_socket;
char name[32];
volatile sig_atomic_t flag = 0;

void handle_signal(int signal);
void *send_message(void *arg);
void *receive_message(void *arg);
void cleanup_and_exit();

int main() {
    struct sockaddr_in server_addr;
    pthread_t send_thread, receive_thread;
    struct sigaction sa;

    // Set up signal handler for graceful shutdown
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Could not create socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    printf("Enter your name: ");
    fgets(name, 32, stdin);
    name[strcspn(name, "\n")] = '\0'; // Remove newline character

    // Send name to the server
    if (write(client_socket, name, strlen(name)) < 0) {
        perror("Write to server failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    printf("Connected to the chat server as %s.\n", name);

    // Create threads for sending and receiving messages
    pthread_create(&send_thread, NULL, send_message, NULL);
    pthread_create(&receive_thread, NULL, receive_message, NULL);

    pthread_join(send_thread, NULL);
    pthread_join(receive_thread, NULL);

    cleanup_and_exit();
    return 0;
}

void *send_message(void *arg) {
    char buffer[BUFFER_SIZE];

    while (1) {
        if (flag) {
            printf("채팅을 종료할까요? (y / n): ");
            char response[3];
            fgets(response, 3, stdin);
            if (response[0] == 'y' || response[0] == 'Y') {
                cleanup_and_exit();
            } else {
                flag = 0;
            }
        } else {
            fgets(buffer, BUFFER_SIZE, stdin);
            if (write(client_socket, buffer, strlen(buffer)) < 0) {
                perror("Write to server failed");
                break;
            }
        }
    }

    return NULL;
}

void *receive_message(void *arg) {
    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = read(client_socket, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }

    return NULL;
}

void handle_signal(int signal) {
    flag = 1;
}

void cleanup_and_exit() {
    printf("\nDisconnecting from the chat server...\n");
    close(client_socket);
    exit(EXIT_SUCCESS);
}
