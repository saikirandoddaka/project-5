#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>

#include "types.h"
#include "osstime.h"

#define KEY 19283746

#define EXIT_ON_ERROR(VAR, STR) if (VAR == -1) { \
		perror(STR); \
		exit(1); \
	}

#define max(a,b) \
  ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b; })

#define min(a,b) \
  ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })

#define KEY_PREFIX 123456

#define PCB_NUM 18
#define RESOURCE_NUM 20
#define RES_INTERVAL 10000

typedef enum { S_NOT_STARTED, S_ACTIVE, S_BLOCKED } process_state;

typedef struct {
	int pid;
    process_state state;
	int msq_to_user;
	int msq_to_oss;
    int blocked_on;
} pcb;

typedef struct {
    bool shared;
    int limit;
    int allocated[PCB_NUM];
} resource;

struct shm_data_t {
	osstime cpu_clock;
	pcb pcbs[PCB_NUM];
    resource resources[RESOURCE_NUM];
};

int shmid;
struct shm_data_t *shm;

void allocate();
void deallocate();
void attach();
void detach();

int rnd(int min, int max);
bool chance(int percent);

#endif
