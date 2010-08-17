#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

typedef void (*simple_func)(void);

#define TEST_NAME "./barbar"
#define TEST_MSG "foobaronyou"

int main(int argc, char** argv){
    systest();
    clibtest();
}

int clibtest(){
}
int systest(){
    char* buf = malloc(1024);
    struct stat* st = malloc(sizeof(struct stat));
    int f = open("/dev/null", O_APPEND, O_RDWR);
    write(f, TEST_MSG, 11);
    pwrite(f, TEST_MSG, 11, 0);
    read(f, buf, 2);
    pread(f, buf, 3, 0xab);
    close(f);

    f = creat(TEST_NAME, O_WRONLY);
    fchmod(f, S_IWOTH | S_IXGRP);
    fstat(f, st);
    fprintf(stdout, "stat info: %d %d %d\n", st->st_dev, st->st_uid, st->st_mode);
    close(f);

    chmod(TEST_NAME, S_IRUSR | S_IWUSR | S_IRGRP);

    stat(TEST_NAME, st);
    fprintf(stdout, "stat info: %d %d %d\n", st->st_dev, st->st_uid, st->st_mode);
    free(st);

    return 0;
}
