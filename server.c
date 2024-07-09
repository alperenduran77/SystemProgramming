#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <math.h>
#include "pseudo.h"

#define OVEN_CAPACITY 6
#define MAX_CLIENTS 250
#define DELIVERY_CAPACITY 3
#define QUEUE_SIZE 300
#define DEFAULT_PORT 8080

typedef struct {
    int socket;
    struct sockaddr_in address;
    int location_x;
    int location_y;
    int client_id;
    pid_t pid;
} client_t;

typedef struct {
    int client_id;
    int location_x;
    int location_y;
    int cook_id;
    int delivery_id;
    pid_t pid;
    int client_socket;
} order_t;

pthread_mutex_t lock;
pthread_cond_t order_cond;
pthread_cond_t ready_cond;
pthread_cond_t oven_cond;

order_t order_queue[QUEUE_SIZE];
int order_queue_start = 0;
int order_queue_end = 0;

order_t ready_queue[QUEUE_SIZE];
int ready_queue_start = 0;
int ready_queue_end = 0;

int cook_thread_pool_size;
int delivery_thread_pool_size;
int delivery_speed;

int server_fd;
FILE *log_file;
int total_orders = 0;
int pending_orders = 0;
volatile sig_atomic_t stop_flag = 0;
volatile sig_atomic_t all_orders_serviced = 0;

int oven_count = 0;

#define MAX_COOKS 20
#define MAX_DELIVERIES 20
// Counters for each cook and delivery person
int cook_order_count[MAX_COOKS] = {0};
int delivery_order_count[MAX_DELIVERIES] = {0};

void enqueue_order(order_t queue[], int *start, int *end, order_t order) {
    queue[*end] = order;
    *end = (*end + 1) % QUEUE_SIZE;
}

order_t dequeue_order(order_t queue[], int *start, int *end) {
    order_t order = queue[*start];
    *start = (*start + 1) % QUEUE_SIZE;
    return order;
}

