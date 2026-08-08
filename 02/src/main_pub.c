#include <sys/msg.h>
#include <sys/ipc.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "types.h"
#include "subscriber.h"
#include "publisher.h"
#include <fcntl.h>


#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#define QUEUE_NAME "/tmp"
#define TOPICS_FILE_NAME "data/topics.txt"
#define TEXTS_FILE_NAME "data/texts.txt"

int running = 1;

void handle_sigint(int sig) {
    printf("\nПолучен сигнал SIGINT (Ctrl+C). Выход...\n");
    running = 0;
    //exit(0);
}

int publisher_process(int msqid, Messege mes) {
    size_t read_bytes = msgsnd(msqid, &mes, (int)(sizeof(Messege) - sizeof(long)), 0);
    if (read_bytes == -1) {
        perror("msgsnd");
    }

    printf("Отправил сообщение\n");
}


int main() {
    key_t key = ftok(QUEUE_NAME, 'A');
    int msqid = msgget(key, 0666);
    if (msqid == -1) {
        if (errno == ENOENT) {
            perror("Ошибка: очереди нет");
            exit(0);
        }
        else {
            perror("Ошибка msgget");
            exit(-1);
        }
    }
    else {
        signal(SIGINT, handle_sigint);
        while (running) {
            Messege mes;
            int timing = 1 + rand() % 5;
            sleep(timing);
            generate_messege(&mes, "send", 1);
            printf("%s\n", mes.text);
            publisher_process(msqid, mes);
        }
    } 
}