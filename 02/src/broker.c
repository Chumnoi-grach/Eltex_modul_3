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


#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#define QUEUE_NAME "/tmp"

int running = 1;

void handle_sigint(int sig) {
    printf("\nПолучен сигнал SIGINT (Ctrl+C). Выход...\n");
    running = 0;
    //exit(0);
}

void init_broker(Broker *broker, int msqid) {
    broker->sub_head = NULL;
    broker->sub_tail = NULL;
    broker->pub_head = NULL;
    broker->pub_tail = NULL;
    broker->queue_id = msqid;
}

void init_topic(Topic *topic, char *buf) {
    if (topic == NULL || buf == NULL){
        return;
    }
    strncpy(topic->topic, buf, sizeof(topic->topic) - 1);
    topic->topic[sizeof(topic->topic) - 1] = '\0';
    topic->next = NULL;
}

int add_subscrider(Broker *broker, Subscriber *subscriber) {
    if (subscriber == NULL || broker == NULL) {
        return -1;
    }
    subscriber->next = NULL;
    if (broker->sub_tail == NULL) {
        broker->sub_head = subscriber;
        broker->sub_tail = subscriber;
        return 0;
    }
    else {
        broker->sub_tail->next = subscriber;
        broker->sub_tail = subscriber;
        return 0;
    }
}

int is_subscriber_in_list(Broker *broker, pid_t pid) {
    if (broker == NULL) {
        return 0;
    }
    Subscriber *temp_subscriber = broker->sub_head;
    while (temp_subscriber != NULL) {
        if (temp_subscriber->pid == pid) return 1;
        temp_subscriber = temp_subscriber->next;
    }
    return 0;
}

Subscriber *get_subscriber_by_broker(Broker *broker, pid_t pid) {
    if (broker == NULL) {
        return NULL;
    }
    Subscriber *temp_subscriber = broker->sub_head;
    while (temp_subscriber != NULL) {
        if (temp_subscriber->pid == pid) return temp_subscriber;
        temp_subscriber = temp_subscriber->next;
    }
    return NULL;
}

int add_publisher(Broker *broker, Publisher *publisher) {
    if (publisher == NULL || broker == NULL) {
        return -1;
    }
    publisher->next = NULL;
    if (broker->pub_tail == NULL) {
        broker->pub_head = publisher;
        broker->pub_tail = publisher;
        return 0;
    }
    else {
        broker->pub_tail->next = publisher;
        broker->pub_tail = publisher;
        return 0;
    }
}

int is_publiser_in_list(Broker *broker, pid_t pid) {
    if (broker == NULL) {
        return 0;
    }
    Publisher *temp_publisher = broker->pub_head;
    while (temp_publisher != NULL) {
        if (temp_publisher->pid == pid) return 1;
        temp_publisher = temp_publisher->next;
    }
    return 0;
}

int broker_process(Broker *broker) {
    // struct msqid_ds buf;
    Messege rcv_mes;
    ControlMessage controlmes;
    char text[MAX_TEXT_SIZE];

    // if (msgctl(broker->queue_id, IPC_STAT, &buf) == -1) {
    //     perror("msgctl failed");
    //     return -1;
    // }
    size_t read_bytes;
    
    if ((read_bytes = msgrcv(broker->queue_id, &rcv_mes, sizeof(Messege), 1, 0)) <= 0) {
        //perror("msgrcv failed");
        if (errno == EINTR) printf("msgrcv: поступил сигнал\n");
        return(1);
    }
    printf("Пришло сообщение\n");
    //TEST
    
    // //rcv_mes.mtype = 1;
    // //rcv_mes.sender_pid = 123;
    // strcpy(rcv_mes.text, "send,12345,epic_topic\nshort text");
    // strcpy(rcv_mes.topic, "TEMA");

    //TEST
    //Копируем заголовок
    char header[MAX_HEADER_SIZE];
    //char temp_buffer[MAX_TEXT_SIZE];
    //strncpy(temp_buffer, rcv_mes.text, MAX_TEXT_SIZE - 1);
    //temp_buffer[MAX_TEXT_SIZE - 1] = '\0';

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

    //Messege snd_mes;
    if (strcmp(controlmes.command, "send") == 0) {
        //добавляем publisher в динамический массив
        if (!is_publiser_in_list(broker, controlmes.sender_pid)) {
            Publisher *new_publisher = malloc(sizeof(Publisher));
            if (new_publisher == NULL) {
                perror("malloc");
                return -5;
            }
            init_publisher(new_publisher, controlmes.sender_pid);
            add_publisher(broker, new_publisher);
        }
        Subscriber *sub_in_list = broker->sub_head;

        // if (broker->sub_head == NULL) {
        //     printf("Нет подписчиков на тему %s\n", controlmes.topic);
        // }
        while (sub_in_list != NULL) {
            Topic *topic_in_list = sub_in_list->head;
            while (topic_in_list != NULL) {
                if (strcmp(topic_in_list->topic, controlmes.topic) == 0) {
                    rcv_mes.mtype = sub_in_list->pid;
                    if (msgsnd(broker->queue_id, &rcv_mes, sizeof(Messege) - sizeof(long), 0) == -1) {
                        perror("msgsnd подписчику");
                        return(-4);
                    }
                    else {
                        printf("send: отправили сообщение подписчику от %d к %ld\nТема: %s\n", controlmes.sender_pid, controlmes.mtype, controlmes.topic);
                    }
                    break;
                }
                topic_in_list = topic_in_list->next;
            }
            sub_in_list = sub_in_list->next;
        }
    }

    else if (strcmp(controlmes.command, "subscribe") == 0) {
        //добавляем subscriber в динамический массив
        if (!is_subscriber_in_list(broker, controlmes.sender_pid)) {
            Subscriber *new_subscriber = malloc(sizeof(Subscriber));
            if (new_subscriber == NULL) {
                perror("malloc");
                return -5;
            }
            init_subscriber(new_subscriber, controlmes.sender_pid);
            add_subscrider(broker, new_subscriber);
        }
        printf("subscribe: subscriber %d получил сообщение по теме %s\n", controlmes.sender_pid, controlmes.topic);
        Subscriber *sub = get_subscriber_by_broker(broker, controlmes.sender_pid);
        
        Topic *topic;
        topic = malloc(sizeof(Topic));
        if (topic == NULL) {
            perror("malloc");
        }
        init_topic(topic, controlmes.topic);
        if (add_topic(sub, topic) == 0) {
            printf("subscribe: broker для subscriber %d добавил новую тему: %s\n", controlmes.sender_pid, controlmes.topic);
        }
    }
    else if (strcmp(controlmes.command, "unsubscribe") == 0) {
        //?
        //добавляем subscriber в динамический массив
        // if (!is_subscriber_in_list(broker, controlmes.sender_pid)) {
        //     Subscriber *new_subscriber = malloc(sizeof(Subscriber));
        //     if (new_subscriber == NULL) {
        //         perror("malloc");
        //         return -5;
        //     }
        //     init_subscriber(&new_subscriber, controlmes.sender_pid);
        //     add_subscrider(broker, new_subscriber);
        // }
        printf("unsubscribe: subscriber %d закончил работу и отписался от %s\n", controlmes.sender_pid, controlmes.topic);
    }
}

