#include <Base.h>
#include <BinaryFile.h>
#include <ElfFile.h>
#include <Function.h>
#include <Instruction.h>
#include <SectionHeader.h>
#include <TextSection.h>
#include <Udis.h>

bool UD_INSTRUCTION_CLASS::usesControlTarget(){
    if (isConditionalBranch() ||
        isUnconditionalBranch() ||
        isFunctionCall() ||
        isSystemCall()){
        return true;
    }
    return false;
}

void UD_INSTRUCTION_CLASS::initializeAnchor(Base* link){
    if (addressAnchor){
        print();
    }
    ASSERT(!addressAnchor);
    ASSERT(link->containsProgramBits());
    addressAnchor = new AddressAnchor(link,this);
}


void UD_INSTRUCTION_CLASS::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    ASSERT(sizeInBytes && "This instruction has no bytes thus it cannot be dumped");

    for (uint32_t i = 0; i < sizeInBytes; i++){
        char byt = mapCharsToByte(GET(insn_hexcode)[2*i], GET(insn_hexcode)[2*i+1]);
        binaryOutputFile->copyBytes(&byt, 1, offset + i);
    }

    // the anchor will now overwrite any original instruction bytes that relate to relative addresses
    if (addressAnchor){
        addressAnchor->dump(binaryOutputFile,offset);
    }
}

void UD_INSTRUCTION_CLASS::computeJumpTableTargets(uint64_t tableBase, Function* func, Vector<uint64_t>* addressList){
    ASSERT(isJumpTableBase() && "Cannot compute jump table targets for this instruction");
    ASSERT(func);
    ASSERT(addressList);

    RawSection* dataSection = textSection->getElfFile()->findDataSectionAtAddr(tableBase);
    if (!dataSection){
        print();
        PRINT_ERROR("Cannot find table base %#llx for this instruction", tableBase);
    }
    ASSERT(dataSection);
    if (!dataSection->getSectionHeader()->hasBitsInFile()){
        dataSection->getSectionHeader()->print();
        return;
    }
    ASSERT(dataSection->getSectionHeader()->hasBitsInFile());

    // read the first location to decide what type of info is stored in the jump table
    uint64_t rawData;
    if (textSection->getElfFile()->is64Bit()){
        rawData = getUInt64(dataSection->getStreamAtAddress(tableBase));
    } else {
        rawData = (uint64_t)getUInt32(dataSection->getStreamAtAddress(tableBase));
    }

    bool directMode;
    // the data found is an address
    if (func->inRange(rawData)){
        directMode = true;

        PRINT_DEBUG_JUMP_TABLE("\tJumpMode for table base %#llx -- Direct", tableBase);
    }
    // the data found is an address offset
    else if (func->inRange(rawData+baseAddress) || absoluteValue(rawData) < JUMP_TABLE_REACHES){
        directMode = false;
        PRINT_DEBUG_JUMP_TABLE("\tJumpMode for table base %#llx -- Indirect", tableBase);
    }
    // the data found is neither of the above -- we interpret this to mean that it is instructions
    else {
        (*addressList).append(tableBase);
        PRINT_DEBUG_JUMP_TABLE("\tJumpMode for table base %#llx -- Instructions", tableBase);
        return;
    }


    uint32_t currByte = 0;
    uint32_t dataLen;
    if (textSection->getElfFile()->is64Bit()){
        dataLen = sizeof(uint64_t);
    } else {
        dataLen = sizeof(uint32_t);
    }
    
    do {
        if (textSection->getElfFile()->is64Bit()){
            rawData = getUInt64(dataSection->getStreamAtAddress(tableBase+currByte));
        } else {
            rawData = (uint64_t)getUInt32(dataSection->getStreamAtAddress(tableBase+currByte));
        }
        currByte += dataLen;
        
        if (!directMode){
            rawData += baseAddress;
        }
        PRINT_DEBUG_JUMP_TABLE("Jump Table target %#llx", rawData);
        (*addressList).append(rawData);
    } while (func->inRange((*addressList).back()) &&
             (tableBase+currByte)-dataSection->getSectionHeader()->GET(sh_addr) < dataSection->getSizeInBytes());
    (*addressList).remove((*addressList).size()-1);
}


