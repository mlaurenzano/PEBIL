#include <stdio.h>
#include <stdlib.h>

#include <InstrumentationCommon.h>

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

#define __wrapper_name(__fname) \
    __fname ## _pebil_wrapper
