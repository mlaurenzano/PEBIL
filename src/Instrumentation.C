#include <Instrumentation.h>

#include <BasicBlock.h>
#include <ElfFileInst.h>
#include <Function.h>
#include <Instruction.h>
#include <InstructionGenerator.h>
#include <TextSection.h>

//#define VERIFY_FILTER

int searchInstPoint(const void* arg1,const void* arg2){
    uint64_t key = *((uint64_t*)arg1);
    InstrumentationPoint* ip = *((InstrumentationPoint**)arg2);

    ASSERT(ip && "Symbol should exist");

    uint64_t val = ip->getInstBaseAddress();

    if(key < val)
        return -1;
    if(key > val)
        return 1;
    return 0;
}


int compareInstPoint(const void* arg1, const void* arg2){
    uint64_t vl1 = (*((InstrumentationPoint**)arg1))->getInstBaseAddress();
    uint64_t vl2 = (*((InstrumentationPoint**)arg2))->getInstBaseAddress();

    if      (vl1 < vl2) return -1;
    else if (vl1 > vl2) return  1;
    else                return  0;
    return 0;
}

Vector<InstrumentationPoint*>* instpointFilterAddressRange(Base* object, Vector<InstrumentationPoint*>* instPoints){
    (*instPoints).sort(compareInstAddress);

    uint64_t lowEnd = object->getBaseAddress();
    uint64_t highEnd = object->getBaseAddress();

    if (object->getType() == PebilClassType_BasicBlock){
        highEnd += ((BasicBlock*)object)->getNumberOfBytes();
    } else if (object->getType() == PebilClassType_Function){
        highEnd += ((Function*)object)->getNumberOfBytes();
    } else {
        __SHOULD_NOT_ARRIVE;
    }

    PRINT_DEBUG_BLOAT_FILTER("Filtering %d points for range [%#llx,%#llx)", (*instPoints).size(), lowEnd, highEnd);
    for (uint32_t i = 0; i < (*instPoints).size(); i++){
        DEBUG_BLOAT_FILTER((*instPoints)[i]->getSourceObject()->print();)
    }

    Vector<InstrumentationPoint*>* filtered = new Vector<InstrumentationPoint*>();
    int32_t lidx = 0, hidx = (*instPoints).size()-1, midx = (lidx + hidx)/2;
    bool searchDone = false;
    while (lidx != hidx && !searchDone && midx != (lidx+hidx)/2){
        midx = (lidx + hidx)/2;
        if ((*instPoints)[midx]->getInstBaseAddress() >= highEnd){
            hidx = midx;
        } else if ((*instPoints)[midx]->getInstBaseAddress() < lowEnd){
            lidx = midx;
        } else {
            searchDone = true;
        }
    }
    for (int32_t i = lidx; i <= hidx; i++){
        if ((*instPoints)[i]->getInstBaseAddress() >= lowEnd &&
            (*instPoints)[i]->getInstBaseAddress() < highEnd){
            (*filtered).append((*instPoints)[i]);
        }        
    }

#ifdef VERIFY_FILTER
    Vector<InstrumentationPoint*>* filtered2 = new Vector<InstrumentationPoint*>();
    for (int32_t i = 0; i < (*instPoints).size(); i++){
        if ((*instPoints)[i]->getInstBaseAddress() >= lowEnd &&
            (*instPoints)[i]->getInstBaseAddress() < highEnd){
            (*filtered2).append((*instPoints)[i]);
        }
    }
    ASSERT((*filtered).size() == (*filtered2).size());
    delete filtered2;
#endif

    return filtered;
}

uint32_t InstrumentationPoint::addPrecursorInstruction(Instruction* inst){
    precursorInstructions.append(inst);
    return precursorInstructions.size();
}

int compareInstAddress(const void* arg1,const void* arg2){
    InstrumentationPoint* ip1 = *((InstrumentationPoint**)arg1);
    InstrumentationPoint* ip2 = *((InstrumentationPoint**)arg2);

    if(ip1->getInstBaseAddress() < ip2->getInstBaseAddress()){
        return -1;
    } else if(ip1->getInstBaseAddress() > ip2->getInstBaseAddress()){
        return 1;
    } else {
        PRINT_DEBUG_POINT_CHAIN("Comparing priority of 2 points at %#llx: %d %d", ip1->getInstBaseAddress(), ip1->getPriority(), ip2->getPriority());
        if (ip1->getPriority() < ip2->getPriority()){
            return -1;
        } else if (ip1->getPriority() > ip2->getPriority()){
            return 1;
        }
    }
    return 0;
}