uint64_t UD_INSTRUCTION_CLASS::findJumpTableBaseAddress(Vector<Instruction*>* functionInstructions){
    ASSERT(isJumpTableBase() && "Cannot compute jump table base for this instruction");

    uint64_t jumpOperand = operands[JUMP_TARGET_OPERAND]->getValue();
    PRINT_DEBUG_JUMP_TABLE("Finding jump table base address for instruction at %#llx", baseAddress);

    // jump target is a register
    if (jumpOperand < X86_64BIT_GPRS){
        if ((*functionInstructions).size()){
            Instruction** allInstructions = new Instruction*[(*functionInstructions).size()];
            for (uint32_t i = 0; i < (*functionInstructions).size(); i++){
                allInstructions[i] = (*functionInstructions)[i];
            }
            qsort(allInstructions,(*functionInstructions).size(),sizeof(Instruction*),compareBaseAddress);

            // search backwards through instructions to find jump table base
            uint64_t prevAddr = baseAddress-1;
            void* prev = NULL;
            do {
                PRINT_DEBUG_JUMP_TABLE("\tTrying Jump base address %#llx", prevAddr);
                prev = bsearch(&prevAddr,allInstructions,(*functionInstructions).size(),sizeof(Instruction*),searchBaseAddress);
                if (prev){
                    Instruction* previousInstruction = *(Instruction**)prev;
                    bool jumpOpFound = false;
                    uint64_t immediate = 0;
                    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
                        Operand* op = previousInstruction->getOperand(i);
                        if (op){
                            if (op->getType() && op->getValue() == jumpOperand){
                                jumpOpFound = true;
                            }
                            if (op->getType() && op->getValue() >= X86_64BIT_GPRS){
                                immediate = op->getValue();
                            }
                            if (previousInstruction->usesRelativeAddress()){
                                immediate = previousInstruction->getRelativeValue() + previousInstruction->getBaseAddress() + previousInstruction->getSizeInBytes();
                            }
                        }
                    }
                    if (jumpOpFound && immediate){
                        delete[] allInstructions;
                        PRINT_DEBUG_JUMP_TABLE("\t\tFound jump table base at %#llx", immediate);
                        if (!textSection->getElfFile()->findDataSectionAtAddr(immediate)){
                            return 0;
                        }
                        return immediate;
                    }
                    prevAddr = previousInstruction->getBaseAddress()-1;
                }
            } while (prev);
            delete[] allInstructions;
        }
    }
    // jump target is a memory location
    else {
        if (!textSection->getElfFile()->findDataSectionAtAddr(operands[JUMP_TARGET_OPERAND]->getValue())){
            return 0;
        }
        return operands[JUMP_TARGET_OPERAND]->getValue();
    }
    return 0;
}

bool UD_INSTRUCTION_CLASS::isControl(){
    return  (isConditionalBranch() || isUnconditionalBranch() || isSystemCall() || isFunctionCall() || isReturn());
}


bool UD_INSTRUCTION_CLASS::usesIndirectAddress(){
    PRINT_WARN(10, "FUNCTION NOT COMPLETE");
    return false;
    /*
    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        if (operands[i]){
                return true;
        }
    }
    return false;
    */
}


bool UD_INSTRUCTION_CLASS::isJumpTableBase(){
    return (isUnconditionalBranch() && usesIndirectAddress());
}

