#ifndef __INSTRUMENTATION_COMMON_H__
#define __INSTRUMENTATION_COMMON_H__
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <stdarg.h>

#define __MAX_STRING_SIZE 1024

//#define USING_MPI_WRAPPERS
#define USING_CSTD_WRAPPERS

//#define COMPILE_32BIT
#ifdef COMPILE_32BIT
  #define DINT_TYPE int32_t
  #define DINT_PRNTSZ l
#else
  #define DINT_TYPE int64_t
  #define DINT_PRNTSZ ll
#endif // COMPILE_32BIT

#define CLOCK_RATE_HZ 2600000000

//#define EXCLUDE_TIMER

__inline__ unsigned long long readtsc(){
    unsigned low, high;
    __asm__ volatile ("rdtsc" : "=a" (low), "=d"(high));
    return ((unsigned long long)low | (((unsigned long long)high) << 32));
}

typedef struct
{
    int64_t pt_vaddr;
    int64_t pt_target;
    int64_t pt_flags;
    int32_t pt_size;
    int32_t pt_blockid;
    unsigned char pt_content[16];
    unsigned char pt_disable[16];
} instpoint_info;

#define __wrapper_name(__fname) \
    __fname ## _pebil_wrapper

// simplistic method to fill format strings
int write_formatstr(char* str, const char* format, int64_t* args){
    int i;
    int isescaped = 0, argcount = 0;
    for (i = 0; i < strlen(format); i++){
        if (format[i] == '\\'){
            isescaped++;
        } else {
            if (format[i] == '%' && !isescaped){
                argcount++;
            }
            isescaped = 0;
        }
    }
    switch (argcount){
    case 0:
        sprintf(str, format);
        break;
    case 1:
        sprintf(str, format, args[0]);
        break;
    case 2:
        sprintf(str, format, args[0], args[1]);
        break;
    case 3:
        sprintf(str, format, args[0], args[1], args[2]);
        break;
    case 4:
        sprintf(str, format, args[0], args[1], args[2], args[3]);
        break;
    case 5:
        sprintf(str, format, args[0], args[1], args[2], args[3], args[4]);
        break;
    case 6:
        sprintf(str, format, args[0], args[1], args[2], args[3], args[4], args[5]);
        break;
    default:
        return;
    }
}

int taskid;
#ifdef USING_MPI_WRAPPERS
#define __taskid taskid
#define __ntasks ntasks
#define __taskmarker "-[t%d]- "

#include <mpi.h>

int __ntasks;
int __wrapper_name(MPI_Init)(int* argc, char*** argv){
    int retval = MPI_Init(argc, argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &__taskid);
    MPI_Comm_size(MPI_COMM_WORLD, &__ntasks);

    fprintf(stdout, "-[p%d]- remapping to taskid %d/%d in MPI_Init wrapper\n", getpid(), __taskid, __ntasks);

    return retval;
}

#else
#define __taskid getpid()
#define __ntasks 1
#define __taskmarker "-[p%d]- "
#endif

#define PRINT_INSTR(__file, ...) fprintf(__file, __taskmarker, __taskid);  \
    fprintf(__file, __VA_ARGS__); \
    fprintf(__file, "\n"); \
    fflush(__file);
#define PRINT_DEBUG(...) 
//#define PRINT_DEBUG(...) PRINT_INSTR(__VA_ARGS__)

#endif // __INSTRUMENTATION_COMMON_H__

