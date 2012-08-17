#define _GNU_SOURCE
#include <dlfcn.h>

#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>

// (file == NULL) implies that lineno is also invalid
void tau_register_func(char** func, char** file, int* lineno, int id) {
    if (*file == NULL){
        printf("tau_register_func: name = %s, id = %d\n", *func, id);
    } else {
        printf("tau_register_func: name = %s, file = %s, lineno = %d, id = %d\n", *func, *file, *lineno, id);
    }
}

void tau_trace_entry(int id) {
    printf("%#lx: tau_trace_entry: id = %d\n", pthread_self(), id);
}

void tau_trace_exit(int id) {
    printf("%#lx: tau_trace_exit: id = %d\n", pthread_self(), id);
}

void* tool_thread_init(pthread_t args){
    printf("initializing thread %#lx\n", args);
}

void* tool_thread_fini(pthread_t args){
    printf("finalizing thread %#lx\n", args);
}


// wrappers for intercepting thread create/destroy
typedef struct {
    void* args;
    int (*fcn)(void*);
} thread_passthrough_args;

int thread_started(void* args){
    thread_passthrough_args* pt_args = (thread_passthrough_args*)args;
    tool_thread_init(pthread_self());

    return pt_args->fcn(pt_args->args);
}

static int __tau_wrapping_clone(int (*fn)(void*), void* child_stack, int flags, void* arg, ...){
    va_list ap;
    va_start(ap, arg);
    pid_t* ptid = va_arg(ap, pid_t*);
    struct user_desc* tls = va_arg(ap, struct user_desc*);
    pid_t* ctid = va_arg(ap, pid_t*);
    va_end(ap);

    int (*clone_ptr)(int (*fn)(void*), void* child_stack, int flags, void* arg, pid_t *ptid, struct user_desc *tls, pid_t *ctid)
        = (int (*)(int (*fn)(void*), void* child_stack, int flags, void* arg, pid_t *ptid, struct user_desc *tls, pid_t *ctid))dlsym(RTLD_NEXT, "clone");

    // TODO: keep this somewhere and destroy it. it currently is a mem leak
    thread_passthrough_args* pt_args = (thread_passthrough_args*)malloc(sizeof(thread_passthrough_args));
    pt_args->fcn = fn;
    pt_args->args = arg;

    return clone_ptr(thread_started, child_stack, flags, (void*)pt_args, ptid, tls, ctid);
}

int __clone(int (*fn)(void*), void* child_stack, int flags, void* arg, ...){
    va_list ap;
    va_start(ap, arg);
    pid_t* ptid = va_arg(ap, pid_t*);
    struct user_desc* tls = va_arg(ap, struct user_desc*);
    pid_t* ctid = va_arg(ap, pid_t*);
    va_end(ap);
    return __tau_wrapping_clone(fn, child_stack, flags, arg, ptid, tls, ctid);
}

int clone(int (*fn)(void*), void* child_stack, int flags, void* arg, ...){
    va_list ap;
    va_start(ap, arg);
    pid_t* ptid = va_arg(ap, pid_t*);
    struct user_desc* tls = va_arg(ap, struct user_desc*);
    pid_t* ctid = va_arg(ap, pid_t*);
    va_end(ap);
    return __tau_wrapping_clone(fn, child_stack, flags, arg, ptid, tls, ctid);
}

int __clone2(int (*fn)(void*), void* child_stack, int flags, void* arg, ...){
    va_list ap;
    va_start(ap, arg);
    pid_t* ptid = va_arg(ap, pid_t*);
    struct user_desc* tls = va_arg(ap, struct user_desc*);
    pid_t* ctid = va_arg(ap, pid_t*);
    va_end(ap);
    return __tau_wrapping_clone(fn, child_stack, flags, arg, ptid, tls, ctid);
}

int pthread_join(pthread_t thread, void **value_ptr){
    pthread_t jthread = thread;

    int (*join_ptr)(pthread_t, void**) = (int (*)(pthread_t, void**))dlsym(RTLD_NEXT, "pthread_join");
    int ret = join_ptr(thread, value_ptr);

    tool_thread_fini(jthread);
    return ret;
}
