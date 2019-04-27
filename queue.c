#include <stdlib.h>
#include "queue.h"

/* A queue implementation based on singly-linked list */

/* Create an empty queue */
queue *make_queue() {
	queue *q = malloc(sizeof(queue));
	q->head = NULL;
	return q;
}

/* Free all memory allocated by the queue */
void free_queue(queue *q) {
	while (q->head != NULL) {
		dequeue(q);
	}
	free(q);
}

/* Check if the queue is empty */
int queue_empty(queue *q) {
	return q->head == NULL;
}

/* Put an item into the queue */
void enqueue(queue *q, DATA_TYPE data) {
	node *n = malloc(sizeof(node));
	n->data = data;
	n->next = NULL;
	if (q->head == NULL) {
		q->head = n;
		q->tail = n;
	} else {
		q->tail->next = n;
		q->tail = n;
	}
}

/* Remove an item from the queue and return it */
DATA_TYPE dequeue(queue *q) {
	node *n = q->head;
	q->head = n->next;
	DATA_TYPE data = n->data;
	free(n);
	return data;
}

DATA_TYPE peek_queue(queue *q) {
	return q->head->data;
}
