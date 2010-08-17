#ifdef PRELOAD_WRAPPERS
#define _GNU_SOURCE
#include <dlfcn.h>
#endif

#include <stdio.h>
#include <IOEvents.h>

#define IO_BUFFER_SIZE 0x100000
#define MAX_MESSAGE_SIZE 2048

typedef struct {
    uint8_t  class;
    uint8_t  offset_class;
    uint8_t  handle_class;
    uint8_t  mode;
    uint16_t event_type;
    uint16_t handle_id;
    uint32_t flags;
    uint64_t source;
    uint64_t size;
    uint64_t offset;
    uint64_t start_time;
    uint64_t end_time;
} io_event;

typedef enum {
    IOOffset_Invalid,
    IOOffset_SET,
    IOOffset_CUR,
    IOOffset_END,
    IOOffset_Total_Types
} IOOffsetClasses;

uint8_t offsetOriginToClass(int origin){
    if (origin == SEEK_SET){
        return IOOffset_SET;
    } else if (origin == SEEK_CUR){
        return IOOffset_CUR;
    } else if (origin == SEEK_END){
        return IOOffset_END;
    }
    return IOOffset_Invalid;
}

typedef struct {
    FILE*    outFile;
    uint32_t size;
    uint32_t freeIdx;
    char     storage[IO_BUFFER_SIZE];
} TraceBuffer_t;

typedef enum {
    IOHandle_Invalid,
    IOHandle_NAME,
    IOHandle_CLIB,
    IOHandle_POSX,
    IOHandle_MPIO,
    IOHandle_HDF5,
    IOHandle_FLIB,
    IOHandle_Total_Types
} IOHandleClasses;
