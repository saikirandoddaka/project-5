#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#include "common.h"
#include "messages.h"
#include "queue.h"
#include "osstime.h"

/* constants */

#define PERMS 0666

// Ratio of normal processes per each rt process
#define NORMAL_PROCS 4

#define BASE_QUANTUM 10000

/* global variables */
bool verbose = false;
uint max_run_time = 3;
int childrenLimit = PCB_NUM;
bool running = true;
FILE* log_file;
uint log_lines = 0;
int taken[PCB_NUM];
osstime next_proc;
osstime next_dedeadlock;

queue queues[4];

/* statistics */
int requests_granted = 0;
int killed_procs = 0;
int terminated_procs = 0;
int dedeadlocks_run = 0;

/* function prototypes */
int find_free_pid();
void spawn_process(int pid_to_spawn);
void cleanup_processes();
void cleanup_process(int pid);
void signalHandler(int sig);

/* Prints a log line.
 * time - add "at xxx:yyy" to the end of message
 * fmt - format string
 */
void logprintf(bool time, const char *fmt, ...) {
	va_list ap;

    if (!verbose)
        return;

	/* Don't print more than 100000 lines */
	if (log_lines > 100000)
		return;

	log_lines++;

    fprintf(log_file, "OSS: ");
	/* Print the message */
	va_start(ap, fmt);
    vfprintf(log_file, fmt, ap);
	va_end(ap);
	/* Append current time if needed */
	if (time)
        fprintf(log_file, " at time %d:%d", shm->cpu_clock.sec, shm->cpu_clock.usec);
    fprintf(log_file, "\n");
    fflush(log_file);
}

void forcelogprintf(const char *fmt, ...) {
	va_list ap;

	log_lines++;

    fprintf(log_file, "OSS: ");
    /* Print the message */
	va_start(ap, fmt);
    vfprintf(log_file, fmt, ap);
	va_end(ap);
    fprintf(log_file, "\n");
    fflush(log_file);
}

void printres() {

    if (!verbose)
        return;

    // shareable
    fprintf(log_file, "        ");
    for (int res = 0; res < RESOURCE_NUM; res++)
        fprintf(log_file, shm->resources[res].shared ? "s   " : "ns  ");
    fprintf(log_file, "\n");
    log_lines++;

    // Header row
    fprintf(log_file, "bl      ");
    for (int res = 0; res < RESOURCE_NUM; res++)
        fprintf(log_file, "R%-3d", res);
    fprintf(log_file, "\n");
    log_lines++;

    // Table
    for (int pid = 0; pid < PCB_NUM; pid++) {
        if (shm->pcbs[pid].state == S_BLOCKED)
            fprintf(log_file, "%-4dP%-3d", shm->pcbs[pid].blocked_on, pid);
        else
            fprintf(log_file, "    P%-3d", pid);
        for (int res = 0; res < RESOURCE_NUM; res++)
            fprintf(log_file, "%-4d", shm->resources[res].allocated[pid]);
        fprintf(log_file, "\n");
        log_lines++;
    }
}

void kill_process(int pid)
{
    kill(taken[pid], SIGUSR1);
    killed_procs++;
    cleanup_process(pid);
}

void init() {

	/* handle signal to stop when "t" seconds elapsed */
	if (signal(SIGALRM, signalHandler) == SIG_ERR) {
		perror("signal SIGALRM\n");
		exit(1);
	}

	if (signal(SIGINT, signalHandler) == SIG_ERR) {
		perror("signal SIGINT\n");
		exit(1);
	}

	if (signal(SIGTERM, signalHandler) == SIG_ERR) {
		perror("signal SIGTERM\n");
		exit(1);
	}

	/* start alarm timer */
    alarm(max_run_time);

    log_file = fopen("log.txt", "w");

    srand(getpid());

    /* shared memory allocation and attach */
    allocate();
	attach();

    // Init shm
    memset(shm, 0, sizeof(struct shm_data_t));

    // Init resources
    for (int i = 0; i < RESOURCE_NUM; i++) {
        // 20% of the resources are shareable
        shm->resources[i].shared = chance(20);
        logprintf(false, "Initializing %s Resource R%d.", shm->resources[i].shared ? "shareable" : "non-shareable", i);
        // Limits are 1-10
        shm->resources[i].limit = (rnd(1, 10));
    }

	/* Some data structures */
	next_proc.sec = 0;
	next_proc.usec = 0;
    next_dedeadlock.sec = 1;
    next_dedeadlock.usec = 0;
}

