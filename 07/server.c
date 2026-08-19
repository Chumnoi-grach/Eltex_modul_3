#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <errno.h>
#include <time.h>
#include <sys/epoll.h>



// socket() → bind() → listen() → accept() → recv()/send(). 
// Клиент: socket() → connect() → send()/recv()



#define PORT 8889
#define MAX_CLIENTS 30
#define BUFFER_SIZE 4096
#define FILE_BUFFER 8192
#define MAX_EVENTS 30
#define MESSAGE_SIZE (BUFFER_SIZE + 64)

int running = 1;
int epoll_fd;
void send_file(const char *filename);
void receive_file(const char *filename, long file_size);
void handle_server_message();
void handle_console_command(char *buffer);

void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\nПоступил сигнал SIGINT (Ctrl + C)\n");
        printf("Выход из чата...\n");
        running = 0;
    }
}



typedef struct {
    int fd;
    char name[32];
    int active;
} Client;

Client clients[MAX_CLIENTS];
int server_fd;
int client_count = 0;

void init_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].active = 0;
        memset(clients[i].name, 0, sizeof(clients[i].name));
    }
}

int find_free_slot() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            return i;
        }
    }
    return -1;
}

Client* find_client_by_fd(int fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].fd == fd) {
            return &clients[i];
        }
    }
    return NULL;
}

void broadcast_message(int sender_fd, const char *message) {    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].fd != sender_fd) {
            send(clients[i].fd, message, strlen(message), 0);
        }
    }
}

void broadcast_file(int sender_fd, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Ошибка: файл %s не найден", filename);
        send(sender_fd, error_msg, strlen(error_msg), 0);
        return;
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char file_info[256];
    snprintf(file_info, sizeof(file_info), "FILE_START:%s:%ld", filename, file_size);
    broadcast_message(sender_fd, file_info);
    
    // Отправляем содержимое файла
    char buffer[FILE_BUFFER];
    size_t bytes_read;
    long total_sent = 0;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active && clients[i].fd != sender_fd) {
                send(clients[i].fd, buffer, bytes_read, 0);
            }
        }
        total_sent += bytes_read;
    }
    
    fclose(file);
    
    // Отправляем конец файла
    char file_end[] = "FILE_END";
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].fd != sender_fd) {
            send(clients[i].fd, file_end, strlen(file_end), 0);
        }
    }
    
    char success_msg[256];
    snprintf(success_msg, sizeof(success_msg), "Файл %s отправлен всем (размер: %ld байт)", filename, file_size);
    send(sender_fd, success_msg, strlen(success_msg), 0);
}

