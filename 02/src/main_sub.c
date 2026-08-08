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


int running = 1;

void handle_sigint(int sig) {
    printf("\nПолучен сигнал SIGINT (Ctrl+C). Выход...\n");
    running = 0;
    //exit(0);
}
int init_sub_for_broker(int msqid, Messege* mes) {
    for (int n = rand() % 10; n--; n > 0){
        if (generate_messege(mes, "subscribe", 1) == -1) {
            perror("generate_messege");
            return -1;
        }
    
        size_t write_bytes = msgsnd(msqid, mes, (int)(sizeof(Messege) - sizeof(long)), 0);
        if (write_bytes == -1) {
            perror("msgsnd");
        }
        printf("Отправил сообщение типа subscribe\n");
    }
}
int subscriber_process(int msqid, Subscriber *sub) {
    Messege rcv_mes;
    ControlMessage controlmes;
    char text[MAX_TEXT_SIZE];
    size_t read_bytes = msgrcv(msqid, &rcv_mes, (int)(sizeof(Messege) - sizeof(long)), getpid(), 0);
    if (read_bytes == -1) {
        perror("msgsnd");
    }

    printf("Получил сообщение\n");

    char header[MAX_HEADER_SIZE];

    //Проверка заголовка
    char *newline = strchr(rcv_mes.text, '\n');
    if (newline == NULL) {
        printf("Нет разделителя \\n в сообщении\n");
        return -3;
    }
    size_t len = newline - rcv_mes.text;
    strncpy(header, rcv_mes.text, len);
    header[len] = '\0';

    
    int comma_count = 0;
    int count = 0;
    while (*(header+count)) {
        if (*(header+count) == ',') comma_count++;
        count++;
    }
    if (comma_count < 2) {
        printf("Нет разделителя , в первой строке\n");
        return(-4);
    }

    strcpy(text, newline + 1);

    //Парсим данные из заголовка
    char sender_pid[MAX_PID_SIZE];
    strcpy(controlmes.command, strtok(header, ","));
    strcpy(sender_pid, strtok(NULL, ","));
    controlmes.sender_pid = (pid_t)atoi(sender_pid);
    strcpy(controlmes.topic, strtok(NULL, ","));
    controlmes.mtype = 1;

    printf("command: %s\n"
        "mtype: %ld\n"
        "sender_pid: %d\n"
        "topic: %s\n\n", 
        controlmes.command,
        controlmes.mtype,
        controlmes.sender_pid,
        controlmes.topic);
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
        Messege mes;
        Subscriber subscriber;
        
        
        init_sub_for_broker(msqid, &mes);
        while (running) {
            int timing = 1 + rand() % 5;
            subscriber_process(msqid, &subscriber);
        }
    } 
}