void uninit() {
    fclose(log_file);
    cleanup_processes();
    deallocate();
}

/* Count actually existing children */
int count_children() {
	int count = 0;
	for (int i = 0; i < PCB_NUM; ++i) {
		if (taken[i] != 0)
			count++;
	}
	return count;
}

void schedule_proc_spawn()
{
    int next_proc_rnd = rnd(1, 500);
    osstime_advance(&next_proc, next_proc_rnd);
}

void maybe_spawn_process()
{
    if (osstime_cmp(&next_proc, &shm->cpu_clock) <= 0) {
        int new_pid = find_free_pid();
        // Skip making a process if already reached limit
        if (new_pid != -1)
            spawn_process(new_pid);
        schedule_proc_spawn();
    }
}

int resource_total_allocated(int res_id)
{
    int total = 0;
    for (int i = 0; i < PCB_NUM; i++) {
        total += shm->resources[res_id].allocated[i];
    }
    return total;
}

void block_process(uint pid, int res_id)
{
    logprintf(false, "Blocking process P%d waiting on resource R%d", pid, res_id);
    shm->pcbs[pid].state = S_BLOCKED;
    shm->pcbs[pid].blocked_on = res_id;
    osstime_advance(&shm->cpu_clock, rnd(10, 50));
}

void allocate_resource(uint pid, int res_id)
{
    pcb *p = &shm->pcbs[pid];
    ipc_message msg;
    msg._msgtyp = 0;
    shm->resources[res_id].allocated[pid]++;

    msg.type = ALLOCATE;
    msg.res_id = res_id;
    msgsnd(p->msq_to_user, &msg, msg_size, 0);
    osstime_advance(&shm->cpu_clock, rnd(1, 10));
    logprintf(true, "Master granting P%d request R%d", pid, res_id);

    requests_granted++;
    if (requests_granted % 20 == 0)
        printres();
}

void unblock_process(uint pid, int res_id)
{
    logprintf(false, "Unblocking Process P%d, and granting it Resource R%d", pid, res_id);
    shm->pcbs[pid].state = S_ACTIVE;
    shm->pcbs[pid].blocked_on = -1;
    osstime_advance(&shm->cpu_clock, rnd(1, 50));
    allocate_resource(pid, res_id);
}

void resource_requested(uint pid, int res_id)
{
    // Non-shared resource
    if (resource_total_allocated(res_id) - shm->resources[res_id].allocated[pid] > 0) {
        block_process(pid, res_id);
        return;
    }

    // Limit reached
    if (resource_total_allocated(res_id) >= shm->resources[res_id].limit) {
        block_process(pid, res_id);
        return;
    }

    // Everything's fine
    allocate_resource(pid, res_id);
}

void wake_up_on_resource(int res_id)
{
    int start = rand() % PCB_NUM;
    for (int i = (start+1)%PCB_NUM; i != start; i = (i+1)%PCB_NUM)
        if (shm->pcbs[i].state == S_BLOCKED &&
                shm->pcbs[i].blocked_on == res_id)
            if (shm->resources[res_id].shared ||
                    resource_total_allocated(res_id) == 0) {
                unblock_process(i, res_id);
                if (!shm->resources[res_id].shared)
                    return;
            }
}

void resource_released(uint pid, int res_id)
{
    shm->resources[res_id].allocated[pid]--;
    wake_up_on_resource(res_id);
}