uint32_t UD_INSTRUCTION_CLASS::getInstructionType(){
    uint32_t optype = X86InstructionType_unknown;
    switch(GET(mnemonic)){
        case UD_I3dnow:
            optype = X86InstructionType_special;
            break;
        case UD_Iaaa:
        case UD_Iaad:
        case UD_Iaam:
        case UD_Iaas:
        case UD_Iadc:
        case UD_Iadd:
            optype = X86InstructionType_int;
            break;
        case UD_Iaddpd:
        case UD_Iaddps:
        case UD_Iaddsd:
        case UD_Iaddss:
        case UD_Iaddsubpd:
        case UD_Iaddsubps:
            optype = X86InstructionType_float;
            break;
        case UD_Iand:
            optype = X86InstructionType_int;
            break;
        case UD_Iandpd:
        case UD_Iandps:
        case UD_Iandnpd:
        case UD_Iandnps:
            optype = X86InstructionType_float;
            break;
        case UD_Iarpl:
            optype = X86InstructionType_special;
            break;
        case UD_Imovsxd:
        case UD_Ibound:
        case UD_Ibsf:
        case UD_Ibsr:
        case UD_Ibswap:
        case UD_Ibt:
        case UD_Ibtc:
        case UD_Ibtr:
        case UD_Ibts:
            optype = X86InstructionType_int;
            break;
        case UD_Icall:
            optype = X86InstructionType_call;
            break;
        case UD_Icbw:
        case UD_Icwde:
        case UD_Icdqe:
        case UD_Iclc:
        case UD_Icld:
        case UD_Iclflush:
        case UD_Iclgi:
        case UD_Icli:
        case UD_Iclts:
        case UD_Icmc:
            optype = X86InstructionType_special;
            break;
        case UD_Icmovo:
        case UD_Icmovno:
        case UD_Icmovb:
        case UD_Icmovae:
        case UD_Icmovz:
        case UD_Icmovnz:
        case UD_Icmovbe:
        case UD_Icmova:
        case UD_Icmovs:
        case UD_Icmovns:
        case UD_Icmovp:
        case UD_Icmovnp:
        case UD_Icmovl:
        case UD_Icmovge:
        case UD_Icmovle:
        case UD_Icmovg:
        case UD_Icmp:
            optype = X86InstructionType_int;
            break;
        case UD_Icmppd:
        case UD_Icmpps:
            optype = X86InstructionType_float;
            break;
        case UD_Icmpsb:
        case UD_Icmpsw:
            optype = X86InstructionType_string;
            break;
        case UD_Icmpsd:
            optype = X86InstructionType_float;
            break;
        case UD_Icmpsq:
            optype = X86InstructionType_string;
            break;
        case UD_Icmpss:
            optype = X86InstructionType_float;
            break;
        case UD_Icmpxchg:
        case UD_Icmpxchg8b:
            optype = X86InstructionType_int;
            break;
        case UD_Icomisd:
        case UD_Icomiss:
            optype = X86InstructionType_float;
            break;
        case UD_Icpuid:
            optype = X86InstructionType_special;
            break;
        case UD_Icvtdq2pd:
        case UD_Icvtdq2ps:
        case UD_Icvtpd2dq:
        case UD_Icvtpd2pi:
        case UD_Icvtpd2ps:
        case UD_Icvtpi2ps:
        case UD_Icvtpi2pd:
        case UD_Icvtps2dq:
        case UD_Icvtps2pi:
        case UD_Icvtps2pd:
        case UD_Icvtsd2si:
        case UD_Icvtsd2ss:
        case UD_Icvtsi2ss:
        case UD_Icvtss2si:
        case UD_Icvtss2sd:
        case UD_Icvttpd2pi:
        case UD_Icvttpd2dq:
        case UD_Icvttps2dq:
        case UD_Icvttps2pi:
        case UD_Icvttsd2si:
        case UD_Icvtsi2sd:
        case UD_Icvttss2si:
            optype = X86InstructionType_float;
            break;
        case UD_Icwd:
        case UD_Icdq:
        case UD_Icqo:
            optype = X86InstructionType_special;
            break;
        case UD_Idaa:
        case UD_Idas:
        case UD_Idec:
        case UD_Idiv:
            optype = X86InstructionType_int;
            break;
        case UD_Idivpd:
        case UD_Idivps:
        case UD_Idivsd:
        case UD_Idivss:
            optype = X86InstructionType_float;
            break;
        case UD_Iemms:
        case UD_Ienter:
            optype = X86InstructionType_special;
            break;
        case UD_If2xm1:
        case UD_Ifabs:
        case UD_Ifadd:
        case UD_Ifaddp:
        case UD_Ifbld:
        case UD_Ifbstp:
        case UD_Ifchs:
        case UD_Ifclex:
        case UD_Ifcmovb:
        case UD_Ifcmove:
        case UD_Ifcmovbe:
        case UD_Ifcmovu:
        case UD_Ifcmovnb:
        case UD_Ifcmovne:
        case UD_Ifcmovnbe:
        case UD_Ifcmovnu:
        case UD_Ifucomi:
        case UD_Ifcom:
        case UD_Ifcom2:
        case UD_Ifcomp3:
        case UD_Ifcomi:
        case UD_Ifucomip:
        case UD_Ifcomip:
        case UD_Ifcomp:
        case UD_Ifcomp5:
        case UD_Ifcompp:
        case UD_Ifcos:
        case UD_Ifdecstp:
        case UD_Ifdiv:
        case UD_Ifdivp:
        case UD_Ifdivr:
        case UD_Ifdivrp:
        case UD_Ifemms:
        case UD_Iffree:
        case UD_Iffreep:
        case UD_Ificom:
        case UD_Ificomp:
        case UD_Ifild:
        case UD_Ifncstp:
        case UD_Ifninit:
        case UD_Ifiadd:
        case UD_Ifidivr:
        case UD_Ifidiv:
        case UD_Ifisub:
        case UD_Ifisubr:
        case UD_Ifist:
        case UD_Ifistp:
        case UD_Ifisttp:
        case UD_Ifld:
        case UD_Ifld1:
        case UD_Ifldl2t:
        case UD_Ifldl2e:
        case UD_Ifldlpi:
        case UD_Ifldlg2:
        case UD_Ifldln2:
        case UD_Ifldz:
            optype = X86InstructionType_float;
            break;
        case UD_Ifldcw:
        case UD_Ifldenv:
            optype = X86InstructionType_special;
            break;
        case UD_Ifmul:
        case UD_Ifmulp:
        case UD_Ifimul:
            optype = X86InstructionType_float;
            break;
        case UD_Ifnop:
            optype = X86InstructionType_nop;
            break;
        case UD_Ifpatan:
        case UD_Ifprem:
        case UD_Ifprem1:
        case UD_Ifptan:
        case UD_Ifrndint:
            optype = X86InstructionType_float;
            break;
        case UD_Ifrstor:
        case UD_Ifnsave:
            optype = X86InstructionType_special;
            break;
        case UD_Ifscale:
        case UD_Ifsin:
        case UD_Ifsincos:
        case UD_Ifsqrt:
        case UD_Ifstp:
        case UD_Ifstp1:
        case UD_Ifstp8:
        case UD_Ifstp9:
        case UD_Ifst:
            optype = X86InstructionType_float;
            break;
        case UD_Ifnstcw:
        case UD_Ifnstenv:
        case UD_Ifnstsw:
            optype = X86InstructionType_special;
            break;
        case UD_Ifsub:
        case UD_Ifsubp:
        case UD_Ifsubr:
        case UD_Ifsubrp:
        case UD_Iftst:
        case UD_Ifucom:
        case UD_Ifucomp:
        case UD_Ifucompp:
        case UD_Ifxam:
        case UD_Ifxch:
        case UD_Ifxch4:
        case UD_Ifxch7:
            optype = X86InstructionType_float;
            break;
        case UD_Ifxrstor:
        case UD_Ifxsave:
            optype = X86InstructionType_special;
            break;
        case UD_Ifpxtract:
        case UD_Ifyl2x:
        case UD_Ifyl2xp1:
        case UD_Ihaddpd:
        case UD_Ihaddps:
            optype = X86InstructionType_float;
            break;
        case UD_Ihlt:
            optype = X86InstructionType_halt;
            break;
        case UD_Ihsubpd:
        case UD_Ihsubps:
            optype = X86InstructionType_float;
            break;
        case UD_Iidiv:
            optype = X86InstructionType_int;
            break;
        case UD_Iin:
            optype = X86InstructionType_io;
            break;
        case UD_Iimul:
        case UD_Iinc:
            optype = X86InstructionType_int;
            break;
        case UD_Iinsb:
        case UD_Iinsw:
        case UD_Iinsd:
            optype = X86InstructionType_io;
            break;
        case UD_Iint1:
        case UD_Iint3:
        case UD_Iint:
        case UD_Iinto:
            optype = X86InstructionType_trap;
            break;
        case UD_Iinvd:
        case UD_Iinvlpg:
        case UD_Iinvlpga:
            optype = X86InstructionType_special;
            break;
        case UD_Iiretw:
        case UD_Iiretd:
        case UD_Iiretq:
            optype = X86InstructionType_return;
            break;
        case UD_Ijo:
        case UD_Ijno:
        case UD_Ijb:
        case UD_Ijae:
        case UD_Ijz:
        case UD_Ijnz:
        case UD_Ijbe:
        case UD_Ija:
        case UD_Ijs:
        case UD_Ijns:
        case UD_Ijp:
        case UD_Ijnp:
        case UD_Ijl:
        case UD_Ijge:
        case UD_Ijle:
        case UD_Ijg:
        case UD_Ijcxz:
        case UD_Ijecxz:
        case UD_Ijrcxz:
            optype = X86InstructionType_cond_branch;
            break;
        case UD_Ijmp:
            optype = X86InstructionType_uncond_branch;
            break;
        case UD_Ilahf:
            optype = X86InstructionType_int;
            break;
        case UD_Ilar:
            optype = X86InstructionType_special;
            break;
        case UD_Ilddqu:
            optype = X86InstructionType_int;
            break;
        case UD_Ildmxcsr:
            optype = X86InstructionType_special;
            break;
        case UD_Ilds:
        case UD_Ilea:
        case UD_Iles:
        case UD_Ilfs:
        case UD_Ilgs:
            optype = X86InstructionType_int;
            break;
        case UD_Ilidt:
            optype = X86InstructionType_special;
            break;
        case UD_Ilss:
            optype = X86InstructionType_int;
            break;
        case UD_Ileave:
        case UD_Ilfence:
        case UD_Ilgdt:
        case UD_Illdt:
        case UD_Ilmsw:
        case UD_Ilock:
            optype = X86InstructionType_special;
            break;
        case UD_Ilodsb:
        case UD_Ilodsw:
        case UD_Ilodsd:
        case UD_Ilodsq:
            optype = X86InstructionType_string;
            break;
        case UD_Iloopnz:
        case UD_Iloope:
        case UD_Iloop:
        case UD_Ilsl:
        case UD_Iltr:
            optype = X86InstructionType_special;
            break;
        case UD_Imaskmovq:
        case UD_Imaxpd:
        case UD_Imaxps:
        case UD_Imaxsd:
        case UD_Imaxss:
            optype = X86InstructionType_float;
            break;
        case UD_Imfence:
            optype = X86InstructionType_special;
            break;
        case UD_Iminpd:
        case UD_Iminps:
        case UD_Iminsd:
        case UD_Iminss:
            optype = X86InstructionType_float;
            break;
        case UD_Imonitor:
            optype = X86InstructionType_special;
            break;
        case UD_Imov:
            optype = X86InstructionType_int;
            break;
        case UD_Imovapd:
        case UD_Imovaps:
        case UD_Imovd:
        case UD_Imovddup:
        case UD_Imovdqa:
        case UD_Imovdqu:
        case UD_Imovdq2q:
        case UD_Imovhpd:
        case UD_Imovhps:
        case UD_Imovlhps:
        case UD_Imovlpd:
        case UD_Imovlps:
        case UD_Imovhlps:
        case UD_Imovmskpd:
        case UD_Imovmskps:
        case UD_Imovntdq:
        case UD_Imovnti:
        case UD_Imovntpd:
        case UD_Imovntps:
        case UD_Imovntq:
        case UD_Imovq:
        case UD_Imovqa:
        case UD_Imovq2dq:
        case UD_Imovsb:
        case UD_Imovsw:
        case UD_Imovsd:
        case UD_Imovsq:
        case UD_Imovsldup:
        case UD_Imovshdup:
        case UD_Imovss:
        case UD_Imovsx:
        case UD_Imovupd:
        case UD_Imovups:
            optype = X86InstructionType_float;
            break;
        case UD_Imovzx:
        case UD_Imul:
            optype = X86InstructionType_int;
            break;
        case UD_Imulpd:
        case UD_Imulps:
        case UD_Imulsd:
        case UD_Imulss:
            optype = X86InstructionType_float;
            break;
        case UD_Imwait:
            optype = X86InstructionType_special;
            break;
        case UD_Ineg:
            optype = X86InstructionType_int;
            break;
        case UD_Inop:
            optype = X86InstructionType_nop;
            break;
        case UD_Inot:
        case UD_Ior:
            optype = X86InstructionType_int;
            break;
        case UD_Iorpd:
        case UD_Iorps:
            optype = X86InstructionType_float;
            break;
        case UD_Iout:
        case UD_Ioutsb:
        case UD_Ioutsw:
        case UD_Ioutsd:
        case UD_Ioutsq:
            optype = X86InstructionType_io;
            break;
        case UD_Ipacksswb:
        case UD_Ipackssdw:
        case UD_Ipackuswb:
        case UD_Ipaddb:
        case UD_Ipaddw:
        case UD_Ipaddq:
        case UD_Ipaddsb:
        case UD_Ipaddsw:
        case UD_Ipaddusb:
        case UD_Ipaddusw:
        case UD_Ipand:
        case UD_Ipandn:
            optype = X86InstructionType_int;
            break;
        case UD_Ipause:
            optype = X86InstructionType_special;
            break;
        case UD_Ipavgb:
        case UD_Ipavgw:
        case UD_Ipcmpeqb:
        case UD_Ipcmpeqw:
        case UD_Ipcmpeqd:
        case UD_Ipcmpgtb:
        case UD_Ipcmpgtw:
        case UD_Ipcmpgtd:
        case UD_Ipextrw:
        case UD_Ipinsrw:
        case UD_Ipmaddwd:
        case UD_Ipmaxsw:
        case UD_Ipmaxub:
        case UD_Ipminsw:
        case UD_Ipminub:
        case UD_Ipmovmskb:
        case UD_Ipmulhuw:
        case UD_Ipmulhw:
        case UD_Ipmullw:
        case UD_Ipmuludq:
        case UD_Ipop:
            optype = X86InstructionType_int;
            break;
        case UD_Ipopa:
        case UD_Ipopad:
            optype = X86InstructionType_special;
            break;
        case UD_Ipopfw:
        case UD_Ipopfd:
        case UD_Ipopfq:
        case UD_Ipor:
            optype = X86InstructionType_int;
            break;
        case UD_Iprefetch:
        case UD_Iprefetchnta:
        case UD_Iprefetcht0:
        case UD_Iprefetcht1:
        case UD_Iprefetcht2:
            optype = X86InstructionType_prefetch;
            break;
        case UD_Ipsadbw:
        case UD_Ipshufd:
        case UD_Ipshufhw:
        case UD_Ipshuflw:
        case UD_Ipshufw:
        case UD_Ipslldq:
        case UD_Ipsllw:
        case UD_Ipslld:
        case UD_Ipsllq:
        case UD_Ipsraw:
        case UD_Ipsrad:
        case UD_Ipsrlw:
        case UD_Ipsrld:
        case UD_Ipsrlq:
        case UD_Ipsrldq:
        case UD_Ipsubb:
        case UD_Ipsubw:
        case UD_Ipsubd:
        case UD_Ipsubq:
        case UD_Ipsubsb:
        case UD_Ipsubsw:
        case UD_Ipsubusb:
        case UD_Ipsubusw:
        case UD_Ipunpckhbw:
        case UD_Ipunpckhwd:
        case UD_Ipunpckhdq:
        case UD_Ipunpckhqdq:
        case UD_Ipunpcklbw:
        case UD_Ipunpcklwd:
        case UD_Ipunpckldq:
        case UD_Ipunpcklqdq:
        case UD_Ipi2fw:
        case UD_Ipi2fd:
        case UD_Ipf2iw:
        case UD_Ipf2id:
        case UD_Ipfnacc:
        case UD_Ipfpnacc:
        case UD_Ipfcmpge:
        case UD_Ipfmin:
        case UD_Ipfrcp:
        case UD_Ipfrsqrt:
        case UD_Ipfsub:
        case UD_Ipfadd:
        case UD_Ipfcmpgt:
        case UD_Ipfmax:
        case UD_Ipfrcpit1:
        case UD_Ipfrspit1:
        case UD_Ipfsubr:
        case UD_Ipfacc:
        case UD_Ipfcmpeq:
        case UD_Ipfmul:
        case UD_Ipfrcpit2:
        case UD_Ipmulhrw:
        case UD_Ipswapd:
        case UD_Ipavgusb:
        case UD_Ipush:
            optype = X86InstructionType_int;
            break;
        case UD_Ipusha:
        case UD_Ipushad:
            optype = X86InstructionType_special;
            break;
        case UD_Ipushfw:
        case UD_Ipushfd:
        case UD_Ipushfq:
        case UD_Ipxor:
        case UD_Ircl:
        case UD_Ircr:
        case UD_Irol:
        case UD_Iror:
            optype = X86InstructionType_int;
            break;
        case UD_Ircpps:
        case UD_Ircpss:
            optype = X86InstructionType_float;
            break;
        case UD_Irdmsr:
            optype = X86InstructionType_int;
            break;
        case UD_Irdpmc:
        case UD_Irdtsc:
        case UD_Irdtscp:
            optype = X86InstructionType_hwcount;
            break;
        case UD_Irepne:
        case UD_Irep:
            optype = X86InstructionType_string;
            break;
        case UD_Iret:
        case UD_Iretf:
            optype = X86InstructionType_return;
            break;
        case UD_Irsm:
            optype = X86InstructionType_special;
            break;
        case UD_Irsqrtps:
        case UD_Irsqrtss:
            optype = X86InstructionType_float;
            break;
        case UD_Isahf:
        case UD_Isal:
        case UD_Isalc:
        case UD_Isar:
        case UD_Ishl:
        case UD_Ishr:
        case UD_Isbb:
            optype = X86InstructionType_int;
            break;
        case UD_Iscasb:
        case UD_Iscasw:
        case UD_Iscasd:
        case UD_Iscasq:
            optype = X86InstructionType_string;
            break;
        case UD_Iseto:
        case UD_Isetno:
        case UD_Isetb:
        case UD_Isetnb:
        case UD_Isetz:
        case UD_Isetnz:
        case UD_Isetbe:
        case UD_Iseta:
        case UD_Isets:
        case UD_Isetns:
        case UD_Isetp:
        case UD_Isetnp:
        case UD_Isetl:
        case UD_Isetge:
        case UD_Isetle:
        case UD_Isetg:
            optype = X86InstructionType_int;
            break;
        case UD_Isfence:
        case UD_Isgdt:
            optype = X86InstructionType_special;
            break;
        case UD_Ishld:
        case UD_Ishrd:
            optype = X86InstructionType_int;
            break;
        case UD_Ishufpd:
        case UD_Ishufps:
            optype = X86InstructionType_float;
            break;
        case UD_Isidt:
        case UD_Isldt:
        case UD_Ismsw:
            optype = X86InstructionType_special;
            break;
        case UD_Isqrtps:
        case UD_Isqrtpd:
        case UD_Isqrtsd:
        case UD_Isqrtss:
            optype = X86InstructionType_float;
            break;
        case UD_Istc:
        case UD_Istd:
        case UD_Istgi:
        case UD_Isti:
        case UD_Iskinit:
        case UD_Istmxcsr:
            optype = X86InstructionType_special;
            break;
        case UD_Istosb:
        case UD_Istosw:
        case UD_Istosd:
        case UD_Istosq:
            optype = X86InstructionType_string;
            break;
        case UD_Istr:
            optype = X86InstructionType_special;
            break;
        case UD_Isub:
            optype = X86InstructionType_int;
            break;
        case UD_Isubpd:
        case UD_Isubps:
        case UD_Isubsd:
        case UD_Isubss:
            optype = X86InstructionType_float;
            break;
        case UD_Iswapgs:
            optype = X86InstructionType_special;
            break;
        case UD_Isyscall:
        case UD_Isysenter:
        case UD_Isysexit:
        case UD_Isysret:
            optype = X86InstructionType_system_call;
            break;
        case UD_Itest:
            optype = X86InstructionType_int;
            break;
        case UD_Iucomisd:
        case UD_Iucomiss:
            optype = X86InstructionType_float;
            break;
        case UD_Iud2:
            optype = X86InstructionType_invalid;
            break;
        case UD_Iunpckhpd:
        case UD_Iunpckhps:
        case UD_Iunpcklps:
        case UD_Iunpcklpd:
            optype = X86InstructionType_float;
            break;
        case UD_Iverr:
        case UD_Iverw:
            optype = X86InstructionType_special;
            break;
        case UD_Ivmcall:
        case UD_Ivmclear:
        case UD_Ivmxon:
        case UD_Ivmptrld:
        case UD_Ivmptrst:
        case UD_Ivmresume:
        case UD_Ivmxoff:
        case UD_Ivmrun:
        case UD_Ivmmcall:
        case UD_Ivmload:
        case UD_Ivmsave:
            optype = X86InstructionType_vmx;
            break;
        case UD_Iwait:
        case UD_Iwbinvd:
        case UD_Iwrmsr:
            optype = X86InstructionType_special;
            break;
        case UD_Ixadd:
        case UD_Ixchg:
            optype = X86InstructionType_int;
            break;
        case UD_Ixlatb:
            optype = X86InstructionType_special;
            break;
        case UD_Ixor:
            optype = X86InstructionType_int;
            break;
        case UD_Ixorpd:
        case UD_Ixorps:
            optype = X86InstructionType_float;
            break;
        case UD_Idb:
        case UD_Iinvalid:
        case UD_Id3vil:
        case UD_Ina:
        case UD_Igrp_reg:
        case UD_Igrp_rm:
        case UD_Igrp_vendor:
        case UD_Igrp_x87:
        case UD_Igrp_mode:
        case UD_Igrp_osize:
        case UD_Igrp_asize:
        case UD_Igrp_mod:
        case UD_Inone:
            optype = X86InstructionType_invalid;
            break;
        default:
            optype = X86InstructionType_unknown;
            break;
    };

    ASSERT(optype != X86InstructionType_unknown && optype != X86InstructionType_invalid);
    return optype;
}

