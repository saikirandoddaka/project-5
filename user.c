#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>

#include "common.h"
#include "messages.h"

#define DEBUG

#ifdef DEBUG
FILE *log_file;
#define LOG(...) { fprintf(log_file, __VA_ARGS__); fprintf(log_file, "\n"); fflush(log_file); }
#else
#define LOG(...) {}
#endif

int running = true;
osstime start_time;
osstime next_term;
osstime next_res;
int allocated[RESOURCE_NUM];

void signalHandler(int sig) {

	switch(sig) {
		case SIGUSR1:
			printf("user signal exiting %d\n", sig);
            running = false;
			break;

		default:
			break;
	}	
}

void res_allocate(int id)
{
    allocated[id]++;
}

void res_deallocate(int id)
{
    allocated[id]--;
}

unsigned int pid;
pcb *my_pcb;

void init() {
	if (signal(SIGUSR1, signalHandler) == SIG_ERR) {
		perror("signal SIGUSR1\n");
		exit(1);
	}

	/* open shared memory that was created by OSS */
    allocate();
	attach();

	/* use pid to initialize random */
	srand(getpid());

	my_pcb = &shm->pcbs[pid];

}

void terminate()
{
    ipc_message msg;
    msg._msgtyp = 0;
    if (chance(20)) {
        LOG("Terminating normally");
        msg.type = RELEASE_ALL_AND_TERMINATE;
        running = false;
        msgsnd(my_pcb->msq_to_oss, &msg, msg_size, 0);
        LOG("Sending TERMINATE");
        return;
    }
    msg.type = IDLE;
    msgsnd(my_pcb->msq_to_oss, &msg, msg_size, 0);
    LOG("Sending IDLE");
}

void request()
{
    ipc_message msg;
    msg._msgtyp = 0;
    msg.type = REQUEST;
    do {
        msg.res_id = rand() % RESOURCE_NUM;
        // Don't request a resource if we're already holding all of it
    } while (allocated[msg.res_id] >= shm->resources[msg.res_id].limit);
    msgsnd(my_pcb->msq_to_oss, &msg, msg_size, 0);
    LOG("Sending REQUEST");
}

void release()
{
    ipc_message msg;
    msg._msgtyp = 0;
    msg.type = RELEASE;
    int allocated_types = 0;
    int to_release;
    int rid;

    for (int i = 0; i < RESOURCE_NUM; i++)
        if (allocated[i] > 0)
            allocated_types++;

    if (allocated_types == 0) {
        request();
        return;
    }
    to_release = rand() % allocated_types + 1;

    for (int i = 0; i <= RESOURCE_NUM; i++) {
        if (allocated[i] > 0)
            to_release--;
        if (to_release == 0) {
            rid = i;
            break;
        }
    }

    res_deallocate(rid);
    msg.res_id = rid;
    msgsnd(my_pcb->msq_to_oss, &msg, msg_size, 0);
    LOG("Sending RELEASE");
}

void process()
{
    ipc_message msg;
    msg._msgtyp = 0;
    LOG("Handling message");
    // Check if terminating
    if (osstime_cmp(&next_term, &shm->cpu_clock) <= 0) {
        terminate();
        osstime_advance(&next_term, rand() % 250);
        return;
    }

    // Check if requesting/releasing resource
    if (osstime_cmp(&next_res, &shm->cpu_clock) <= 0) {
        if (rand() % 2) {
            request();
        } else {
            release();
        }
        osstime_advance(&next_res, rand() % RES_INTERVAL);
        return;
    }
    msg.type = IDLE;
    msgsnd(my_pcb->msq_to_oss, &msg, msg_size, 0);
    LOG("Sending IDLE");
}

void main_loop() {
	ipc_message msg;
    msg._msgtyp = 0;

    LOG("Main loop");
    // Get oss message
    if (-1 == msgrcv(my_pcb->msq_to_user, &msg, msg_size, 0, 0) && errno == EINTR) {
        // msgq disconnected
        running = false;
        return;
    }

    LOG("%d Received message", pid);
    switch (msg.type) {
    case PROCESS:
        LOG("Process");
        process();
        break;
    case ALLOCATE:
        LOG("Allocate");
        res_allocate(msg.res_id);
        break;
    default:
        LOG("Terminate");
        msg.type = RELEASE_ALL_AND_TERMINATE;
        msgsnd(my_pcb->msq_to_oss, &msg, msg_size, 0);
        running = false;
    }
}

void deinit() {
	detach();
}

int main(int argc, char *argv[]) {

	pid = atoi(argv[1]);

    printf("Process %d started\n", pid);

#ifdef DEBUG
    log_file = fopen(argv[1], "w");
#endif
    init();

    start_time = shm->cpu_clock;
    next_term = shm->cpu_clock;
    osstime_advance(&next_term, 100000000);
    next_res = shm->cpu_clock;
    osstime_advance(&next_res, rand() % RES_INTERVAL);

    while(running) {
		main_loop();
		usleep(0);
	}

	deinit();

    printf("Process %d terminated\n", pid);

#ifdef DEBUG
    fclose(log_file);
#endif
    return 0;
}
