#ifndef SUBSCRIBE_H
#define SUBSCRIBE_H
#include "types.h"
#include <stdlib.h>

void init_subscriber(Subscriber *sub, pid_t pid);

int add_topic(Subscriber *sub, Topic *topic);

void init_topic(Topic *topic, char *buf);
#endif