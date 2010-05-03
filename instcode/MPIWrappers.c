#include <InstrumentationCommon.h>
#include <mpi.h>

int initwrapper(){
}

int finishwrapper(){
}

int pebilwrap_MPI_Init(int *argc, char ***argv){
    int rank;
    int retvalue = MPI_Init(argc, argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    PRINT_INSTR(stdout, "Called MPI_Init -- return %d from rank %d", retvalue, rank);
    return retvalue;
}

int pebilwrap_MPI_Finalize(){
    PRINT_INSTR(stdout, "Called MPI_Finalize");
}
