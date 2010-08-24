/* 
 * This file is part of the pebil project.
 * 
 * Copyright (c) 2010, University of California Regents
 * All rights reserved.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>

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
    TIMER_EXECUTE(int retval = vfprintf(stream, format, argp);)
    if (stream == NULL){
        sprintf(message, "fprintf ([%d]%s:%d): fileno=INV timer=%lld\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], TIMER_VALUE);
        storeToBuffer(message, strlen(message));
    } else {
        sprintf(message, "fprintf ([%d]%s:%d): size=%d fileno=%hhd timer=%lld\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], strlen(format), stream->_fileno, TIMER_VALUE);
        storeToBuffer(message, strlen(message));
    }
    return retval;
}

int __wrapper_name(fseek)(FILE* stream, long int offset, int origin){
    if (stream == NULL){        
        sprintf(message, "fseek ([%d]%s:%d): fileno=INV\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex]);
        storeToBuffer(message, strlen(message));
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

        sprintf(message, "fseek ([%d]%s:%d): offset=%lld origin=%s fileno=%hhd\n\0\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], offset, org, stream->_fileno);
        storeToBuffer(message, strlen(message));
        free(org);
    }

    return fseek(stream, offset, origin);
}

size_t __wrapper_name(fwrite)(const void* ptr, size_t size, size_t count, FILE* stream){
    TIMER_EXECUTE(int retval = fwrite(ptr, size, count, stream);)
    if (stream == NULL){
        sprintf(message, "fwrite ([%d]%s:%d): fileno=INV timer=%lld\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], TIMER_VALUE);
        storeToBuffer(message, strlen(message));
    } else {
        sprintf(message, "fwrite ([%d]%s:%d): size=%d fileno=%hhd timer=%lld\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], size * count, stream->_fileno, TIMER_VALUE);
        storeToBuffer(message, strlen(message));
    }

    return retval;
}

size_t __wrapper_name(fread)(void* ptr, size_t size, size_t count, FILE* stream){
    TIMER_EXECUTE(int retval = fread(ptr, size, count, stream);)
    if (stream == NULL){
        sprintf(message, "fread ([%d]%s:%d): fileno=INV timer=%lld\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], TIMER_VALUE);
        storeToBuffer(message, strlen(message));
    } else {
        sprintf(message, "fread ([%d]%s:%d): size=%d fileno=%hhd timer=%lld\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], size * count, stream->_fileno, TIMER_VALUE);
        storeToBuffer(message, strlen(message));
    }

    return retval;
}

int __wrapper_name(puts)(const char* str){
    TIMER_EXECUTE(int retval = puts(str);)
    sprintf(message, "puts ([%d]%s:%d): size=%d timer=%lld\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], strlen(str), TIMER_VALUE);
        storeToBuffer(message, strlen(message));
    return retval;
}

int __wrapper_name(fflush)(FILE* stream){
    TIMER_EXECUTE(int retval = fflush(stream);)
    if (stream == NULL){
        sprintf(message, "fflush ([%d]%s:%d): fileno=ALL timer=%lld\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], TIMER_VALUE);
        storeToBuffer(message, strlen(message));
    } else {
        sprintf(message, "fflush ([%d]%s:%d): fileno=%hhd timer=%lld\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], stream->_fileno, TIMER_VALUE);
        storeToBuffer(message, strlen(message));
    }
    return retval;
}

int __wrapper_name(fclose)(FILE* stream){
    if (stream == NULL){
        sprintf(message, "fclose ([%d]%s:%d): fileno=INV\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex]);
        storeToBuffer(message, strlen(message));
    } else {
        sprintf(message, "fclose ([%d]%s:%d): fileno=%hhd\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], stream->_fileno);
        storeToBuffer(message, strlen(message));
    }
    return fclose(stream);
}

FILE* __wrapper_name(fopen)(const char* filename, const char* mode){
    FILE* retval = fopen(filename, mode);
    sprintf(message, "fopen ([%d]%s:%d): file=%s mode=%s fileno=%d\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], filename, mode, retval->_fileno);
    storeToBuffer(message, strlen(message));
    return retval;
}

int __wrapper_name(fstat)(int fildes, struct stat *buf){
    TIMER_EXECUTE(int retval = fstat(fildes, buf);)
    sprintf(message, "fstat ([%d]%s:%d): handle=%d\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fildes);
    storeToBuffer(message, strlen(message));
    return retval;
}

/* should implement these also
int lstat(const char *pathname, struct stat *buf);
int fstat64(int fildes, struct stat64 *buf);
int lstat64(const char *pathname, struct stat64 *buf);
int stat64(const char *pathname, struct stat64 *buf);
*/

int __wrapper_name(fchmod)(int fildes, mode_t mode){
    TIMER_EXECUTE(int retval = fchmod(fildes, mode);)
    sprintf(message, "fchmod ([%d]%s:%d): handle=%d mode=%d\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fildes, mode);
    storeToBuffer(message, strlen(message));
    return retval;
}

