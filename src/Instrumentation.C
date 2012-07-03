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

#include <Instrumentation.h>

#include <BasicBlock.h>
#include <ElfFileInst.h>
#include <Function.h>
#include <X86Instruction.h>
#include <X86InstructionFactory.h>
#include <TextSection.h>

void InstrumentationPoint::setFlagsProtectionMethod(FlagsProtectionMethods p){
    protectionMethod = p;
}

uint32_t map64BitArgToReg(uint32_t idx){
    uint32_t argumentRegister;
    ASSERT(idx <= Num__64_bit_StackArgs);

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
        PRINT_ERROR("Cannot pass more than %d argument to an instrumentation function", Num__64_bit_StackArgs);
        __SHOULD_NOT_ARRIVE;
    }
    return argumentRegister;
}

void InstrumentationPoint::print(){
    PRINT_INFOR("Instrumentation point at %#llx -> %#llx: size %d, priority %s(%d), protection %s, mode %s, loc %s, offset %d, function %s", getInstBaseAddress(), getInstSourceAddress(), numberOfBytes, InstPriorityNames[priority], priority, FlagsProtectionMethodNames[getFlagsProtectionMethod()], InstrumentationModeNames[instrumentationMode], InstLocationNames[instLocation], offsetFromPoint, point->getContainer()->getName());
}

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
    (*instPoints).sort(compareInstBaseAddress);

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

    return filtered;
}

uint32_t InstrumentationPoint::addPrecursorInstruction(X86Instruction* inst){
    precursorInstructions.append(inst);
    return precursorInstructions.size();
}

uint32_t InstrumentationPoint::addPostcursorInstruction(X86Instruction* inst){
    postcursorInstructions.append(inst);
    return postcursorInstructions.size();
}


int compareInstFuncBaseAddress(const void* arg1, const void* arg2){
    InstrumentationPoint* ip1 = *((InstrumentationPoint**)arg1);
    InstrumentationPoint* ip2 = *((InstrumentationPoint**)arg2);

    if (ip1->getSourceObject()->getContainer()->getBaseAddress() <
        ip2->getSourceObject()->getContainer()->getBaseAddress()){
        return -1;
    } else if (ip1->getSourceObject()->getContainer()->getBaseAddress() > 
               ip2->getSourceObject()->getContainer()->getBaseAddress()){
        return 1;
    } else {
        return compareInstBaseAddress(arg1, arg2);
    }
    __SHOULD_NOT_ARRIVE;
    return 0;
}


int compareInstBaseAddress(const void* arg1,const void* arg2){
    InstrumentationPoint* ip1 = *((InstrumentationPoint**)arg1);
    InstrumentationPoint* ip2 = *((InstrumentationPoint**)arg2);

    // first compare by base address
    if (ip1->getInstBaseAddress() < ip2->getInstBaseAddress()){
        return -1;
    } else if(ip1->getInstBaseAddress() > ip2->getInstBaseAddress()){
        return 1;
    } else {
        // then see if they are the same instruction
        if (ip1->getSourceObject() < ip2->getSourceObject()){
            return -1;
        } else if (ip1->getSourceObject() > ip2->getSourceObject()){
            return 1;
        } else {
            PRINT_DEBUG_POINT_CHAIN("Comparing priority of 2 points at %#llx: %d %d", ip1->getInstBaseAddress(), ip1->getPriority(), ip2->getPriority());
            // then compare by where the inst is happening relcative to the instruction
            if (ip1->getInstLocation() < ip2->getInstLocation()){
                return -1;
            } else if (ip1->getInstLocation() > ip2->getInstLocation()){
                return 1;
            } else {
                // then compare by priority
                if (ip1->getPriority() < ip2->getPriority()){
                    return -1;
                } else if (ip1->getPriority() > ip2->getPriority()){
                    return 1;
                }
            }
        }
    }
    return 0;
}

int compareInstSourceAddress(const void* arg1,const void* arg2){
    InstrumentationPoint* ip1 = *((InstrumentationPoint**)arg1);
    InstrumentationPoint* ip2 = *((InstrumentationPoint**)arg2);

    if(ip1->getInstSourceAddress() < ip2->getInstSourceAddress()){
        return -1;
    } else if(ip1->getInstSourceAddress() > ip2->getInstSourceAddress()){
        return 1;
    } else {
        PRINT_DEBUG_POINT_CHAIN("Comparing priority of 2 points at %#llx: %d %d", ip1->getInstSourceAddress(), ip1->getPriority(), ip2->getPriority());
        if (ip1->getPriority() < ip2->getPriority()){
            return -1;
        } else if (ip1->getPriority() > ip2->getPriority()){
            return 1;
        }
    }
    return 0;
}

