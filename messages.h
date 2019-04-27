#ifndef MESSAGES_H
#define MESSAGES_H

#include <stdlib.h>

#include "osstime.h"
#include "types.h"

typedef enum {
    ANY,
// oss -> user
    PROCESS, ALLOCATE,
// user -> oss
    REQUEST, RELEASE, IDLE, RELEASE_ALL_AND_TERMINATE
} message_type;

typedef struct {
    union {
        message_type type;
        long _msgtyp;
    };
    int res_id;
} ipc_message;

extern const size_t msg_size;

#endif
