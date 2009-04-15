#include <stdio.h>

int main(int argc){

    __asm("call *%eax");
    
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