uint32_t InstrumentationPoint64::generateTrampoline(Vector<X86Instruction*>* insts, uint64_t textBaseAddress, uint64_t offset, 
                                                    uint64_t returnOffset, bool doReloc, uint64_t regStorageBase, uint64_t currentOffset){
    ASSERT(!trampolineInstructions.size() && "Cannot generate trampoline instructions more than once");

    trampolineOffset = offset;

    uint32_t trampolineSize = 0;

    FlagsProtectionMethods protectionMethod = getFlagsProtectionMethod();

    bool stackIsSafe = true;
    TextObject* to = point->getContainer();
    ASSERT(to->getType() == PebilClassType_Function);
    Function* f = (Function*)to;
    BasicBlock* bb = f->getBasicBlockAtAddress(point->getBaseAddress());
    if (f->hasLeafOptimization() || bb->isEntry()){
        stackIsSafe = false;
    }
    if (instrumentation->getType() == PebilClassType_InstrumentationFunction &&
        ((InstrumentationFunction*)instrumentation)->hasSkipWrapper()){
        stackIsSafe = true;
    } 

    bool protectStack = false;
    if (protectionMethod != FlagsProtectionMethod_none || !stackIsSafe){
        protectStack = true;
    }

    BitSet<uint32_t>* protectRegs = getProtectedRegisters();
    uint32_t countProt = 0;
    for (uint32_t i = 0; i < X86_64BIT_GPRS; i++){
        if (i == X86_REG_SP){
            continue;
        }
        if (protectRegs->contains(i)){
            countProt++;
        }
    }
    if (countProt > 0){
        protectStack = true;
    }

    if (protectStack){
        trampolineInstructions.append(X86InstructionFactory64::emitLoadRegImmReg(X86_REG_SP, -1*Size__trampoline_autoinc, X86_REG_SP));
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }

    for (uint32_t i = 0; i < X86_64BIT_GPRS; i++){
        if (protectRegs->contains(i)){
            trampolineInstructions.append(X86InstructionFactory64::emitStackPush(i));
            trampolineSize += trampolineInstructions.back()->getSizeInBytes();
        }
    }

    if (protectionMethod == FlagsProtectionMethod_full){
        trampolineInstructions.append(X86InstructionFactory::emitPushEflags());
        trampolineSize += trampolineInstructions.back()->getSizeInBytes(); 
    } else if (protectionMethod == FlagsProtectionMethod_light){
        trampolineInstructions.append(X86InstructionFactory64::emitStackPush(X86_REG_AX));
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();        

        trampolineInstructions.append(X86InstructionFactory64::emitLoadAHFromFlags());
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

        trampolineInstructions.append(X86InstructionFactory::emitCallRelative(offset + trampolineSize, getTargetOffset()));
        trampolineInstructions.back()->setBaseAddress(textBaseAddress + currentOffset + trampolineSize);
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();

        if (instrumentation->getType() == PebilClassType_InstrumentationFunction && ((InstrumentationFunction*)instrumentation)->hasSkipWrapper()){
            ((InstrumentationFunction*)instrumentation)->addPLTHook(trampolineInstructions.back());            
        }
    }

    while (hasMorePostcursorInstructions()){
        trampolineInstructions.append(removeNextPostcursorInstruction());
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }

    // restore eflags
    if (protectionMethod == FlagsProtectionMethod_full){
        trampolineInstructions.append(X86InstructionFactory::emitPopEflags());
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    } else if (protectionMethod == FlagsProtectionMethod_light){
        trampolineInstructions.append(X86InstructionFactory64::emitStoreAHToFlags());
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();

        trampolineInstructions.append(X86InstructionFactory64::emitStackPop(X86_REG_AX));
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }

    for (uint32_t i = 0; i < X86_64BIT_GPRS; i++){
        uint32_t idx = X86_64BIT_GPRS - i - 1;
        if (protectRegs->contains(idx)){
            trampolineInstructions.append(X86InstructionFactory64::emitStackPop(idx));
            trampolineSize += trampolineInstructions.back()->getSizeInBytes();
        }
    }
    delete protectRegs;

    if (protectStack){
        trampolineInstructions.append(X86InstructionFactory64::emitLoadRegImmReg(X86_REG_SP, Size__trampoline_autoinc, X86_REG_SP));
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
            if (!(*insts)[i]->isNop()){
                trampolineInstructions.append((*insts)[i]);
                trampolineSize += trampolineInstructions.back()->getSizeInBytes();
            } else {
                delete (*insts)[i];
            }
        }
        
        ASSERT(numberOfBranches < 2 && "Cannot have multiple branches in a basic block");

        trampolineInstructions.append(X86InstructionFactory::emitJumpRelative(offset+trampolineSize,returnOffset));
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }
    return trampolineSize;
}

