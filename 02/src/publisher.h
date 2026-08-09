#ifndef PUBLISHER_H
#define PUBLISHER_H

#include "types.h"
#include <stdlib.h>

void init_publisher(Publisher *pub, pid_t pid);

int get_str_by_file(char *file, int num_srt, char *buf);

int generate_messege(Messege *mes, char *command, long mtype, Topic *topic);

#endif