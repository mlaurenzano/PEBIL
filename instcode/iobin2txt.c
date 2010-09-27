/* 
 * This file is part of the pebil project.
 * 
 * Copyright (c) 2010, University of California Regents
 * All rights reserved.
 * 
 * This program is free software: you can redistribute it and/or modify
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
#include <stdint.h>
#include <IOWrappers.h>
#include <IOEvents.h>

char buffer[__IO_BUFFER_SIZE];

void print_usage(char* msg, char* prg, uint32_t doexit){
    fprintf(stderr, "%s\n\n", msg);
    fprintf(stderr, "usage: %s <iologfile>\n", prg);
    if (doexit){
        exit(doexit);
    }
}

void printRecordHeader(uint32_t header){
    fprintf(stdout, "%10s(%4dB): ", IORecordTypeNames[GET_RECORD_TYPE(header)], GET_RECORD_SIZE(header));
}

void printEventInfo(EventInfo_t* event){
    fprintf(stdout, "unqid %5lld: stime=%lld + %lld, class=%4s, o_class=%8s, h_class=%4s, mode=%hhd, handle=%lld, pid=%d, tid=%llx, size=%lld, offset=%lld, e_type=%s\n\0",
            event->unqid, event->start_time, event->end_time - event->start_time,
            IOEventClassNames[event->class], IOOffsetClassNames[event->offset_class], IOHandleClassNames[event->handle_class], event->mode,
            event->handle_id, event->tinfo.process, event->tinfo.thread, event->size, event->offset, IOEventNames[event->event_type]);
}

void printIOFileName(IOFileName_t* filereg, char* name){
    fprintf(stdout, "unqid %5lld: h_class=%s\ta_type=%s\tnumchars=%d\thandle=%lld\tname=%s\tcomm=%d\n",
            filereg->event_id, IOHandleClassNames[filereg->handle_class], IOFileAccessNames[filereg->access_type], filereg->numchars, filereg->handle, name, filereg->communicator);
}

int processBufferAt(int curr){
    int processedBytes = 0;

    uint32_t header;
    EventInfo_t event;
    IOFileName_t filereg;
    char name[__MAX_MESSAGE_SIZE];

    memcpy(&header, &(buffer[curr + processedBytes]), sizeof(uint32_t));
    processedBytes += sizeof(uint32_t);
    printRecordHeader(header);

    uint8_t rtype = GET_RECORD_TYPE(header);
    switch(rtype){
    case IORecord_Invalid:
        fprintf(stderr, "invalid record type found\n");
        exit(-1);
        break;
    case IORecord_EventInfo:
        memcpy(&event, &(buffer[curr + processedBytes]), sizeof(EventInfo_t));
        processedBytes += sizeof(EventInfo_t);
        printEventInfo(&event);
        break;
    case IORecord_FileName:
        memcpy(&filereg, &(buffer[curr + processedBytes]), sizeof(IOFileName_t));
        memcpy(&name, &(buffer[curr + processedBytes + sizeof(IOFileName_t)]), GET_RECORD_SIZE(header) - sizeof(IOFileName_t));
        processedBytes += GET_RECORD_SIZE(header);
        printIOFileName(&filereg, &name);
        break;
    default:
        fprintf(stderr, "invalid record type found\n");
        exit(-1);
        break;        
    }

    return processedBytes;
}

int main(int argc, char** argv){
    if (argc != 2){
        print_usage("incorrect number of arguments", argv[0], 2);
    }
    FILE* inp = fopen(argv[1], "rb");
    if (inp == NULL){
        print_usage("cannot open supplied input file", argv[0], 3);
    }    
    fprintf(stdout, "***** converting input file %s *****\n", argv[1]);

    int rdsz;
    do {
        rdsz = fread(&buffer, 1, __IO_BUFFER_SIZE, inp);
        int currbuff = 0;
        while (currbuff < rdsz){
            currbuff += processBufferAt(currbuff);
        }
    } while (rdsz == __IO_BUFFER_SIZE);

    fclose(inp);
}