uint32_t InstrumentationPoint64::generateTrampoline(Vector<Instruction*>* insts, uint64_t textBaseAddress, uint64_t offset, 
                                                    uint64_t returnOffset, bool doReloc, uint64_t regStorageBase, bool stackIsSafe){
    ASSERT(!trampolineInstructions.size() && "Cannot generate trampoline instructions more than once");

    trampolineOffset = offset;

    uint32_t trampolineSize = 0;

#ifndef OPTIMIZE_NONLEAF
    stackIsSafe = false;
#endif

    BitSet<uint32_t>* usedRegs = new BitSet<uint32_t>(X86_64BIT_GPRS);
    usedRegs->insert(X86_REG_SP);
    /*
    if (insts){
        for (uint32_t i = 0; i < (*insts).size(); i++){
            (*insts)[i]->touchedRegisters(usedRegs);
        }
    }
    */
    ~(*usedRegs);

    uint32_t tempReg1 = X86_64BIT_GPRS;

    for (uint32_t i = 0; i < X86_64BIT_GPRS; i++){
        if (usedRegs->contains(i)){
            if (tempReg1 == X86_64BIT_GPRS){
                tempReg1 = i;
            }
        }
    }
    delete usedRegs;
    if (tempReg1 == X86_64BIT_GPRS){
        PRINT_INFOR("Unable to alocate registers");
        for (uint32_t i = 0; i < (*insts).size(); i++){
            (*insts)[i]->print();
        }
    }
    
    ASSERT(tempReg1 < X86_64BIT_GPRS && "Could not find free registers for this instrumentation point");
    //    PRINT_INFOR("using temp reg %d", tempReg1);

    if (!stackIsSafe){
        trampolineInstructions.append(InstructionGenerator64::generateLoadRegImmReg(X86_REG_SP, -1*Size__trampoline_autoinc, X86_REG_SP));
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }


    if (protectionMethod == FlagsProtectionMethod_full){
        trampolineInstructions.append(InstructionGenerator::generatePushEflags());
        trampolineSize += trampolineInstructions.back()->getSizeInBytes(); 
    } else if (protectionMethod == FlagsProtectionMethod_light){
        trampolineInstructions.append(InstructionGenerator64::generateMoveRegToMem(X86_REG_AX, regStorageBase));
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();        
        trampolineInstructions.append(InstructionGenerator64::generateLoadAHFromFlags());
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }

    while (hasMorePrecursorInstructions()){
        trampolineInstructions.append(removeNextPrecursorInstruction());
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }

    if (!instrumentation->requiresDistinctTrampoline()){
        PRINT_DEBUG_INST("Generating inlined instructions for trampoline %#llx + %d, %#llx", textBaseAddress+offset, trampolineSize, textBaseAddress+getTargetOffset());
        while (instrumentation->hasMoreCoreInstructions()){
            trampolineInstructions.append(instrumentation->removeNextCoreInstruction());
            trampolineSize += trampolineInstructions.back()->getSizeInBytes();
        }
    } else {
        PRINT_DEBUG_INST("Generating relative call for trampoline %#llx + %d, %#llx", textBaseAddress + offset, trampolineSize, textBaseAddress+getTargetOffset());
        trampolineInstructions.append(InstructionGenerator::generateCallRelative(offset + trampolineSize, getTargetOffset()));
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }

    // this should be unused for now
    ASSERT(!hasMorePostcursorInstructions());
    while (hasMorePostcursorInstructions()){
        trampolineInstructions.append(removeNextPostcursorInstruction());
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }

    // restore eflags
    if (protectionMethod == FlagsProtectionMethod_full){
        trampolineInstructions.append(InstructionGenerator::generatePopEflags());
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    } else if (protectionMethod == FlagsProtectionMethod_light){
        trampolineInstructions.append(InstructionGenerator64::generateStoreAHToFlags());
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
        
        trampolineInstructions.append(InstructionGenerator64::generateMoveMemToReg(regStorageBase, X86_REG_AX));
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }

    if (!stackIsSafe){
        trampolineInstructions.append(InstructionGenerator64::generateLoadRegImmReg(X86_REG_SP, Size__trampoline_autoinc, X86_REG_SP));
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }

    uint64_t displacementDist = returnOffset - (offset + trampolineSize + numberOfBytes);

    if (doReloc){
        ASSERT(insts);
        if ((*insts).size()){
            PRINT_DEBUG_FUNC_RELOC("Moving instructions from %#llx to %#llx for relocation", (*insts)[0]->getProgramAddress(), (*insts)[0]->getBaseAddress());
        }

        int32_t numberOfBranches = 0;
        for (uint32_t i = 0; i < (*insts).size(); i++){
            if ((*insts)[i]->isControl() && !(*insts)[i]->isReturn()){
                numberOfBranches++;
                if ((*insts)[i]->bytesUsedForTarget() < sizeof(uint32_t)){
                    PRINT_DEBUG_FUNC_RELOC("This instruction uses %d bytes for target calculation", (*insts)[i]->bytesUsedForTarget());
                    (*insts)[i]->convertTo4ByteTargetOperand();
                }
            }
            (*insts)[i]->setBaseAddress(textBaseAddress+offset+trampolineSize);
            if (!(*insts)[i]->isNoop()){
                trampolineInstructions.append((*insts)[i]);
                trampolineSize += trampolineInstructions.back()->getSizeInBytes();
            } else {
                delete (*insts)[i];
            }
        }
        
        ASSERT(numberOfBranches < 2 && "Cannot have multiple branches in a basic block");

        trampolineInstructions.append(InstructionGenerator::generateJumpRelative(offset+trampolineSize,returnOffset));
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }
    return trampolineSize;
}