bool UD_INSTRUCTION_CLASS::controlFallsThrough(){
    if (isHalt()
        || isReturn()
        || isUnconditionalBranch()
        || isJumpTableBase()){
        return false;
    }

    return true;
}


UD_OPERAND_CLASS::UD_OPERAND_CLASS(struct ud_operand* init, uint32_t idx){
    operandIndex = idx;
    memcpy(&entry, init, sizeof(struct ud_operand));

    verify();
}

bool UD_OPERAND_CLASS::verify(){
    if (GET(size) % 8 != 0){
        PRINT_ERROR("Illegal operand size %d", GET(size));
        return false;
    }
    if (GET(size) > 64){
        PRINT_ERROR("Operand size too large %d", GET(size));
        return false;
    }
    return true;
}

bool UD_OPERAND_CLASS::isRelative(){
    return (GET(offset) != 0);
}

UD_INSTRUCTION_CLASS::~UD_INSTRUCTION_CLASS(){
    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        if (operands[i]){
            delete operands[i];
        }
    }
    delete[] operands;
}

UD_OPERAND_CLASS* UD_INSTRUCTION_CLASS::getOperand(uint32_t idx){
    ASSERT(operands);
    ASSERT(idx < MAX_OPERANDS && "Index into operand table has a limited range");

    return operands[idx];
}

