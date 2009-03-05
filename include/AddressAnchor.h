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
    AddressAnchor(Base* lnk, Base* par);
    ~AddressAnchor();

    uint64_t getLinkOffset();

    Base* getLink() { return link; }
    Base* updateLink(Base* newLink);
    Base* getLinkedParent() { return linkedParent; }
    uint32_t getIndex() { return index; }

    uint32_t setIndex(uint32_t idx) { index = idx; return index; }
    uint64_t getLinkBaseAddress() { ASSERT(link); return link->getBaseAddress(); }

    bool verify();
    void print();
    void dump(BinaryOutputFile* b, uint32_t offset);
};

#endif // _AddressAnchor_h_
