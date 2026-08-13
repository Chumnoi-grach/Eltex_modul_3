#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>

#define SHM_SIZE 1024
#define SHM_NAME "/my_shm"
#define SEM_NAME "/my_sem"
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

int running = 1;
void sigint_handler(int sig) {
    printf("\nПолучен сигнал SIGINT!\n");
    printf("Завершаю работу...\n");
    running = 0;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, sigint_handler);
    srand(time(NULL));

    int *data;
    int shm_fd;
    sem_t *sem;
    sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0666, 1);
    
    // Потребитель
    if (sem == SEM_FAILED && errno == EEXIST) {
        printf("Consumer: открытие семафора...\n");
        sem = sem_open(SEM_NAME, 0);
        if (sem == SEM_FAILED) {
            perror("Consumer: sem_open");
            exit(1);
        }

        printf("Consumer: открытие разделяемой памяти...\n");
        shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
        if (shm_fd == -1) {
            perror("Consumer: shm_open");
            exit(1);
        }

        struct stat shm_stat;
        if (fstat(shm_fd, &shm_stat) == -1) {
            perror("Consumer: fstat");
            exit(1);
        }

        //mmap - альтернатива чтения/записи данных (read/write), только через указатели, а не дескрипторы
        data = (int*)mmap(NULL, shm_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (data == MAP_FAILED) {
            perror("Consumer: mmap");
            exit(1);
        }

        int current_offset = 0;
        while (running) {
            printf("Consumer: попытка захватить семафор\n\n");
            
            if (sem_wait(sem) == -1) {
                if (errno == EINTR) continue;
                perror("Consumer: sem_wait");
                break;
            }
            printf("Consumer: захватил семафор\n");

            int *ptr = data + current_offset;
            int size = ptr[0];
            int next_offset = ptr[1];
    
            if (size == -1 && next_offset == 0) {
                printf("Consumer: достигнут конец списка\n");
                sem_post(sem);
                break;
            }
            else if (size == 0 && next_offset != 0) {
                printf("Consumer: блок с адресом %d уже обработан, пропускаю\n", current_offset);
                current_offset = next_offset;
                sem_post(sem);
                continue;
            }
            else if (size != 0) {
                int min = MIN_VAL, max = MAX_VAL;
                handle_numbers(size, ptr + 2, &min, &max);
                printf("Промежуток от %d до %d:\n\tmin = %d;\n\tmax = %d\n", current_offset, next_offset, min, max);
                ptr[0] = 0;
                current_offset = next_offset;
            }
        
            sem_post(sem);
            printf("Consumer: освободил семафор\n");
            sleep(1 + rand() % 3);
        }
        
        munmap(data, shm_stat.st_size);
        close(shm_fd);
    }
    // Производитель
    else if (sem != SEM_FAILED) {
        printf("Producer: создал семафор\n");
        printf("Producer: попытка создать разделяемую память...\n");
        
        shm_fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0666);
        if (shm_fd == -1) {
            perror("Producer: shm_open");
            exit(1);
        }

        if (ftruncate(shm_fd, SHM_SIZE) == -1) {
            perror("Producer: ftruncate");
            exit(1);
        }

        data = (int*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (data == MAP_FAILED) {
            perror("Producer: mmap");
            exit(1);
        }

        memset(data, 0, SHM_SIZE);
        int remaining_cells = SHM_SIZE / sizeof(int);

        while (running) {
            int rand_val = rand() % (SHM_SIZE / sizeof(int) / 5) + 2 + 1;
            int current_offset = (SHM_SIZE / sizeof(int) - remaining_cells);
            int *ptr = data + current_offset;
            
            printf("Producer: попытка захватить семафор\n\n");
            
            if (sem_wait(sem) == -1) {
                if (errno == EINTR) continue;
                perror("Producer: sem_wait");
                break;
            }
            printf("Producer: захватил семафор\n");
            
            if (remaining_cells < 2) {
                ptr[0] = -1;
                ptr[1] = 0;
                sem_post(sem);
                break;
            }
            
            ptr[0] = (rand_val < remaining_cells ? rand_val : remaining_cells) - 2;
            ptr[1] = current_offset + ptr[0] + 2;

            for (int i = 0; i < ptr[0]; i++) {
                ptr[2 + i] = rand() % 20001 - 10000;
            }
            remaining_cells -= ptr[0];
            remaining_cells -= 2;

            sem_post(sem);
            printf("Producer: освободил семафор\n");

            int is_readed = 0;
            
            printf("Producer: жду прочтения\n");
            while (!is_readed) {
                if (sem_wait(sem) == -1) {
                    if (errno == EINTR) continue;
                    perror("Producer: sem_wait check");
                    break;
                }
                is_readed = (ptr[0] == 0);
                sem_post(sem);
                
                struct timespec ts;
                ts.tv_sec = 0; 
                ts.tv_nsec = 100 * 1000000; 
                nanosleep(&ts, NULL);
            }
        }
        
        printf("Producer: память закончилась, прекращаю работу\n");

        munmap(data, SHM_SIZE);
        close(shm_fd);
        sleep(5);

        if (shm_unlink(SHM_NAME) == -1) {
            perror("Producer: shm_unlink");
        }
        if (sem_close(sem) == -1) {
            perror("Producer: sem_close");
        }
        if (sem_unlink(SEM_NAME) == -1) {
            perror("Producer: sem_unlink");
        }
        printf("Producer: закрыл все\n");
    } 
    else {
        perror("sem_open");
        exit(1);
    }

    return 0;
}