uint32_t InstrumentationPoint32::generateTrampoline(Vector<Instruction*>* insts, uint64_t textBaseAddress, uint64_t offset, 
                                                    uint64_t returnOffset, bool doReloc, uint64_t regStorageBase, bool stackIsSafe){
    ASSERT(!trampolineInstructions.size() && "Cannot generate trampoline instructions more than once");

    trampolineOffset = offset;

    uint32_t trampolineSize = 0;

#ifndef OPTIMIZE_NONLEAF
    stackIsSafe = false;
#endif

    BitSet<uint32_t>* usedRegs = new BitSet<uint32_t>(X86_32BIT_GPRS);
    usedRegs->insert(X86_REG_SP);
    /*
    if (insts){
        for (uint32_t i = 0; i < (*insts).size(); i++){
            (*insts)[i]->touchedRegisters(usedRegs);
        }
    }
    */
    ~(*usedRegs);

    uint32_t tempReg1 = X86_32BIT_GPRS;

    for (uint32_t i = 0; i < X86_32BIT_GPRS; i++){
        if (usedRegs->contains(i)){
            if (tempReg1 == X86_32BIT_GPRS){
                tempReg1 = i;
            }
        }
    }
    delete usedRegs;
    if (tempReg1 == X86_32BIT_GPRS){
        PRINT_INFOR("Unable to alocate registers");
        for (uint32_t i = 0; i < (*insts).size(); i++){
            (*insts)[i]->print();
        }
    }
    
    ASSERT(tempReg1 < X86_32BIT_GPRS && "Could not find free registers for this instrumentation point");
    //    PRINT_INFOR("using temp reg %d", tempReg1);

    if (!stackIsSafe){
        trampolineInstructions.append(InstructionGenerator32::generateLoadRegImmReg(X86_REG_SP, -1*Size__trampoline_autoinc, X86_REG_SP));
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }

    if (protectionMethod == FlagsProtectionMethod_full){
        trampolineInstructions.append(InstructionGenerator::generatePushEflags());
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    } else if (protectionMethod == FlagsProtectionMethod_light){
        trampolineInstructions.append(InstructionGenerator32::generateMoveRegToMem(X86_REG_AX, regStorageBase));
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
        trampolineInstructions.append(InstructionGenerator32::generateLoadAHFromFlags());
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }

    while (hasMorePrecursorInstructions()){
        trampolineInstructions.append(removeNextPrecursorInstruction());
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }

    if (!instrumentation->requiresDistinctTrampoline()){
        PRINT_DEBUG_INST("Generating inlined instructions for trampoline %#llx + %d, %#llx", textBaseAddress+offset, trampolineSize, textBaseAddress+getTargetOffset());
        while (instrumentation->hasMoreCoreInstructions()){
            trampolineInstructions.append(instrumentation->removeNextCoreInstruction());
            trampolineSize += trampolineInstructions.back()->getSizeInBytes();
        }
    } else {
        PRINT_DEBUG_INST("Generating relative call for trampoline %#llx + %d, %#llx", textBaseAddress + offset, trampolineSize, textBaseAddress+getTargetOffset());
        trampolineInstructions.append(InstructionGenerator::generateCallRelative(offset + trampolineSize, getTargetOffset()));
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }

    // postcursor should be unused for now
    ASSERT(!hasMorePostcursorInstructions());
    while (hasMorePostcursorInstructions()){
        trampolineInstructions.append(removeNextPostcursorInstruction());
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }

    if (protectionMethod == FlagsProtectionMethod_full){
        trampolineInstructions.append(InstructionGenerator::generatePopEflags());
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    } else if (protectionMethod== FlagsProtectionMethod_light){
        trampolineInstructions.append(InstructionGenerator32::generateStoreAHToFlags());
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
        
        trampolineInstructions.append(InstructionGenerator32::generateMoveMemToReg(regStorageBase, X86_REG_AX));
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }

    if (!stackIsSafe){
        trampolineInstructions.append(InstructionGenerator32::generateLoadRegImmReg(X86_REG_SP, Size__trampoline_autoinc, X86_REG_SP));
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }

    uint64_t displacementDist = returnOffset - (offset + trampolineSize + numberOfBytes);

    if (doReloc){
        ASSERT(insts);
        if ((*insts).size()){
            PRINT_DEBUG_FUNC_RELOC("Moving instructions from %#llx to %#llx for relocation", (*insts)[0]->getProgramAddress(), (*insts)[0]->getBaseAddress());
        }

        int32_t numberOfBranches = 0;
        for (uint32_t i = 0; i < (*insts).size(); i++){
            if ((*insts)[i]->isControl() && !(*insts)[i]->isReturn()){
                numberOfBranches++;
                if ((*insts)[i]->bytesUsedForTarget() < sizeof(uint32_t)){
                    PRINT_DEBUG_FUNC_RELOC("This instruction uses %d bytes for target calculation", (*insts)[i]->bytesUsedForTarget());
                    (*insts)[i]->convertTo4ByteTargetOperand();
                }
            }
            (*insts)[i]->setBaseAddress(textBaseAddress+offset+trampolineSize);
            (*insts)[i]->setBaseAddress(textBaseAddress+offset+trampolineSize);
            if (!(*insts)[i]->isNoop()){
                trampolineInstructions.append((*insts)[i]);
                trampolineSize += trampolineInstructions.back()->getSizeInBytes();
            } else {
                delete (*insts)[i];
            }
        }
        
        ASSERT(numberOfBranches < 2 && "Cannot have multiple branches in a basic block");

        trampolineInstructions.append(InstructionGenerator::generateJumpRelative(offset + trampolineSize, returnOffset));
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }

    return trampolineSize;
}


