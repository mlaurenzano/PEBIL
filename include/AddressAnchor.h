#ifndef _AddressAnchor_h_
#define _AddressAnchor_h_

#include <Base.h>
#include <Instruction.h>

class AddressAnchor {
private:
    Base* link;
    Instruction* linkedInstruction;

    uint64_t getLinkOffset();
public:
    AddressAnchor(Base* lnk, Instruction* par);
    ~AddressAnchor();

    Base* getLink() { return link; }
    Instruction* getLinkedInstruction() { return linkedInstruction; }

    bool verify();
    void print();
    void dump(BinaryOutputFile* b, uint32_t offset);
};

#endif // _AddressAnchor_h_
