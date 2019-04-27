#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "common.h"

void deallocate() {
	struct shmid_ds shmid_ds;
	int result;

	result = shmctl(shmid, IPC_RMID, &shmid_ds);
	EXIT_ON_ERROR(result, "shmctl")
}

void allocate() {
	shmid = shmget (KEY, sizeof(struct shm_data_t), IPC_CREAT | 0666);

	EXIT_ON_ERROR(shmid, "shmget")
}

void attach() {
	shm = shmat(shmid, NULL, 0);
    if (shm == (void*)-1) {
        perror("shmat");
        exit(1);
    }
}

void detach() {
	int result;

	result = shmdt(shm);
	EXIT_ON_ERROR(result, "shmdt")
}

int rnd(int min, int max)
{
    return rand() % (max-min+1) + min;
}

bool chance(int percent)
{
    return (rand() % 100) < percent;
}
