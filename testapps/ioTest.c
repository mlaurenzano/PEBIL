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
#include <assert.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#define BUFFER_SIZE 1024
#define TEST_NAME "./barbar"
#define TEST_MSG "foobaronyou"

int run_mpio(){
#ifdef HAVE_MPI
    printf("Running MPI tests\n");
    int i, n;
    int rank, flag;
    MPI_File fh;
    MPI_Status status;
    MPI_Info info, ininfo;
    char buf[BUFFER_SIZE];
    char* val = malloc(BUFFER_SIZE);
    bzero(val, BUFFER_SIZE);
    int err;

    MPI_Info_create(&ininfo);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Info_set(ininfo, "_pebil_testid", "somehint");
    err = MPI_File_open(MPI_COMM_WORLD, "scratch", MPI_MODE_RDONLY, ininfo, &fh);
    if (err){
        printf("error %d\n", err);
        exit(-1);
    }

    printf("file info %d -- %#llx\n", sizeof(MPI_File), fh);
    MPI_File fh2 = fh;
    assert(fh2 == fh && "File handle opaque object behaves unexpectedly");

    MPI_File_get_info(fh, &info);
    MPI_Info_set(info, "_pebil_testid", "somehint");
    MPI_File_set_info(fh, ininfo);

    // does file handle have our hint? probably not
    MPI_File_get_info(fh, &info);
    MPI_Info_get_nkeys(info, &n);
    for (i = 0; i < n; i++){
        MPI_Info_get_nthkey(info, i, val);
        MPI_Info_get(info, val, BUFFER_SIZE, buf, &flag);
        printf("\t(%d): %s -> %s\n", i, val, buf);
    }

    if (rank % 2 == 0){
        MPI_File_seek(fh2, rank * BUFFER_SIZE, MPI_SEEK_SET);
    }

    MPI_File_read(fh, buf, BUFFER_SIZE, MPI_CHAR, &status);
    MPI_File_close(&fh);

    free(val);
#else // HAVE_MPI#
    printf("Skipping MPI tests since configuration was done without MPI support\n");
#endif //HAVE_MPI
    return 1;
}

int run_posx_libc(){
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

    return 1;
}

int main(int argc, char** argv){
#ifdef HAVE_MPI
    MPI_Init(&argc, &argv);
#endif

    run_posx_libc();
    run_mpio();

#ifdef HAVE_MPI
    MPI_Finalize();
#endif

}