void process(uint pid)
{
    pcb *p = &shm->pcbs[pid];
    ipc_message msg;
    msg._msgtyp = 0;
    msg.type = PROCESS;
    msgsnd(p->msq_to_user, &msg, msg_size, 0);
    if (-1 == msgrcv(p->msq_to_oss, &msg, msg_size, 0, 0)) {
        if (errno == EINTR)
            return;
        perror("msgrcv");
        exit(1);
    }
    switch (msg.type) {
    case REQUEST:
        logprintf(true, "Master has detected Process P%d requesting R%d", pid, msg.res_id);
        resource_requested(pid, msg.res_id);
        break;
    case RELEASE:
        logprintf(true, "Master has acknowledged Process P%d releasing R%d", pid, msg.res_id);
        resource_released(pid, msg.res_id);
        break;
    case IDLE:
        break;
    case RELEASE_ALL_AND_TERMINATE:
        logprintf(true, "Process P%d terminating normally", pid);
        terminated_procs++;
        cleanup_process(pid);
        break;
    default:
        fprintf(stderr, "Unknown message type in oss: %d", msg.type);
        exit(1);
    }
    osstime_advance(&shm->cpu_clock, rnd(1, 10));
}

void dedeadlock()
{
    bool visited[PCB_NUM];
    memset(visited, false, sizeof(visited));

    osstime_advance(&shm->cpu_clock, rnd(50, 100));
    forcelogprintf("Master running deadlock detection at %d:%d", shm->cpu_clock.sec, shm->cpu_clock.usec);

    queue *next = make_queue();
    for (int i = 0; i < PCB_NUM; i++)
        if (shm->pcbs[i].state == S_BLOCKED) {
            enqueue(next, i);
            break;
        }
    while (!queue_empty(next)) {
        uint npid = dequeue(next);
        int critical_resource = shm->pcbs[npid].blocked_on;
        visited[npid] = true;
        for (int i = 0; i < PCB_NUM; i++) {
            if (shm->resources[critical_resource].allocated[i] && i != npid) {
                if (visited[i]) {
                    forcelogprintf("Process P%d is part of a deadlock", i);
                    kill_process(i);
                    dedeadlock();
                    return;
                }
                if (shm->pcbs[i].state == S_BLOCKED)
                    enqueue(next, i);
            }
        }
    }
    osstime_advance(&shm->cpu_clock, rnd(50, 100));
    forcelogprintf("System not in deadlock");
    free_queue(next);
}

void maint() {
    bool have_running_process = false;
	// Spawn new processes
    maybe_spawn_process();

    // Tell all processes to do their things
    for (int i = 0; i < PCB_NUM && running; i++)
        if (shm->pcbs[i].state == S_ACTIVE) {
            process(i);
            have_running_process = true;
        }

    // Run deadlock detection algorithm
    if (osstime_cmp(&next_dedeadlock, &shm->cpu_clock) <= 0) {
        dedeadlocks_run++;
        dedeadlock();
        osstime_advance(&next_dedeadlock, 100000000);
    }

    osstime_advance(&shm->cpu_clock, rnd(10, 50));

    // Speed up time to spawning more processes and/or deadlock resolution if no running processes
    if (!have_running_process)
        osstime_advance(&shm->cpu_clock, 10000);
}

void main_loop() {
	maint();
	usleep(0);
}

int main(int argc, char *argv[]) {

	init();

    if (!strcmp(argv[1], "-v"))
        verbose = true;

    spawn_process(0);
    schedule_proc_spawn();

	/* main loop */
    while(running) {
		main_loop();
	}
	printf("Terminating oss\n");
    forcelogprintf("Terminating oss=========================================");
    forcelogprintf("Granted %d resources", requests_granted);
    forcelogprintf("Processes terminated normally: %d", terminated_procs);
    forcelogprintf("Processes killed by deadlock recovery: %d", killed_procs);
    forcelogprintf("Deadlock recoveries run: %d", dedeadlocks_run);
    forcelogprintf("Closing log at time %d:%d", shm->cpu_clock.sec, shm->cpu_clock.usec);

	uninit();

	return 0;
}

