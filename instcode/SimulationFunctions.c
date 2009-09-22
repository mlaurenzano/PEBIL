#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <dlfcn.h>

#include <InstrumentationCommon.h>

#define PRINT_MINIMUM 1
#define FILTER 1
#define INV_ADDRESS 0

int32_t filter = 0;

DINT_TYPE processTrace(DINT_TYPE* bufferloc, DINT_TYPE* bufferptr){
    DINT_TYPE memloc = *bufferloc;
    DINT_TYPE memval;
    int buffclear = 0;

    PRINT_INSTR("raw args: m[%x]=%#x m[%x]=%lld", bufferloc, *bufferloc, bufferptr, *bufferptr);

    while (0 < *bufferptr){
        //        PRINT_INSTR("trying %d %llx %llx", *bufferptr, bufferloc, *bufferloc);
        memloc = *bufferloc;
        if (memloc == 0){
            memval = 0;
        } else {
            memval = *((DINT_TYPE*)memloc);
        }
        if (filter % FILTER == 0){
            //            PRINT_INSTR("iteration %d; mom[%#llx]\t%#llx", filter, memloc, memval);
        }
        filter++;
        (*bufferptr)--;
        bufferloc++;
    }

    return 0;
}
