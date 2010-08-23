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
    fprintf(stdout, "%s(%dB): ", IORecordTypeNames[GET_RECORD_TYPE(header)], GET_RECORD_SIZE(header));
}

void printEventInfo(EventInfo_t* event){
    fprintf(stdout, "\tunqid %5lld: class=%s, o_class=%s, h_class=%hhd, mode=%hhd, e_type=%s, h_id=%hd, source=%lld, size=%lld, offset=%lld\n\0",
            event->unqid, IOEventClassNames[event->class], IOOffsetClassNames[event->offset_class], event->handle_class, event->mode,
            IOEventNames[event->event_type], event->handle_id, event->source, event->size, event->offset);
}

void printIOFileName(IOFileName_t* filereg, char* name){
    fprintf(stdout, "\tunqid %5lld: h_class %hhd\ta_type %hhd\tnumchars %d\thandle %d name %s\n",
            filereg->event_id, filereg->handle_class, filereg->access_type, filereg->numchars, filereg->handle, name);
}

int processBufferAt(int curr){
    int processed = 0;

    uint32_t header;
    EventInfo_t event;
    IOFileName_t filereg;
    char name[__MAX_MESSAGE_SIZE];

    memcpy(&header, &(buffer[curr + processed]), sizeof(uint32_t));
    processed += sizeof(uint32_t);
    printRecordHeader(header);

    uint8_t rtype = GET_RECORD_TYPE(header);
    switch(rtype){
    case IORecord_Invalid:
        fprintf(stderr, "invalid record type found\n");
        exit(-1);
        break;
    case IORecord_EventInfo:
        memcpy(&event, &(buffer[curr + processed]), sizeof(EventInfo_t));
        processed += sizeof(EventInfo_t);
        printEventInfo(&event);
        break;
    case IORecord_FileName:
        memcpy(&filereg, &(buffer[curr + processed]), sizeof(IOFileName_t));
        memcpy(&name, &(buffer[curr + processed + sizeof(IOFileName_t)]), GET_RECORD_SIZE(header) - sizeof(IOFileName_t));
        processed += GET_RECORD_SIZE(header);
        printIOFileName(&filereg, &name);
        break;
    default:
        fprintf(stderr, "invalid record type found\n");
        exit(-1);
        break;        
    }

    return processed;
}

int main(int argc, char** argv){
    if (argc != 2){
        print_usage("incorrect number of arguments", argv[0], 2);
    }
    fprintf(stdout, "reading input file %s\n", argv[1]);
    FILE* inp = fopen(argv[1], "rb");
    if (inp == NULL){
        print_usage("cannot open supplied input file", argv[0], 3);
    }    

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
