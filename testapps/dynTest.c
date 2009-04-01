#include <stdio.h>

#define DEFAULT_TRIP_COUNT 1234567

int main(int argc, char* argv[]){
    int i, n, k = 0;


    if (argc < 2){
        n = DEFAULT_TRIP_COUNT;
    } else {
        sscanf(argv[1], "%d", &n);
    }

    for (i = 0; i < n; i++){
        if (k % 2){
            k += 1;
        } else {
            k += n;
        }
    }

    printf("Ran loop %d times -- %d\n", n, k);
    printf("(Test Application Successfull)\n");
    fflush(stdout);
    return 0;
}