int is_queue_empty(int start, int end) {
    return start == end;
}
void* handle_client(void* arg) {
    client_t *client = (client_t*)arg;
    char buffer[1024] = {0};
    read(client->socket, buffer, 1024);  // Read the order from the client

    // Parse the client PID and other details from the buffer
    sscanf(buffer, "Client PID: %d", &client->pid);

    int num_orders;
    if (sscanf(buffer, "%d new customers", &num_orders) == 1) {
        printf("%d new customers... serving\n", num_orders);
        fflush(stdout);
        fprintf(log_file, "%d new customers... serving\n", num_orders);
        close(client->socket);
        free(client);
        pthread_exit(NULL);
    }
    if (strncmp(buffer, "Ctrl+C received by client (PID:", 30) == 0) {
        int pid;
        sscanf(buffer, "Ctrl+C received by client (PID: %d)", &pid);
        printf("order cancelled PID %d\n", pid);  // Print message on server side
        fflush(stdout);
        fprintf(log_file, "order cancelled PID %d\n", pid);
        fflush(log_file);
        printf("^C.. Upps quiting.. writing log file\n");
        fflush(stdout);
        close(client->socket);
        free(client);
        pthread_exit(NULL);
    }

    if (strncmp(buffer, "All customers served", 20) == 0) {
        // Find the most efficient cook
        int most_efficient_cook = 0;
        int max_cook_orders = 0;
        for (int i = 0; i < cook_thread_pool_size; i++) {
            if (cook_order_count[i] > max_cook_orders) {
                max_cook_orders = cook_order_count[i];
                most_efficient_cook = i;
            }
        }

        // Find the most efficient delivery person
        int most_efficient_delivery = 0;
        int max_delivery_orders = 0;
        for (int i = 0; i < delivery_thread_pool_size; i++) {
            if (delivery_order_count[i] > max_delivery_orders) {
                max_delivery_orders = delivery_order_count[i];
                most_efficient_delivery = i;
            }
        }

        printf("Thanks Cook %d and Moto %d\n", most_efficient_cook, most_efficient_delivery);
        fflush(stdout);
        fprintf(log_file, "Thanks Cook %d and Moto %d\n", most_efficient_cook, most_efficient_delivery);
        fflush(log_file);
        printf("active waiting for connections\n");
        fflush(stdout);
        fprintf(log_file, "active waiting for connections\n");
        fflush(log_file);

        close(client->socket);
        free(client);
        pthread_exit(NULL);
    }

    fprintf(log_file, "Received order: %s\n", buffer);
    fflush(log_file);

    sscanf(buffer, "Order from client %d (PID: %d) at location (%d, %d)", &client->client_id, &client->pid, &client->location_x, &client->location_y);

    pthread_mutex_lock(&lock);
    order_t order = {client->client_id, client->location_x, client->location_y, -1, -1, client->pid, client->socket};
    enqueue_order(order_queue, &order_queue_start, &order_queue_end, order);
    total_orders++;
    pending_orders++;
    pthread_cond_signal(&order_cond);
    pthread_mutex_unlock(&lock);

    // Send order placed confirmation message to the client
    char confirmation_message[256];
    snprintf(confirmation_message, sizeof(confirmation_message), "Order for client %d (PID: %d) placed successfully", client->client_id, client->pid);
    send(client->socket, confirmation_message, strlen(confirmation_message), 0);
    
    while (pending_orders > 0) {
        sleep(1);
    }

    close(client->socket);
    free(client);
    pthread_exit(NULL);
}
void* cook_thread(void* arg) {
    int cook_id = *(int*)arg;
    free(arg);

    while (1) {
        pthread_mutex_lock(&lock);
        while (is_queue_empty(order_queue_start, order_queue_end) && !stop_flag) {
            pthread_cond_wait(&order_cond, &lock);
        }
        if (stop_flag) {
            pthread_mutex_unlock(&lock);
            pthread_exit(NULL);
        }

        order_t order = dequeue_order(order_queue, &order_queue_start, &order_queue_end);
        order.cook_id = cook_id;
        cook_order_count[cook_id]++;  // Increment the cook's order count
        pthread_mutex_unlock(&lock);

        //fprintf(log_file, "Cook %d received order from client %d (PID: %d) for location (%d, %d)\n", cook_id, order.client_id, order.pid, order.location_x, order.location_y);
        //fflush(log_file);

        // Simulate preparation time by calculating the pseudo-inverse of a 30x40 matrix
        double matrix[MATRIX_ROWS * MATRIX_COLS];
        double pseudo_inv[MATRIX_COLS * MATRIX_ROWS];

        // Initialize matrix with random values
        for (int i = 0; i < MATRIX_ROWS * MATRIX_COLS; i++) {
            matrix[i] = rand() / (double)RAND_MAX;
        }

        // Measure preparation time
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        pseudo_inverse(matrix, MATRIX_ROWS, MATRIX_COLS, pseudo_inv);
        clock_gettime(CLOCK_MONOTONIC, &end);
        double preparation_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
 
        // Print the preparation time
        fprintf(log_file, "Preparation time for cook %d: %.6f seconds\n", cook_id, preparation_time);
        fflush(log_file);
        sleep(preparation_time);
        fprintf(log_file, "Cook %d prepared order for client %d.\n",cook_id,order.client_id);
        fflush(log_file);
        // Send order prepared message to the client
        char prepared_message[256];
        snprintf(prepared_message, sizeof(prepared_message), "Order for client %d (PID: %d) has been prepared by cook %d.", order.client_id, order.pid, cook_id);
        send(order.client_socket, prepared_message, strlen(prepared_message), 0);

        // Place the meal in the oven
        pthread_mutex_lock(&lock);
        while (oven_count >= OVEN_CAPACITY) {
            pthread_cond_wait(&oven_cond, &lock);
        }
        oven_count++;
        pthread_mutex_unlock(&lock);

        // Simulate cooking time
         sleep(preparation_time/2);
        
        fprintf(log_file, "Cook %d cooked order for client %d.\n",cook_id,order.client_id);
        fflush(log_file);

        char cook_message[256];
        snprintf(cook_message, sizeof(cook_message), "Order for client %d (PID: %d) has been cooked by cook %d.", order.client_id, order.pid, cook_id);
        send(order.client_socket, cook_message, strlen(cook_message), 0);

        // Meal is ready, remove from oven
        pthread_mutex_lock(&lock);
        oven_count--;
        enqueue_order(ready_queue, &ready_queue_start, &ready_queue_end, order);
        pthread_cond_signal(&oven_cond);
        pthread_cond_signal(&ready_cond);
        pthread_mutex_unlock(&lock);

    }

    pthread_exit(NULL);
}

void* delivery_thread(void* arg) {
    int delivery_id = *(int*)arg;
    free(arg);

    while (1) {
        pthread_mutex_lock(&lock);

        // Collect up to DELIVERY_CAPACITY orders
        order_t delivery_orders[DELIVERY_CAPACITY];
        int order_count = 0;

        while (order_count < DELIVERY_CAPACITY && !is_queue_empty(ready_queue_start, ready_queue_end)) {
            delivery_orders[order_count] = dequeue_order(ready_queue, &ready_queue_start, &ready_queue_end);
            delivery_orders[order_count].delivery_id = delivery_id;
            order_count++;
        }

        // Wait if there are no orders to deliver
        while (order_count == 0 && !stop_flag) {
            pthread_cond_wait(&ready_cond, &lock);
            while (order_count < DELIVERY_CAPACITY && !is_queue_empty(ready_queue_start, ready_queue_end)) {
                delivery_orders[order_count] = dequeue_order(ready_queue, &ready_queue_start, &ready_queue_end);
                delivery_orders[order_count].delivery_id = delivery_id;
                order_count++;
            }
        }

        if (stop_flag) {
            pthread_mutex_unlock(&lock);
            pthread_exit(NULL);
        }

        pthread_mutex_unlock(&lock);

        // Deliver the collected orders
        for (int i = 0; i < order_count; i++) {
            order_t order = delivery_orders[i];
            double distance = findSQRT(order.location_x * order.location_x + order.location_y * order.location_y);
            double time = (distance / delivery_speed);
            sleep(time);

            fprintf(log_file, "Delivered to location (%d, %d) by delivery person %d for client %d (PID: %d) at %f second\n", order.location_x, order.location_y, delivery_id, order.client_id, order.pid,time);
            fflush(log_file);
            fprintf(log_file,"----------------------------------------------------------------------------------------------------\n");
            printf("Done serving client %d (PID: %d) @ (%d, %d)\n", order.client_id, order.pid, order.location_x, order.location_y);
            fflush(stdout);

            // Send acknowledgment to client
            int client_socket = order.client_socket;
            char ack_message[1024];
            sprintf(ack_message, "Order for client %d (PID: %d) has been delivered by delivery person %d.", order.client_id, order.pid, delivery_id);
            send(client_socket, ack_message, strlen(ack_message), 0);
            close(client_socket);

            pthread_mutex_lock(&lock);
            delivery_order_count[delivery_id]++;  // Increment the delivery person's order count
            pending_orders--;
            if (pending_orders == 0) {
                all_orders_serviced = 1;
                pthread_cond_signal(&order_cond);  // Wake up the main thread if waiting
            }
            pthread_mutex_unlock(&lock);
        }
    }

    pthread_exit(NULL);
}

