#include <stdio.h>

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
} IOEvent_t;

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

typedef enum {
    IOE_Class_Invalid,
    IOE_Class_CLIB,
    IOE_Class_POSX,
    IOE_Class_HDF5,
    IOE_Class_MPIO,
    IOE_Class_FLIB,
    IOE_Class_Total_Types
} IOEventClasses;

typedef enum {
    IOE_Invalid,
    IOE_CLIB_vfprintf,
    IOE_CLIB_fseek,
    IOE_CLIB_libc_write,
    IOE_CLIB_fwrite,
    IOE_CLIB_libc_read,
    IOE_CLIB_fread,
    IOE_CLIB_puts,
    IOE_CLIB_fflush,
    IOE_CLIB_libc_close,
    IOE_CLIB_fclose,
    IOE_CLIB_libc_open64,
    IOE_CLIB_fopen,
    IOE_MPIO_File_open,
    IOE_MPIO_File_close,
    IOE_MPIO_File_seek,
    IOE_MPIO_File_read_at,
    IOE_MPIO_File_read_at_all,
    IOE_MPIO_File_iread_at,
    IOE_MPIO_File_iread_at_all,
    IOE_MPIO_File_read,
    IOE_MPIO_File_read_all,
    IOE_MPIO_File_iread,
    IOE_MPIO_File_iread_all,
    IOE_MPIO_File_write_at,
    IOE_MPIO_File_write_at_all,
    IOE_MPIO_File_iwrite_at,
    IOE_MPIO_File_iwrite_at_all,
    IOE_HDF5_open,
    IOE_HDF5_close,
    IOE_FLIB_open,
    IOE_FLIB_close,
    IOE_SYSC_open,
    IOE_SYSC_close,
    IOE_Total_Types
} IOEvents;

extern const char* IOEventNames[IOE_Total_Types];

const char* IOEventNames[IOE_Total_Types] = {
    "Invalid",
    "CLIB_vfprintf",
    "CLIB_fseek",
    "CLIB_libc_write",
    "CLIB_fwrite",
    "CLIB_libc_read",
    "CLIB_fread",
    "CLIB_puts",
    "CLIB_fflush",
    "CLIB_libc_close",
    "CLIB_fclose",
    "CLIB_libc_open64",
    "CLIB_fopen",
    "MPIO_File_open",
    "MPIO_File_close",
    "MPIO_File_seek",
    "MPIO_File_read_at",
    "MPIO_File_read_at_all",
    "MPIO_File_iread_at",
    "MPIO_File_iread_at_all",
    "MPIO_File_read",
    "MPIO_File_read_all",
    "MPIO_File_iread",
    "MPIO_File_iread_all",
    "MPIO_File_write_at",
    "MPIO_File_write_at_all",
    "MPIO_File_iwrite_at",
    "MPIO_File_iwrite_at_all",
    "HDF5_open",
    "HDF5_close",
    "FLIB_open",
    "FLIB_close",
    "SYSC_open",
    "SYSC_close"
};
