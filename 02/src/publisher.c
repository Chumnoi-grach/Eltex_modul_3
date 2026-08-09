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


void init_publisher(Publisher *pub, pid_t pid) {
    pub->pid = pid;
    pub->next = NULL;
}

int get_str_by_file(char *file, int num_srt, char *buf) {
    
    int collection_of_topics = open(file, O_RDONLY);
    if (collection_of_topics == -1) {
        perror("Ошибка открытия файла");
    }
    char c;
    int current_num_str = 0;
    int topic_index = 0;
    while (read(collection_of_topics, &c, 1)) {
        if (c == '\n') {
            current_num_str++;
        }
        if (current_num_str == num_srt && c != '\n') {
            buf[topic_index++] = c;
        }
        if (current_num_str > num_srt) {
            break;
        }
    }
    buf[topic_index] = '\0';
    close(collection_of_topics);
    return topic_index;
}

int generate_messege(Messege *mes, char *command, long mtype, Topic *topic) {
    char text[MAX_TEXT_SIZE];
    strcpy(text, command);
    strcat(text, ",");

    char pid_buf[20];
    sprintf(pid_buf, "%d,", getpid());
    strcat(text, pid_buf);

    char topic_buf[MAX_TOPIC_SIZE];
    int target_num_str = rand() % 20;
    if (get_str_by_file(TOPICS_FILE_NAME, target_num_str, topic_buf) == 0) {
        return -1;
    }
    strcat(text, topic_buf);
    strcat(text, "\n");
    char text_buf[MAX_TEXT_SIZE];
    if (get_str_by_file(TEXTS_FILE_NAME, target_num_str, text_buf) == 0) {
        return -2;
    }
    strcat(text, text_buf);
    strcpy(mes->text, text);
    //strcpy(mes->topic, topic_buf);
    mes->mtype = mtype;
    strcpy(topic->topic, topic_buf);

    return 0;
}