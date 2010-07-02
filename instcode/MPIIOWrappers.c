/* This program is free software: you can redistribute it and/or modify
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

#include <InstrumentationCommon.h>
#include <mpi.h>


// MPI IO file basic ops
int __wrapper_name(MPI_File_open)(MPI_Comm comm, char* filename, int amode, MPI_Info info, MPI_File *fh){
    int retval = MPI_File_open(comm, filename, amode, info, fh);
    PRINT_INSTR(logfile, "MPI_File_open ([%d]%s:%d): comm=%x file=%s mode=%d mpifile=%d", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], comm, filename, amode, *fh);
    return retval;
}

int __wrapper_name(MPI_File_close)(MPI_File* fh){
    PRINT_INSTR(logfile, "MPI_File_close ([%d]%s:%d): mpifile=%d", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], *fh);
    return MPI_File_close(fh);
}

int __wrapper_name(MPI_File_seek)(MPI_File fh, MPI_Offset offset, int whence){
    PRINT_INSTR(logfile, "MPI_File_seek ([%d]%s:%d): mpifile=%d offset=%lld whence=%d", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fh, offset, whence);
    return MPI_File_seek(fh, offset, whence);
}


// MPI IO read calls
int __wrapper_name(MPI_File_read_at)(MPI_File fh, MPI_Offset offset, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    starttimer();
    int retval = MPI_File_read_at(fh, offset, buf, count, datatype, status);
    stoptimer();
    PRINT_INSTR(logfile, "MPI_File_read_at ([%d]%s:%d): mpifile=%d offset=%lld count=%lld timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fh, offset, count, gettimer());
    return retval;
}

int __wrapper_name(MPI_File_read_at_all)(MPI_File fh, MPI_Offset offset, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    starttimer();
    int retval = MPI_File_read_at_all(fh, offset, buf, count, datatype, status);
    stoptimer();
    PRINT_INSTR(logfile, "MPI_File_read_at_all ([%d]%s:%d): mpifile=%d offset=%lld count=%lld timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fh, offset, count, gettimer());
    return retval;
}

int __wrapper_name(MPI_File_iread_at)(MPI_File fh, MPI_Offset offset, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    starttimer();
    int retval = MPI_File_iread_at(fh, offset, buf, count, datatype, status);
    stoptimer();
    PRINT_INSTR(logfile, "MPI_File_iread_at ([%d]%s:%d): mpifile=%d offset=%lld count=%lld timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fh, offset, count, gettimer());
    return retval;
}

/*
int __wrapper_name(MPI_File_iread_at_all)(MPI_File fh, MPI_Offset offset, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    starttimer();
    int retval = MPI_File_iread_at_all(fh, offset, buf, count, datatype, status);
    stoptimer();
    PRINT_INSTR(logfile, "MPI_File_iread_at_all ([%d]%s:%d): mpifile=%d offset=%lld count=%lld timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fh, offset, count, gettimer());
    return retval;
}
*/

int __wrapper_name(MPI_File_read)(MPI_File fh, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    starttimer();
    int retval = MPI_File_read(fh, buf, count, datatype, status);
    stoptimer();
    PRINT_INSTR(logfile, "MPI_File_read ([%d]%s:%d): mpifile=%d count=%lld timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fh, count, gettimer());
    return retval;
}

int __wrapper_name(MPI_File_read_all)(MPI_File fh, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    starttimer();
    int retval = MPI_File_read_all(fh, buf, count, datatype, status);
    stoptimer();
    PRINT_INSTR(logfile, "MPI_File_read_all ([%d]%s:%d): mpifile=%d count=%lld timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fh, count, gettimer());
    return retval;
}

int __wrapper_name(MPI_File_iread)(MPI_File fh, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    starttimer();
    int retval = MPI_File_iread(fh, buf, count, datatype, status);
    stoptimer();
    PRINT_INSTR(logfile, "MPI_File_iread ([%d]%s:%d): offset=%lld count=%lld timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fh, count, gettimer());
    return retval;
}

/*
int __wrapper_name(MPI_File_iread_all)(MPI_File fh, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    starttimer();
    int retval = MPI_File_iread_all(fh, buf, count, datatype, status);
    stoptimer();
    PRINT_INSTR(logfile, "MPI_File_iread_all ([%d]%s:%d): mpifile=%d count=%lld timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fh, count, gettimer());
    return retval;
}
*/

// MPI IO write calls
int __wrapper_name(MPI_File_write_at)(MPI_File fh, MPI_Offset offset, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    starttimer();
    int retval = MPI_File_write_at(fh, offset, buf, count, datatype, status);
    stoptimer();
    PRINT_INSTR(logfile, "MPI_File_write_at ([%d]%s:%d): mpifile=%d offset=%lld count=%lld timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fh, offset, count, gettimer());
    return retval;
}

int __wrapper_name(MPI_File_write_at_all)(MPI_File fh, MPI_Offset offset, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    starttimer();
    int retval = MPI_File_write_at_all(fh, offset, buf, count, datatype, status);
    stoptimer();
    PRINT_INSTR(logfile, "MPI_File_write_at_all ([%d]%s:%d): mpifile=%d offset=%lld count=%lld timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fh, offset, count, gettimer());
    return retval;
}

int __wrapper_name(MPI_File_iwrite_at)(MPI_File fh, MPI_Offset offset, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    starttimer();
    int retval = MPI_File_iwrite_at(fh, offset, buf, count, datatype, status);
    stoptimer();
    PRINT_INSTR(logfile, "MPI_File_iwrite_at ([%d]%s:%d): mpifile=%d offset=%lld count=%lld timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fh, offset, count, gettimer());
    return retval;
}

/*
int __wrapper_name(MPI_File_iwrite_at_all)(MPI_File fh, MPI_Offset offset, void* buf, int count, MPI_Datatype datatype, MPI_Status* status){
    starttimer();
    int retval = MPI_File_iwrite_at_all(fh, offset, buf, count, datatype, status);
    stoptimer();
    PRINT_INSTR(logfile, "MPI_File_iwrite_at_all ([%d]%s:%d): mpifile=%d offset=%lld count=%lld timer=%lld", *currentSiteIndex, fileNames[*currentSiteIndex], lineNumbers[*currentSiteIndex], fh, offset, count, gettimer());
    return retval;
}
*/
