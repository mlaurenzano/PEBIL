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
