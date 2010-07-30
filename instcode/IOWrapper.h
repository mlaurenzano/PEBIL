#include <stdio.h>

#define IO_BUFFER_SIZE 0x100000
#define MAX_MESSAGE_SIZE 2048

typedef struct {
    FILE*    outFile;
    uint32_t size;
    uint32_t freeIdx;
    char     storage[IO_BUFFER_SIZE];
} TraceBuffer_t;

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
} IOEventTypes;

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