void sigint_handler(int sig) {
    printf("\nCtrl+C received. Preparing to shut down...\n");
    stop_flag = 1;

    // Broadcast the condition variable to wake up all threads
    pthread_cond_broadcast(&order_cond);
    pthread_cond_broadcast(&ready_cond);
    pthread_cond_broadcast(&oven_cond);
}

void* server_shutdown(void* arg) {
    while (!stop_flag) {
        sleep(1);
    }

    printf("Shutting down the server...\n");

    pthread_mutex_lock(&lock);
    while (!is_queue_empty(order_queue_start, order_queue_end)) {
        order_t order = dequeue_order(order_queue, &order_queue_start, &order_queue_end);
        printf("Order from client %d (PID: %d) for location (%d, %d)\n", order.client_id, order.pid, order.location_x, order.location_y);
    }
    while (!is_queue_empty(ready_queue_start, ready_queue_end)) {
        order_t order = dequeue_order(ready_queue, &ready_queue_start, &ready_queue_end);
        printf("Order from client %d (PID: %d) for location (%d, %d)\n", order.client_id, order.pid, order.location_x, order.location_y);
    }
    pthread_mutex_unlock(&lock);


    close(server_fd);
    fclose(log_file);

    exit(0);
}

int main(int argc, char const *argv[]) {
    if (argc != 5 && argc != 6) {
        fprintf(stderr, "Usage: %s <IP> <cook_thread_pool_size> <oven_capacity> <delivery_speed> [<port>]\n", argv[0]);
        return -1;
    }

    const char *ip = argv[1];
    cook_thread_pool_size = atoi(argv[2]);
    int oven_capacity = atoi(argv[3]);
    delivery_speed = atoi(argv[4]);
    int port = (argc == 6) ? atoi(argv[5]) : DEFAULT_PORT;

    if (cook_thread_pool_size > MAX_COOKS) cook_thread_pool_size = MAX_COOKS;
    if (oven_capacity > OVEN_CAPACITY) oven_capacity = OVEN_CAPACITY;
    if (delivery_speed > MAX_DELIVERIES) delivery_speed = MAX_DELIVERIES;

    int opt = 1;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    client_t *client;

    pthread_t cook_threads[MAX_COOKS];
    pthread_t delivery_threads[MAX_DELIVERIES];
    pthread_t shutdown_thread;

    log_file = fopen("server_log.txt", "a");
    if (log_file == NULL) {
        perror("Failed to open log file");
        return -1;
    }

    signal(SIGINT, sigint_handler);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip);
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < cook_thread_pool_size; i++) {
        int *cook_id = malloc(sizeof(int));
        *cook_id = i;
        pthread_create(&cook_threads[i], NULL, cook_thread, cook_id);
    }

    for (int i = 0; i < delivery_speed; i++) {
        int *delivery_id = malloc(sizeof(int));
        *delivery_id = i;
        pthread_create(&delivery_threads[i], NULL, delivery_thread, delivery_id);
    }

    pthread_create(&shutdown_thread, NULL, server_shutdown, NULL);

    printf("PideShop active waiting for connection …\n");
    fflush(stdout);

    while (1) {
        if ((client = malloc(sizeof(client_t))) == NULL) {
            perror("Failed to allocate memory for client");
            continue;
        }
        if ((client->socket = accept(server_fd, (struct sockaddr *)&client->address, (socklen_t *)&addrlen)) < 0) {
            perror("accept");
            free(client);
            continue;
        }
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void *)client);
        pthread_detach(thread_id);
    }

    for (int i = 0; i < cook_thread_pool_size; i++) {
        pthread_join(cook_threads[i], NULL);
    }
    for (int i = 0; i < delivery_speed; i++) {
        pthread_join(delivery_threads[i], NULL);
    }
    pthread_join(shutdown_thread, NULL);

    return 0;
}

