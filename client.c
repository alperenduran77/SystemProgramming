#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>

#define DEFAULT_PORT 8080

volatile sig_atomic_t stop_flag = 0;

void handle_sigint(int sig) {
    stop_flag = 1;
}
void send_shutdown_message(const char *ip, int port) {
    struct sockaddr_in serv_addr;
    int sock = 0;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return;
    }

    char message[256];
    snprintf(message, sizeof(message), "Ctrl+C received by client (PID: %d)", getpid());
    send(sock, message, strlen(message), 0);
    printf("Shutdown message sent to server by client (PID: %d)\n", getpid());
    close(sock);
}


int get_random_location(int max_value) {
    return rand() % (max_value + 1);
}

void send_all_served_message(const char *ip, int port) {
    struct sockaddr_in serv_addr;
    int sock = 0;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return;
    }

    send(sock, "All customers served", strlen("All customers served"), 0);
    close(sock);
}

void* listen_to_server(void* arg) {
    FILE *log_file = fopen("client_log.txt", "a");
    if (!log_file) {
        perror("Failed to open log file");
        pthread_exit(NULL);
    }

    int sock = *(int*)arg;
    char buffer[1024] = {0};
    while (1) {
        int valread = read(sock, buffer, 1024);
        if (valread > 0) {
            buffer[valread] = '\0';
            fprintf(log_file, "Server message: %s\n", buffer);
            fflush(log_file);
        } else {
            break;
        }
    }
   
    fclose(log_file);
    pthread_exit(NULL);
}

int main(int argc, char const *argv[]) {
    if (argc != 5 && argc != 6) {
        fprintf(stderr, "Usage: %s <IP> <num_orders> <x_max_location> <y_max_location> [<port>]\n", argv[0]);
        return -1;
    }

    signal(SIGINT, handle_sigint);

    const char *ip = argv[1];
    int num_orders = atoi(argv[2]);
    int x_max_location = atoi(argv[3]);
    int y_max_location = atoi(argv[4]);
    int port = (argc == 6) ? atoi(argv[5]) : DEFAULT_PORT;

    srand(time(NULL));  // Seed the random number generator

    struct sockaddr_in serv_addr;
    int sock = 0;
    char buffer[1024] = {0};

    printf("PID %d....\n", getpid());

    // Send the number of orders to the server at the start
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    snprintf(buffer, sizeof(buffer), "%d", num_orders);
    send(sock, buffer, strlen(buffer), 0);
    close(sock);

    for (int i = 0; i < num_orders; i++) {
        int x_location = get_random_location(x_max_location);
        int y_location = get_random_location(y_max_location);

        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            printf("\n Socket creation error \n");
            return -1;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);

        if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
            printf("\nInvalid address/ Address not supported \n");
            return -1;
        }

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            printf("\nConnection Failed \n");
            return -1;
        }
        sleep(1);
        snprintf(buffer, sizeof(buffer), "Order from client %d (PID: %d) at location (%d, %d)", i, getpid(), x_location, y_location);
        send(sock, buffer, strlen(buffer), 0);
        printf("Order placed by client %d (PID: %d) at location (%d, %d)\n", i, getpid(), x_location, y_location);

        // Create a new thread to listen for messages from the server
        pthread_t listener_thread;
        pthread_create(&listener_thread, NULL, listen_to_server, (void*)&sock);

        // Wait for the listener thread to finish
        pthread_join(listener_thread, NULL);

        if (stop_flag) {
            printf(" ^C signal .. cancelling orders.. editing log..  \n");
            send_shutdown_message(ip, port);
            break;
        }
    }

    FILE *log_file = fopen("client_log.txt", "a");
    if (!log_file) {
        perror("Failed to open log file");
        return -1;
    }
    fprintf(log_file, "------------------------------------------\n");
    fclose(log_file);

    printf("All customers served \n");
    printf("log file written .. \n");
    send_all_served_message(ip, port); // Send the message to the server

    return 0;
}
