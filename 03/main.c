#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <mqueue.h>
#include <signal.h>
#include <pthread.h>


#define BUFFER_SIZE 256
#define BASE_MESSEGE_PRIORITY 1
#define MAX_MSG_COUNT 10
#define MAX_MSG_SIZE 256 

int running = 1;
unsigned int priority = 1;


void sigint_handler(int sig) {
    printf("\nПолучен сигнал SIGINT!\n");
    printf("Завершаю работу...\n");
    running = 0;
}

typedef struct {
    mqd_t mq;
} thread_args_t;


void * user_func_read_consol_write_mes(void *arg) {
    thread_args_t *args = (thread_args_t*)arg;
    mqd_t mq_write = args->mq;
    char buffer[BUFFER_SIZE];

    printf("user_func_read_consol_write_mes запущен, дескриптор: %d\n", mq_write);

    while (running) {
        printf(" > ");
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            strcpy(buffer, "EOF сообщение\n");
        }
        int bytes;
        if ((bytes = mq_send(mq_write, buffer, strlen(buffer), BASE_MESSEGE_PRIORITY)) == -1) {
            if (errno == EAGAIN) {
                printf("Очередь переполнена, сообщение не отправлено\n");
            } else {
                perror("mq_send");
                break;
            }
            printf("Отправлено неуспешно %d байт\n", bytes);
            break;
        }
    }
    mq_send(mq_write, "Другой пользователь закончил диалог\n", BUFFER_SIZE, BASE_MESSEGE_PRIORITY + 1);
    sleep(1);
    printf("user_func_read_consol_write_mes закончил работу\n");
    mq_close(mq_write);
    return NULL;
}

void * user_func_read_mes_write_consol(void *arg) {
    thread_args_t *args = (thread_args_t*)arg;
    mqd_t mq_read = args->mq;
    unsigned int priority;
    char buffer[BUFFER_SIZE];

    printf("user_func_read_mes_write_consol запущен, дескриптор: %d\n", mq_read);
    printf("Ожидаю сообщение...\n");
    while (running) {
        int bytes = mq_receive(mq_read, buffer, BUFFER_SIZE, &priority);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            printf("\nПолучено: %s > ", buffer);
            fflush(stdout);

            if (priority > BASE_MESSEGE_PRIORITY) {
                running = 0;
                printf("\nПолучено сообщение о завершении\n");
                break;
            }
        }
        else if (bytes == -1) {
            if (errno == EAGAIN) {
                continue;
            } else {
                perror("mq_receive");
                break;
            }
        }
    }
    printf("user_func_read_mes_write_consol закончил работу\n");
    mq_close(mq_read);
    return NULL;
}

void init_queue_attr(struct mq_attr *attr) {
    attr->mq_maxmsg = MAX_MSG_COUNT;
    attr->mq_msgsize = MAX_MSG_SIZE;
    attr->mq_flags = 0;
}


int main(int argc, char* argv[]) {
    if (argc < 2) exit(-1);
    
    signal(SIGINT, sigint_handler);

    char mq_name_1[256] = "/";
    char mq_name_2[256] = "/";
    int is_second_user = 0;

    strcat(mq_name_1, argv[1]);
    strcat(mq_name_1, "_1");
    strcat(mq_name_2, argv[1]);
    strcat(mq_name_2, "_2");
    printf("%s\n", mq_name_1);
    printf("%s\n", mq_name_2);


    struct mq_attr attr;
    init_queue_attr(&attr);

    mqd_t mq_1 = mq_open(mq_name_1, O_CREAT | O_EXCL | O_RDONLY | O_NONBLOCK, 0666, &attr);
    perror("mq_open");
    mqd_t mq_2 = -1;

    if (mq_1 == -1) {
        printf("Запущена второй пользователь. Создаю очереди...\n");
        mq_1 = mq_open(mq_name_1, O_WRONLY);
        if (mq_1 == -1) {
            perror("очередь mq_1 не открываеся");
            mq_unlink(mq_name_1);
            exit(0);
        }
        mq_2 = mq_open(mq_name_2, O_RDONLY | O_NONBLOCK);
        if (mq_2 == -1) {
            perror("очередь mq_2 не открываеся");
            mq_unlink(mq_name_2);
            exit(0);
        }
        printf("Очереди созданы\n");
        is_second_user = 1;

        pthread_t thread_id_read, thread_id_write;
        thread_args_t thread_args_read_mes, thread_args_write_mes;
        thread_args_write_mes.mq = mq_1;
        
        if (pthread_create(&thread_id_read, NULL, user_func_read_consol_write_mes, &thread_args_write_mes) != 0) {
            perror("Ошибка создания потока");
            exit(-1);
        }
        thread_args_read_mes.mq = mq_2;
        if (pthread_create(&thread_id_write, NULL, user_func_read_mes_write_consol, &thread_args_read_mes) != 0) {
            perror("Ошибка создания потока");
            exit(-1);
        }

        pthread_join(thread_id_read, NULL);
        pthread_join(thread_id_write, NULL);       

    }
    else {
        printf("Запущена первый пользователь. Создаю очереди...\n");
        mq_2 = mq_open(mq_name_2, O_CREAT | O_WRONLY | O_EXCL , 0666, &attr);
        if (mq_2 == -1) {
            perror("очередь mq_2 не открываеся");
            mq_unlink(mq_name_2);
            exit(0);
        }
        printf("Очереди созданы\n");

        pthread_t thread_id_read, thread_id_write;
        thread_args_t thread_args_read_mes, thread_args_write_mes;
        thread_args_write_mes.mq = mq_2;
        
        if (pthread_create(&thread_id_read, NULL, user_func_read_consol_write_mes, &thread_args_write_mes) != 0) {
            perror("Ошибка создания потока");
            exit(-1);
        }
        thread_args_read_mes.mq = mq_1;
        if (pthread_create(&thread_id_write, NULL, user_func_read_mes_write_consol, &thread_args_read_mes) != 0) {
            perror("Ошибка создания потока");
            exit(-1);
        }

        pthread_join(thread_id_read, NULL);
        pthread_join(thread_id_write, NULL);

        mq_unlink(mq_name_1);
        mq_unlink(mq_name_2);
    }
}