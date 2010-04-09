#ifndef __INSTRUMENTATION_COMMON_H__
#define __INSTRUMENTATION_COMMON_H__
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>

#define __MAX_STRING_SIZE 1024

//#define COMPILE_32BIT
#ifdef COMPILE_32BIT
  #define DINT_TYPE int32_t
  #define DINT_PRNTSZ l
#else
  #define DINT_TYPE int64_t
  #define DINT_PRNTSZ ll
#endif // COMPILE_32BIT

#define PRINT_INSTR(__file, ...) fprintf(__file, "-[p%d]- ", getpid());  \
    fprintf(__file, __VA_ARGS__); \
    fprintf(__file, "\n"); \
    fflush(__file);
#define PRINT_DEBUG(...) 
//#define PRINT_DEBUG(...) PRINT_INSTR(__VA_ARGS__)

#define CLOCK_RATE_HZ 2600000000

//#define EXCLUDE_TIMER

__inline__ unsigned long long readtsc(){
    unsigned low, high;
    __asm__ volatile ("rdtsc" : "=a" (low), "=d"(high));
    return ((unsigned long long)low | (((unsigned long long)high) << 32));
}
#endif // __INSTRUMENTATION_COMMON_H__
