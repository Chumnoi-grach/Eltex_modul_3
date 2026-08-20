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

#define PORT 8888
#define BUFFER_SIZE 1024

int sockfd;
struct sockaddr_in broadcast_addr;

void signal_handler(int sig) {
    char msg[] = "УЧАСТНИК ПОКИНУЛ ЧАТ";
    sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
    close(sockfd);
    printf("\nЧат завершен.\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    int port = PORT;
    
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Неверный порт. Используйте 1-65535\n");
            exit(1);
        }
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }
    
    int broadcast = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        perror("setsockopt");
        exit(1);
    }
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt");
        exit(1);
    }

    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(sockfd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("bind");
        exit(1);
    }
    
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(port);
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    
    signal(SIGINT, signal_handler);
    
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);
    
    char join_msg[] = "НОВЫЙ УЧАСТНИК ВОШЕЛ В ЧАТ";
    sendto(sockfd, join_msg, strlen(join_msg), 0,
           (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
    
    printf("Чат запущен. Введите сообщения (Ctrl+C для выхода):\n");
    printf("> ");
    fflush(stdout);
    
    fd_set readfds;
    char buffer[BUFFER_SIZE];
    
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);
        
        int max_fd = (sockfd > STDIN_FILENO) ? sockfd : STDIN_FILENO;
        select(max_fd + 1, &readfds, NULL, NULL, NULL);
        
        // Получение сообщений
        if (FD_ISSET(sockfd, &readfds)) {
            struct sockaddr_in sender;
            socklen_t sender_len = sizeof(sender);
            int n = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
                           (struct sockaddr*)&sender, &sender_len);
            if (n > 0) {
                buffer[n] = '\0';
                
                struct sockaddr_in local_check;
                socklen_t local_len = sizeof(local_check);
                getsockname(sockfd, (struct sockaddr*)&local_check, &local_len);
                
                if (sender.sin_addr.s_addr != local_check.sin_addr.s_addr) {
                    char ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &sender.sin_addr, ip, sizeof(ip));
                    printf("\r[%s] %s\n", ip, buffer);
                    printf("> ");
                    fflush(stdout);
                }
            }
        }
        
        // Отправка сообщений
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            fgets(buffer, BUFFER_SIZE, stdin);
            buffer[strcspn(buffer, "\n")] = 0;
            
            if (strlen(buffer) > 0) {
                sendto(sockfd, buffer, strlen(buffer), 0,
                       (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
            }
            printf("> ");
            fflush(stdout);
        }
    }
    
    close(sockfd);
    return 0;
}