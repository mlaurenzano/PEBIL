#include <InstrumentationCommon.h>

int __wrapper_name(printf)(const char* format, ...){
    PRINT_INSTR(stdout, "printf");
    va_list argp;
    va_start(argp, format);
    int retval = __wrapper_name(vfprintf)(stdout, format, argp);
    va_end(argp);
    return retval;
}

int __wrapper_name(vprintf)(const char* format, va_list argp){
    PRINT_INSTR(stdout, "vprintf");
    int retval = __wrapper_name(vfprintf)(stdout, format, argp);
    return retval;
}

int __wrapper_name(fprintf)(FILE* stream, const char* format, ...){
    PRINT_INSTR(stdout, "fprintf");
    va_list argp;
    va_start(argp, format);
    int retval = __wrapper_name(vfprintf)(stream, format, argp);
    va_end(argp);
    return retval;
}

int __wrapper_name(vfprintf)(FILE* stream, const char* format, va_list argp){
    PRINT_INSTR(stdout, "vfprintf");
    starttimer();
    int retval = vfprintf(stream, format, argp);
    stoptimer();
    if (stream == NULL){
        PRINT_INSTR(logfile, "fprintf ([%d]%s:%d): fileno=INV timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], gettimer());
    } else {
        PRINT_INSTR(logfile, "fprintf ([%d]%s:%d): size=%d fileno=%hhd timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], strlen(format), stream->_fileno, gettimer());
    }
    return retval;
}

int __wrapper_name(fseek)(FILE* stream, long int offset, int origin){
    if (stream == NULL){        
        PRINT_INSTR(logfile, "fseek ([%d]%s:%d): fileno=INV", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex]);
    } else {
        char* org = malloc(9);
        sprintf(org, "SEEK_INV\0");
        if (origin == SEEK_SET){
            sprintf(org, "SEEK_SET\0");
        } else if (origin == SEEK_CUR){
            sprintf(org, "SEEK_CUR\0");
        } else if (origin == SEEK_END){
            sprintf(org, "SEEK_END\0");
        }
        PRINT_INSTR(logfile, "fseek ([%d]%s:%d): offset=%lld origin=%s fileno=%hhd", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], offset, org, stream->_fileno);
        free(org);
    }

    return fseek(stream, offset, origin);
}

size_t __wrapper_name(libc_write)(int fd, const void* ptr, size_t count){
    starttimer();
    int retval = write(fd, ptr, count);
    stoptimer();
    PRINT_INSTR(stdout, "libc_write ([%d]%s:%d): handle=%d size=%d timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fd, count, gettimer());

    return retval;
}

size_t __wrapper_name(fwrite)(const void* ptr, size_t size, size_t count, FILE* stream){
    starttimer();
    int retval = fwrite(ptr, size, count, stream);
    stoptimer();
    if (stream == NULL){
        PRINT_INSTR(logfile, "fwrite ([%d]%s:%d): fileno=INV timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], gettimer());
    } else {
        PRINT_INSTR(logfile, "fwrite ([%d]%s:%d): size=%d fileno=%hhd timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], size * count, stream->_fileno, gettimer());
    }

    return retval;
}

size_t __wrapper_name(libc_read)(int fd, void* buf, size_t count){
    starttimer();
    int retval = read(fd, buf, count);
    stoptimer();
    PRINT_INSTR(stdout, "libc_read ([%d]%s:%d): handle=%d size=%d timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fd, count, gettimer());    
    return retval;
}

size_t __wrapper_name(fread)(void* ptr, size_t size, size_t count, FILE* stream){
    starttimer();
    int retval = fread(ptr, size, count, stream);
    stoptimer();
    if (stream == NULL){
        PRINT_INSTR(logfile, "fread ([%d]%s:%d): fileno=INV timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], gettimer());
    } else {
        PRINT_INSTR(logfile, "fread ([%d]%s:%d): size=%d fileno=%hhd timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], size * count, stream->_fileno, gettimer());
    }

    return retval;
}

int __wrapper_name(puts)(const char* str){
    int retval = puts(str);
    PRINT_INSTR(logfile, "puts ([%d]%s:%d): size=%d timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], strlen(str), gettimer());
    return retval;
}

int __wrapper_name(fflush)(FILE* stream){
    starttimer();
    int retval = fflush(stream);
    stoptimer();
    if (stream == NULL){
        PRINT_INSTR(logfile, "fflush ([%d]%s:%d): fileno=ALL timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], gettimer());
    } else {
        PRINT_INSTR(logfile, "fflush ([%d]%s:%d): fileno=%hhd timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], stream->_fileno, gettimer());
    }
    return retval;
}

int __wrapper_name(libc_close)(int fd){
    int retval = close(fd);
    PRINT_INSTR(logfile, "libc_close ([%d]%s:%d): handle=%d", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fd);
}

int __wrapper_name(fclose)(FILE* stream){
    if (stream == NULL){
        PRINT_INSTR(logfile, "fclose ([%d]%s:%d): fileno=INV", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex]);
    } else {
        PRINT_INSTR(logfile, "fclose ([%d]%s:%d): fileno=%hhd", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], stream->_fileno);
    }
    return fclose(stream);
}

// is this variadic? there is no vopen version, but i've seen it called without flags. for now we just dont use flags
int __wrapper_name(libc_open64)(const char* filename, int mode, int flags){
    int retval = open64(filename, mode, flags);
    PRINT_INSTR(logfile, "libc_open64 ([%d]%s:%d): file=%s mode=%d handle=%d", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], filename, mode, retval);
    return retval;
}

FILE* __wrapper_name(fopen)(const char* filename, const char* mode){
    FILE* retval = fopen(filename, mode);
    PRINT_INSTR(logfile, "fopen ([%d]%s:%d): file=%s mode=%s fileno=%d", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], filename, mode, retval->_fileno);
    return retval;
}

