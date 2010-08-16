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
    uint8_t  file_mode;
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
    IO_Offset_Invalid,
    IO_Offset_SET,
    IO_Offset_CUR,
    IO_Offset_END,
    IO_Offset_Total_Types
} IOOffsetClasses;

typedef struct {
    FILE*    outFile;
    uint32_t size;
    uint32_t freeIdx;
    char     storage[IO_BUFFER_SIZE];
} TraceBuffer_t;

typedef enum {
    IO_Handle_Invalid,
    IO_Handle_NAME,
    IO_Handle_CLIB,
    IO_Handle_POSX,
    IO_Handle_MPIO,
    IO_Handle_HDF5,
    IO_Handle_FLIB,
    IO_Handle_Total_Types
} IOHandleClasses;
