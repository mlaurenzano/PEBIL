#include <stdio.h>

int main(int argc){

    memcpy((void*)0x12345678,(void*)0xdeadbeef,0x9999);
    
    int i = 0;
    while (i < 1000000000){
        i++;
    }
    fprintf(stdout, "i=%d", i);

    /*
    char f = '1';
    fprintf(stdout, "%c", f);
    int j = i+argc;

    switch (j){
    case 0:
        i++;
        break;
    case 1:
        i--;
    case 2:
        i--;
        break;
    default:
        i += 100;
        break;
    }
    fprintf(stdout, "i=%d", i);
    */
}
