#ifndef OSSTIME_H
#define OSSTIME_H

#include "types.h"

typedef struct {
	ulong sec;
	ulong usec;
} osstime;

void osstime_add(osstime *left, osstime *right);
void osstime_advance(osstime *left, uint usec);
void osstime_sub(osstime *left, osstime *right);
void osstime_mul(osstime *left, double right);
int osstime_cmp(osstime *left, osstime *right);
osstime *make_osstime(ulong sec, ulong usec);

#endif
