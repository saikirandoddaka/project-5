#ifndef QUEUE_H
#define QUEUE_H

#include "types.h"

#define DATA_TYPE uint

typedef struct __node_t {
	struct __node_t *next;
	DATA_TYPE data;
} node;

typedef struct {
	node *head;
	node *tail;
} queue;

queue *make_queue();
void free_queue(queue *q);
int queue_empty(queue *q);
void enqueue(queue *q, DATA_TYPE data);
DATA_TYPE dequeue(queue *q);
DATA_TYPE peek_queue(queue *q);

#endif
