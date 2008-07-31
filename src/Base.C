#include <Base.h>

extern uint16_t switchEndian(uint16_t hword){
    uint16_t b1 = (hword >> 8) & 0x00ff;
    uint16_t b2 = (hword << 8) & 0xff00;
    return (b1 | b2);
}

extern uint32_t switchEndian(uint32_t word){
    uint32_t b1 = (word >> 24) & 0x000000ff;
    uint32_t b2 = (word >>  8) & 0x0000ff00;
    uint32_t b3 = (word <<  8) & 0x00ff0000;
    uint32_t b4 = (word << 24) & 0xff000000;
    return (b1 | b2 | b3 | b4);
}

extern uint64_t switchEndian(uint64_t lword){
    int64_t mask = 0xff;
    uint64_t b1 = (lword >> 56) & mask;
    mask = mask << 8;
    uint64_t b2 = (lword >> 40) & mask;
    mask = mask << 8;
    uint64_t b3 = (lword >> 24) & mask;
    mask = mask << 8;
    uint64_t b4 = (lword >>  8) & mask;
    mask = mask << 8;
    uint64_t b5 = (lword <<  8) & mask;
    mask = mask << 8;
    uint64_t b6 = (lword << 24) & mask;
    mask = mask << 8;
    uint64_t b7 = (lword << 40) & mask;
    mask = mask << 8;
    uint64_t b8 = (lword << 56) & mask;
    return (b1 | b2 | b3 | b4 | b5 | b6 | b7 | b8);
}

extern bool isPowerOfTwo(uint32_t n){
    uint32_t currVal = 1;
    for (uint32_t i = 0; i < sizeof(uint32_t) * 8; i++){
        if (n == currVal) return true;
        currVal *= 2;
    }
    return false;
}


extern uint64_t nextAlignAddress(uint64_t addr, uint32_t align){
    if (align == 0 || align == 1 || addr == 0){
        return addr;
    }
    if (align > addr){
        return align;
    }
    ASSERT(isPowerOfTwo(align) && "alignment must be a power of 2 to call this function");
    if (align % addr){
        return addr + (addr % align);
    }
    return addr;    
}


extern uint64_t nextAlignAddressHalfWord(uint64_t addr){
    //    return nextAlignAddress(addr,sizeof(uint16_t));
    addr += (addr % 2 ? 2 : 0);
    addr >>= 1;
    addr <<= 1;
    return addr;
}
extern uint64_t nextAlignAddressWord(uint64_t addr){
    //    return nextAlignAddress(addr,sizeof(uint32_t));
    addr += (addr % 4 ? 4 : 0);
    addr >>= 2;
    addr <<= 2;
    return addr;
}
extern uint64_t nextAlignAddressDouble(uint64_t addr){
    //    return nextAlignAddress(addr,sizeof(uint64_t));
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