void signalHandler(int sig) {

	printf("signalHandler %d\n", sig);

	switch(sig) {
		case SIGINT:
		case SIGTERM:
			printf("handling SIGINT/SIGTERM signal\n");
            running = false;
			break;
		case SIGALRM:
			printf("handling SIGALRM signal\n");
            running = false;
			break;

		default:
			break;
	}
}

/* Finds a free PID for new process or -1 if no PID available */
int find_free_pid() {
	for (int i = 0; i < PCB_NUM; ++i) {
		if (taken[i] == 0) {
			return i;
		}
	}
	return -1;
}

/* Spawns a new user process */
void spawn_process(int pid_to_spawn) {
	pid_t pid;
	int i = 0;

	/* Do nothing if children limit reached */
	if (count_children() >= childrenLimit)
		return;

    logprintf(true, "Spawning a new Process P%d", pid_to_spawn);

	/* Fork a process */
	pid = fork();
	if (pid) {
		/* Create structures for keeping its data in master process */
		taken[pid_to_spawn] = pid;
		pcb *pcb = &shm->pcbs[pid_to_spawn];
		pcb->pid = pid_to_spawn;
		pcb->state = S_ACTIVE;
        pcb->blocked_on = -1;

		key_t k1 = KEY_PREFIX + 2*pid+1;
		key_t k2 = KEY_PREFIX + 2*pid+2;

		pcb->msq_to_user = msgget(k1, IPC_CREAT | PERMS);
		pcb->msq_to_oss = msgget(k2, IPC_CREAT | PERMS);
		EXIT_ON_ERROR(pcb->msq_to_user, "msgget");
		EXIT_ON_ERROR(pcb->msq_to_oss, "msgget");
	} else {
		/* Run user process */
		char *pid_str = malloc(4 * sizeof(char));
		sprintf(pid_str, "%d", pid_to_spawn);
		execl("./user", "./user", pid_str, (char*)NULL);
		perror("execl: ");
		exit(1);
	}
}

int msqrm(int msqid) {
    if (-1 == msgctl(msqid, IPC_RMID, NULL))
        perror("msgrm");
}

/* Delete a user process's data structures */
void cleanup_process(int pid) {
	pcb *pcb = &shm->pcbs[pid];
    char released[1024], resstr[128];
    released[0] = 0;
    resstr[0] = 0;

    logprintf(true, "Terminating process p%d", pid);

	msqrm(pcb->msq_to_oss);
	msqrm(pcb->msq_to_user);

    waitpid(taken[pid], NULL, 0);
    taken[pid] = 0;

    for (int i = 0; i < RESOURCE_NUM; i++)
        if (shm->resources[i].allocated[pid] > 0) {
            sprintf(resstr, " R%d:%d", i, shm->resources[i].allocated[pid]);
            strcat(released, resstr);
        }
    logprintf(false, "Released resources: %s", released);
    for (int i = 0; i < RESOURCE_NUM; i++)
        if (shm->resources[i].allocated[pid] > 0) {
            shm->resources[i].allocated[pid] = 0;
            wake_up_on_resource(i);
        }

    memset(pcb, 0, sizeof(pcb));
    pcb->blocked_on = -1;
}

/* Kill all processes with SIGUSR1 */
void cleanup_processes() {
	int i;
	for (i = 0; i < PCB_NUM; i++) {
		pcb *pcb = &shm->pcbs[i];
		msqrm(pcb->msq_to_oss);
		msqrm(pcb->msq_to_user);

		if (taken[i] == 0)
			continue;
		printf("kill %d\n", taken[i]);
		kill(taken[i], SIGUSR1);
		cleanup_process(i);
	}
}

