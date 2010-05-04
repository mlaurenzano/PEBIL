#include <InstrumentationCommon.h>
#include <mpi.h>

// MPI IO file basic ops
int __wrapper_name(MPI_File_open)(MPI_Comm comm, char* filename, int amode, MPI_Info info, MPI_File *fh){
    PRINT_INSTR(logfile, "MPI_File_open (%s:%d): -- %x %s %d %x %x", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], comm, filename, amode, info, fh);
    return MPI_File_open(comm, filename, amode, info, fh);
}
int __wrapper_name(MPI_File_close)(MPI_File fh){
    PRINT_INSTR(logfile, "MPI_File_close (%s:%d):", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex]);
    return MPI_File_close(fh);
}
int __wrapper_name(MPI_File_seek)(MPI_File fh, MPI_Offset offset, int whence){
    PRINT_INSTR(logfile, "MPI_File_seek (%s:%d):", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex]);
    return MPI_File_seek(fh, offset, whence);
}

// MPI IO read calls
int __wrapper_name(MPI_File_read_at)(MPI_File fh, MPI_Offset offset, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    PRINT_INSTR(logfile, "MPI_File_read_at (%s:%d):", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex]);
    return MPI_File_read_at(fh, offset, buf, count, datatype, status);
}
int __wrapper_name(MPI_File_read_at_all)(MPI_File fh, MPI_Offset offset, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    PRINT_INSTR(logfile, "MPI_File_read_at_all (%s:%d):", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex]);
    return MPI_File_read_at_all(fh, offset, buf, count, datatype, status);
}
int __wrapper_name(MPI_File_iread_at)(MPI_File fh, MPI_Offset offset, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    PRINT_INSTR(logfile, "MPI_File_iread_at (%s:%d):", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex]);
    return MPI_File_iread_at(fh, offset, buf, count, datatype, status);
}
/*
int __wrapper_name(MPI_File_iread_at_all)(MPI_File fh, MPI_Offset offset, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    PRINT_INSTR(logfile, "MPI_File_iread_at_all (%s:%d):", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex]);
    return MPI_File_iread_at_all(fh, offset, buf, count, datatype, status);
}
*/
int __wrapper_name(MPI_File_read)(MPI_File fh, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    PRINT_INSTR(logfile, "MPI_File_read (%s:%d):", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex]);
    return MPI_File_read(fh, buf, count, datatype, status);
}
int __wrapper_name(MPI_File_read_all)(MPI_File fh, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    PRINT_INSTR(logfile, "MPI_File_read_all (%s:%d):", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex]);
    return MPI_File_read_all(fh, buf, count, datatype, status);
}
int __wrapper_name(MPI_File_iread)(MPI_File fh, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    PRINT_INSTR(logfile, "MPI_File_iread (%s:%d):", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex]);
    return MPI_File_iread(fh, buf, count, datatype, status);
}
/*
int __wrapper_name(MPI_File_iread_all)(MPI_File fh, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    PRINT_INSTR(logfile, "MPI_File_iread_all (%s:%d):", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex]);
    return MPI_File_iread_all(fh, buf, count, datatype, status);
}
*/
// MPI IO write calls
int __wrapper_name(MPI_File_write_at)(MPI_File fh, MPI_Offset offset, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    PRINT_INSTR(logfile, "MPI_File_write_at (%s:%d):", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex]);
    return MPI_File_write_at(fh, offset, buf, count, datatype, status);
}
int __wrapper_name(MPI_File_write_at_all)(MPI_File fh, MPI_Offset offset, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    PRINT_INSTR(logfile, "MPI_File_write_at_all (%s:%d):", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex]);
    return MPI_File_write_at_all(fh, offset, buf, count, datatype, status);
}
int __wrapper_name(MPI_File_iwrite_at)(MPI_File fh, MPI_Offset offset, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    PRINT_INSTR(logfile, "MPI_File_iwrite_at (%s:%d):", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex]);
    return MPI_File_iwrite_at(fh, offset, buf, count, datatype, status);
}
/*
int __wrapper_name(MPI_File_iwrite_at_all)(MPI_File fh, MPI_Offset offset, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    PRINT_INSTR(logfile, "MPI_File_iwrite_at_all (%s:%d):", fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex]);
    return MPI_File_iwrite_at_all(fh, offset, buf, count, datatype, status);
}
*/
