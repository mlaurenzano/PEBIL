#include <IOWrappers.h>

#define __all_wrapper_decisions \
    if (0) {} \
    __wrapper_decision(puts, puts, 1, full, const char*) \
    __wrapper_decision(__printf_chk, printf, 3, variadic, FILE*, const char*, int64_t*) \
    __wrapper_decision(printf, printf, 3, variadic, FILE*, const char*, int64_t*) \
    __wrapper_decision(__fprintf_chk, fprintf, 4, variadic, const char*, FILE*, const char*, int64_t*) \
    __wrapper_decision(fprintf, printf, 3, variadic, FILE*, const char*, int64_t*) \
    __wrapper_decision(fflush, fflush, 1, full, FILE*) \
    __wrapper_decision(fopen, fopen, 2, full, const char*, const char*) \
    __wrapper_decision(fclose, fclose, 1, full, FILE*) \
    else { PRINT_INSTR(logfile, "No wrapper found for function %s: %llx %llx %llx %llx %llx %llx", functionNames[*idx], args[0], args[1], args[2], args[3], args[4], args[5]); }


void __wrapper_name(puts)(const char* str){
    PRINT_INSTR(logfile, "puts: %s", str);
}

void __wrapper_name(fflush)(FILE* stream){
    PRINT_INSTR(logfile, "fflush: %x", stream);
}

void __wrapper_name(fclose)(FILE* stream){
    PRINT_INSTR(logfile, "fclose: %x", stream);
}

void __wrapper_name(fprintf)(int32_t* f, FILE* stream, const char* format, int64_t* args){
    char str[__MAX_STRING_SIZE];
    write_formatstr(str, format, args);
    PRINT_INSTR(logfile, "fprintf: file %d (%x) -- %s", stream, f, str);
}

void __wrapper_name(printf)(FILE* stream, const char* format, int64_t* args){
    char str[__MAX_STRING_SIZE];
    write_formatstr(str, format, args);
    PRINT_INSTR(logfile, "fprintf: file %d -- %s", stream, str);
}

void __wrapper_name(fopen)(const char* filename, const char* mode){
    PRINT_INSTR(logfile, "fopen: file %s, mode %s", filename, mode);
}

// put the wrapper just above here! make sure its named using the conventions and has the same interface as its namesake
