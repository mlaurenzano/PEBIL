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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFFER_SIZE 1024
#define TEST_NAME "./barbar"
#define TEST_MSG "foobaronyou"

int main(int argc, char** argv){
    runtest();
}

int runtest(){
    char* buf = malloc(BUFFER_SIZE);
    struct stat* st = malloc(sizeof(struct stat));
    int f = open(TEST_NAME, O_APPEND, O_RDWR);
    write(f, TEST_MSG, 11);
    pwrite(f, TEST_MSG, 5, 4);
    pwrite64(f, TEST_MSG, 5, 4);
    read(f, buf, 2);
    pread(f, buf, 3, 0xab);
    pread64(f, buf, 3, 0xab);
    close(f);

    f = open64("/dev/null", O_APPEND, O_RDWR);
    fchmod(f, S_IWOTH | S_IXGRP);
    fstat(f, st);
    close(f);

    f = creat(TEST_NAME, O_WRONLY);
    fprintf(stdout, "stat info: %d %d %d\n", st->st_dev, st->st_uid, st->st_mode);
    close(f);

    chmod(TEST_NAME, S_IRUSR | S_IWUSR | S_IRGRP);

    stat(TEST_NAME, st);
    fprintf(stdout, "stat info: %d %d %d\n", st->st_dev, st->st_uid, st->st_mode);

    FILE* h = fopen("/dev/null", "rw");
    fseek(h, 3, SEEK_END);
    puts(TEST_MSG);
    fflush(h);
    fflush(stdout);
    fclose(h);

    free(st);

    return 0;
}