uint32_t InstrumentationSnippet::addSnippetInstruction(Instruction* inst){
    snippetInstructions.append(inst);
    return snippetInstructions.size();
}

uint32_t InstrumentationFunction::wrapperSize(){
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < wrapperInstructions.size(); i++){
        totalSize += wrapperInstructions[i]->getSizeInBytes();
    }
    return totalSize;
}
uint32_t InstrumentationFunction::procedureLinkSize(){
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < procedureLinkInstructions.size(); i++){
        totalSize += procedureLinkInstructions[i]->getSizeInBytes();
    }
    return totalSize;
}
uint32_t Instrumentation::bootstrapSize(){
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < bootstrapInstructions.size(); i++){
        totalSize += bootstrapInstructions[i]->getSizeInBytes();
    }
    return totalSize;
}
uint32_t InstrumentationFunction::globalDataSize(){
    return Size__32_bit_Global_Offset_Table_Entry;
}

Instrumentation::Instrumentation(PebilClassTypes typ)
    : Base(typ)
{
    bootstrapOffset = 0;
}

Instrumentation::~Instrumentation(){
    for (uint32_t i = 0; i < bootstrapInstructions.size(); i++){
        delete bootstrapInstructions[i];
    }
}

uint32_t InstrumentationFunction::addArgument(uint64_t offset){
    Argument arg;

    arg.offset = offset;
    arguments.append(arg);
    return arguments.size();
}

void InstrumentationFunction::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currentOffset = procedureLinkOffset;
    if (!isStaticLinked()){
        for (uint32_t i = 0; i < procedureLinkInstructions.size(); i++){
            procedureLinkInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
            currentOffset += procedureLinkInstructions[i]->getSizeInBytes();
        }
    }

    currentOffset = bootstrapOffset;
    for (uint32_t i = 0; i < bootstrapInstructions.size(); i++){
        bootstrapInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
        currentOffset += bootstrapInstructions[i]->getSizeInBytes();
    }
    currentOffset = wrapperOffset;
    for (uint32_t i = 0; i < wrapperInstructions.size(); i++){
        wrapperInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
        currentOffset += wrapperInstructions[i]->getSizeInBytes();
    }
}

uint64_t InstrumentationFunction::getEntryPoint(){
    return wrapperOffset;
}

uint64_t InstrumentationSnippet::getEntryPoint(){
    return snippetOffset;
}


uint32_t InstrumentationFunction64::generateProcedureLinkInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t realPLTAddress){
    ASSERT(!procedureLinkInstructions.size() && "This array should not be initialized");

    if (isStaticLinked()){
        ASSERT(!realPLTAddress);
        return 0;
    }

    uint64_t procedureLinkAddress = textBaseAddress + procedureLinkOffset;
    procedureLinkInstructions.append(InstructionGenerator64::generateIndirectRelativeJump(procedureLinkAddress,dataBaseAddress + globalDataOffset));
    procedureLinkInstructions.append(InstructionGenerator64::generateStackPushImm(relocationOffset));

    uint32_t returnAddress = procedureLinkAddress + procedureLinkSize();
    procedureLinkInstructions.append(InstructionGenerator64::generateJumpRelative(returnAddress,realPLTAddress));

    ASSERT(procedureLinkSize() == procedureLinkReservedSize());
    return procedureLinkInstructions.size();
}

