#ifndef TYPES_H
#define TYPES_H

#include <sys/types.h>
#define MAX_TOPIC_SIZE 255
#define MAX_TEXT_SIZE 8192
#define MAX_HEADER_SIZE 8192
#define MAX_COMMAND_SIZE 20
#define MAX_PID_SIZE 20
typedef struct Topic Topic;
typedef struct Subscriber Subscriber;
typedef struct Publisher Publisher;
typedef struct Messege Messege;
typedef struct Broker Broker;
typedef struct ControlMessage ControlMessage;

#define QUEUE_NAME "/tmp"
#define TOPICS_FILE_NAME "data/topics.txt"
#define TEXTS_FILE_NAME "data/texts.txt"


//список у подписчика
typedef struct Topic {
    char topic[MAX_TOPIC_SIZE];
    Topic *next;
}Topic;

typedef struct Messege {
    long mtype;
    //char topic[MAX_TOPIC_SIZE];
    char text[MAX_TEXT_SIZE];
    //pid_t sender_pid;
}Messege;

// -b 
// 1 приоритет всегда
// topic, payload
// SIGINT всем пользователям
typedef struct Broker {
    //надо ли
    //надо
    Subscriber *sub_head;
    Subscriber *sub_tail;
    Publisher *pub_head;
    Publisher *pub_tail;

    int queue_id;
}Broker;

typedef struct Subscriber {
    pid_t pid;
    Topic *head;
    Topic *tail;
    Subscriber *next;
}Subscriber;

//тема в аргументах запуска
//Приоритет исходящего сообщения равен 1
typedef struct Publisher{
    pid_t pid;
    Publisher *next;
}Publisher;

typedef struct ControlMessage {
    long mtype; // Всегда 1 для брокера
    pid_t sender_pid;
    char command[MAX_COMMAND_SIZE]; // "subscribe", "unsubscribe", "send"
    char topic[MAX_TOPIC_SIZE];
} ControlMessage;

#endif