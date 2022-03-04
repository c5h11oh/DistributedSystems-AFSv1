#include "timespec_lib.h"

#include <time.h>

// Reference: https://stackoverflow.com/questions/53708076/what-is-the-proper-way-to-use-clock-gettime

void sub_timespec(struct timespec t1, struct timespec t2, struct timespec *td) {
  td->tv_nsec = t2.tv_nsec - t1.tv_nsec;
  td->tv_sec  = t2.tv_sec - t1.tv_sec;
  if (td->tv_nsec < 0) {
    td->tv_nsec += NS_PER_SECOND;
    td->tv_sec--;
  }
}