uint32_t InstrumentationFunction32::generateProcedureLinkInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t realPLTAddress){
    ASSERT(!procedureLinkInstructions.size() && "This array should not be initialized");

    if (isStaticLinked()){
        ASSERT(!realPLTAddress);
        return 0;
    }

    PRINT_DEBUG_INST("Generating PLT instructions at offset %llx", procedureLinkOffset);

    procedureLinkInstructions.append(InstructionGenerator32::generateJumpIndirect(dataBaseAddress + globalDataOffset));
    procedureLinkInstructions.append(InstructionGenerator32::generateStackPushImm(relocationOffset));
    uint32_t returnAddress = textBaseAddress + procedureLinkOffset + procedureLinkSize();
    procedureLinkInstructions.append(InstructionGenerator32::generateJumpRelative(returnAddress,realPLTAddress));

    ASSERT(procedureLinkSize() == procedureLinkReservedSize());

    return procedureLinkInstructions.size();
}

uint32_t InstrumentationFunction64::generateBootstrapInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress){
    PRINT_DEBUG_DATA_PLACEMENT("Inst function global offset data is at %#llx", dataBaseAddress + globalDataOffset);
    bootstrapInstructions.append(InstructionGenerator64::generateMoveImmToReg(globalData, X86_REG_CX));
    bootstrapInstructions.append(InstructionGenerator64::generateMoveImmToReg(dataBaseAddress + globalDataOffset, X86_REG_DX));
    bootstrapInstructions.append(InstructionGenerator64::generateMoveRegToRegaddr(X86_REG_CX, X86_REG_DX));

    while (bootstrapSize() < bootstrapReservedSize()){
        bootstrapInstructions.append(InstructionGenerator64::generateNoop());
    }
    ASSERT(bootstrapSize() == bootstrapReservedSize());
    return bootstrapInstructions.size();
}

uint32_t InstrumentationFunction32::generateBootstrapInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress){
    bootstrapInstructions.append(InstructionGenerator32::generateMoveImmToReg(globalData,X86_REG_CX));
    bootstrapInstructions.append(InstructionGenerator32::generateMoveRegToMem(X86_REG_CX,dataBaseAddress + globalDataOffset));

    while (bootstrapSize() < bootstrapReservedSize()){
        bootstrapInstructions.append(InstructionGenerator32::generateNoop());
    }
    ASSERT(bootstrapSize() == bootstrapReservedSize());
    return bootstrapInstructions.size();
}

uint32_t InstrumentationFunction64::generateWrapperInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress){
    ASSERT(!wrapperInstructions.size() && "This array should be empty");

    wrapperInstructions.append(InstructionGenerator64::generatePushEflags());
    for (uint32_t i = 0; i < X86_64BIT_GPRS; i++){
        wrapperInstructions.append(InstructionGenerator64::generateStackPush(i));
    }

    ASSERT(arguments.size() < MAX_ARGUMENTS_64BIT && "More arguments must be pushed onto stack, which is not yet implemented"); 

    for (uint32_t i = 0; i < arguments.size(); i++){
        uint32_t idx = arguments.size() - i - 1;
        uint32_t argumentRegister;

        // use GPRs rdi,rsi,rdx,rcx,r8,r9, then push onto stack
        switch(idx){
        case 0:
            argumentRegister = X86_REG_DI;
            break;
        case 1:
            argumentRegister = X86_REG_SI;
            break;
        case 2:
            argumentRegister = X86_REG_DX;
            break;
        case 3:
            argumentRegister = X86_REG_CX;
            break;
        case 4:
            argumentRegister = X86_REG_R8;
            break;
        case 5:
            argumentRegister = X86_REG_R9;
            break;
        default:
            PRINT_ERROR("Cannot pass more than %d argument to an instrumentation function", MAX_ARGUMENTS_64BIT);
            __SHOULD_NOT_ARRIVE;
        }

        wrapperInstructions.append(InstructionGenerator64::generateMoveImmToReg(dataBaseAddress + arguments[idx].offset, argumentRegister));

        /*
        bootstrapInstructions.append(InstructionGenerator64::generateMoveImmToReg(arguments[idx].value, X86_REG_CX));
        bootstrapInstructions.append(InstructionGenerator64::generateMoveImmToReg(dataBaseAddress + arguments[idx].offset, X86_REG_DX));
        bootstrapInstructions.append(InstructionGenerator::generateMoveRegToRegaddr(X86_REG_CX, X86_REG_DX));
        */
    }

    /*
    wrapperInstructions.append(InstructionGenerator64::generateStackPush(X86_REG_BX));
    wrapperInstructions.append(InstructionGenerator64::generateXorRegReg(X86_REG_AX, X86_REG_AX));
    wrapperInstructions.append(InstructionGenerator64::generateRegSubImm(X86_REG_SP,8));
    */

    uint64_t wrapperTargetOffset = 0;
    if (isStaticLinked()){
        wrapperTargetOffset = functionEntry - textBaseAddress;
    } else {
        wrapperTargetOffset = procedureLinkOffset;
    }
    wrapperInstructions.append(InstructionGenerator64::generateCallRelative(wrapperOffset + wrapperSize(), wrapperTargetOffset));

    for (uint32_t i = 0; i < X86_64BIT_GPRS; i++){
        wrapperInstructions.append(InstructionGenerator64::generateStackPop(X86_64BIT_GPRS-1-i));
    }
    wrapperInstructions.append(InstructionGenerator64::generatePopEflags());

    wrapperInstructions.append(InstructionGenerator64::generateReturn());

    while (wrapperSize() < wrapperReservedSize()){
        wrapperInstructions.append(InstructionGenerator64::generateNoop());
    }
    ASSERT(wrapperSize() == wrapperReservedSize());

    return wrapperInstructions.size();
}