void send_sigint_to_all(Broker *broker) {
    if (broker == NULL) {
        return;
    }
    Subscriber *sub_in_list = broker->sub_head;
    

    while (sub_in_list != NULL) {
        if (kill(sub_in_list->pid, SIGINT) == 0) {
            printf("Сигнал отправлен успешно подписчику с PID: %d\n", sub_in_list->pid);
        } else if (errno == ESRCH) {
            printf("   Подписчик %d уже завершен\n", sub_in_list->pid);
        }
        else {
            perror("Ошибка отправки сигнала");
        }
        sub_in_list = sub_in_list->next;
    }

    Publisher *pub_in_list = broker->pub_head;
    while (pub_in_list != NULL) {
        if (kill(pub_in_list->pid, SIGINT) == 0) {
            printf("Сигнал отправлен успешно подписчику с PID: %d\n", pub_in_list->pid);
        }
        else {
            perror("Ошибка отправки сигнала");
        }
        pub_in_list = pub_in_list->next;
    }



    int timeout_seconds = 5;
    printf("Брокер: ожидание завершения процессов (таймаут %d сек)...\n", timeout_seconds);
    
    int waited = 0;
    int all_terminated = 0;
    
    while (waited < timeout_seconds && !all_terminated) {
        sleep(1);
        waited++;

        int alive = 0;
        
        // Проверяем подписчиков
        Subscriber *sub_check = broker->sub_head;
        while (sub_check != NULL) {
            if (kill(sub_check->pid, 0) == 0) {
                alive = 1;
                printf("  Ожидание подписчика %d... (%d сек)\n", sub_check->pid, waited);
                break;
            }
            sub_check = sub_check->next;
        }
        
        // Проверяем издателей
        if (!alive) {
            Publisher *pub_check = broker->pub_head;
            while (pub_check != NULL) {
                if (kill(pub_check->pid, 0) == 0) {
                    alive = 1;
                    printf("  Ожидание издателя %d... (%d сек)\n", pub_check->pid, waited);
                    break;
                }
                pub_check = pub_check->next;
            }
        }
        
        if (!alive) {
            all_terminated = 1;
            printf("Брокер: все процессы завершены (через %d сек)\n", waited);
        }
    }
}

int main() {
    key_t key = ftok(QUEUE_NAME, 'A');
    int msqid = msgget(key, IPC_CREAT | IPC_EXCL | 0666);
    if (msqid == -1) {
        if (errno == EEXIST) {
            perror("Ошибка: Брокер уже существует!");
            exit(0);
        }
        else {
            perror("Ошибка msgget");
            exit(-1);
        }
    }
    else {
        Broker broker;
        init_broker(&broker, msqid);
        signal(SIGINT, handle_sigint);
        while (running) {
            broker_process(&broker);
        }
        send_sigint_to_all(&broker);
        if (msgctl(msqid, IPC_RMID, NULL) == -1) {
            perror("Ошибка: не удалось удалить очередь");
            exit(-2);
        }
    } 
}