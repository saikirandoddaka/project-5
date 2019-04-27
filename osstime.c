#include <stdlib.h>
#include <math.h>
#include "osstime.h"

/* This module contains implementation of osstime functions
 * osstime struct holds sec:usec which is used by oss for time-tracking
 */

/* Adds right to left's time */
void osstime_add(osstime *left, osstime *right) {
    left->sec += right->sec;
    left->usec += right->usec;
    if (left->usec > 100000000) {
        left->usec -= 100000000;
        left->sec++;
    }
}

/* Adds usec to left's time */
void osstime_advance(osstime *left, uint usec) {
    left->usec += usec;
    left->sec += left->usec / 100000000;
    left->usec = left->usec % 100000000;
}

/* Finds time difference between left and right */
void osstime_sub(osstime *left, osstime *right) {
    if (left->usec < right->usec) {
        left->sec--;
        left->usec += 100000000;
    }
    left->sec -= right->sec;
    left->usec -= right->usec;
}

/* Multiplies time in left by coefficient in right */
void osstime_mul(osstime *left, double right) {
    double sec = left->sec * right;
    left->sec = trunc(sec);
    left->usec = (sec - left->sec) * 100000000 + left->usec * right;
}

/* Compares two times. Returns
 * -1 if left is greater, 1 if right is greater and 0 if they're equal
 */
int osstime_cmp(osstime *left, osstime *right) {
    if (left->sec < right->sec)
        return -1;
    if (left->sec > right->sec)
        return 1;
    if (left->usec < right->usec)
        return -1;
    if (left->usec > right->usec)
        return 1;
    return 0;
}

/* Creates an osstime struct with specified times */
osstime *make_osstime(ulong sec, ulong usec) {
	osstime *t = malloc(sizeof(osstime));
	t->sec = sec;
	t->usec = usec;
	return t;
}