uint32_t InstrumentationFunction32::generateWrapperInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress){
    ASSERT(!wrapperInstructions.size() && "This array should be empty");

    wrapperInstructions.append(InstructionGenerator32::generatePushEflags());
    for (uint32_t i = 0; i < X86_32BIT_GPRS; i++){
        wrapperInstructions.append(InstructionGenerator32::generateStackPush(i));
    }

    for (uint32_t i = 0; i < arguments.size(); i++){
        uint32_t idx = arguments.size() - i - 1;

        // everything is passed on the stack
        wrapperInstructions.append(InstructionGenerator32::generateMoveImmToReg(dataBaseAddress + arguments[idx].offset, X86_REG_DX));
        wrapperInstructions.append(InstructionGenerator32::generateStackPush(X86_REG_DX));

        /*
        bootstrapInstructions.append(InstructionGenerator32::generateMoveImmToReg(arguments[idx].value, X86_REG_CX));
        bootstrapInstructions.append(InstructionGenerator32::generateMoveRegToMem(X86_REG_CX, dataBaseAddress + arguments[idx].offset));
        */
    }

    wrapperInstructions.append(InstructionGenerator32::generateCallRelative(wrapperOffset + wrapperSize(), procedureLinkOffset));

    for (uint32_t i = 0; i < arguments.size(); i++){
        wrapperInstructions.append(InstructionGenerator32::generateStackPop(X86_REG_CX));
    }

    for (uint32_t i = 0; i < X86_32BIT_GPRS; i++){
        wrapperInstructions.append(InstructionGenerator32::generateStackPop(X86_32BIT_GPRS-1-i));
    }
    wrapperInstructions.append(InstructionGenerator32::generatePopEflags());

    wrapperInstructions.append(InstructionGenerator32::generateReturn());

    while (wrapperSize() < wrapperReservedSize()){
        wrapperInstructions.append(InstructionGenerator32::generateNoop());
    }
    ASSERT(wrapperSize() == wrapperReservedSize());

    return wrapperInstructions.size();
}

uint32_t InstrumentationFunction64::generateGlobalData(uint64_t textBaseAddress){
    globalData = textBaseAddress + procedureLinkOffset + PLT_RETURN_OFFSET_64BIT;
    return globalData;
}

uint32_t InstrumentationFunction32::generateGlobalData(uint64_t textBaseAddress){
    globalData = textBaseAddress + procedureLinkOffset + PLT_RETURN_OFFSET_32BIT;
    return globalData;
}

uint32_t InstrumentationFunction::sizeNeeded(){
    uint32_t totalSize = wrapperSize() + procedureLinkSize() + bootstrapSize();
    return totalSize;
}

InstrumentationFunction::InstrumentationFunction(uint32_t idx, char* funcName, uint64_t dataoffset, uint64_t fEntry)
    : Instrumentation(PebilClassType_InstrumentationFunction)
{
    index = idx;

    functionEntry = fEntry;

    functionName = new char[strlen(funcName)+1];
    strcpy(functionName,funcName);

    procedureLinkOffset = 0;
    bootstrapOffset = 0;
    wrapperOffset = 0;

    globalData = 0;
    globalDataOffset = dataoffset;

    distinctTrampoline = true;
}

InstrumentationFunction::~InstrumentationFunction(){
    if (functionName){
        delete[] functionName;
    }
    for (uint32_t i = 0; i < procedureLinkInstructions.size(); i++){
        delete procedureLinkInstructions[i];
    }
    for (uint32_t i = 0; i < wrapperInstructions.size(); i++){
        delete wrapperInstructions[i];
    }
}

void InstrumentationFunction::print(){
    PRINT_INFOR("Instrumentation Function (%d) %s -- is static linked? %d", index, functionName, isStaticLinked());
    PRINT_INFOR("\t\tProcedure Link Instructions: %d", procedureLinkInstructions.size());
    PRINT_INFOR("\t\tProcedure Link Offset      : %lld", procedureLinkOffset);
    for (uint32_t i = 0; i < procedureLinkInstructions.size(); i++){
        procedureLinkInstructions[i]->print();
    }
    PRINT_INFOR("\t\tBootstrap Instructions     : %d", bootstrapInstructions.size());
    PRINT_INFOR("\t\tBootstrap Offset           : %lld", bootstrapOffset);
    for (uint32_t i = 0; i < bootstrapInstructions.size(); i++){
        bootstrapInstructions[i]->print();
    }
    PRINT_INFOR("\t\tWrapper Instructions       : %d", wrapperInstructions.size());
    PRINT_INFOR("\t\tWrapper Offset             : %lld", wrapperOffset);
    for (uint32_t i = 0; i < wrapperInstructions.size(); i++){
        wrapperInstructions[i]->print();
    }
    PRINT_INFOR("\t\tGlobal Data                : %x", globalData);
    PRINT_INFOR("\t\tGlobal Data Offset         : %lld", globalDataOffset);

}