UD_INSTRUCTION_CLASS::UD_INSTRUCTION_CLASS(TextSection* text, uint64_t baseAddr, char* buff, uint8_t src, uint32_t idx, bool doReformat)
    : Base(ElfClassTypes_Instruction)
{
    ud_t ud_obj;
    ud_init(&ud_obj);
    ud_set_input_buffer(&ud_obj, (uint8_t*)buff, MAX_X86_INSTRUCTION_LENGTH);
    ud_set_mode(&ud_obj, 32);
    ud_set_syntax(&ud_obj, NULL);

    sizeInBytes = ud_disassemble(&ud_obj);
    if (sizeInBytes) {
        memcpy(&entry, &ud_obj, sizeof(struct ud));
    } else {
        PRINT_ERROR("Problem doing instruction disassembly");
    }

    baseAddress = baseAddr;
    instructionIndex = idx;
    byteSource = src;
    textSection = text;
    addressAnchor = NULL;

    operands = new UD_OPERAND_CLASS*[MAX_OPERANDS];

    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        ud_operand op = GET(operand)[i];
        operands[i] = NULL;
        if (op.type){
            operands[i] = new UD_OPERAND_CLASS(&GET(operand)[i], i);
        }
    }

    leader = false;
    
    verify();

    //    print();
}

