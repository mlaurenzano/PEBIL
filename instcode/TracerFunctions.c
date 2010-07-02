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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <InstrumentationCommon.h>

FILE* logfile;
int64_t timerstart;
int64_t timerstop;
char* appName;
int32_t* currentSiteIndex;
char** fileNames;
int32_t* lineNumbers;

inline int starttimer(){
    timerstart = readtsc();
}

inline int stoptimer(){
    timerstop = readtsc();
}

inline int64_t gettimer(){
    return timerstop - timerstart;
}

#define printtimer(__stream) PRINT_INSTR(__stream, "timer value (in cycles): %lld", gettimer())


// do any initialization here
// NOTE: on static-linked binaries, calling any functions from here will cause some problems
int32_t initwrapper(int32_t* indexLoc, char** fNames, int32_t* lNum){
    // at each call site we will put the index of the originating point in this location
    currentSiteIndex = indexLoc;

    fileNames = fNames;
    lineNumbers = lNum;

    logfile = stdout;

#ifdef USING_MPI_WRAPPERS
    // use an unlikely value, so if we see this value we know there was
    // a problem getting task id
    taskid = 0xdeadbeef;
#else
    taskid = 0;
#endif
}

// do any cleanup here
int32_t finishwrapper(){
}

#ifdef USING_CSTD_WRAPPERS
#include <IOWrappers.c>
#endif // USING_CSTD_WRAPPERS

#ifdef USING_MPI_WRAPPERS
#include <MPIIOWrappers.c>
#endif // USING_MPI
