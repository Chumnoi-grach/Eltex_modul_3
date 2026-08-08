#include <sys/msg.h>
#include <sys/ipc.h>

#define MAX_TOPIC_SIZE 255
#define MAX_TEXT_SIZE 8192
#define MAX_COMMAND_SIZE 20

//список у подписчика
typedef struct Topic {
    char topic[MAX_TOPIC_SIZE];
    Topic *next;
}Topic;

typedef struct Messege {
    char topic[MAX_TOPIC_SIZE];
    char text[MAX_TEXT_SIZE];
    long mtype;
    pid_t sender_pid;
}Messege;

// -b 
// 1 приоритет всегда
// topic, payload
// SIGINT всем пользователям
typedef struct Broker {
    //надо ли
    //надо
    Subscriber *read_head;
    Publisher *pub_head;
    int queue_id;
    //int running;
}Broker;

typedef struct Subscriber {
    pid_t pid;
    Topic *head;
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

// typedef struct ControlMessage {
//     long mtype;                         // Всегда 1 для брокера
//     pid_t sender_pid;
//     char command[20];                   // "subscribe", "unsubscribe", "send"
//     char topic[MAX_TOPIC_SIZE];
// } ControlMessage;