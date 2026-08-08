#include "types.h"
#include "subscriber.h"
#include <stdlib.h>

void init_subscriber(Subscriber *sub, pid_t pid) {
    sub->pid = pid;
    sub->head = NULL;
    sub->tail = NULL;
    sub->next = NULL;
}

int add_topic(Subscriber *sub, Topic *topic) {
    if (sub == NULL || topic == NULL) {
        return -1;
    }
    topic->next = NULL;
    if (sub->tail == NULL) {
        sub->head = topic;
        sub->tail = topic;
        return 0;
    } else {
        sub->tail->next = topic;
        sub->tail = topic;
        return 0;
    }
}