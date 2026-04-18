#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>

#define D_FILENAME "/etc/passwd"
#define D_LINE_MAX 1024
#define D_EMPTY_MAX 1

#define D_SEM_PROJ_ID 123123
#define D_SHM_PROJ_ID 321321

typedef struct SharedData {
  int numbers[D_LINE_MAX];
  int size;
  int min;
  int max;
} SharedData;

union semun {
  int val;
  struct semid_ds *buf;
  unsigned short *array;
};

struct sembuf lock_res = {0, -1, 0};
struct sembuf rel_res = {0, 1, 0};
struct sembuf push[2] = {{1, -1, IPC_NOWAIT}, {2, 1, IPC_NOWAIT}};
struct sembuf pop[2] = {{1, 1, IPC_NOWAIT}, {2, -1, IPC_NOWAIT}};

int is_running = 1;

pid_t pid = -1;
int shmid = -1;
int semid = -1;
SharedData *shared = (void *)-1;

int data_processed = 0;

int sem_init(const char *filename);
int shm_init(const char *filename);

int init(char *filename);
int writer_cleanup();
int reader_cleanup();

void handle_int(int sig);

int produce_data(int data[D_LINE_MAX]);
int put_data(int data[D_LINE_MAX], int size);
int writer_loop();

int get_min_max(int text[D_LINE_MAX], int size, int *min, int *max);
int get_data();
int reader_loop();

int main(int argc, char **argv) {
  if (argc > 2) {
    fprintf(stderr, "Usage: %s [filename]\n", argv[0]);
    fprintf(stderr,
            "     filename - optional, if not present /etc/passwd is used\n");
    return EXIT_FAILURE;
  }

  struct sigaction sa_int = {.sa_handler = handle_int, .sa_flags = SA_RESTART};
  if (sigaction(SIGINT, &sa_int, NULL) == -1) {
    perror("Failed to register SIGINT handler");
    return EXIT_FAILURE;
  }

  srand(time(NULL));

  int status = EXIT_SUCCESS;

  char *filename = argc == 2 ? argv[1] : D_FILENAME;

  if (init(filename) == -1) {
    writer_cleanup();
    return EXIT_FAILURE;
  }

  pid = fork();
  if (pid == -1) {
    writer_cleanup();
    return EXIT_FAILURE;
  }

  if (pid == 0) {
    pid = getpid();
    status = reader_loop() == -1 ? EXIT_FAILURE : EXIT_SUCCESS;
    if (reader_loop() == -1) {
      status = EXIT_FAILURE;
    }
  } else {
    pid = getpid();
    status = writer_loop() == -1 ? EXIT_FAILURE : EXIT_SUCCESS;
    printf("PARENT [%d]: Data arrays processed: %d\n", pid, data_processed);
    if (writer_cleanup() == -1) {
      status = EXIT_FAILURE;
    }
  }

  return status;
}

int reader_cleanup() {
  if (shared != (void *)-1 && shmdt(shared) == -1) {
    perror("Failed to detach shared memory");
    return -1;
  }

  return 0;
}

int writer_cleanup() {
  int status = 0;
  if (semid != -1 && semctl(semid, 0, IPC_RMID) == -1) {
    perror("Failed to remove semaphore");
    status = -1;
  }

  if (shared != (void *)-1 && shmdt(shared) == -1) {
    perror("Failed to detach shared memory");
    status = -1;
  }

  if (shmid != -1 && shmctl(shmid, IPC_RMID, NULL) == -1) {
    perror("Failed to close shared memory");
    status = -1;
  }

  return status;
}

int init(char *filename) {
  shmid = shm_init(filename);
  if (shmid == -1) {
    return -1;
  }

  semid = sem_init(filename);
  if (semid == -1) {
    return -1;
  }

  return 0;
}

int shm_init(const char *filename) {
  key_t key = ftok(filename, D_SHM_PROJ_ID);
  if (key == -1) {
    perror("Failed to generate key for shared memory");
    return -1;
  }

  int shmid = shmget(key, sizeof(SharedData), IPC_CREAT | IPC_EXCL | 0600);
  if (shmid == -1) {
    perror("Failed to get shared memory");
    return -1;
  }

  shared = (SharedData *)shmat(shmid, NULL, 0);
  if (shared == (void *)-1) {
    perror("Failed to attach shared memory");
    return -1;
  }

  return shmid;
}

