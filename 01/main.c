#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <getopt.h>
#include <fcntl.h>

#define MAX_FILENAME 256
#define BUFFER_LENGTH 256
#define MSG_READY 1
#define MSG_FILE_INFO 2
#define MSG_DATA 3
#define MSG_DONE 4

typedef struct {
    int type;
    // 0x01 - готовность
    // 0x02 - информация о файле
    // 0x03 - данные
    // 0x04 - завершение
    int length;
    char filename[MAX_FILENAME];
} Message;


void child_func(int read_fd, int write_fd) {
    Message msg;
    char buffer[BUFFER_LENGTH];
    int out_fd = -1;
    
    // 1. Отправляем готовность
    msg.type = MSG_READY;
    msg.length = 0;
    if (write(write_fd, &msg, sizeof(Message)) < 0) {
        perror("child: write ready");
        return;
    }
    printf("Child: Sent READY\n");
    
    // 2. Основной цикл
    while (1) {
        int n = read(read_fd, &msg, sizeof(Message));
        if (n <= 0) {
            if (n == 0) {
                printf("Child: Parent closed pipe\n");
            } else {
                perror("child: read");
            }
            break;
        }
        
        switch (msg.type) {
            case MSG_FILE_INFO: {
                // Создаем файл .copy
                char copied_file[MAX_FILENAME + 10];
                snprintf(copied_file, sizeof(copied_file), "%s.copy", msg.filename);
                out_fd = open(copied_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (out_fd < 0) {
                    perror("child: create output file");
                    break;
                }
                
                printf("Child: Receiving file '%s' (%d bytes)\n", msg.filename, msg.length);
                
                // Получаем данные
                int bytes_received = 0;
                while (bytes_received < msg.length) {
                    // Читаем заголовок следующего блока
                    n = read(read_fd, &msg, sizeof(Message));
                    if (n <= 0) {
                        perror("child: read header");
                        break;
                    }
                    
                    if (msg.type == MSG_DONE) {
                        close(out_fd);
                        out_fd = -1;
                        return;
                    }
                    
                    if (msg.type == MSG_DATA) {
                        // Читаем данные
                        int bytes_received = 0;
                        while (bytes_received < msg.length) {
                            // Читаем заголовок
                            //n = read(read_fd, &msg, sizeof(Message));
                            if (n <= 0) break;
    
                            if (msg.type == MSG_DATA) {
                                // Читаем данные
                                int bytes_read = read(read_fd, buffer, msg.length);
                                //printf("Прочли: %s\n", buffer);
                                write(out_fd, buffer, bytes_read);
                                bytes_received += bytes_read;
                            }
                        }
                    }
                }
                
                close(out_fd);
                out_fd = -1;
                printf("Child: Finished receiving file (%d bytes)\n", bytes_received);
                break;
            }
            
            case MSG_DONE:
                printf("Child: Received DONE\n");
                if (out_fd >= 0) {
                    close(out_fd);
                    out_fd = -1;
                }
                return;
                
            default:
                fprintf(stderr, "Child: Unknown message type %d\n", msg.type);
                break;
        }
    }
}

void parent_func(int read_fd, int write_fd, char **argv, int argc) {
    Message msg;
    char buffer[BUFFER_LENGTH];

    if (read(read_fd, &msg, sizeof(Message)) <= 0) {
        exit(-1);
    }
    if (msg.type != MSG_READY) {
        exit(-1);
    }
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strchr(argv[i],'p')) {
                i++;
                continue;
            }
        }
            
        //Принятие готовности дочернего процесса принимать сообщение
        
        //Открытие файла для чтения
        int fd = open(argv[i], O_RDONLY);
        off_t size = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);

            
        int delivered_bytes;

        if (fd == -1) {
            printf("%s file is not exist\n", argv[i]);
        }
        else {
            //Отправка сообщения
            strcpy(msg.filename, argv[i]);
            msg.type = MSG_FILE_INFO;
            msg.length = (int)size;

            //Отправка сообщения обработки файла
            if (write(write_fd, &msg, sizeof(Message)) < 0) {
                perror("parent: write file info");
                close(fd);
                break;
            }
            //Отправка данных
            while ((delivered_bytes = read(fd, buffer, BUFFER_LENGTH)) > 0) {
                msg.type = MSG_DATA;
                msg.length = delivered_bytes;
                write(write_fd, &msg, sizeof(Message));
                write(write_fd, buffer, delivered_bytes);
            }
        }
    }

    msg.type = MSG_DONE;
    msg.length = 0;
    write(write_fd, &msg, sizeof(Message));

        //обработка сообщения о готовности передачи
        //скидываем имя файла и кол-во байт

        //!!! Содержимое файла передается блоками из нескольких байтов.
        //скидываем сообщение о завершении программы и ждем завершения
    //wait(NULL);
    //close(write_fd);
    //close(read_fd);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [-p pipe_name] file1 [file2 ...]\n", argv[0]);
        exit(1);
    }

    int is_named_pipe = 0;
    char pipe_name[FILENAME_MAX];
    int opt;

    int pipe_with_data;
    while ((opt = getopt(argc, argv, "p:")) != -1)  {
        switch (opt) {
            case 'p':
                if (strlen(optarg) < MAX_FILENAME) {
                    strcpy(pipe_name, optarg);
                    printf("pipe_name зарегистрирован: %s\n", pipe_name);
                    is_named_pipe = 1;
                }
                else {
                    perror("pipename is very long");
                }
                break;
            // case '?':
            //     fprintf(stderr, "Использование несуществующего флага: %c\n", opt);
            //     break;
            default:
                fprintf(stderr, "there is no such parameter for pipename\n");
                break;
        }
    }




    
    // for (int i = 1; i < argc; i++) {
    //     if (argv[i][0] == '-') {
    //         if (strchr(argv[i],'p')) {
    //             i++;
    //             continue;
    //         }
    //     }
    //     int fd = open(argv[i], O_RDONLY);

    //     if (fd == -1) {
    //         printf("%s file is not exist\n", argv[i]);
    //     }
    //     else {
    //         printf("%s file is there\n", argv[i]);
    //         close(fd);
    //     }
    // }

    printf("Создаем канал ");


    int parent_write_fd, parent_read_fd;
    int child_read_fd, child_write_fd;

    
    //close не забыть
    int pipefd[2];
    int pipefd2[2];

    char pipe_name2[256];
    


    // if (is_named_pipe){
    //     strcpy(pipe_name2, pipe_name);
    //     if (strlen(pipe_name2) >= MAX_FILENAME) {
    //         perror("pipename2 is very long");
    //     }
    //     strcat(pipe_name2, "2");

    //     printf("ИМЕНОВАННЫЙ 1\n");
    //     if ((mkfifo(pipe_name, 0666)) == -1) {
    //         fprintf(stderr, "Именованный канал не создан");
    //     }
    //     printf("ИМЕНОВАННЫЙ 2\n");
    //     if ((mkfifo(pipe_name2, 0666)) == -1) {
    //         fprintf(stderr, "Именованный канал не создан");
    //     }
    //     parent_write_fd = open(pipe_name, O_RDWR);
    //     parent_read_fd = open(pipe_name2, O_RDWR);
    //     printf("первое открытие\n");
    //     if (parent_write_fd < 0 || parent_read_fd < 0) {
    //         perror("parent: open named pipes");
    //         unlink(pipe_name);
    //         unlink(pipe_name2);
    //         exit(1);
    //     }
        
    //     // Открываем каналы для дочернего
    //     child_read_fd = open(pipe_name, O_RDONLY);
    //     child_write_fd = open(pipe_name2, O_WRONLY);
    //     printf("второе открытие\n");
    //     if (child_read_fd < 0 || child_write_fd < 0) {
    //         perror("child: open named pipes");
    //         close(parent_write_fd);
    //         close(parent_read_fd);
    //         unlink(pipe_name);
    //         unlink(pipe_name2);
    //         exit(1);
    //     }  
    // }
    // else {
    //     printf("НЕИМЕНОВАННЫЙ\n");
    //     if (pipe(pipefd) < 0 || pipe(pipefd2) < 0) {
    //         perror("pipe");
    //         exit(1);
    //     }
    //     parent_write_fd = pipefd[1];
    //     parent_read_fd = pipefd2[0];
    //     child_read_fd = pipefd[0];
    //     child_write_fd = pipefd2[1];
    // }
    


    if (is_named_pipe){
        strcpy(pipe_name2, pipe_name);
        if (strlen(pipe_name2) >= MAX_FILENAME) {
            perror("pipename2 is very long");
        }
        strcat(pipe_name2, "2");

        printf("ИМЕНОВАННЫЙ 1\n");
        if ((mkfifo(pipe_name, 0666)) == -1) {
            fprintf(stderr, "Именованный канал не создан");
            exit(0);
        }
        printf("ИМЕНОВАННЫЙ 2\n");
        if ((mkfifo(pipe_name2, 0666)) == -1) {
            fprintf(stderr, "Именованный канал не создан");
            exit(0);
        }
    }
    else {
        printf("НЕИМЕНОВАННЫЙ\n");
        if (pipe(pipefd) < 0 || pipe(pipefd2) < 0) {
            perror("pipe");
            exit(1);
        }
    }

    //Форкаем
    printf("Форкаем\n");
    int pid = fork();
    //error
    if (pid == -1) {
        perror("fork");
        return -1;
    }
    //Child




    else if (pid == 0) {
        
        if (is_named_pipe) {
        // Открываем каналы для дочернего
        child_read_fd = open(pipe_name, O_RDONLY);
        child_write_fd = open(pipe_name2, O_WRONLY);
        printf("открытие у дочернего\n");
        if (child_read_fd < 0 || child_write_fd < 0) {
            perror("child: open named pipes");
            unlink(pipe_name);
            unlink(pipe_name2);
            exit(1);
        }  
        }
        else {
            child_read_fd = pipefd[0];
            child_write_fd = pipefd2[1];
        }



        if (!is_named_pipe) {
            close(pipefd[1]);
            close(pipefd2[0]);
        }
        
        child_func(child_read_fd, child_write_fd);
        
        // Закрываем дескрипторы
        close(child_read_fd);
        if (child_write_fd != child_read_fd) {
            close(child_write_fd);
        }
        exit(0);
    }
    //parent
    else {
        if (is_named_pipe) {
        
        parent_write_fd = open(pipe_name, O_WRONLY);
        parent_read_fd = open(pipe_name2, O_RDONLY);
        printf("открытие у родителя\n");
        if (parent_write_fd < 0 || parent_read_fd < 0) {
            perror("parent: open named pipes");
            unlink(pipe_name);
            unlink(pipe_name2);
            exit(1);
        }
        }
        else {
            parent_read_fd = pipefd2[0];
            parent_write_fd = pipefd[1];
        }

        if (!is_named_pipe) {
            close(pipefd[0]);
            close(pipefd2[1]);
        }
        
        parent_func(parent_read_fd, parent_write_fd, argv, argc);
        
        // Закрываем дескрипторы
        close(parent_write_fd);
        if (parent_read_fd != parent_write_fd) {
            close(parent_read_fd);
        }
        
        // Ждем завершения дочернего процесса
        wait(NULL);
        
        // Удаляем именованный канал если использовался
        if (is_named_pipe) {
            unlink(pipe_name);
            unlink(pipe_name2);
            printf("Named pipe removed: %s\n", pipe_name);
        }
    }
}