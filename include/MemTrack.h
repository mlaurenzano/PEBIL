/*
Copyright (c) 2002, 2008 Curtis Bartley
All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

- Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the
distribution.

- Neither the name of Curtis Bartley nor the names of any other
contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _MemTrack_h_
#define _MemTrack_h_

#include <Base.h>
#ifdef DEBUG_MEMTRACK
#include <typeinfo>

namespace MemTrack
{
    /* ---------------------------------------- class MemStamp */

    class MemStamp
    {
    public:        // member variables
        char const * const filename;
        int const lineNum;
    public:        // construction/destruction
        MemStamp(char const *filename, int lineNum)
            : filename(filename), lineNum(lineNum) { }
            ~MemStamp() { }
    };

    /* ---------------------------------------- memory allocation and stamping prototypes */

    void *TrackMalloc(size_t size);
    void TrackFree(void *p);
    void TrackStamp(void *p, const MemStamp &stamp, char const *typeName);
    void TrackDumpBlocks();
    void TrackListMemoryUsage(double sigPrint);

    /* ---------------------------------------- operator * (MemStamp, ptr) */

    template <class T> inline T *operator*(const MemStamp &stamp, T *p)
        {
            TrackStamp(p, stamp, typeid(T).name());
            return p;
        }

}    // namespace MemTrack

/* ---------------------------------------- new macro */

#define MEMTRACK_NEW MemTrack::MemStamp(__FILE__, __LINE__) * new
#define new MEMTRACK_NEW
#endif // DEBUG_MEMTRACK

#endif // _MemTrack_h_