uint32_t InstrumentationPoint32::generateTrampoline(Vector<X86Instruction*>* insts, uint64_t textBaseAddress, uint64_t offset, 
                                                    uint64_t returnOffset, bool doReloc, uint64_t regStorageBase, uint64_t currentOffset){
    __FUNCTION_NOT_IMPLEMENTED;
    uint32_t trampolineSize = 0;
    return trampolineSize;
}


uint32_t InstrumentationSnippet::addSnippetInstruction(X86Instruction* inst){
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

void InstrumentationFunction::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset, uint64_t addr){
    uint32_t currentOffset = procedureLinkOffset;
    if (!isStaticLinked()){
        for (uint32_t i = 0; i < procedureLinkInstructions.size(); i++){
            procedureLinkInstructions[i]->setBaseAddress(addr + currentOffset); 
            procedureLinkInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
            currentOffset += procedureLinkInstructions[i]->getSizeInBytes();
        }
    }

    currentOffset = bootstrapOffset;
    for (uint32_t i = 0; i < bootstrapInstructions.size(); i++){
        bootstrapInstructions[i]->setBaseAddress(addr + currentOffset);
        bootstrapInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
        currentOffset += bootstrapInstructions[i]->getSizeInBytes();
    }

    currentOffset = wrapperOffset;
    for (uint32_t i = 0; i < wrapperInstructions.size(); i++){
        wrapperInstructions[i]->setBaseAddress(addr + currentOffset);
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
    procedureLinkInstructions.append(X86InstructionFactory64::emitIndirectRelativeJump(procedureLinkAddress,dataBaseAddress + globalDataOffset));
    if (pltHooks.size()){
        ASSERT(skipWrapper);
        procedureLinkInstructions.back()->setBaseAddress(procedureLinkAddress);
	for (uint32_t i = 0; i < pltHooks.size(); i++){
	    pltHooks[i]->initializeAnchor(procedureLinkInstructions.back());
	}
    }

    procedureLinkInstructions.append(X86InstructionFactory64::emitStackPushImm(relocationOffset));
    uint32_t returnAddress = procedureLinkAddress + procedureLinkSize();
    procedureLinkInstructions.append(X86InstructionFactory64::emitJumpRelative(returnAddress,realPLTAddress));

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

    uint64_t procedureLinkAddress = textBaseAddress + procedureLinkOffset;
    procedureLinkInstructions.append(X86InstructionFactory32::emitJumpIndirect(dataBaseAddress + globalDataOffset));
    if (pltHooks.size()){
        ASSERT(skipWrapper);
        procedureLinkInstructions.back()->setBaseAddress(procedureLinkAddress);
	for (uint32_t i = 0; i < pltHooks.size(); i++){
	    pltHooks[i]->initializeAnchor(procedureLinkInstructions.back());
	}
    }

    procedureLinkInstructions.append(X86InstructionFactory32::emitStackPushImm(relocationOffset));
    uint32_t returnAddress = textBaseAddress + procedureLinkOffset + procedureLinkSize();
    procedureLinkInstructions.append(X86InstructionFactory32::emitJumpRelative(returnAddress,realPLTAddress));

    ASSERT(procedureLinkSize() == procedureLinkReservedSize());

    return procedureLinkInstructions.size();
}

uint32_t InstrumentationFunction64::generateBootstrapInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress){
    PRINT_DEBUG_DATA_PLACEMENT("Inst function global offset data is at %#llx", dataBaseAddress + globalDataOffset);
    bootstrapInstructions.append(X86InstructionFactory64::emitMoveImmToReg(globalData, X86_REG_CX));
    bootstrapInstructions.append(X86InstructionFactory64::emitMoveImmToReg(dataBaseAddress + globalDataOffset, X86_REG_DX));
    bootstrapInstructions.append(X86InstructionFactory64::emitMoveRegToRegaddr(X86_REG_CX, X86_REG_DX));

    uint32_t nopBytes = bootstrapReservedSize() - bootstrapSize();
    Vector<X86Instruction*>* nops = X86InstructionFactory64::emitNopSeries(nopBytes);
    while ((*nops).size()){
        bootstrapInstructions.append((*nops).remove(0));
    }
    delete nops;
    ASSERT(bootstrapSize() == bootstrapReservedSize());
    return bootstrapInstructions.size();
}