UD_INSTRUCTION_CLASS::UD_INSTRUCTION_CLASS(struct ud* init)
    : Base(ElfClassTypes_Instruction)
{
    memcpy(&entry, init, sizeof(struct ud));

    sizeInBytes = ud_insn_len(&entry);
    baseAddress = 0;

    operands = new UD_OPERAND_CLASS*[MAX_OPERANDS];

    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        ud_operand op = GET(operand)[i];
        operands[i] = NULL;
        if (op.type){
            operands[i] = new UD_OPERAND_CLASS(&GET(operand)[i], i);
        }
    }
    
    verify();
}

void UD_INSTRUCTION_CLASS::print(){
    PRINT_INFOR("%#llx:\t%16s\t%s\t%d %d %d %d", getBaseAddress(), GET(insn_hexcode), GET(insn_buffer), getInstructionType(), usesControlTarget(), isFunctionCall(), getSizeInBytes());
    PRINT_INFOR("%d(%s)\t%hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd", GET(mnemonic), ud_mnemonics_str[GET(mnemonic)],
                GET(error), GET(pfx_rex), GET(pfx_seg), GET(pfx_opr), GET(pfx_adr), GET(pfx_lock), GET(pfx_rep),
                GET(pfx_repe), GET(pfx_repne), GET(pfx_insn), GET(default64), GET(opr_mode), GET(adr_mode),
                GET(br_far), GET(br_near), GET(implicit_addr), GET(c1), GET(c2), GET(c3));

    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        ud_operand op = GET(operand)[i];
        if (op.type){
            getOperand(i)->print();
        }
    }
}

