#include <stdio.h>

// (file == NULL) implies that lineno is also invalid
void tau_register_func(char **func, char** file, int* lineno, int* id) {
    if (*file == NULL){
        printf("tau_register_func: name = %s, id = %d\n", *func, *id);
    } else {
        printf("tau_register_func: name = %s, file = %s, lineno = %d, id = %d\n", *func, *file, *lineno, *id);
    }
}

void tau_trace_entry(int* id) {
    printf("tau_trace_entry: id = %d\n", *id);
}

void tau_trace_exit(int* id) {
    printf("tau_trace_exit: id = %d\n", *id);
}