uint32_t InstrumentationFunction32::generateBootstrapInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress){
    bootstrapInstructions.append(X86InstructionFactory32::emitMoveImmToReg(globalData,X86_REG_CX));
    bootstrapInstructions.append(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX,dataBaseAddress + globalDataOffset));

    uint32_t nopBytes = bootstrapReservedSize() - bootstrapSize();
    Vector<X86Instruction*>* nops = X86InstructionFactory64::emitNopSeries(nopBytes);
    while ((*nops).size()){
        bootstrapInstructions.append((*nops).remove(0));
    }
    delete nops;
    ASSERT(bootstrapSize() == bootstrapReservedSize());
    return bootstrapInstructions.size();
}

uint32_t InstrumentationFunction64::generateWrapperInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t fxStorageOffset, ElfFileInst* elfInst){
    ASSERT(!wrapperInstructions.size() && "This array should be empty");

    wrapperInstructions.append(X86InstructionFactory64::emitLoadRegImmReg(X86_REG_SP, -1*Size__trampoline_autoinc, X86_REG_SP));

    if (assumeFlagsUnsafe){
        wrapperInstructions.append(X86InstructionFactory64::emitPushEflags());
    }

    for (uint32_t i = 0; i < X86_64BIT_GPRS; i++){
        wrapperInstructions.append(X86InstructionFactory64::emitStackPush(i));
    }
    uint64_t fxStor = nextAlignAddress(fxStorageOffset + sizeof(uint64_t), 16);

    if (assumeFunctionFP){
        wrapperInstructions.append(linkInstructionToData(X86InstructionFactory64::emitFxSave(0), elfInst, fxStor, true));
    }
    ASSERT(arguments.size() <= Num__64_bit_StackArgs && "More arguments must be pushed onto stack, which is not yet implemented"); 
    
    for (uint32_t i = 0; i < arguments.size(); i++){
        uint32_t idx = arguments.size() - i - 1;
        
        if (i <= Num__64_bit_StackArgs){
            uint32_t argumentRegister = map64BitArgToReg(idx);            
            wrapperInstructions.append(linkInstructionToData(X86InstructionFactory64::emitLoadRipImmReg(0, argumentRegister), elfInst, arguments[idx].offset, true));
            //wrapperInstructions.append(X86InstructionFactory64::emitMoveImmToReg(dataBaseAddress + arguments[idx].offset, argumentRegister));
        } else {
            PRINT_ERROR("64Bit instrumentation supports only %d args currently", Num__64_bit_StackArgs);
        }
    }
    
    uint64_t wrapperTargetOffset = 0;
    if (isStaticLinked()){
        wrapperTargetOffset = functionEntry - textBaseAddress;
        if (textBaseAddress > functionEntry){
            wrapperTargetOffset = (uint32_t)wrapperTargetOffset;
        }
    } else {
        wrapperTargetOffset = procedureLinkOffset;
    }
    
    // align the stack
    wrapperInstructions.append(linkInstructionToData(X86InstructionFactory64::emitLoadRipImmReg(0, X86_REG_R14), elfInst, fxStorageOffset, true));
    wrapperInstructions.append(X86InstructionFactory64::emitMoveRegToRegaddr(X86_REG_SP, X86_REG_R14));
    wrapperInstructions.append(X86InstructionFactory64::emitLoadRegImmReg(X86_REG_SP, -1*Size__trampoline_stackalign, X86_REG_SP));
    wrapperInstructions.append(X86InstructionFactory64::emitMoveImmToReg((uint32_t)~(Size__trampoline_stackalign - 1), X86_REG_R15));
    wrapperInstructions.append(X86InstructionFactory64::emitRegAndReg(X86_REG_SP, X86_REG_R15));

    wrapperInstructions.append(X86InstructionFactory64::emitCallRelative(wrapperOffset + wrapperSize(), wrapperTargetOffset));

    wrapperInstructions.append(linkInstructionToData(X86InstructionFactory64::emitLoadRipImmReg(0, X86_REG_R14), elfInst, fxStorageOffset, true));
    wrapperInstructions.append(X86InstructionFactory64::emitMoveRegaddrToReg(X86_REG_R14, X86_REG_SP));

    if (assumeFunctionFP){
        wrapperInstructions.append(linkInstructionToData(X86InstructionFactory64::emitFxRstor(0), elfInst, fxStor, true));
    }
    for (uint32_t i = 0; i < X86_64BIT_GPRS; i++){
        wrapperInstructions.append(X86InstructionFactory64::emitStackPop(X86_64BIT_GPRS-1-i));
    }
    if (assumeFlagsUnsafe){
        wrapperInstructions.append(X86InstructionFactory64::emitPopEflags());
    }
    
    wrapperInstructions.append(X86InstructionFactory64::emitLoadRegImmReg(X86_REG_SP, Size__trampoline_autoinc, X86_REG_SP));
    wrapperInstructions.append(X86InstructionFactory64::emitReturn());
    
    uint32_t nopBytes = wrapperReservedSize() - wrapperSize();
    Vector<X86Instruction*>* nops = X86InstructionFactory64::emitNopSeries(nopBytes);
    while ((*nops).size()){
        wrapperInstructions.append((*nops).remove(0));
    }
    delete nops;
    ASSERT(wrapperSize() == wrapperReservedSize());

    return wrapperInstructions.size();
}

