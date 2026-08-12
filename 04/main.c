//#inclide <>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <error.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
//#include "type.h"
#include <errno.h>
#include <sys/sem.h>
#include <string.h>
#include <signal.h>
#include <time.h>




#define SHM_SIZE 1024
#define SHM_KEY 2
#define SEM_KEY 2
#define MAX_VAL -2147483648
#define MIN_VAL 2147483647

int handle_numbers(int size, int *data, int *minar, int *maxar) {
    if (minar == NULL || maxar == NULL || data == NULL) return -1;
    *maxar = MAX_VAL;
    *minar = MIN_VAL;
    for (int i = 0; i < size; i++) {
        if (data[i] > *maxar) {
            *maxar = data[i];
        }
        if (data[i] < *minar) {
            *minar = data[i];
        }
    }
    return 0;
}

int check_sem_val(int semid, int semnum) {
    // Возвращает текущее значение счетчика семафора
    int val = semctl(semid, semnum, GETVAL);
    if (val == -1) {
        perror("semctl check");
        exit(1);
    }
    return val; // Если 0 — занят (при логике бинарного семафора)
}

struct sembuf lock_op = {0, -1, 0};
struct sembuf unlock_op = {0, 1, 0};

int running = 1;
void sigint_handler(int sig) {
    printf("\nПолучен сигнал SIGINT!\n");
    printf("Завершаю работу...\n");
    running = 0;
}

// Функция для захвата семафора
void sem_lock(int semid) {
    if (semop(semid, &lock_op, 1) == -1) {
        if (errno == EIDRM) {
            printf("Семафор был удален другим процессом\n");
            exit(0);
        }
        perror("semop lock");
        exit(1);
    }
}

// Функция для освобождения семафора
void sem_unlock(int semid) {
    if (semop(semid, &unlock_op, 1) == -1) {
        if (errno == EIDRM) {
            printf("Семафор был удален другим процессом\n");
            exit(0);
        }
        perror("semop unlock");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, sigint_handler);
    srand(time(NULL));

    //int running_costumer = 1;
    int *data;
    int shmid;
    int semid = semget(SEM_KEY, 1, IPC_CREAT | IPC_EXCL | 0666);

    //Потребитель
    if (semid == -1) {
        if (errno != EEXIST) {
            perror("semget failed");
            exit(1);
        }
        //Доступ к рзделяемой памяти
        printf("Consumer: открытие семафора...\n");

        semid = semget(SEM_KEY, 1, 0);
        if (semid == -1) {
            perror("semget consumer");
            exit(1);
        }

        printf("Consumer: открытие разделяемой памяти...\n");

        shmid = shmget(SHM_KEY, SHM_SIZE, 0);
        if (shmid == -1) {
            perror("shmget consumer");
            exit(1);
        }
        data = shmat(shmid, NULL, 0);
        if (data == (int*)-1) {
            perror("shmat consumer");
            exit(1);
        }

        int current_offset = 0;
        while (running) {
            printf("Consumer: попытка захватить семафор\n\n");
            sem_lock(semid);
            printf("Consumer: захватил семафор\n");

            
            int *ptr = data + current_offset;
            int size = ptr[0];
            int next_offset = ptr[1];
    
            if (size == -1 && next_offset == 0) {
                printf("Consumer: достигнут конец списка\n");
                sem_unlock(semid);
                break;
            }
            else if (size == 0 && next_offset != 0) {
                printf("Consumer: блок с адресом %d уже обработан, пропускаю\n", current_offset);
                current_offset = next_offset;
                continue;
            }
            else if (size != 0) {
                int min = MIN_VAL, max = MAX_VAL;
                handle_numbers(size, ptr, &min, &max);
                printf("Промежуток от %d до %d:\n\tmin = %d;\n\tmax = %d\n", current_offset, next_offset, min, max);
                ptr[0] = 0;
                current_offset = next_offset;
            }
        
            sem_unlock(semid);
            printf("Consumer: освободил семафор\n");
            sleep(1 + rand() % 3);
            
        }
        shmdt(data);
    }
    //Производитель
    else {
        int running_producer = 1;
        if (semctl(semid, 0, SETVAL, 1) == -1) {
            perror("semctl");
            exit(1);
        }
        printf("Producer: создал семафор семафора\n");
        printf("Producer: попытка открыть разделяемую память...\n");
        shmid = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT | IPC_EXCL | 0666);
        //Доступ к рзделяемой памяти
        data = shmat(shmid, NULL, 0);
        if (data == (int*)-1) {
            perror("producer shmat");
            exit(1);
        }

        memset(data, 0, SHM_SIZE);
        int remaining_cells = SHM_SIZE / sizeof(int);

        while (running) {
            
            int rand_val = rand() % (SHM_SIZE / sizeof(int) / 5) + 2 + 1;
            int current_offset = (SHM_SIZE / sizeof(int) - remaining_cells);
            int *ptr = data + current_offset;
            
            printf("Producer: попытка захватить семафор\n\n");
            sem_lock(semid);
            printf("Producer: захватил семафор\n");            
            if (remaining_cells < 2) {
                ptr[0] = -1;
                ptr[1] = 0;
                sem_unlock(semid);
                break;
            }
            ptr[0] = (rand_val < remaining_cells ? rand_val : remaining_cells) - 2;
            //block.next_offset = current_offset + block.size;
            ptr[1] = current_offset + ptr[0] + 2;
            //block.data = data;
            
            

            for (int i = 0; i < ptr[0]; i++) {
                ptr[2 + i] = rand();
            }
            remaining_cells -= ptr[0];
            remaining_cells -= 2;

            sem_unlock(semid);
            printf("Producer: освободил семафор\n");


            int is_readed = 0;
            
            printf("Producer: жду прочтения\n");
            while (!is_readed) {
                sem_lock(semid);
                is_readed = (ptr[0] == 0);
                sem_unlock(semid);
                sleep(1);
            }
            
        }
        printf("Producer: память закончилась, прекращаю работу\n");

        shmdt(data);
        sleep(5);

        //sleep(10);
        if (shmctl(shmid, IPC_RMID, NULL) == -1) {
            perror("shmctl producer");
        }
        if (semctl(semid, 0, IPC_RMID, 0) == -1) {
            perror("semctl producer");
        }
        printf("Producer: закрыл все\n");
    }

    return 0;
}