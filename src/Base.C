#include <Base.h>

int compareBaseAddress(const void* arg1,const void* arg2){
    Base* inst1 = *((Base**)arg1);
    Base* inst2 = *((Base**)arg2);
    uint64_t vl1 = inst1->getBaseAddress();
    uint64_t vl2 = inst2->getBaseAddress();

    if(vl1 < vl2)
        return -1;
    if(vl1 > vl2)
        return 1;
    return 0;
}

int searchBaseAddressExact(const void* arg1, const void* arg2){
    uint64_t key = *((uint64_t*)arg1);
    Base* inst = *((Base**)arg2);

    ASSERT(inst && "Base should exist");

    uint64_t val = inst->getBaseAddress();

    if (key < val)
        return -1;
    if (key > val)
        return 1;
    return 0;
}

int searchBaseAddress(const void* arg1, const void* arg2){
    uint64_t key = *((uint64_t*)arg1);
    Base* inst = *((Base**)arg2);

    ASSERT(inst && "Base should exist");

    uint64_t val_low = inst->getBaseAddress();
    uint64_t val_high = val_low + inst->getSizeInBytes();

    if (key < val_low)
        return -1;
    if (key >= val_high)
        return 1;
    return 0;
}


extern bool isPowerOfTwo(uint32_t n){
    uint32_t currVal = 1;
    for (uint32_t i = 0; i < sizeof(uint32_t) * 8; i++){
        if (n == currVal) return true;
        currVal *= 2;
    }
    return false;
}



uint32_t align_mask_or[32] = {
    0x00000001, 0x00000002, 0x00000004, 0x00000008,
    0x00000010, 0x00000020, 0x00000040, 0x00000080,
    0x00000100, 0x00000200, 0x00000400, 0x00000800,
    0x00001000, 0x00002000, 0x00004000, 0x00008000,
    0x00010000, 0x00020000, 0x00040000, 0x00080000,
    0x00100000, 0x00200000, 0x00400000, 0x00800000,
    0x01000000, 0x02000000, 0x04000000, 0x08000000,
    0x10000000, 0x20000000, 0x40000000, 0x80000000
};

uint32_t align_mask_and[32] = {
    0xffffffff, 0xfffffffe, 0xfffffffc, 0xfffffff8,
    0xfffffff0, 0xffffffe0, 0xffffffc0, 0xffffff80,
    0xffffff00, 0xfffffe00, 0xfffffc00, 0xfffff800,
    0xfffff000, 0xffffe000, 0xffffc000, 0xffff8000,
    0xffff0000, 0xfffe0000, 0xfffc0000, 0xfff80000,
    0xfff00000, 0xffe00000, 0xffc00000, 0xff800000,
    0xff000000, 0xfe000000, 0xfc000000, 0xf8000000,
    0xf0000000, 0xe0000000, 0xc0000000, 0x80000000
};

extern uint64_t nextAlignAddress(uint64_t addr, uint32_t align){
    if (align == 0 || align == 1 || addr == 0){
        return addr;
    }
    if (align > addr){
        return align;
    }
    ASSERT(isPowerOfTwo(align) && "alignment must be a power of 2 to call this function");
    int32_t pow = -1;
    uint32_t tmp = align;
    while (tmp > 0){
        pow++;
        tmp = tmp >> 1;
    }
    uint64_t aligned = addr & align_mask_and[pow];
    if (aligned < addr){
        aligned = aligned + align_mask_or[pow];
    }

    /*
    uint64_t ans2 = addr;
    while (ans2 % align){
        ans2++;
    }
    PRINT_INFOR("Alignment: align(%llx,%x)=%llx/%llx", addr, align, aligned, ans2);
    PRINT_INFOR("Masks align: and[%d]=0x%x, or[%d]=0x%x", pow, align_mask_and[pow], pow, align_mask_or[pow]);
    ASSERT(aligned == ans2);
    */

    return aligned;
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
        entry.fields.section  = ++s;
        entry.fields.function = INVALID_FIELD;
        entry.fields.block    = INVALID_FIELD;
        entry.fields.memop    = INVALID_FIELD;
    }
}

HashCode::HashCode(uint32_t s,uint32_t f){
    entry.bits = INVALID_FIELD;
    if(validSection(s) && validFunction(f)){
        entry.fields.section  = ++s;
        entry.fields.function = ++f;
        entry.fields.block    = INVALID_FIELD;
        entry.fields.memop    = INVALID_FIELD;
    }
}

HashCode::HashCode(uint32_t s,uint32_t f,uint32_t b){
    entry.bits = INVALID_FIELD;
    if(validSection(s) && validFunction(f) && validBlock(b)){
        entry.fields.section  = ++s;
        entry.fields.function = ++f;
        entry.fields.block    = ++b;
        entry.fields.memop    = INVALID_FIELD;
    }
}

HashCode::HashCode(uint32_t s,uint32_t f,uint32_t b,uint32_t m){
    entry.bits = INVALID_FIELD;
    if(validSection(s) && validFunction(f) && validBlock(b) && validMemop(m)){
        entry.fields.section  = ++s;
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

