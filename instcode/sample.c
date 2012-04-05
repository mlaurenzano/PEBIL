#include <stdio.h>

void trace_register_func(char **func, int* id) {
    printf("trace_register_func: name = %s, id = %d\n", *func, *id);
}

void trace_program_end(){
}

void traceEntry(int* id) {
    printf("traceEntry: id = %d\n", *id);
}

void traceExit(int* id) {
    printf("traceExit: id = %d\n", *id);
}
