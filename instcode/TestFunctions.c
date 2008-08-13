#include <stdio.h>

int smalltest(){
    fprintf(stdout, "You are calling a test function in a shared library. WOW!\n");
    fprintf(stdout, "(Test Application Successfull)\n");
    fflush(stdout);
    exit(3);
    return 3;
}
