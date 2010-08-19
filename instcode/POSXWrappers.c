/* This program is free software: you can redistribute it and/or modify
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

#include <sys/stat.h>

int __wrapper_name(posx_chmod)(const char* path, mode_t mode){
    TIMER_EXECUTE(int retval = chmod(path, mode);)
    sprintf(message, "posx_chmod ([%d]%s:%d): file=%d mode=%d\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], path, mode);
    storeToBuffer(message, strlen(message));

    return retval;
}

size_t __wrapper_name(posx_pwrite)(int fd, const void* ptr, size_t count, off_t offset){
    TIMER_EXECUTE(int retval = pwrite(fd, ptr, count, offset);)
    sprintf(message, "posx_pwrite ([%d]%s:%d): handle=%d size=%d timer=%lld\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fd, count, TIMER_VALUE);
    storeToBuffer(message, strlen(message));

    return retval;
}

size_t __wrapper_name(posx_pwrite64)(int fd, const void* ptr, size_t count, off_t offset){
    TIMER_EXECUTE(int retval = pwrite64(fd, ptr, count, offset);)
    sprintf(message, "posx_pwrite64 ([%d]%s:%d): handle=%d size=%d timer=%lld\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fd, count, TIMER_VALUE);
    storeToBuffer(message, strlen(message));

    return retval;
}

size_t __wrapper_name(posx_write)(int fd, const void* ptr, size_t count){
    TIMER_EXECUTE(int retval = write(fd, ptr, count);)
    sprintf(message, "posx_write ([%d]%s:%d): handle=%d size=%d timer=%lld\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fd, count, TIMER_VALUE);
    storeToBuffer(message, strlen(message));

    return retval;
}

size_t __wrapper_name(posx_pread)(int fd, void* buf, size_t count, off_t offset){
    TIMER_EXECUTE(int retval = pread(fd, buf, count, offset);)
    sprintf(message, "posx_pread ([%d]%s:%d): handle=%d size=%d timer=%lld\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fd, count, TIMER_VALUE);    
    storeToBuffer(message, strlen(message));

    return retval;
}

size_t __wrapper_name(posx_pread64)(int fd, void* buf, size_t count, off_t offset){
    TIMER_EXECUTE(int retval = pread64(fd, buf, count, offset);)
    sprintf(message, "posx_pread64 ([%d]%s:%d): handle=%d size=%d timer=%lld\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fd, count, TIMER_VALUE);    
    storeToBuffer(message, strlen(message));

    return retval;
}

size_t __wrapper_name(posx_read)(int fd, void* buf, size_t count){
    TIMER_EXECUTE(int retval = read(fd, buf, count);)
    sprintf(message, "posx_read ([%d]%s:%d): handle=%d size=%d timer=%lld\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fd, count, TIMER_VALUE);    
    storeToBuffer(message, strlen(message));

    return retval;
}

int __wrapper_name(posx_close)(int fd){
    TIMER_EXECUTE(int retval = close(fd);)
    sprintf(message, "posx_close ([%d]%s:%d): handle=%d\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fd);
    storeToBuffer(message, strlen(message));

    return retval;
}

// is this variadic? there is no vopen version, but i've seen it called without flags. for now we just dont use flags
int __wrapper_name(posx_open64)(const char* filename, int mode, int flags){
    TIMER_EXECUTE(int retval = open64(filename, mode, flags);)
    sprintf(message, "posx_open64 ([%d]%s:%d): file=%s mode=%d handle=%d\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], filename, mode, retval);
    storeToBuffer(message, strlen(message));
    return retval;
}

int __wrapper_name(posx_stat)(const char *pathname, struct stat *buf){
    TIMER_EXECUTE(int retval = stat(pathname, buf);)
    sprintf(message, "posx_stat ([%d]%s:%d): file=%s\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], pathname);
    storeToBuffer(message, strlen(message));
    return retval;
}

int __wrapper_name(posx_creat)(const char* filename, int flags){
    TIMER_EXECUTE(int retval = creat(filename, flags);)
    sprintf(message, "posx_creat ([%d]%s:%d): file=%s, flags=%d\n\0", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], filename, flags);
    storeToBuffer(message, strlen(message));
    return retval;
}

/* should implement these also
int lstat(const char *pathname, struct stat *buf);
int fstat64(int fildes, struct stat64 *buf);
int lstat64(const char *pathname, struct stat64 *buf);
int stat64(const char *pathname, struct stat64 *buf);
*/
