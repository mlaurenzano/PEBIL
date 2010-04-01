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
    __fname ## _pebil_iowrapper

// args are: name in app, name in wrapper (below), # of args, type (full or variadic), arg types
#define __wrapper_decision(__fname, __wname, __nargs, __typ, ...)\
    __wrapper_decision_ ## __nargs ## arg_ ## __typ(__fname, __wname, __VA_ARGS__)

#define __elif_begin else if (!strcmp(functionNames[*idx],
#define __wrapper_decision_1arg_full(__fname, __wname, __a0)    \
    __elif_begin #__fname)) { __wrapper_name(__wname)((__a0)args[0]); }
#define __wrapper_decision_1arg_variadic(__fname, __wname, __a0)    \
    __elif_begin #__fname)) { __wrapper_name(__wname)((__a0)&args[0]); }
#define __wrapper_decision_2arg_full(__fname, __wname, __a0, __a1)           \
    __elif_begin #__fname)) { __wrapper_name(__wname)((__a0)args[0], (__a1)args[1]); }
#define __wrapper_decision_2arg_variadic(__fname, __wname, __a0, __a1)       \
    __elif_begin #__fname)) { __wrapper_name(__wname)((__a0)args[0], (__a1)&args[1]); }
#define __wrapper_decision_3arg_full(__fname, __wname, __a0, __a1, __a2)          \
    __elif_begin #__fname)) { __wrapper_name(__wname)((__a0)args[0], (__a1)args[1], (__a2)args[2]); }
#define __wrapper_decision_3arg_variadic(__fname, __wname, __a0, __a1, __a2)          \
    __elif_begin #__fname)) { __wrapper_name(__wname)((__a0)args[0], (__a1)args[1], (__a2)&args[2]); }
#define __wrapper_decision_4arg_full(__fname, __wname, __a0, __a1, __a2, __a3) \
    __elif_begin #__fname)) { __wrapper_name(__wname)((__a0)args[0], (__a1)args[1], (__a2)args[2], (__a3)args[3]); }
#define __wrapper_decision_4arg_variadic(__fname, __wname, __a0, __a1, __a2, __a3) \
    __elif_begin #__fname)) { __wrapper_name(__wname)((__a0)args[0], (__a1)args[1], (__a2)args[2], (__a3)&args[3]); }

