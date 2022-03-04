#ifndef TIMESPEC_LIB_H
#define TIMESPEC_LIB_H

constexpr long NS_PER_SECOND = 1000000000;

void sub_timespec(struct timespec t1, struct timespec t2, struct timespec *td);

#endif