uint32_t InstrumentationFunction32::generateWrapperInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t fxStorageOffset, ElfFileInst* elfInst){
    ASSERT(!wrapperInstructions.size() && "This array should be empty");

    if (!skipWrapper){
        if (assumeFlagsUnsafe){
            wrapperInstructions.append(X86InstructionFactory32::emitPushEflags());
        }
        for (uint32_t i = 0; i < X86_32BIT_GPRS; i++){
            wrapperInstructions.append(X86InstructionFactory32::emitStackPush(i));
        }
        
        if (assumeFunctionFP){
            wrapperInstructions.append(X86InstructionFactory32::emitFxSave(fxStorageOffset));
        }
        
        for (uint32_t i = 0; i < arguments.size(); i++){
            uint32_t idx = arguments.size() - i - 1;
            
            // everything is passed on the stack
            wrapperInstructions.append(X86InstructionFactory32::emitMoveImmToReg(dataBaseAddress + arguments[idx].offset, X86_REG_DX));
            wrapperInstructions.append(X86InstructionFactory32::emitStackPush(X86_REG_DX));
        }
    }

    // keep the stack aligned
    wrapperInstructions.append(X86InstructionFactory32::emitCallRelative(wrapperOffset + wrapperSize(), procedureLinkOffset));

    if (!skipWrapper){
        if (assumeFunctionFP){
            wrapperInstructions.append(X86InstructionFactory32::emitFxRstor(dataBaseAddress + fxStorageOffset));
        }
        for (uint32_t i = 0; i < arguments.size(); i++){
            wrapperInstructions.append(X86InstructionFactory32::emitStackPop(X86_REG_CX));
        }
        
        for (uint32_t i = 0; i < X86_32BIT_GPRS; i++){
            wrapperInstructions.append(X86InstructionFactory32::emitStackPop(X86_32BIT_GPRS-1-i));
        }
        if (assumeFlagsUnsafe){
            wrapperInstructions.append(X86InstructionFactory32::emitPopEflags());
        }
    }

    wrapperInstructions.append(X86InstructionFactory32::emitReturn());

    uint32_t nopBytes = wrapperReservedSize() - wrapperSize();
    Vector<X86Instruction*>* nops = X86InstructionFactory64::emitNopSeries(nopBytes);
    while ((*nops).size()){
        wrapperInstructions.append((*nops).remove(0));
    }
    delete nops;
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
    skipWrapper = false;

    assumeFunctionFP = true;
    assumeFlagsUnsafe = true;
}

