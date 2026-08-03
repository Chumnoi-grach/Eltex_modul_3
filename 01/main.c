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

int main(int argc, char* argv[]) {
    int is_named_pipe = 0;
    char *pipe_name;
    //-p;
    int opt;

    int pipe_with_data;
    while ((opt = getopt(argc, argv, "p:")) != -1)  {
        switch (opt) {
            case 'p':
                if (strlen(optarg) < MAX_FILENAME) {
                    pipe_name = optarg;
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

    
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strchr(argv[i],'p')) {
                i++;
                continue;
            }
        }
        if (open(argv[i], O_RDONLY) == -1) {
            printf("%s file is not exist\n", argv[i]);
        }
        else {
            printf("%s file is there\n", argv[i]);
        }
    }

    printf("Создаем канал ");
    int pipefd[2];
    if (is_named_pipe){
        printf("ИМЕНОВАННЫЙ\n");
        if ((mkfifo(pipe_name, 0002)) == -1) {
            fprintf(stderr, "Именованный канал не создан");
        }
    }
    else {
        printf("НЕИМЕНОВАННЫЙ\n");
        pipe(pipefd);
    }




    // int pid = fork();
    // //error
    // if (pid == -1) {
    //     perror("fork");
    //     return -1;
    // }
    // //Child
    // else if (pid == 0) {
    //     pipe
    //     //сообщение о готовности передачи
    //     for (int i = 0; i < argc; i++) {
    //         if () {
    //             perror("file is not exist");
    //         }
    //     }
    //     +.copy

    //     //обработка -p флага (имя созданного канала)
    // }
    // //parrent
    // else {
    //     for (int i = 1; i < argc; i++) {
    //         if (argv[i][0] == '-') {
    //             if (strchr(argv[i],'p')) {
    //                 i++;
    //                 continue;
    //             }
    //         }
    //         if (open(argv[i], O_RDONLY) == -1) {
    //             printf("%s file is not exist\n", argv[i]);
    //         }
    //         else {
    //             printf("%s file is there\n", argv[i]);
    //         }
    //     }
    //     //обработка сообщения о готовности передачи
    //     //скидываем имя файла и кол-во байт

    //     //!!! Содержимое файла передается блоками из нескольких байтов.
    //     //скидываем сообщение о завершении программы и ждем завершения
    //     wait();
    // }
}