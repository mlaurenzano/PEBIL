
#include <pthread.h>
#include <assert.h>

#include <InstrumentationCommon.h>

// public function declarations
extern "C" {
    void pebil_set_data(pthread_t thread_id, uint64_t image_id, void * data);
    void * pebil_get_data(pthread_t thread_id, uint64_t image_id);
    void tool_init_thread(pthread_t thread_id);
    void tool_init_image(uint64_t image_id);
}


//
void pebil_set_data(pthread_t thread_id, uint64_t image_id, void * data)
{

}

void * pebil_get_data(pthread_t thread_id, uint64_t image_id)
{
    return NULL;
}

void tool_init_thread(pthread_t thread_id)
{

}

void tool_init_image(uint64_t image_id)
{

}

