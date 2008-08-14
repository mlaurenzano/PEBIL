#include <stdio.h>

int smalltest(){
    fprintf(stdout, "You are calling a test function in a shared library. WOW!\n");
    fprintf(stdout, "Hello tikir\n");
    fflush(stdout);
    return 3;
}
