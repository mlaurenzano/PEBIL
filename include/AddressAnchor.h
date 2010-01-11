#ifndef _AddressAnchor_h_
#define _AddressAnchor_h_

#include <Base.h>

extern int searchLinkBaseAddressExact(const void* arg1, const void* arg2);
extern int searchLinkBaseAddress(const void* arg1, const void* arg2);
extern int compareLinkBaseAddress(const void* arg1,const void* arg2);

class AddressAnchor {
private:
    Base* link;
    Base* linkedParent;

    uint32_t index;

    void dump8(BinaryOutputFile* b, uint32_t offset);
    void dump16(BinaryOutputFile* b, uint32_t offset);
    void dump32(BinaryOutputFile* b, uint32_t offset);
    void dump64(BinaryOutputFile* b, uint32_t offset);

    void dumpInstruction(BinaryOutputFile* b, uint32_t offset);
    void dumpDataReference(BinaryOutputFile* b, uint32_t offset);
public:
    // this gets accessed A LOT and is a performance bottleneck for instrumentation, so
    // we are basically caching link->getBaseAddress() and making it public so it doesn't
    // require a function access to get to it.
    uint64_t linkBaseAddress;

    AddressAnchor(Base* lnk, Base* par);
    ~AddressAnchor();

    uint64_t getLinkOffset();
    uint64_t getLinkValue();

    Base* getLink() { return link; }
    
    void refreshCache();
    Base* updateLink(Base* newLink);
    Base* getLinkedParent() { return linkedParent; }
    uint32_t getIndex() { return index; }

    void setIndex(uint32_t idx) { index = idx; }

    bool verify();
    void print();
    void dump(BinaryOutputFile* b, uint32_t offset);
};

#endif // _AddressAnchor_h_
