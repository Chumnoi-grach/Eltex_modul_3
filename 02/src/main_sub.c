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
#include <time.h>


#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE


int running = 1;

void handle_sigint(int sig) {
    printf("\nПолучен сигнал SIGINT (Ctrl+C). Выход...\n");
    running = 0;
    //exit(0);
}
int init_sub_for_broker(int msqid, Subscriber *sub) {
    Messege mes;
    int n = rand() % 10+5;
    printf("генерируем %d топиков\n", n);
    for (int i = 0; i < n; i++){
        Topic *topic = malloc(sizeof(Topic));
        if (generate_messege(&mes, "subscribe", 1, topic) == -1) {
            perror("generate_messege");
            return -1;
        }
        Topic *current = sub->head;
        int skip = 0;
        while (current != NULL) {
            if (strcmp(current->topic, topic->topic) == 0) {
                skip = 1;
            }
            current = current->next;
        }
        if (skip) continue;
        size_t write_bytes = msgsnd(msqid, &mes, (int)(sizeof(Messege) - sizeof(long)), 0);
        if (write_bytes == -1) {
            perror("msgsnd");
            return -1;
        }
        
        add_topic(sub, topic);

        printf("Отправил сообщение типа subscribe с темой: %s\n", topic->topic);
    }
    return 0;
}
int subscriber_process(int msqid, Subscriber *sub) {
    Messege rcv_mes;
    ControlMessage controlmes;
    char text[MAX_TEXT_SIZE];
    size_t read_bytes = msgrcv(msqid, &rcv_mes, (int)(sizeof(Messege) - sizeof(long)), getpid(), 0);
    if (read_bytes == -1) {
        if (errno == EINTR) {
            printf("Прерывание msgrcv сигналом SIGINT\n");
            return 1;
        }
        perror("msgrcv");
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
    
    // Topic *topic_in_list = sub->head;
    // while (topic_in_list != NULL) {
    //     if (strcmp(topic_in_list->topic, controlmes.topic) == 0) {
    //         Topic *topic = malloc(sizeof(Topic));
    //         if (topic == NULL) {
    //             perror("malloc");
    //         }
    //         init_topic(topic, controlmes.topic);
    //         // if (add_topic(sub, topic) == 0) {
    //         //     printf("subscribe: broker для subscriber %d добавил новую тему: %s\n", controlmes.sender_pid, controlmes.topic);
    //         // }
    //     }
    //     topic_in_list = topic_in_list->next;
    // }
}

int generate_unsub_messege(Messege *mes, long mtype, Topic *topic) {
    char text[MAX_TEXT_SIZE];
    strcpy(text, "unsubscribe,");

    char pid_buf[20];
    sprintf(pid_buf, "%d,", getpid());
    strcat(text, pid_buf);

    // char topic_buf[MAX_TOPIC_SIZE];
    // strcpy(topic_buf, topic->topic);
    strcat(text, topic->topic);
    strcat(text, "\n");
    char text_buf[MAX_TEXT_SIZE];
    // if (get_str_by_file(TEXTS_FILE_NAME, target_num_str, text_buf) == 0) {
    //     return -2;
    // }
    // strcat(text, text_buf);
    strcpy(mes->text, text);
    //strcpy(mes->topic, topic_buf);
    mes->mtype = mtype;
    //strcpy(topic->topic, topic_buf);

    return 0;
}

int unsub_for_broker(int msqid, Subscriber *sub) {
    Topic *topic_in_list = sub->head;
    Messege mes;
    while (topic_in_list != NULL) {
        if (generate_unsub_messege(&mes, 1, topic_in_list) == -1) {
            perror("generate_messege");
            return -1;
        }
        size_t write_bytes = msgsnd(msqid, &mes, (int)(sizeof(Messege) - sizeof(long)), 0);
        if (write_bytes == -1) {
            if (errno == EIDRM) {
                printf("Очередь уже удалена, пропускаем отписку для %s\n", topic_in_list->topic);
                return 0;
            }
            perror("msgsnd в unsub");
            return -1;
        }
        printf("Отправил unsubscribe сообщение %s broker\n", topic_in_list->topic);
        topic_in_list = topic_in_list->next;
    }
}


int main() {
    key_t key = ftok(QUEUE_NAME, 'A');
    int msqid = msgget(key, 0666);
    srand(time(NULL));
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
        
        init_sub_for_broker(msqid, &subscriber);
        while (running) {
            int timing = 1 + rand() % 5;
            int result = subscriber_process(msqid, &subscriber);
            if (result == -1) {
                printf("Ввыход из цикла\n");
                break;
            }
            usleep(10000);
        }

        unsub_for_broker(msqid, &subscriber);
    }
}