#include <Base.h>
#include <Iterator.h>

uint64_t AddressIterator::operator++(){ 
    currAddress += (unitSize() - (currAddress % unitSize())); 
    return currAddress; 
}
uint64_t AddressIterator::operator--(){ 
    currAddress -= (currAddress % unitSize() ? currAddress % unitSize() : unitSize());
    return currAddress; 
}
uint64_t AddressIterator::operator++(int n) { 
    uint64_t ret = currAddress; 
    operator++();
    return ret; 
}
uint64_t AddressIterator::operator--(int n) { 
    uint64_t ret = currAddress; 
    operator--();
    return ret; 
}
uint64_t AddressIterator::readBytes(char* ptr){ 
    uint64_t ret = 0;
    if(unitSize() == sizeof(uint32_t)){
        uint32_t buff = 0;
        memcpy(&buff,ptr,sizeof(uint32_t));
        ret = buff;
    } else if(unitSize() == sizeof(uint64_t)){
        memcpy(&ret,ptr,sizeof(uint64_t));
    }
    return ret;
}