uint32_t InstrumentationSnippet::generateSnippetControl(){
    if (distinctTrampoline){
        snippetInstructions.append(InstructionGenerator::generateReturn());
    }
    return snippetInstructions.size();
}

void InstrumentationSnippet::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currentOffset = bootstrapOffset;
    for (uint32_t i = 0; i < bootstrapInstructions.size(); i++){
        bootstrapInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
        currentOffset += bootstrapInstructions[i]->getSizeInBytes();
    }
    currentOffset = snippetOffset;
    for (uint32_t i = 0; i < snippetInstructions.size(); i++){
        snippetInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
        currentOffset += snippetInstructions[i]->getSizeInBytes();
    }
}


void InstrumentationSnippet::print(){
    __FUNCTION_NOT_IMPLEMENTED;
}

uint32_t InstrumentationSnippet::dataSize(){
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < numberOfDataEntries; i++){
        totalSize += dataEntrySizes[i];
    }
    return totalSize;
}

uint32_t InstrumentationSnippet::bootstrapSize(){
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < bootstrapInstructions.size(); i++){
        totalSize += bootstrapInstructions[i]->getSizeInBytes();
    }
    return totalSize;    
}

uint32_t InstrumentationSnippet::snippetSize(){
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < snippetInstructions.size(); i++){
        totalSize += snippetInstructions[i]->getSizeInBytes();
    }
    return totalSize;
}

uint32_t InstrumentationSnippet::reserveData(uint64_t offset, uint32_t size){
    uint32_t* newSizes = new uint32_t[numberOfDataEntries+1];
    uint64_t* newOffsets = new uint64_t[numberOfDataEntries+1];

    memcpy(newSizes, dataEntrySizes, sizeof(uint32_t) * numberOfDataEntries);
    memcpy(newOffsets, dataEntryOffsets, sizeof(uint64_t) * numberOfDataEntries);

    newSizes[numberOfDataEntries] = size;
    newOffsets[numberOfDataEntries] = offset;

    if (dataEntrySizes){
        delete[] dataEntrySizes;
    }
    if (dataEntryOffsets){
        delete[] dataEntryOffsets;
    }

    dataEntrySizes = newSizes;
    dataEntryOffsets = newOffsets;

    numberOfDataEntries++;
    return numberOfDataEntries-1;
}

InstrumentationSnippet::InstrumentationSnippet()
    : Instrumentation(PebilClassType_InstrumentationSnippet)
{
    snippetOffset = 0;    

    numberOfDataEntries = 0;
    dataEntrySizes = NULL;
    dataEntryOffsets = NULL;

    distinctTrampoline = SNIPPET_TRAMPOLINE_DEFAULT;
}

InstrumentationSnippet::~InstrumentationSnippet(){
    for (uint32_t i = 0; i < snippetInstructions.size(); i++){
        delete snippetInstructions[i];
    }
}

Vector<Instruction*>* InstrumentationPoint::swapInstructionsAtPoint(bool isChain, Vector<Instruction*>* replacements){
    Instruction* instruction = (Instruction*)point;
    ASSERT(instruction->getContainer() && instruction->getContainer()->getType() == PebilClassType_Function);
    Function* func = (Function*)instruction->getContainer();
    if (isChain){
        return func->swapInstructions(getInstBaseAddress() - Size__uncond_jump, replacements);
    } else {
        return func->swapInstructions(getInstSourceAddress(), replacements);
    }
}

void InstrumentationPoint::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currentOffset = trampolineOffset;
    for (uint32_t i = 0; i < trampolineInstructions.size(); i++){
        trampolineInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
        currentOffset += trampolineInstructions[i]->getSizeInBytes();
    }
}

uint32_t InstrumentationPoint::sizeNeeded(){
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < trampolineInstructions.size(); i++){
        totalSize += trampolineInstructions[i]->getSizeInBytes();
    }
    return totalSize;
}

uint64_t InstrumentationPoint::getInstBaseAddress(){
    ASSERT(point);
    return point->getBaseAddress();
}

uint64_t InstrumentationPoint::getInstSourceAddress(){
    Function* f = (Function*)point->getContainer();
    if (f->isRelocated()){
        return getInstBaseAddress() - getNumberOfBytes();
    } else {
        return getInstBaseAddress();
    }
    __SHOULD_NOT_ARRIVE;
}

