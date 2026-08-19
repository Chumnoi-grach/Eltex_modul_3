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

#define PORT 8889
#define BUFFER_SIZE 4096
#define FILE_BUFFER 8192
#define SERVER_IP "127.0.0.1"

int sockfd;
char username[32];
int running = 1;

void send_file(const char *filename);
void receive_file(const char *filename, long file_size);
void handle_server_message();
void handle_console_command(char *buffer);

void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\nПоступил сигнал SIGINT (Ctrl + C)\n");
        printf("Выход из чата...\n");
        running = 0;
        //close(sockfd);
    }
}

void receive_file(const char *filename, long file_size) {

    char full_filename[256];
    snprintf(full_filename, sizeof(full_filename), "received_%s", filename);
    
    int fd = open(full_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        return;
    }
    
    char buffer[FILE_BUFFER];
    long received = 0;
    int n;
    
    while (received < file_size) {
        n = recv(sockfd, buffer, sizeof(buffer), 0);
        if (n <= 0) break;
        write(fd, buffer, n);
        received += n;
    }
    
    close(fd);
    printf("\nФайл сохранен как: %s\n", full_filename);
}

void handle_server_message() {
    char buffer[BUFFER_SIZE + 256];
    int n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    
    if (n <= 0) {
        printf("\nСоединение с сервером потеряно.\n");
        running = 0;
        return;
    }
    
    buffer[n] = '\0';
    
    if (strncmp(buffer, "JOIN:", 5) == 0) {
        printf("\r*** %s присоединился к чату ***\n", buffer + 5);
        printf("> ");
        fflush(stdout);
        return;
    }
    
    if (strncmp(buffer, "LEAVE:", 6) == 0) {
        printf("\r*** %s покинул чат ***\n", buffer + 6);
        printf("> ");
        fflush(stdout);
        return;
    }
    
    if (strncmp(buffer, "FILE_START:", 11) == 0) {
        char filename[256];
        long file_size;
        sscanf(buffer, "FILE_START:%255[^:]:%ld", filename, &file_size);
        receive_file(filename, file_size);
        printf("> ");
        fflush(stdout);
        return;
    }
    
    if (strcmp(buffer, "FILE_END") == 0) {
        printf("\nПолучение файла завершено\n");
        printf("> ");
        fflush(stdout);
        return;
    }
    
    if (strncmp(buffer, "MES:", 4) == 0) {
        printf("\r%s\n", buffer + 4);
    } else {
        printf("\r%s\n", buffer);
    }
    
    printf("> ");
    fflush(stdout);
}

//если еще будут команды
void handle_console_command(char *buffer) {
    char command[30];
    while (*buffer == ' ') buffer++;
    strncpy(command, buffer, strcspn(buffer, " "));
    command[strcspn(buffer, " ")] = '\0';

    //file
    if (strcmp(command, "/file") == 0) {
        char *filename = buffer + strcspn(buffer, " ");
        while (*filename == ' ') filename++;

        if (strlen(filename) > 0) {
            printf("Отправка файла: %s\n", filename);
            send_file(filename);
        } else {
            printf("Использование: /file <имя_файла>\n");
        }
    //message
    } else {
        char mes[BUFFER_SIZE];
        snprintf(mes, sizeof(mes), "MES:%s", buffer);

        send(sockfd, mes, strlen(mes), 0);
    }
    printf("> ");
    fflush(stdout);
}


void send_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Ошибка: не могу открыть файл %s\n", filename);
        return;
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Отправляем заголовок
    char header[256];
    snprintf(header, sizeof(header), "FILE_START:%s:%ld", filename, file_size);
    send(sockfd, header, strlen(header), 0);
    sleep(1);
    
    // Отправляем данные по частям
    char buffer[FILE_BUFFER];
    size_t bytes;
    long sent = 0;
    
    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(sockfd, buffer, bytes, 0);
        sent += bytes;
        printf("Отправлено: %ld / %ld байт (%.1f%%)", 
               sent, file_size, (float)sent / file_size * 100);
        fflush(stdout);
    }
    sleep(1);
    // Отправляем конец
    char end[] = "FILE_END";
    send(sockfd, end, strlen(end), 0);
    
    fclose(file);
    printf("\nФайл %s отправлен (размер: %ld байт)\n", filename, file_size);
    printf("> ");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        perror("argv parameters");
        exit(1);
    }
    int port = PORT;
    strcpy(username, argv[1]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }
    
    // Настройка адреса сервера
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    
    printf("Пытаюсь законектиться с сервером\n");
    // Подключение к серверу
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        printf("Убедитесь, что сервер запущен на порту %d\n", PORT);
        exit(1);
    }
    printf("Законектился с сервером\n");

    char system_messege[BUFFER_SIZE];
    snprintf(system_messege, sizeof(system_messege), "INIT:%s", username);
    send(sockfd, system_messege, strlen(system_messege), 0);
    
    printf("Отправил имя серверу\n");

    signal(SIGINT, signal_handler);
    
    printf("\nЧАТ ЗАПУЩЕН\n");
    printf("Ваш ник: %s\n", username);
    printf("Сервер: %s:%d\n", SERVER_IP, PORT);
    printf("Функции:\n");
    printf("  /file <имя_файла> - отправить файл всем\n");
    printf("  Ctrl+C - выход\n\n");
    printf("> ");
    fflush(stdout);
    
    fd_set read_fds;
    char buffer[BUFFER_SIZE];
    
    while (running) {
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        
        int max_fd = (sockfd > STDIN_FILENO) ? sockfd : STDIN_FILENO;
        
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            if (running) {
                perror("select");
            }
            break;
        }
        
        // Сообщение от сервера
        if (FD_ISSET(sockfd, &read_fds)) {
            handle_server_message();
        }
        
        // Ввод с клавиатуры
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strcspn(buffer, "\n")] = '\0';
            
            if (strlen(buffer) == 0) {
                printf("> ");
                fflush(stdout);
                continue;
            }
            
            // Обработка команд
            handle_console_command(buffer);
            
            fflush(stdout);
        }
    }
    snprintf(system_messege, sizeof(system_messege), "CLOSE:%s", username);
    send(sockfd, system_messege, strlen(system_messege), 0);
    
    close(sockfd);
    return 0;
}