const char* ud_optype_str[] = { "reg", "mem", "ptr", "imm", "jimm", "const" };
const char* ud_regtype_str[] = { "undefined", "8bit gpr", "16bit gpr", "32bit gpr", "64bit gpr", "seg reg", 
                                 "ctrl reg", "dbg reg", "mmx reg", "x87 reg", "xmm reg", "pc reg" };

uint32_t regbase_to_type(uint32_t base){
    if (IS_8BIT_GPR(base))         return RegType_8Bit;
    else if (IS_16BIT_GPR(base))   return RegType_16Bit;
    else if (IS_32BIT_GPR(base))   return RegType_32Bit;
    else if (IS_64BIT_GPR(base))   return RegType_64Bit;
    else if (IS_SEGMENT_REG(base)) return RegType_Segment;
    else if (IS_CONTROL_REG(base)) return RegType_Control;
    else if (IS_DEBUG_REG(base))   return RegType_Debug;
    else if (IS_MMX_REG(base))     return RegType_MMX;
    else if (IS_X87_REG(base))     return RegType_X87;
    else if (IS_XMM_REG(base))     return RegType_XMM;
    else if (IS_PC_REG(base))      return RegType_PC;
    __SHOULD_NOT_ARRIVE;
    return 0;
}


void UD_OPERAND_CLASS::print(){
    ud_operand op = entry;

    ASSERT(op.type);

    PRINT_INFOR("\traw operand %d: type=%d(%d), base=%d, index=%d, lval=%#llx, size=%hhd, offset=%hhd, scale=%hhd", 
                operandIndex, op.type, op.type - UD_OP_REG, op.base, op.index, op.lval, op.size, op.offset, op.scale);
    
    char typstr[32];
    char valstr[32];
    bzero(typstr, 32);
    bzero(valstr, 32);

    if (op.type == UD_OP_REG){
        sprintf(typstr, "%s\0", ud_regtype_str[regbase_to_type(op.base)]);
        sprintf(valstr, "%s\0", ud_reg_tab[op.base-1]);
    } else if (op.type == UD_OP_MEM){
        sprintf(typstr, "%s%d\0", ud_optype_str[op.type - UD_OP_REG], op.size);
        if (op.base){
            sprintf(valstr, "%s + %llx\0", ud_reg_tab[op.base-1], op.lval);
        } else {
            sprintf(valstr, "%llx\0", op.lval);
        }
    } else if (op.type == UD_OP_PTR){
        __FUNCTION_NOT_IMPLEMENTED;
    } else if (op.type == UD_OP_IMM || op.type == UD_OP_JIMM){
        sprintf(typstr, "%s%d\0", ud_optype_str[op.type - UD_OP_REG], op.size);
        sprintf(valstr, "%#llx\0", op.lval);
    } else if (op.type == UD_OP_CONST){
        sprintf(typstr, "%s\0", ud_optype_str[op.type - UD_OP_REG]);
        sprintf(valstr, "%d\0", op.lval);
    } else {
        __SHOULD_NOT_ARRIVE;
    }
    
    PRINT_INFOR("\t[op%d] %s: %s", operandIndex, typstr, valstr, op.index);
}

bool UD_INSTRUCTION_CLASS::verify(){

    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        ud_operand op = GET(operand)[i];
        if (op.type){
            if (!IS_OPERAND_TYPE(op.type)){
                PRINT_ERROR("Found operand with nonsensical type %d", op.type);
                return false;
            }
            if (!operands[i]){
                PRINT_ERROR("Found null operand where one should exist");
                return false;
            }
        }
        if (op.base){
            if (!IS_REG(op.base)){
                PRINT_ERROR("Found operand with nonsensical base %d", op.base);
                return false;
            }
        }
        if (op.index){
            if (!IS_GPR(op.index)){
                PRINT_ERROR("Found operand with nonsensical index %d", op.index);
                return false;
            }
        }
    }

    return true;
}

