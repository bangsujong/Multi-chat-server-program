#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048
#define PORT 8888

typedef struct {
    struct sockaddr_in address;
    int sockfd;
    char name[32];
} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int server_socket;

void handle_client(client_t *client);
void send_message_to_all_clients(char *message);
void send_message_to_all_clients_except_sender(char *message, int sender_sockfd);
void remove_client(int client_socket);
void handle_exit(int signal);
void list_current_users(int client_socket);

void *client_handler(void *arg) {
    client_t *client = (client_t *)arg;
    handle_client(client);
    return NULL;
}

int main() {
    int new_socket, addr_len;
    struct sockaddr_in server_addr, client_addr;
    pthread_t tid;
    struct sigaction sa;

    // Set up signal handler for graceful shutdown
    sa.sa_handler = handle_exit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    // Initialize client sockets list
    memset(clients, 0, sizeof(clients));

    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Could not create socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_socket, 3) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Chat server started on port %d\n", PORT);

    while (1) {
        addr_len = sizeof(struct sockaddr_in);
        new_socket = accept(server_socket, (struct sockaddr *)&client_addr, (socklen_t *)&addr_len);
        if (new_socket < 0) {
            perror("Accept failed");
            continue;
        }

        pthread_mutex_lock(&clients_mutex);

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] == NULL) {
                client_t *client = (client_t *)malloc(sizeof(client_t));
                client->address = client_addr;
                client->sockfd = new_socket;
                clients[i] = client;
                pthread_create(&tid, NULL, client_handler, (void *)client);
                pthread_detach(tid);
                break;
            }
        }

        pthread_mutex_unlock(&clients_mutex);
    }

    return 0;
}

void handle_client(client_t *client) {
    char buffer[BUFFER_SIZE];
    int bytes_read;

    // Receive the client's name
    if ((bytes_read = read(client->sockfd, client->name, sizeof(client->name) - 1)) > 0) {
        client->name[bytes_read] = '\0';
        char join_msg[BUFFER_SIZE];
        snprintf(join_msg, sizeof(join_msg), "--- 새로운 사용자 [%s]이 참가 했습니다. ---\n", client->name);
        send_message_to_all_clients(join_msg);
        list_current_users(client->sockfd);

        printf("[%s] joined.\n", client->name);
    }

    while ((bytes_read = read(client->sockfd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        char message[BUFFER_SIZE];
        snprintf(message, sizeof(message), "[%s]: %s", client->name, buffer);
        send_message_to_all_clients_except_sender(message, client->sockfd);

        printf("[%s]: %s", client->name, buffer);
    }

    remove_client(client->sockfd);
    close(client->sockfd);
    pthread_exit(NULL);
}

void send_message_to_all_clients(char *message) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL) {
            if (write(clients[i]->sockfd, message, strlen(message)) < 0) {
                perror("Write to client failed");
                remove_client(clients[i]->sockfd);
                close(clients[i]->sockfd);
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void send_message_to_all_clients_except_sender(char *message, int sender_sockfd) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL && clients[i]->sockfd != sender_sockfd) {
            if (write(clients[i]->sockfd, message, strlen(message)) < 0) {
                perror("Write to client failed");
                remove_client(clients[i]->sockfd);
                close(clients[i]->sockfd);
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int client_socket) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL && clients[i]->sockfd == client_socket) {
            char leave_msg[BUFFER_SIZE];
            snprintf(leave_msg, sizeof(leave_msg), "--- 사용자 [%s]이 나갔습니다. ---\n", clients[i]->name);
            send_message_to_all_clients(leave_msg);

            printf("[%s] leaved.\n", clients[i]->name);
            
            free(clients[i]);
            clients[i] = NULL;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void handle_exit(int signal) {
    printf("\nShutting down server...\n");
    close(server_socket);
    exit(EXIT_SUCCESS);
}

void list_current_users(int client_socket) {
    pthread_mutex_lock(&clients_mutex);
    char user_list[BUFFER_SIZE] = "현재 대화 중인 사용자:\n";
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL) {
            strcat(user_list, clients[i]->name);
            strcat(user_list, "\n");
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    write(client_socket, user_list, strlen(user_list));
}