InstrumentationPoint::InstrumentationPoint(Base* pt, Instrumentation* inst, InstrumentationModes instMode, FlagsProtectionMethods flagsMethod, InstLocations loc)
    : Base(PebilClassType_InstrumentationPoint)
{
    if (pt->getType() == PebilClassType_Instruction){
        point = (Instruction*)pt;
    } else if (pt->getType() == PebilClassType_BasicBlock){
        ASSERT(((BasicBlock*)pt)->getInstruction(0));
        point = (Instruction*)((BasicBlock*)pt)->getInstruction(0);
    } else if (pt->getType() == PebilClassType_Function){
        Function* f = (Function*)pt;
        BasicBlock* bb = f->getBasicBlockAtAddress(f->getBaseAddress());
        ASSERT(bb);
        ASSERT(bb->getInstruction(0));
        point = bb->getInstruction(0);
    } else {
        PRINT_INFOR("Instrumentation point type not allowed %d", pt->getType());
        __SHOULD_NOT_ARRIVE;
    }
    ASSERT(point);

    instrumentation = inst;
    instrumentation->setInstrumentationPoint(this);

    instrumentationMode = instMode;
    protectionMethod = flagsMethod;

    if (instrumentationMode == InstrumentationMode_inline && instrumentation->getType() != PebilClassType_InstrumentationSnippet){
        PRINT_ERROR("InstrumentationMode_inline can only be used with snippets");
    }    

    instLocation = loc;

    trampolineOffset = 0;
    priority = InstPriority_regular;

    verify();
}

InstrumentationPoint32::InstrumentationPoint32(Base* pt, Instrumentation* inst, InstrumentationModes instMode, FlagsProtectionMethods flagsMethod, InstLocations loc) :
    InstrumentationPoint(pt, inst, instMode, flagsMethod, loc)
{
    numberOfBytes = 0;
    if (instMode == InstrumentationMode_inline){

        // count the number of bytes the tool wants
        InstrumentationSnippet* snippet = (InstrumentationSnippet*)instrumentation;
        for (uint32_t i = 0; i < snippet->getNumberOfCoreInstructions(); i++){
            numberOfBytes += snippet->getCoreInstruction(i)->getSizeInBytes();
        }

        // then add the number of bytes needed for state protection
        if (protectionMethod == FlagsProtectionMethod_full){
            numberOfBytes += Size__flag_protect_full;
        } else if (protectionMethod == FlagsProtectionMethod_light){
            numberOfBytes += Size__32_bit_flag_protect_light;
        } else if (protectionMethod == FlagsProtectionMethod_none){
            numberOfBytes += 0;
        } else {
            PRINT_ERROR("Protection method is invalid");
        }
    } else {
        numberOfBytes = Size__uncond_jump;
    }
    ASSERT(numberOfBytes);
}

InstrumentationPoint64::InstrumentationPoint64(Base* pt, Instrumentation* inst, InstrumentationModes instMode, FlagsProtectionMethods flagsMethod, InstLocations loc) :
    InstrumentationPoint(pt, inst, instMode, flagsMethod, loc)
{
    numberOfBytes = 0;
    if (instMode == InstrumentationMode_inline){

        // count the number of bytes the tool wants
        InstrumentationSnippet* snippet = (InstrumentationSnippet*)instrumentation;
        for (uint32_t i = 0; i < snippet->getNumberOfCoreInstructions(); i++){
            numberOfBytes += snippet->getCoreInstruction(i)->getSizeInBytes();
        }

        // then add the number of bytes needed for state protection
        if (protectionMethod == FlagsProtectionMethod_full){
            numberOfBytes += Size__flag_protect_full;
        } else if (protectionMethod == FlagsProtectionMethod_light){
            numberOfBytes += Size__64_bit_flag_protect_light;
        } else if (protectionMethod == FlagsProtectionMethod_none){
            numberOfBytes += 0;
        } else {
            PRINT_ERROR("Protection method is invalid");
        }
    } else {
        numberOfBytes = Size__uncond_jump;
    }
    ASSERT(numberOfBytes);
}


bool InstrumentationPoint::verify(){
    if (!point->containsProgramBits()){
        PRINT_ERROR("Instrumentation point not allowed to be type %d", point->getType());
        return false;
    }
    if (priority == InstPriority_undefined || priority >= InstPriority_Total_Types){
        PRINT_ERROR("Instrumentation point not allowed to have priority %d", priority);
        return false;
    }
    if (instrumentationMode == InstrumentationMode_inline && instrumentation->getType() != PebilClassType_InstrumentationSnippet){
        PRINT_ERROR("InstrumentationMode_inline can only be used with snippets");
        return false;
    }

    return true;
}

InstrumentationPoint::~InstrumentationPoint(){
    for (uint32_t i = 0; i < trampolineInstructions.size(); i++){
        delete trampolineInstructions[i];
    }
}

void InstrumentationPoint::print(){
    __FUNCTION_NOT_IMPLEMENTED;
}
