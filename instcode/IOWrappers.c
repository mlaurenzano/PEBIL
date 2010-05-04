#include <IOWrappers.h>
#include <stdarg.h>

#define USING_MPI
#ifdef USING_MPI
#include <mpi.h>
int __wrapper_name(MPI_File_open)(MPI_Comm comm, char* filename, int amode, MPI_Info info, MPI_File *fh){
    PRINT_INSTR(logfile, "file open -- %x %s %d %x %x", comm, filename, amode, info, fh);
    return MPI_File_open(comm, filename, amode, info, fh);
}
void __wrapper_name(MPI_File_close)(int fh){
    PRINT_INSTR(logfile, "MPI_File_close");
    return MPI_File_close(fh);
}
#endif // USING_MPI

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
    if (stream == NULL){
        PRINT_INSTR(logfile, "fprintf: null file!");
    } else {
        PRINT_INSTR(logfile, "fprintf (%s:%d): approx len %d to file %hhd", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], strlen(format), stream->_fileno);
    }

    int retval = vfprintf(stream, format, argp);
    return retval;
}

int __wrapper_name(fseek)(FILE* stream, long int offset, int origin){
    if (stream == NULL){        
        PRINT_INSTR(logfile, "fseek: null file!");
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
        PRINT_INSTR(logfile, "fseek (%s:%d): %d bytes from %s in file %hhd", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], offset, org, stream->_fileno);
        free(org);
    }

    return fseek(stream, offset, origin);
}

size_t __wrapper_name(fwrite)(const void* ptr, size_t size, size_t count, FILE* stream){
    if (stream == NULL){
        PRINT_INSTR(logfile, "fwrite: null file!");
    } else {
        PRINT_INSTR(logfile, "fwrite (%s:%d):  %d bytes from file %hhd", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], size * count, stream->_fileno);
    }
    starttimer();
    int retval = fwrite(ptr, size, count, stream);
    stoptimer();
    printtimer(logfile);

    return retval;
}

size_t __wrapper_name(fread)(void* ptr, size_t size, size_t count, FILE* stream){
    if (stream == NULL){
        PRINT_INSTR(logfile, "fread: null file!");
    } else {
        PRINT_INSTR(logfile, "fread (%s:%d): %d bytes from file %hhd", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], size * count, stream->_fileno);
    }
    starttimer();
    int retval = fread(ptr, size, count, stream);
    stoptimer();
    printtimer(logfile);

    return retval;
}

int __wrapper_name(puts)(const char* str){
    PRINT_INSTR(logfile, "puts (%s:%d): %s", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], str);
    return puts(str);
}

int __wrapper_name(fflush)(FILE* stream){
    if (stream == NULL){
        PRINT_INSTR(logfile, "fflush: all");
    } else {
        PRINT_INSTR(logfile, "fflush (%s:%d): %hhd", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], stream->_fileno);
    }
    starttimer();
    int retval = fflush(stream);
    stoptimer();
    printtimer(logfile);

    return retval;
}

int  __wrapper_name(fclose)(FILE* stream){
    if (stream == NULL){
        PRINT_INSTR(logfile, "fclose: null file!");
    } else {
        PRINT_INSTR(logfile, "fclose (%s:%d): %hhd", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], stream->_fileno);
    }
    return fclose(stream);
}

int __wrapper_name(fopen)(const char* filename, const char* mode){
    PRINT_INSTR(logfile, "fopen (%s:%d): file %s, mode %s", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], filename, mode);
    return fopen(filename, mode);
}

