#include <Base.h>

char* VersionTag::tag = "revision REVISION";

extern uint64_t nextAlignAddressHalfWord(uint64_t addr){
    addr += (addr % 2 ? 2 : 0);
    addr >>= 1;
    addr <<= 1;
    return addr;
}
extern uint64_t nextAlignAddressWord(uint64_t addr){
    addr += (addr % 4 ? 4 : 0);
    addr >>= 2;
    addr <<= 2;
    return addr;
}
extern uint64_t nextAlignAddressDouble(uint64_t addr){
    addr += (addr % 8 ? 8 : 0);
    addr >>= 3;
    addr <<= 3;
    return addr;
}

HashCode::HashCode(uint32_t s){
    entry.bits = INVALID_FIELD;
    if(validSection(s)){
        entry.fields.section  = s;
        entry.fields.function = INVALID_FIELD;
        entry.fields.block    = INVALID_FIELD;
        entry.fields.memop    = INVALID_FIELD;
    }
}

HashCode::HashCode(uint32_t s,uint32_t f){
    entry.bits = INVALID_FIELD;
    if(validSection(s) && validFunction(f)){
        entry.fields.section  = s;
        entry.fields.function = ++f;
        entry.fields.block    = INVALID_FIELD;
        entry.fields.memop    = INVALID_FIELD;
    }
}

HashCode::HashCode(uint32_t s,uint32_t f,uint32_t b){
    entry.bits = INVALID_FIELD;
    if(validSection(s) && validFunction(f) && validBlock(b)){
        entry.fields.section  = s;
        entry.fields.function = ++f;
        entry.fields.block    = ++b;
        entry.fields.memop    = INVALID_FIELD;
    }
}

HashCode::HashCode(uint32_t s,uint32_t f,uint32_t b,uint32_t m){
    entry.bits = INVALID_FIELD;
    if(validSection(s) && validFunction(f) && validBlock(b) && validMemop(m)){
        entry.fields.section  = s;
        entry.fields.function = ++f;
        entry.fields.block    = ++b;
        entry.fields.memop    = ++m;
    }
}


#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

double timer()
{
    struct timeval timestr;
    struct timezone* tzp = 0;
    gettimeofday(&timestr, tzp);
    return (double)timestr.tv_sec + 1.0E-06*(double)timestr.tv_usec;
}

