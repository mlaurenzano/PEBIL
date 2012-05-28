
#include <pthread.h>
#include <assert.h>

#include <InstrumentationCommon.h>

// public function declarations
void pebil_set_data(pthread_t thread_id, uint64_t image_id, void * data);
void * pebil_get_data(pthread_t thread_id, uint64_t image_id);
void tool_image_init(uint64_t * image_id);



//
void pebil_set_data(pthread_t thread_id, uint64_t image_id, void * data)
{
    int err = pthread_set_specific(image_id, data);
    if( err ) {
        perror("pthread_set_specific:");
    }
}

void * pebil_get_data(pthread_t thread_id, uint64_t image_id)
{
    assert( pthread_self() == thread_id );
    return pthread_get_specific(image_id);
}

void tool_image_init(uint64_t * image_id)
{
    *image_id = 0;
    pthread_key_create((pthread_key_t*)image_id, NULL);
}

void * tool_thread_init(tool_thread_args * args)
{
    return args->start_function(args->function_args);
}