void InstrumentationFunction::assumeNoFunctionFP(){
    assumeFunctionFP = false;
}

void InstrumentationFunction::assumeNoFlagsUnsafe(){
    assumeFlagsUnsafe = false;
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
        snippetInstructions.append(X86InstructionFactory::emitReturn());
    }
    return snippetInstructions.size();
}

void InstrumentationSnippet::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset, uint64_t addr){
    uint32_t currentOffset = bootstrapOffset;
    for (uint32_t i = 0; i < bootstrapInstructions.size(); i++){
        bootstrapInstructions[i]->setBaseAddress(addr + currentOffset);
        bootstrapInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
        currentOffset += bootstrapInstructions[i]->getSizeInBytes();
    }

    currentOffset = snippetOffset;
    for (uint32_t i = 0; i < snippetInstructions.size(); i++){
        snippetInstructions[i]->setBaseAddress(addr + currentOffset);
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
    canOverflow = true;
}

InstrumentationSnippet::~InstrumentationSnippet(){
    for (uint32_t i = 0; i < snippetInstructions.size(); i++){
        delete snippetInstructions[i];
    }
}

Vector<X86Instruction*>* InstrumentationPoint::swapInstructionsAtPoint(Vector<X86Instruction*>* replacements){
    X86Instruction* instruction = (X86Instruction*)point;
    ASSERT(instruction->getContainer() && instruction->getContainer()->getType() == PebilClassType_Function);
    Function* func = (Function*)instruction->getContainer();

    return func->swapInstructions(getInstSourceAddress(), replacements);
}

BitSet<uint32_t>* getProtectedRegs(InstLocations loc, X86Instruction* xins, Vector<X86Instruction*>* insert){
    BitSet<uint32_t>* n = new BitSet<uint32_t>(X86_ALU_REGS);

    //xins->print();
    for (uint32_t i = 0; i < insert->size(); i++){
        X86Instruction* ins = (*insert)[i];
        //ins->print();
        RegisterSet* defs = ins->getRegistersDefined();
        for (uint32_t j = 0; j < X86_ALU_REGS; j++){
            if (loc == InstLocation_prior && !xins->isRegDeadIn(j) && defs->containsRegister(j)){
                n->insert(j);
            }
            if (loc == InstLocation_after && !xins->isRegDeadOut(j) && defs->containsRegister(j)){
                n->insert(j);
            }
        }
        delete defs;
    }
    //n->print();

    return n;
}

BitSet<uint32_t>* InstrumentationPoint::getProtectedRegisters(){
    Vector<X86Instruction*>* insns = new Vector<X86Instruction*>();
    for (uint32_t i = 0; i < countPrecursorInstructions(); i++){
        insns->append(getPrecursorInstruction(i));
    }
    if (instrumentation->getType() == PebilClassType_InstrumentationSnippet){
        for (uint32_t i = 0; i < instrumentation->getNumberOfCoreInstructions(); i++){
            insns->append(instrumentation->getCoreInstruction(i));
        }
    }
    for (uint32_t i = 0; i < countPostcursorInstructions(); i++){
        insns->append(getPostcursorInstruction(i));
    }

    BitSet<uint32_t>* p = getProtectedRegs(getInstLocation(), point, insns);
    delete insns;

    return p;
}

FlagsProtectionMethods getFlagsMethod(InstLocations loc, X86Instruction* xins, Vector<X86Instruction*>* insert, bool canOverflow){
    BitSet<uint32_t>* liveSet = new BitSet<uint32_t>(X86_FLAG_BITS);

    // figure out which flags are live at the instrumentation point
    for (uint32_t i = 0; i < X86_FLAG_BITS; i++){
        if (loc == InstLocation_prior){
            if (!xins->isFlagDeadIn(i)){
                liveSet->insert(i);
            }
        } else if (loc == InstLocation_after){
            if (!xins->isFlagDeadOut(i)){
                liveSet->insert(i);
            }
        } else {
            ASSERT(false);
        }
    }

    if (liveSet->empty()){
        delete liveSet;
        return FlagsProtectionMethod_none;
    }

    // figure out which flags are defined by the snippet
    BitSet<uint32_t>* flagsSquashed = new BitSet<uint32_t>(X86_FLAG_BITS);
    for (uint32_t i = 0; i < insert->size(); i++){
        X86Instruction* s = (*insert)[i];
        BitSet<uint32_t>* def = s->getFlagsDefined();
        for (uint32_t j = 0; j < X86_FLAG_BITS; j++){
            if (def->contains(j)){
                flagsSquashed->insert(j);
            }
        }

        delete def;
    }

    // unset overflow bit if instructed to ignore it
    if (canOverflow == false){
        flagsSquashed->remove(X86_FLAG_OF);
    }

    // figure out where live and squashed bits intersect
    BitSet<uint32_t>* protectBits = new BitSet<uint32_t>(X86_FLAG_BITS);
    for (uint32_t i = 0; i < X86_FLAG_BITS; i++){
        if (liveSet->contains(i) && flagsSquashed->contains(i)){
            protectBits->insert(i);
        }
    }    

    // use the weakest allowable protection method based on previous analysis
    FlagsProtectionMethods prot = FlagsProtectionMethod_none;
    for (uint32_t i = 0; i < X86_FLAG_BITS; i++){
        if (protectBits->contains(i)){
            if (CONTAINS_FLAG(__flag_mask__protect_light, i)){
                prot = FlagsProtectionMethod_light;
            } else if (CONTAINS_FLAG(__flag_mask__protect_full, i)){
                prot = FlagsProtectionMethod_full;
                break;
            }
        }
    }

    /*
    xins->print();
    liveSet->print();
    flagsSquashed->print();
    protectBits->print();
    */

    delete flagsSquashed;
    delete liveSet;
    delete protectBits;

    return prot;
}

FlagsProtectionMethods InstrumentationPoint::getFlagsProtectionMethod(){
    if (protectionMethod != FlagsProtectionMethod_undefined){
        return protectionMethod;
    }

    Vector<X86Instruction*>* insns = new Vector<X86Instruction*>();
    for (uint32_t i = 0; i < countPrecursorInstructions(); i++){
        insns->append(getPrecursorInstruction(i));
    }
    if (instrumentation->getType() == PebilClassType_InstrumentationSnippet){
        for (uint32_t i = 0; i < instrumentation->getNumberOfCoreInstructions(); i++){
            insns->append(instrumentation->getCoreInstruction(i));
        }
    }
    for (uint32_t i = 0; i < countPostcursorInstructions(); i++){
        insns->append(getPostcursorInstruction(i));
    }

    protectionMethod = getFlagsMethod(getInstLocation(), point, insns, instrumentation->canOverflow);
    delete insns;

    return protectionMethod;
}

void InstrumentationPoint::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset, uint64_t addr){
    uint32_t currentOffset = trampolineOffset;
    for (uint32_t i = 0; i < trampolineInstructions.size(); i++){
        trampolineInstructions[i]->setBaseAddress(addr + currentOffset);
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

InstrumentationPoint::InstrumentationPoint(Base* pt, Instrumentation* inst, InstrumentationModes instMode, InstLocations loc)
    : Base(PebilClassType_InstrumentationPoint)
{

    if (pt->getType() == PebilClassType_X86Instruction){
        point = (X86Instruction*)pt;
    } else if (pt->getType() == PebilClassType_BasicBlock){
        ASSERT(((BasicBlock*)pt)->getLeader());
        point = (X86Instruction*)((BasicBlock*)pt)->getLeader();
    } else if (pt->getType() == PebilClassType_Function){
        Function* f = (Function*)pt;
        BasicBlock* bb = f->getBasicBlockAtAddress(f->getBaseAddress());
        ASSERT(bb);
        ASSERT(bb->getLeader());
        point = bb->getLeader();
    } else {
        PRINT_INFOR("Instrumentation point type not allowed %d", pt->getType());
        __SHOULD_NOT_ARRIVE;
    }
    ASSERT(point);

    instrumentation = inst;
    instrumentation->setInstrumentationPoint(this);

    instrumentationMode = instMode;
    protectionMethod = FlagsProtectionMethod_undefined;
    deadRegs = new BitSet<uint32_t>(X86_ALU_REGS);

    instLocation = loc;
    trampolineOffset = 0;
    priority = InstPriority_regular;
    offsetFromPoint = 0;

    verify();
}

void InstrumentationPoint32::insertStateProtection(){
    __FUNCTION_NOT_IMPLEMENTED;
}

InstrumentationPoint32::InstrumentationPoint32(Base* pt, Instrumentation* inst, InstrumentationModes instMode, InstLocations loc) :
    InstrumentationPoint(pt, inst, instMode, loc)
{
    numberOfBytes = 0;
}

void InstrumentationPoint64::insertStateProtection(){
    // state protection added to the actual contents of the instrumentation
    if (instrumentationMode == InstrumentationMode_inline){
        ASSERT(instrumentation->getType() == PebilClassType_InstrumentationSnippet);

        FlagsProtectionMethods protectionMethod = getFlagsProtectionMethod();
        BitSet<uint32_t>* protectedRegs = getProtectedRegisters();

        uint32_t countProt = 0;
        for (uint32_t i = 0; i < X86_64BIT_GPRS; i++){
            if (i == X86_REG_SP){
                continue;
            }
            if (protectedRegs->contains(i)){
                instrumentation->prependCoreInstruction(X86InstructionFactory64::emitStackPush(i));
                instrumentation->appendCoreInstruction(X86InstructionFactory64::emitStackPop(i));
                countProt++;
            }
        }
        delete protectedRegs;

        // then add the number of bytes needed for state protection
        if (protectionMethod == FlagsProtectionMethod_full){
            instrumentation->prependCoreInstruction(X86InstructionFactory::emitPushEflags());
            instrumentation->appendCoreInstruction(X86InstructionFactory::emitPopEflags());
            countProt++;
        } else if (protectionMethod == FlagsProtectionMethod_light){
            instrumentation->prependCoreInstruction(X86InstructionFactory64::emitLoadAHFromFlags());
            instrumentation->prependCoreInstruction(X86InstructionFactory64::emitStackPush(X86_REG_AX));

            instrumentation->appendCoreInstruction(X86InstructionFactory64::emitStoreAHToFlags());
            instrumentation->appendCoreInstruction(X86InstructionFactory64::emitStackPop(X86_REG_AX));

            countProt++;
        } else if (protectionMethod == FlagsProtectionMethod_none){
        } else {
            PRINT_ERROR("Protection method is invalid");
        }

        bool protectStack = false;
        TextObject* to = point->getContainer();
        ASSERT(to->getType() == PebilClassType_Function);
        Function* f = (Function*)to;
        BasicBlock* bb = f->getBasicBlockAtAddress(point->getBaseAddress());

        if (!f || !bb){
            protectStack = true;
        } else {
            if (f->hasLeafOptimization() || bb->isEntry()){
                protectStack = true;
            }
        }
        if (protectStack && countProt > 0){
            instrumentation->prependCoreInstruction(X86InstructionFactory64::emitLoadRegImmReg(X86_REG_SP, -1*Size__trampoline_autoinc, X86_REG_SP));
            instrumentation->appendCoreInstruction(X86InstructionFactory64::emitLoadRegImmReg(X86_REG_SP, Size__trampoline_autoinc, X86_REG_SP));
        }

        // count the number of bytes the tool wants
        if (instrumentation->getType() == PebilClassType_InstrumentationSnippet){
            InstrumentationSnippet* snippet = (InstrumentationSnippet*)instrumentation;
            for (uint32_t i = 0; i < snippet->getNumberOfCoreInstructions(); i++){
                numberOfBytes += snippet->getCoreInstruction(i)->getSizeInBytes();
            }
        } else {
            __SHOULD_NOT_ARRIVE;
        }

    }
    // state protection put in the wrapper during generateWrapper
    else {
        numberOfBytes = Size__uncond_jump;
    }
    ASSERT(numberOfBytes);
}

InstrumentationPoint64::InstrumentationPoint64(Base* pt, Instrumentation* inst, InstrumentationModes instMode, InstLocations loc) :
    InstrumentationPoint(pt, inst, instMode, loc)
{
    numberOfBytes = 0;
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
    /*
    if (instrumentationMode == InstrumentationMode_inline && instrumentation->getType() != PebilClassType_InstrumentationSnippet){
        PRINT_ERROR("InstrumentationMode_inline can only be used with snippets");
        return false;
    }
    */

    return true;
}

InstrumentationPoint::~InstrumentationPoint(){
    for (uint32_t i = 0; i < trampolineInstructions.size(); i++){
        delete trampolineInstructions[i];
    }
    if (deadRegs){
        delete deadRegs;
    }
}