int sem_init(const char *filename) {
  key_t key = ftok(filename, D_SEM_PROJ_ID);
  if (key == -1) {
    perror("Failed to generate key for semaphore");
    return -1;
  }

  // 1mutex, 2empty - N, 3full - 0
  int nsems = 3;
  int semid = semget(key, nsems, IPC_CREAT | IPC_EXCL | 0600);
  if (semid == -1) {
    perror("Failed to create/open semaphore");
    return -1;
  }

  union semun arg;
  arg.val = 1;
  if (semctl(semid, 0, SETVAL, arg) == -1) {
    perror("Failed to set semaphore value");
    return -1;
  }

  arg.val = D_EMPTY_MAX;
  if (semctl(semid, 1, SETVAL, arg) == -1) {
    perror("Failed to set semaphore value");
    return -1;
  }

  arg.val = 0;
  if (semctl(semid, 2, SETVAL, arg) == -1) {
    perror("Failed to set semaphore value");
    return -1;
  }

  return semid;
}

int writer_loop() {
  while (is_running) {
    int data[D_LINE_MAX];
    int size = produce_data(data);
    int status = put_data(data, size);
    if (status == -1) {
      return -1;
    }

    if (status == 1) {
      usleep(500000);
    }
  }

  return 0;
}

int produce_data(int data[D_LINE_MAX]) {
  int num_count = rand() % (D_LINE_MAX / 2) + D_LINE_MAX / 2;
  for (int i = 0; i < num_count; i++) {
    data[i] = rand();
  }

  return num_count;
}

int put_data(int data[D_LINE_MAX], int size) {
  if (semop(semid, push, 2) == -1) {
    if (errno == EAGAIN) {
      printf("PARENT [%d]: Buffer full\n", pid);
    }

    return errno == EAGAIN ? 1 : -1;
  }

  if (semop(semid, &lock_res, 1) == -1) {
    perror("Failed to lock mutex semaphore");
    return -1;
  }

  if (shared->size == -1) {
    printf("PARNET [%d]: Received Min: %d, Max: %d\n", pid, shared->min,
           shared->max);
    data_processed++;
  }

  shared->size = size;
  memcpy(shared->numbers, data, size * sizeof(int));

  if (semop(semid, &rel_res, 1) == -1) {
    perror("Failed to release mutex semaphore");
    return -1;
  }

  return 0;
}

int reader_loop() {
  while (is_running) {
    int status = get_data();
    if (status == -1) {
      return -1;
    }

    if (status == 1) {
      usleep(500000);
      continue;
    }
  }

  return 0;
}

int get_min_max(int numbers[D_LINE_MAX], int size, int *min, int *max) {
  if (size < 1) {
    return 0;
  }

  *min = numbers[0];
  *max = numbers[0];

  for (int i = 0; i < size; i++) {
    int num = numbers[i];
    if (num < *min) {
      *min = num;
    } else {
      *max = num;
    }
  }

  return 0;
}

int get_data() {
  if (semop(semid, pop, 2) == -1) {
    if (errno == EAGAIN) {
      printf("CHILD [%d]: Buffer empty\n", pid);
    }

    return errno == EAGAIN ? 1 : -1;
  }

  if (semop(semid, &lock_res, 1) == -1) {
    perror("Failed to lock mutex semaphore");
    return -1;
  }

  if (shared->size > 0) {
    get_min_max(shared->numbers, shared->size, &shared->min, &shared->max);
    shared->size = -1;
    printf("CHILD [%d]: Found min: %d, max: %d\n", pid, shared->min,
           shared->max);
  }

  if (semop(semid, &rel_res, 1) == -1) {
    perror("Failed to release mutex semaphore");
    return -1;
  }

  return 0;
}

void handle_int(int sig) {
  if (sig == SIGINT) {
    is_running = 0;
  }
}