void handle_client_message(int client_fd) {
    char buffer[BUFFER_SIZE];
    int n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    printf("Поступил пакет: %s\n", buffer);
    if (n <= 0) {
        // Клиент отключился
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd == client_fd && clients[i].active) {
                char leave_msg[256];
                snprintf(leave_msg, sizeof(leave_msg), "LEAVE:%s", clients[i].name);
                broadcast_message(client_fd, leave_msg);
                printf("[%s] Отключился\n", clients[i].name);
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                
                clients[i].active = 0;
                clients[i].fd = -1;
                client_count--;
                close(client_fd);
                break;
            }
        }
        return;
    }
    
    buffer[n] = '\0';
    
    Client *client = find_client_by_fd(client_fd);
    if (!client || !client->active) return;
    
    if (strncmp(buffer, "CLOSE:", 6) == 0) {
        char leave_msg[256];
        snprintf(leave_msg, sizeof(leave_msg), "LEAVE:%s", client->name);
        broadcast_message(client_fd, leave_msg);
        printf("[%s] Отключился (CLOSE)\n", client->name);
        
        int idx = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd == client_fd) {
                idx = i;
                break;
            }
        }
        
        if (idx != -1) {
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
            clients[idx].active = 0;
            clients[idx].fd = -1;
            client_count--;
        }
        close(client_fd);
        return;
    }
    
    if (strncmp(buffer, "FILE_START:", 11) == 0) {
        char filename[256];
        long file_size;
        if (sscanf(buffer, "FILE_START:%255[^:]:%ld", filename, &file_size) == 2) {
            printf("Файл: %s, Размер: %ld\n", filename, file_size);
        }
        else {
            printf("Файл: не удалось распарсить\n");
        }

        printf("[%s] Отправляет файл: %s (размер: %ld)\n", client->name, filename, file_size);
        
        char full_msg[MESSAGE_SIZE];
        snprintf(full_msg, sizeof(full_msg), "FILE_START:%s:%ld", filename, file_size);
        broadcast_message(client_fd, full_msg);
        printf("Файл отправлен\n");

        return;
    }
    
    if (strcmp(buffer, "FILE_END") == 0) {
        printf("[%s] Файл отправлен полностью\n", client->name);
        broadcast_message(client_fd, "FILE_END");
        return;
    }
    
    if (strncmp(buffer, "MES:", 4) == 0) {
        char full_msg[MESSAGE_SIZE];
        snprintf(full_msg, sizeof(full_msg), "MES:[%s] %s", client->name, buffer + 4);
        printf("[%s] %s\n", client->name, buffer + 4);
        broadcast_message(client_fd, full_msg);
        return;
    }
    
    printf("Отправляю %s\n", buffer);
    broadcast_message(client_fd, buffer);
    printf("Отправил %s\n", buffer);
}

void handle_new_connection() {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) {
        perror("accept");
        return;
    }
    
    if (client_count >= MAX_CLIENTS) {
        char msg[] = "Сервер переполнен. Попробуйте позже.";
        send(client_fd, msg, strlen(msg), 0);
        close(client_fd);
        printf("Сервер переполнен. Отказ клиенту.\n");
        return;
    }
    
    
    int slot = find_free_slot();
    if (slot < 0) {
        close(client_fd);
        return;
    }
    
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = client_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
        perror("epoll_ctl: client");
        close(client_fd);
        return;
    }
    
    char init_msg[BUFFER_SIZE];
    int n = recv(client_fd, init_msg, sizeof(init_msg) - 1, 0);
    if (n <= 0) {
        close(client_fd);
        return;
    }
    init_msg[n] = '\0';

    char name[32];
    if (strncmp(init_msg, "INIT:", 5) == 0) {
        sscanf(init_msg, "INIT:%s", name);
    } else {
        close(client_fd);
        return;
    }
    
    clients[slot].fd = client_fd;
    strcpy(clients[slot].name, name);
    clients[slot].active = 1;
    client_count++;
    
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
    printf("[%s] Подключился (IP: %s, FD: %d)\n", name, ip, client_fd);
    printf("Всего клиентов: %d\n", client_count);
    
    char join_msg[256];
    snprintf(join_msg, sizeof(join_msg), "%s присоединился к чату", name);
    broadcast_message(client_fd, join_msg);
}

int main() {
    init_clients();
    signal(SIGINT, signal_handler);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }
    
    int reuse = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(1);
    }
    
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(1);
    }
    
    printf("TCP ЧАТ СЕРВЕР\n");
    printf("Порт: %d\n", PORT);
    printf("Сервер запущен. Ожидание подключений...\n\n");
    
    epoll_fd = epoll_create1(0);

    if (epoll_fd < 0) {
        perror("epoll_create");
        exit(1);
    }
    

    struct epoll_event ev;
    ev.data.fd = server_fd;
    ev.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
        perror("epoll_ctl: server");
        exit(1);
    }

    struct epoll_event events[MAX_EVENTS];

    // int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);

    while (running) {
        // timeout: -1 = бесконечно
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            if (errno == EINTR) break; //поменять на continue при желании
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;
            
            if (fd == server_fd) {
                handle_new_connection();
            }
            else {
                Client *client = find_client_by_fd(fd);
                if (client && client->active) {
                    handle_client_message(fd);
                }
            }
        }
    }
    
    close(server_fd);
    return 0;
}