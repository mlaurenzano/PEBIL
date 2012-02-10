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

#include <X86Instruction.h>

#include <Base.h>
#include <BasicBlock.h>
#include <BinaryFile.h>
#include <ElfFile.h>
#include <ElfFileInst.h>
#include <Function.h>
#include <X86Instruction.h>
#include <X86InstructionFactory.h>
#include <Instrumentation.h>
#include <SectionHeader.h>
#include <TextSection.h>

uint32_t X86Instruction::getDefUseDist(){
    if (container->isFunction() && !((Function*)container)->doneDefUse()){
        ((Function*)container)->computeDefUse();
    }
    return defUseDist;
}
void X86Instruction::setDefUseDist(uint32_t dudist){ 
    defUseDist = dudist;
}



HashCode* X86Instruction::generateHashCode(BasicBlock* bb){
    HashCode* hc = new HashCode(bb->getHashCode().getSection(),
                                bb->getHashCode().getFunction(),
                                bb->getHashCode().getBlock(),
                                getIndex());
    return hc;
}

static ud_t ud_blank;
void X86Instruction::initBlankUd(bool is64bit){
    ud_t ud_obj;

    ud_init(&ud_obj);
    if (is64bit){
        ud_set_mode(&ud_obj, 64);
    } else {
        ud_set_mode(&ud_obj, 32);
    }
    ud_set_syntax(&ud_obj, DISASSEMBLY_MODE);

    memcpy(&ud_blank, &ud_obj, sizeof(ud_t));
}


void copy_ud_to_compact(struct ud_compact* comp, struct ud* reg){
    memcpy(comp->insn_bytes, reg->insn_bytes, sizeof(char) * 16);
    memcpy(comp->insn_buffer, reg->insn_buffer, sizeof(char) * INSTRUCTION_PRINT_SIZE);
    comp->mnemonic = reg->mnemonic;
    memcpy(comp->operand, reg->operand, sizeof(struct ud_operand) * MAX_OPERANDS);
    comp->pfx_seg = reg->pfx_seg;
    comp->pfx_rep = reg->pfx_rep;
    comp->adr_mode = reg->adr_mode;
    comp->flags_use = reg->flags_use;
    comp->flags_def = reg->flags_def;
    comp->impreg_use = reg->impreg_use;
    comp->impreg_def = reg->impreg_def;
}

uint32_t X86Instruction::countExplicitOperands(){
    uint32_t opCount = 0;
    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        if (getOperand(i)){
            opCount++;
        }
    }
    return opCount;
}

bool X86Instruction::isLoad(){
    if (isStackPop()){
        return true;
    }
    if (CHECK_IMPLICIT_LOAD){
        return true;
    }
    if (isExplicitMemoryOperation()){
        OperandX86* mem = getMemoryOperand();
        ASSERT(mem);
        Vector<OperandX86*>* uses = getSourceOperands();
        for (uint32_t i = 0; i < uses->size(); i++){
            if (mem->getOperandIndex() == (*uses)[i]->getOperandIndex()){
                delete uses;
                return true;
            }
        }
        delete uses;
    }
    return false;
}

bool X86Instruction::isStore(){
    if (isStackPush()){
        return true;
    }
    if (CHECK_IMPLICIT_STORE){
        return true;
    }
    if (isExplicitMemoryOperation()){
        OperandX86* mem = getMemoryOperand();
        ASSERT(mem);
        OperandX86* dest = getDestOperand();
        if (dest && mem->getOperandIndex() == dest->getOperandIndex()){
            return true;
        }
    }
    return false;
}

bool X86Instruction::isSpecialRegOp(){
    return (getInstructionType() == X86InstructionType_special);
}

bool X86Instruction::isLogicOp(){
    return false;
}

uint32_t X86Instruction::getNumberOfMemoryBytes(){
    if (isMemoryOperation()){
        if (isImplicitMemoryOperation()){
            OperandX86* op = operands[0];
            if (!op){
                return GET(adr_mode)/8;
            }
            ASSERT(op);
            if (op->GET(size)){
                return op->GET(size)/8;
            } else {
                if (IS_32BIT_GPR(op->GET(base))){
                    return sizeof(uint32_t);
                } else if (IS_64BIT_GPR(op->GET(base))){
                    return sizeof(uint64_t);
                } else {
                    print();
                    PRINT_ERROR("unexpected register size in implicit mem op");
                }
            }
        } else { // isExplicitMemoryOperation()
            OperandX86* op = getMemoryOperand();
            ASSERT(op);
            if (op->GET(size)){
                return op->GET(size)/8;
            }
            return GET(adr_mode)/8;
        }
    }
    return 0;
}

inline bool X86Instruction::usesFlag(uint32_t flg) { 
    return (GET(flags_use) & (1 << flg));
}

inline bool X86Instruction::defsFlag(uint32_t flg) { 
    return (GET(flags_def) & (1 << flg));
}

inline bool X86Instruction::usesAluReg(uint32_t alu){
    return (GET(impreg_use) & (1 << alu));
}

inline bool X86Instruction::defsAluReg(uint32_t alu){
    return (GET(impreg_def) & (1 << alu));
}

void X86Instruction::setLiveIns(BitSet<uint32_t>* live){
    if (liveIns){
        print();
    }
    ASSERT(!liveIns);
    liveIns = new BitSet<uint32_t>(*(live));

    DEBUG_LIVE_REGS(
                    PRINT_INFO();
                    PRINT_OUT("\t\tlive-ins %d registers: ", (*liveIns).size());
                    for (uint32_t i = 0; i < X86_64BIT_GPRS; i++){
                        if ((*liveIns).contains(i)){
                            PRINT_OUT("reg:%d ", i);
                        }
                    }    
                    PRINT_OUT("\n");    
                    )
}

void X86Instruction::setLiveOuts(BitSet<uint32_t>* live){
    ASSERT(!liveOuts);
    liveOuts = new BitSet<uint32_t>(*(live));

    DEBUG_LIVE_REGS(
                    PRINT_INFO();
                    PRINT_OUT("\t\tliveOuts %d registers: ", (*liveOuts).size());
                    for (uint32_t i = 0; i < X86_64BIT_GPRS; i++){
                        if ((*liveOuts).contains(i)){
                            PRINT_OUT("reg:%d ", i);
                        }
                    }    
                    PRINT_OUT("\n");
                    )
}

bool X86Instruction::isGPRegDeadIn(uint32_t idx){
    ASSERT(idx < X86_64BIT_GPRS);
    __FUNCTION_NOT_IMPLEMENTED;
    return false;
}

bool X86Instruction::allFlagsDeadIn(){
    if (!liveIns){
        return false;
    }
    for (uint32_t i = 0; i < X86_FLAG_BITS; i++){
        if (liveIns->contains(i)){
            return false;
        }
    }
    return true;
}

bool X86Instruction::allFlagsDeadOut(){
    if (!liveOuts){
        return false;
    }
    for (uint32_t i = 0; i < X86_FLAG_BITS; i++){
        if (liveOuts->contains(i)){
            return false;
        }
    }
    return true;
}

BitSet<uint32_t>* X86Instruction::getUseRegs(){
    BitSet<uint32_t>* regs = new BitSet<uint32_t>(X86_FLAG_BITS);
    for (uint32_t i = 0; i < X86_FLAG_BITS; i++){
        if (usesFlag(i)){
            regs->insert(i);
        }
    }
    return regs;
}

BitSet<uint32_t>* X86Instruction::getDefRegs(){
    BitSet<uint32_t>* regs = new BitSet<uint32_t>(X86_FLAG_BITS);
    for (uint32_t i = 0; i < X86_FLAG_BITS; i++){
        if (defsFlag(i)){
            regs->insert(i);
        }
    }
    return regs;
}

uint32_t convertGPRegUd(uint32_t reg){
   return reg + UD_R_RAX; 
}

uint32_t convertUdGPReg(uint32_t reg){
    ASSERT(reg && IS_GPR(reg));
    if (IS_8BIT_GPR(reg)){
        return reg - UD_R_AL;
    } else if (IS_16BIT_GPR(reg)){
        return reg - UD_R_AX;
    } else if (IS_32BIT_GPR(reg)){
        return reg - UD_R_EAX;
    } else if (IS_64BIT_GPR(reg)){
        return reg - UD_R_RAX;
    }
    __SHOULD_NOT_ARRIVE;
    return 0;
}

bool OperandX86::isSameOperand(OperandX86* other){
    if (other->getValue() == getValue() &&
        other->GET(base) == GET(base) &&
        other->GET(index) == GET(index) &&
        other->GET(scale) == GET(scale) &&
        other->GET(type) == GET(type)){
        return true;
    }
    return false;
}

OperandX86* X86Instruction::getDestOperand(){
    // compares and branches dont define anything
    if (isConditionCompare() || isBranch() || CHECK_IMPLICIT_STORE){
        return NULL;
    }
    return operands[DEST_OPERAND];
}

Vector<OperandX86*>* X86Instruction::getSourceOperands(){
    Vector<OperandX86*>* ops = new Vector<OperandX86*>();
    uint32_t numni = countValidNonimm();

    /* S/D, S, [S] */
    if (numni < 3 && !isMoveOperation()){
        if (operands[0]){
            ops->append(operands[0]);
        }
        if (operands[1]){
            ops->append(operands[1]);
        }
        if (operands[2]){
            ops->append(operands[2]);
        }
    /* D, S, [S, [S]] */
    } else {
        if (operands[1]){
            ops->append(operands[1]);
        }        
        if (operands[2]){
            ops->append(operands[2]);
        }        
        if (operands[3]){
            ops->append(operands[3]);
        }        
    }
    return ops;
}

uint32_t X86Instruction::countValidNonimm(){
    uint32_t nimm = 0;
    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        if (operands[i] && operands[i]->getType() != UD_OP_IMM){
            nimm++;
        }
    }
    return nimm;
}

bool X86Instruction::isConditionCompare(){
    int32_t m = GET(mnemonic);

    if (
        (m == UD_Ibt) ||
        (m == UD_Ibtc) ||
        (m == UD_Ibtr) ||
        (m == UD_Ibts) ||
        (m == UD_Icmppd) ||
        (m == UD_Icmpps) ||
        (m == UD_Icmpsb) ||
        (m == UD_Icmpsd) ||
        (m == UD_Icmpsq) ||
        (m == UD_Icmpss) ||
        (m == UD_Icmpsw) ||
        (m == UD_Icmpxchg8b) ||
        (m == UD_Icmpxchg) ||
        (m == UD_Icmp) ||
        (m == UD_Iftst) ||
        (m == UD_Ipcmpeqb) ||
        (m == UD_Ipcmpeqd) ||
        (m == UD_Ipcmpeqq) ||
        (m == UD_Ipcmpeqw) ||
        (m == UD_Ipcmpestri) ||
        (m == UD_Ipcmpestrm) ||
        (m == UD_Ipcmpgtb) ||
        (m == UD_Ipcmpgtd) ||
        (m == UD_Ipcmpgtq) ||
        (m == UD_Ipcmpgtw) ||
        (m == UD_Ipcmpistri) ||
        (m == UD_Ipcmpistrm) ||
        (m == UD_Ipfcmpeq) ||
        (m == UD_Ipfcmpge) ||
        (m == UD_Ipfcmpgt) ||
        (m == UD_Iptest) ||
        (m == UD_Itest) ||
        (m == UD_Ivcmppd) ||
        (m == UD_Ivcmpps) ||
        (m == UD_Ivcmpsd) ||
        (m == UD_Ivcmpss) ||
        (m == UD_Ivpcmpeqb) ||
        (m == UD_Ivpcmpeqd) ||
        (m == UD_Ivpcmpeqq) ||
        (m == UD_Ivpcmpeqw) ||
        (m == UD_Ivpcmpestri) ||
        (m == UD_Ivpcmpestrm) ||
        (m == UD_Ivpcmpgtb) ||
        (m == UD_Ivpcmpgtd) ||
        (m == UD_Ivpcmpgtq) ||
        (m == UD_Ivpcmpgtw) ||
        (m == UD_Ivpcmpistri) ||
        (m == UD_Ivpcmpistrm) ||
        (m == UD_Ivtestpd) ||
        (m == UD_Ivtestps)){
        return true;
    }
    return false;
}


uint32_t convertUdXMMReg(uint32_t reg){
    ASSERT(reg && IS_XMM_REG(reg));
    return reg - UD_R_XMM0 + X86_FPREG_XMM0;
}

uint32_t convertUdYMMReg(uint32_t reg){
    ASSERT(reg && IS_YMM_REG(reg));
    return reg - UD_R_YMM0 + X86_FPREG_XMM0;
}

uint32_t OperandX86::getBaseRegister(){
    ASSERT(GET(base) && IS_ALU_REG(GET(base)));
    if (IS_GPR(GET(base))){
        return convertUdGPReg(GET(base));
    } else if (IS_XMM_REG(GET(base))){
        return convertUdXMMReg(GET(base));
    } else if (IS_YMM_REG(GET(base))){
        return convertUdYMMReg(GET(base));
    } 
    __SHOULD_NOT_ARRIVE;
    return 0;
}
uint32_t OperandX86::getIndexRegister(){
    ASSERT(GET(index) && IS_ALU_REG(GET(index)));
    if (IS_GPR(GET(index))){
        return convertUdGPReg(GET(index));
    } else if (IS_XMM_REG(GET(index))){
        return convertUdXMMReg(GET(index));
    } else if (IS_YMM_REG(GET(index))){
        return convertUdYMMReg(GET(index));
    } 
    __SHOULD_NOT_ARRIVE;
    return 0;
}

void OperandX86::touchedRegisters(BitSet<uint32_t>* regs){
    if (GET(base) && IS_ALU_REG(GET(base))){
        regs->insert(getBaseRegister());
    }
    if (GET(index) && IS_ALU_REG(GET(index))){
        regs->insert(getIndexRegister());
    }
}

void X86Instruction::impliedUses(BitSet<uint32_t>* regs){
    for (uint32_t i = 0; i < X86_ALU_REGS; i++){
        if (usesAluReg(i)){
            regs->insert(i);
        }
    }
}

void X86Instruction::impliedDefs(BitSet<uint32_t>* regs){
    for (uint32_t i = 0; i < X86_ALU_REGS; i++){
        if (defsAluReg(i)){
            regs->insert(i);
        }
    }
}

#define op_has(op, reg) (op->GET(reg) && IS_ALU_REG(op->GET(reg)))
LinkedList<X86Instruction::ReachingDefinition*>* X86Instruction::getDefs(){

    LinkedList<ReachingDefinition*>* defList =
        new LinkedList<ReachingDefinition*>();

    // explicit defines
    OperandX86* def = getDestOperand();
    if (def){
        DefLocation loc;
        loc.value = def->getValue();
        loc.base = op_has(def, base) ? def->getBaseRegister() : X86_ALU_REGS;
        loc.index = op_has(def, index) ? def->getIndexRegister() : X86_ALU_REGS;
        loc.offset = def->GET(offset);
        loc.scale = def->GET(scale);
        loc.type = def->GET(type);
        defList->insert(new ReachingDefinition(this, loc));
    }

    // Get the implied register defines
    BitSet<uint32_t> imp_regs(X86_ALU_REGS);
    impliedDefs(&imp_regs);

    for (uint32_t i = 0; i < X86_ALU_REGS; ++i) {
        if (imp_regs.contains(i)) {
            struct DefLocation loc;
            bzero(&loc, sizeof(loc));
            loc.base = i;
            loc.type = UD_OP_REG;
            defList->insert(new ReachingDefinition(this, loc));
        }
    } 

    return defList;
}

LinkedList<X86Instruction::ReachingDefinition*>* X86Instruction::getUses(){

    LinkedList<ReachingDefinition*>* useList =
        new LinkedList<ReachingDefinition*>();

    OperandX86* def = getDestOperand();    
    if (def){
        if (def->getType() == UD_OP_MEM || def->getType() == UD_OP_PTR){
            DefLocation loc;
            loc.value = def->getValue();
            loc.base = op_has(def, base) ? def->getBaseRegister() : X86_ALU_REGS;
            loc.index = op_has(def, index) ? def->getIndexRegister() : X86_ALU_REGS;
            loc.offset = def->GET(offset);
            loc.scale = def->GET(scale);
            loc.type = def->GET(type);
            useList->insert(new ReachingDefinition(this, loc));            
        }
    }

    Vector<OperandX86*>* uses = getSourceOperands();
    for (uint32_t i; i < uses->size(); i++){
        OperandX86* use = (*uses)[i];
        DefLocation loc;
        loc.value = use->getValue();
        loc.base = op_has(use, base) ? use->getBaseRegister() : X86_ALU_REGS;
        loc.index = op_has(use, index) ? use->getIndexRegister() : X86_ALU_REGS;
        loc.offset = use->GET(offset);
        loc.scale = use->GET(scale);
        loc.type = use->GET(type);
        useList->insert(new ReachingDefinition(this, loc));
    }
    delete uses;

    // Get the implied register uses
    BitSet<uint32_t> imp_regs(X86_ALU_REGS);
    impliedUses(&imp_regs);

    for (uint32_t i = 0; i < X86_ALU_REGS; ++i) {
        if (imp_regs.contains(i)) {
            struct DefLocation loc;
            bzero(&loc, sizeof(loc));
            loc.base = i;
            loc.type = UD_OP_REG;
            useList->insert(new ReachingDefinition(this, loc));
        }
    } 

    return useList;
}

bool X86Instruction::ReachingDefinition::invalidatedBy(ReachingDefinition* other) {

    // this definition is invalidated if other defines the same location
    return ((this->location.type == other->location.type &&
            this->location.base == other->location.base &&
            this->location.index == other->location.index &&
            this->location.offset == other->location.offset &&
            this->location.scale == other->location.scale &&
            this->location.value == other->location.value)
            || // or if other defines a register used to compute this location
            (other->location.type == UD_OP_REG &&
               (other->location.base == this->location.base ||
               other->location.base == this->location.index)));
}


void X86Instruction::ReachingDefinition::print() {

    if (location.type == UD_OP_REG) {
        printf("REG: %s\n", location.base < X86_ALU_REGS ? alu_name_map[location.base] : "None");
    } else if (location.type == UD_OP_MEM) {
        printf("MEM: %lld(%s, %s, %d)", location.value,
                                      location.base < X86_ALU_REGS ? alu_name_map[location.base] : "None",
                                      location.index < X86_ALU_REGS ? alu_name_map[location.index] : "None",
                                      location.scale);
    } else if (location.type == UD_OP_PTR) {
        printf("PTR");
    } else if (location.type == UD_OP_IMM) {
        printf("IMM");
    } else if (location.type == UD_OP_JIMM) {
        printf("JIMM");
    } else if (location.type == UD_OP_CONST) {
        printf("CONST");
    } else {
        printf("???");
    }

    if (defined_by != NULL)
        defined_by->print();
    else
        printf("???\n");
}

void X86Instruction::touchedRegisters(BitSet<uint32_t>* regs){
    for (uint32_t i = 0 ; i < MAX_OPERANDS; i++){
        if (operands[i]){
            operands[i]->touchedRegisters(regs);
        }
    }
}

OperandX86* X86Instruction::getMemoryOperand(){
    ASSERT(isMemoryOperation());
    if (isExplicitMemoryOperation()){
        for (uint32_t i = 0; i < MAX_OPERANDS; i++){
            if (operands[i] && operands[i]->GET(type) == UD_OP_MEM){
                return operands[i];
            }
        }
        __SHOULD_NOT_ARRIVE;
        return NULL;
    } else { // isImplicitMemoryOperation()
        for (uint32_t i = 0; i < MAX_OPERANDS; i++){
            // implicit mem ops only have 1 operand
            if (operands[i]){
                return operands[i];
            }
        }
    }
    __SHOULD_NOT_ARRIVE;
    return NULL;
}

bool X86Instruction::isStackPush(){
    if (GET(mnemonic) == UD_Ipush   ||
        GET(mnemonic) == UD_Ipusha  ||
        GET(mnemonic) == UD_Ipushad ||
        GET(mnemonic) == UD_Ipushfw ||
        GET(mnemonic) == UD_Ipushfd ||
        GET(mnemonic) == UD_Ipushfq ){
        return true;
    }
    return false;
}

bool X86Instruction::isStackPop(){
    if (GET(mnemonic) == UD_Ipop    ||
        GET(mnemonic) == UD_Ipopa   ||
        GET(mnemonic) == UD_Ipopad  ||
        GET(mnemonic) == UD_Ipopfw  ||
        GET(mnemonic) == UD_Ipopfd  ||
        GET(mnemonic) == UD_Ipopfq  ){
        return true;
    }
    return false;
}

bool X86Instruction::isMemoryOperation(){
    return (isImplicitMemoryOperation() || isExplicitMemoryOperation());
}

bool X86Instruction::isImplicitMemoryOperation(){
    if (isStackPush() || isStackPop()){
        return true;
    }
    return false;
}

bool X86Instruction::isExplicitMemoryOperation(){
    uint32_t memCount = 0;
    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        if (operands[i] && operands[i]->GET(type) == UD_OP_MEM){
            if (!IS_LOADADDR(GET(mnemonic)) && !IS_PREFETCH(GET(mnemonic)) && !isNop()){
                memCount++;
            }
        }
    }
    ASSERT(!memCount || memCount == 1 && "Shouldn't have found multiple memops in an instruction");
    if (memCount){
        return true;
    }
    return false;
}

bool X86Instruction::isStringOperation(){
    if (getInstructionType() == X86InstructionType_string){
        return true;
    }
    return false;
}

bool X86Instruction::isMoveOperation(){
    if (getInstructionType() == X86InstructionType_move){
        return true;
    }
    return false;
}

bool X86Instruction::isIntegerOperation(){
    if (getInstructionType() == X86InstructionType_int){
        return true;
    }
    return false;
}

bool X86Instruction::isFloatPOperation(){
    if (getInstructionType() == X86InstructionType_float){
        return true;
    }
    else if (getInstructionType() == X86InstructionType_simd){
        return true;
    }
    else if (getInstructionType() == X86InstructionType_avx){
        return true;
    }
    return false;
}

uint32_t OperandX86::getBytesUsed(){
    if (GET(type) == UD_OP_MEM){
        return (GET(offset) >> 3);
    }
    return (GET(size) >> 3);
}

uint32_t X86Instruction::getDstSizeInBytes(){
    OperandX86* op;
    if(op = getOperand(0))
        return op->GET(size) >> 3;
    else
        return 0;
}

int64_t OperandX86::getValue(){
    int64_t value;
    if (getBytesUsed() == 0){
        return 0;
    } else if (getBytesUsed() == sizeof(uint8_t)){
        value = (int64_t)GET_A(sbyte, lval);
    } else if (getBytesUsed() == sizeof(uint16_t)){
        value = (int64_t)GET_A(sword, lval);
    } else if (getBytesUsed() == sizeof(uint32_t)){
        value = (int64_t)GET_A(sdword, lval);
    } else if (getBytesUsed() == sizeof(uint64_t)){
        value = (int64_t)GET_A(sqword, lval);
        // TODO: for now we just yield 0 for types larger than 64 (xmm/ymm)
    } else if (getBytesUsed() == sizeof(uint64_t) + sizeof(uint64_t)){
        value = 0;
    } else if (getBytesUsed() == sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t)){
        value = 0;
    } else { 
        print();
        PRINT_INFOR("%s", instruction->GET(insn_buffer));
        PRINT_INFOR("size %d", getBytesUsed());
        __SHOULD_NOT_ARRIVE;
    }
    return value;
}

uint32_t OperandX86::getBytePosition(){
    return GET(position);
}

bool OperandX86::isRelative(){
    if (GET(type) == UD_OP_JIMM){
        return true;
    }
    if (GET(base) == UD_R_RIP){
        return true;
    }
    return false;
}

bool X86Instruction::usesRelativeAddress(){
    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        if (operands[i] && operands[i]->isRelative()){
            return true;
        }
    }
    return false;
}

int64_t X86Instruction::getRelativeValue(){
    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        if (operands[i] && operands[i]->isRelative()){
            return operands[i]->getValue();
        }
    }
    __SHOULD_NOT_ARRIVE;
    return 0;
}

uint64_t X86Instruction::getTargetAddress(){
    uint64_t tgtAddress;
    if (getInstructionType() == X86InstructionType_uncondbr ||
        getInstructionType() == X86InstructionType_condbr){
        if (addressAnchor){ 
           tgtAddress = getBaseAddress() + addressAnchor->getLinkValue() + getSizeInBytes();
        } else if (operands && operands[JUMP_TARGET_OPERAND]){
            if (operands[JUMP_TARGET_OPERAND]->getType() == UD_OP_JIMM){
                tgtAddress = getBaseAddress();
                tgtAddress += operands[JUMP_TARGET_OPERAND]->getValue();
                tgtAddress += getSizeInBytes();
                PRINT_DEBUG_OPTARGET("Set next address to 0x%llx = 0x%llx + 0x%llx + %d", tgtAddress, getBaseAddress(), operands[JUMP_TARGET_OPERAND]->getValue(), getSizeInBytes());
            } else {
                tgtAddress = getBaseAddress() + getSizeInBytes();
            }
        } else {
            tgtAddress = 0;
        }
    }
    else if (getInstructionType() == X86InstructionType_call){
        if (addressAnchor){
            tgtAddress = getBaseAddress() + addressAnchor->getLinkValue() + getSizeInBytes();
        } else if (operands && operands[JUMP_TARGET_OPERAND]){
            if (operands[JUMP_TARGET_OPERAND]->getType() == UD_OP_JIMM){
                tgtAddress = getBaseAddress();
                tgtAddress += operands[JUMP_TARGET_OPERAND]->getValue();
                tgtAddress += getSizeInBytes();
                PRINT_DEBUG_OPTARGET("Set next address to 0x%llx = 0x%llx + 0x%llx + %d", tgtAddress, getBaseAddress(), operands[JUMP_TARGET_OPERAND]->getValue(), getSizeInBytes());
            } else {
                tgtAddress = 0;
            }
        } else {
            tgtAddress = 0;
        }

    }
    else if (getInstructionType() == X86InstructionType_syscall){
        tgtAddress = 0;
    } else {
        tgtAddress = getBaseAddress() + getSizeInBytes();
    }

    return tgtAddress;
}

uint32_t X86Instruction::bytesUsedForTarget(){
    if (isControl()){
        if (isUnconditionalBranch() || isConditionalBranch() || isFunctionCall()){
            if (operands && operands[JUMP_TARGET_OPERAND]){
                return operands[JUMP_TARGET_OPERAND]->getBytesUsed();
            }
        }
    }
    return 0;
}

uint32_t X86Instruction::convertTo4ByteTargetOperand(){
    ASSERT(isControl());

    PRINT_DEBUG_INST("Before mod");
    DEBUG_INST(print();)

    // extract raw bytes from hex representation
    char rawBytes[MAX_X86_INSTRUCTION_LENGTH];
    memcpy(rawBytes, GET(insn_bytes), sizeInBytes);

    uint32_t additionalBytes = 0;

    if (bytesUsedForTarget() && bytesUsedForTarget() < sizeof(uint32_t)){
        if (isUnconditionalBranch()){
            if (sizeInBytes != 2){
                print();
                PRINT_INFOR("faulty instruction location: function %s addr %#llx", getContainer()->getName(), getProgramAddress());
            }
            ASSERT(sizeInBytes == 2); // we expect a single byte for the opcode and a single byte for the target offset
            if (!addressAnchor){
                print();
                PRINT_ERROR("Instruction at address %#llx should have an address anchor", getBaseAddress());
            }
            ASSERT(addressAnchor);

            rawBytes[0] -= 0x02;
            additionalBytes = 3;
            uint32_t operandValue = getOperand(JUMP_TARGET_OPERAND)->getValue();
            memcpy(rawBytes + 1, &operandValue, sizeof(uint32_t));

        } else if (isConditionalBranch()){
            if (sizeInBytes != 2){
                PRINT_WARN(4,"Conditional Branch with 3 bytes encountered");
                print();
            }

            if (!addressAnchor){
                print();
                PRINT_ERROR("Instruction at address %#llx should have an address anchor", getBaseAddress());
            }
            ASSERT(addressAnchor);

            additionalBytes = 4;
            uint32_t operandValue = getOperand(JUMP_TARGET_OPERAND)->getValue();
            memcpy(rawBytes + 2, &operandValue, sizeof(uint32_t));
            rawBytes[1] = rawBytes[0] + 0x10;
            rawBytes[0] = 0x0f;

        } else if (isFunctionCall()){
            //            PRINT_WARN(8, "Unhandled short call at address %#llx", getProgramAddress());
        } else if (isReturn()){
            // nothing to do since returns dont have target ops
            ASSERT(sizeInBytes == 1);
            for (uint32_t i = 0; i < MAX_OPERANDS; i++){
                ASSERT(!getOperand(i));
            }
        } else {
            PRINT_ERROR("Unknown branch type %d not handled currently", getInstructionType());
            __SHOULD_NOT_ARRIVE;
        }
    }

    if (additionalBytes){

        if (operands){
            for (uint32_t i = 0; i < MAX_OPERANDS; i++){
                if (operands[i]){
                    delete operands[i];
                }
            }
            delete[] operands;
        }

        ud_t ud_obj;
        memcpy(&ud_obj, &ud_blank, sizeof(ud_t));
        ud_set_input_buffer(&ud_obj, (uint8_t*)rawBytes, MAX_X86_INSTRUCTION_LENGTH);

        sizeInBytes = ud_disassemble(&ud_obj);
        if (sizeInBytes) {
            copy_ud_to_compact(&entry, &ud_obj);
        } else {
            PRINT_ERROR("Problem doing instruction disassembly");
        }

        operands = new OperandX86*[MAX_OPERANDS];
        for (uint32_t i = 0; i < MAX_OPERANDS; i++){
            ud_operand op = GET(operand)[i];
            operands[i] = NULL;
            if (op.type){
                operands[i] = new OperandX86(this, &GET(operand)[i], i);
            }
        }

    }

#ifdef DEBUG_INST
    PRINT_DEBUG_INST("After mod");
    DEBUG_INST(print();)
#endif

    return sizeInBytes;
}

void X86Instruction::binutilsPrint(FILE* stream){
    fprintf(stream, "%llx: ", getBaseAddress());

    for (int32_t i = 0; i < sizeInBytes; i++){
        fprintf(stream, "%02x ", GET(insn_bytes)[i]);
    }

    if (sizeInBytes < 8){
        for (int32_t i = 8 - sizeInBytes; i > 0; i--){
            fprintf(stream, "   ");
        }
    }
    fprintf(stream, "\t%s", GET(insn_buffer));

    if (usesRelativeAddress()){
        if (addressAnchor){
            fprintf(stream, "\t#x@ %llx", addressAnchor->getLink()->getBaseAddress());
        } 
    }

    fprintf(stream, "\n");
}


bool X86Instruction::usesControlTarget(){
    if (isConditionalBranch() ||
        isUnconditionalBranch() ||
        isFunctionCall() ||
        isSystemCall()){
        return true;
    }
    return false;
}

void X86Instruction::initializeAnchor(Base* link){
    if (addressAnchor){
        print();
    }
    ASSERT(!addressAnchor);
    ASSERT(link->containsProgramBits());
    addressAnchor = new AddressAnchor(link,this);
}

void X86Instruction::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    ASSERT(sizeInBytes && "This instruction has no bytes thus it cannot be dumped");

    binaryOutputFile->copyBytes((char*)GET(insn_bytes), sizeInBytes, offset);

    // the anchor will now overwrite any original instruction bytes that relate to relative addresses
    if (addressAnchor){
        addressAnchor->dump(binaryOutputFile,offset);
    }
}

TableModes X86Instruction::computeJumpTableTargets(uint64_t tableBase, Function* func, Vector<uint64_t>* addressList, Vector<uint64_t>* tableStorageList){
    ASSERT(isJumpTableBase() && "Cannot compute jump table targets for this instruction");
    ASSERT(func);
    ASSERT(addressList);
    ASSERT(tableStorageList);

    TableModes tableMode = TableMode_undefined;

    RawSection* dataSection = container->getTextSection()->getElfFile()->findDataSectionAtAddr(tableBase);
    if (!dataSection){
        print();
        PRINT_ERROR("Cannot find table base %#llx for this instruction", tableBase);
    }
    ASSERT(dataSection);
    if (!dataSection->getSectionHeader()->hasBitsInFile()){
        return tableMode;
    }
    ASSERT(dataSection->getSectionHeader()->hasBitsInFile());

    // read the first location to decide what type of info is stored in the jump table
    uint64_t rawData;
    if (container->getTextSection()->getElfFile()->is64Bit()){
        rawData = getUInt64(dataSection->getStreamAtAddress(tableBase));
    } else {
        rawData = (uint64_t)getUInt32(dataSection->getStreamAtAddress(tableBase));
    }

    // the data found is an address
    if (func->inRange(rawData)){
        tableMode = TableMode_direct;
        PRINT_DEBUG_JUMP_TABLE("\tJumpMode for table base %#llx -- Direct", tableBase);
    }
    // the data found is an address offset
    else if (func->inRange(rawData+baseAddress) || absoluteValue(rawData) < JUMP_TABLE_REACHES){
        tableMode = TableMode_indirect;
        PRINT_DEBUG_JUMP_TABLE("\tJumpMode for table base %#llx -- Indirect", tableBase);
    }
    // the data found is neither of the above -- we interpret this to mean that it is instructions
    else {
        tableMode = TableMode_instructions;
        (*addressList).append(tableBase);
        (*tableStorageList).append(rawData);
        PRINT_DEBUG_JUMP_TABLE("\tJumpMode for table base %#llx -- Instructions", tableBase);
        return tableMode;
    }
    ASSERT(tableMode && tableMode < TableMode_Total_Types);

    uint32_t currByte = 0;
    uint32_t dataLen;
    if (container->getTextSection()->getElfFile()->is64Bit()){
        dataLen = sizeof(uint64_t);
    } else {
        dataLen = sizeof(uint32_t);
    }
    

    /* Modified by Jingyue */
    /*
     * The original do-while has logic problems. Change it to while-do
     * while (in data range) {
     *   get target
     *   if (target not in function range)
     *     break;
     *   add to instruction list
     *   increase currByte
     * }
     */
    while ((tableBase + currByte) -
           dataSection->getSectionHeader()->GET(sh_addr) <
           dataSection->getSizeInBytes()) {

        /* Get the target */
        if (container->getTextSection()->getElfFile()->is64Bit()){
            rawData = getUInt64(dataSection->getStreamAtAddress(tableBase+currByte));
        } else {
            rawData = (uint64_t)getUInt32(dataSection->getStreamAtAddress(tableBase+currByte));
        }
        
        if (!tableMode){
            rawData += baseAddress;
        }
        if (!func->inRange(rawData)){
            break;
        }

        PRINT_DEBUG_JUMP_TABLE("Jump Table target %#llx", rawData);
        (*addressList).append(rawData);
        (*tableStorageList).append(tableBase+currByte);

        currByte += dataLen;
    }

    return tableMode;
}


uint64_t X86Instruction::findJumpTableBaseAddress(Vector<X86Instruction*>* functionInstructions){
    ASSERT(isJumpTableBase() && "Cannot compute jump table base for this instruction");

    uint64_t jumpOperand;
#ifdef JUMPTABLE_USE_REGISTER_OPS
    if (operands[JUMP_TARGET_OPERAND]->getType() == UD_OP_MEM){
        jumpOperand = operands[JUMP_TARGET_OPERAND]->getValue();
    } 
    else if (operands[JUMP_TARGET_OPERAND]->getType() == UD_OP_REG){
        jumpOperand = operands[JUMP_TARGET_OPERAND]->getBaseRegister();
    }
#else // JUMPTABLE_USE_REGISTER_OPS
    jumpOperand = operands[JUMP_TARGET_OPERAND]->getValue();
#endif // JUMPTABLE_USE_REGISTER_OPS
    PRINT_DEBUG_JUMP_TABLE("Finding jump table base address for instruction at %#llx", baseAddress);

    // jump target is a register
    if (jumpOperand < X86_64BIT_GPRS){
        if ((*functionInstructions).size()){
            X86Instruction** allInstructions = new X86Instruction*[(*functionInstructions).size()];
            for (uint32_t i = 0; i < (*functionInstructions).size(); i++){
                allInstructions[i] = (*functionInstructions)[i];
            }
            qsort(allInstructions,(*functionInstructions).size(),sizeof(X86Instruction*),compareBaseAddress);

            // search backwards through instructions to find jump table base
            uint64_t prevAddr = baseAddress-1;
            void* prev = NULL;
            do {
                PRINT_DEBUG_JUMP_TABLE("\tTrying Jump base address %#llx", prevAddr);
                prev = bsearch(&prevAddr,allInstructions,(*functionInstructions).size(),sizeof(X86Instruction*),searchBaseAddress);
                if (prev){
                    X86Instruction* previousInstruction = *(X86Instruction**)prev;
                    bool jumpOpFound = false;
                    uint64_t immediate = 0;
                    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
                        OperandX86* op = previousInstruction->getOperand(i);
                        if (op){
                            if (op->getType()){
#ifdef JUMPTABLE_USE_REGISTER_OPS
                                if (op->getType() == UD_OP_MEM && op->getValue() == jumpOperand){
                                    jumpOpFound = true;
                                } else if (op->getType() == UD_OP_REG && op->getBaseRegister() == jumpOperand){
                                    jumpOpFound = true;
                                }
#else // JUMPTABLE_USE_REGISTER_OPS
                                if (op->getValue() == jumpOperand){
                                    jumpOpFound = true;
                                }
#endif // JUMPTABLE_USE_REGISTER_OPS
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
                        if (!container->getTextSection()->getElfFile()->findDataSectionAtAddr(immediate)){
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
        if (!container->getTextSection()->getElfFile()->findDataSectionAtAddr(operands[JUMP_TARGET_OPERAND]->getValue())){
            return 0;
        }
        return operands[JUMP_TARGET_OPERAND]->getValue();
    }
    return 0;
}

bool X86Instruction::isControl(){
    return  (isConditionalBranch() || isUnconditionalBranch() || isSystemCall() || isFunctionCall() || isReturn());
}


bool X86Instruction::usesIndirectAddress(){
    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        if (operands[i]){
            if (operands[i]->getType() == UD_OP_MEM){
                return true;
            }
            if (operands[i]->getType() == UD_OP_REG){
                return true;
            }
        }
    }
    return false;
}


bool X86Instruction::isJumpTableBase(){
    return (isUnconditionalBranch() && usesIndirectAddress());
}

bool X86Instruction::isIndirectBranch(){
    return (isBranch() && usesIndirectAddress());
}

bool X86Instruction::isBinUnknown() { return  X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_unknown; }
bool X86Instruction::isBinInvalid() { return  X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_invalid; }
bool X86Instruction::isBinCond()    { return  X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_cond;    }
bool X86Instruction::isBinUncond()  { return  X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_uncond;  }
bool X86Instruction::isBinBin()     { return  X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_bin;     }
bool X86Instruction::isBinBinv()    { return  X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_binv;    }
bool X86Instruction::isBinInt()     { return  X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_int;     }
bool X86Instruction::isBinIntv()    { return  X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_intv;    }
bool X86Instruction::isBinFloat()   { return  X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_float;   }
bool X86Instruction::isBinFloatv()  { return  X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_floatv;  }
bool X86Instruction::isBinFloats()  { return  X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_floats;  }
bool X86Instruction::isBinMove()    { return  X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_move;    }
bool X86Instruction::isBinSystem()  { return  X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_system;  }
bool X86Instruction::isBinStack()   { return  X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_stack;   }
bool X86Instruction::isBinOther()   { return  X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_other;   }
bool X86Instruction::isBinCache()   { return  X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_cache;   }
bool X86Instruction::isBinString()  { return  X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_string;  }
bool X86Instruction::isBinByte()    { return (X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_int)    && (X86InstructionClassifier::getInstructionMemSize(this)) == 1; }
bool X86Instruction::isBinBytev()   { return (X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_intv)   && (X86InstructionClassifier::getInstructionMemSize(this)) == 1; }
bool X86Instruction::isBinWord()    { return (X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_int)    && (X86InstructionClassifier::getInstructionMemSize(this)) == 2; }
bool X86Instruction::isBinWordv()   { return (X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_intv)   && (X86InstructionClassifier::getInstructionMemSize(this)) == 2; }
bool X86Instruction::isBinDword()   { return (X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_int)    && (X86InstructionClassifier::getInstructionMemSize(this)) == 4; }
bool X86Instruction::isBinDwordv()  { return (X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_intv)   && (X86InstructionClassifier::getInstructionMemSize(this)) == 4; }
bool X86Instruction::isBinQword()   { return (X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_int)    && (X86InstructionClassifier::getInstructionMemSize(this)) == 8; }
bool X86Instruction::isBinQwordv()  { return (X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_intv)   && (X86InstructionClassifier::getInstructionMemSize(this)) == 8; }
bool X86Instruction::isBinSingle()  { return (X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_float)  && (X86InstructionClassifier::getInstructionMemSize(this)) == 4; }
bool X86Instruction::isBinSinglev() { return (X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_floatv) && (X86InstructionClassifier::getInstructionMemSize(this)) == 4; }
bool X86Instruction::isBinSingles() { return (X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_floats) && (X86InstructionClassifier::getInstructionMemSize(this)) == 4; }
bool X86Instruction::isBinDouble()  { return (X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_float)  && (X86InstructionClassifier::getInstructionMemSize(this)) == 8; }
bool X86Instruction::isBinDoublev() { return (X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_floatv) && (X86InstructionClassifier::getInstructionMemSize(this)) == 8; }
bool X86Instruction::isBinDoubles() { return (X86InstructionClassifier::getInstructionBin(this) == X86InstructionBin_floats) && (X86InstructionClassifier::getInstructionMemSize(this)) == 8; }
bool X86Instruction::isBinMem()     { return (X86InstructionClassifier::getInstructionMemLocation(this) != 0); }

void X86Instruction::printBin(){
    if(isBinUnknown())      printf("Unknown");
    else if(isBinInvalid()) printf("Invalid");
    else if(isBinCond())    printf("Cond");
    else if(isBinUncond())  printf("Uncond");
    else if(isBinBin())     printf("Bin");
    else if(isBinBinv())    printf("Binv");
    else if(isBinInt())     printf("Int");
    else if(isBinIntv())    printf("Intv");
    else if(isBinFloat())   printf("Float");
    else if(isBinFloatv())  printf("Floatv");
    else if(isBinFloats())  printf("Floats");
    else if(isBinMove())    printf("Move");
    else if(isBinSystem())  printf("System");
    else if(isBinStack())   printf("Stack");
    else if(isBinOther())   printf("Other");
    else if(isBinCache())   printf("Cache");
    else if(isBinString())  printf("String");
    else if(isBinByte())    printf("Byte");
    else if(isBinBytev())   printf("Bytev");
    else if(isBinWord())    printf("Word");
    else if(isBinWordv())   printf("Wordv");
    else if(isBinDword())   printf("Dword");
    else if(isBinDwordv())  printf("Dwordv");
    else if(isBinQword())   printf("Qword");
    else if(isBinQwordv())  printf("Qwordv");
    else if(isBinSingle())  printf("Single");
    else if(isBinSinglev()) printf("Singlev");
    else if(isBinSingles()) printf("Singles");
    else if(isBinDouble())  printf("Double");
    else if(isBinDoublev()) printf("Doublev");
    else if(isBinDoubles()) printf("Doubles");
    printf("\n");
}

bool X86Instruction::controlFallsThrough(){
    if (isHalt() ||
        isReturn() ||
        isUnconditionalBranch() ||
        isJumpTableBase()){
        return false;
    }
    return true;
}


OperandX86::OperandX86(X86Instruction* inst, struct ud_operand* init, uint32_t idx){
    instruction = inst;
    memcpy(&entry, init, sizeof(struct ud_operand));
    operandIndex = idx;

    verify();
}

bool OperandX86::verify(){
    if (GET(size)){
        if (GET(size) != 8 &&
            GET(size) != 16 &&
            GET(size) != 32 &&
            GET(size) != 48 &&
            GET(size) != 64 &&
            GET(size) != 80 &&
            GET(size) != 128 &&
            GET(size) != 256){
            print();
            PRINT_ERROR("Illegal operand size %d", GET(size));
            return false;
        }
    }

    return true;
}

X86Instruction::~X86Instruction(){
    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        if (operands[i]){
            delete operands[i];
        }
    }
    delete[] operands;

    if (addressAnchor){
        delete addressAnchor;
    }

    if (liveIns){
        delete liveIns;
    }
    if (liveOuts){
        delete liveOuts;
    }
    if (rawBytes){
        delete[] rawBytes;
    }
}

OperandX86* X86Instruction::getOperand(uint32_t idx){
    ASSERT(operands);
    ASSERT(idx < MAX_OPERANDS && "Index into operand table has a limited range");

    return operands[idx];
}


X86Instruction::X86Instruction(TextObject* cont, uint64_t baseAddr, char* buff, uint8_t src, uint32_t idx, bool is64bit, uint32_t sz)
    : Base(PebilClassType_X86Instruction)
{
    ud_t ud_obj;
    memcpy(&ud_obj, &ud_blank, sizeof(ud_t));
    ud_set_input_buffer(&ud_obj, (uint8_t*)buff, MAX_X86_INSTRUCTION_LENGTH);

    sizeInBytes = ud_disassemble(&ud_obj);
    if (sizeInBytes) {
        copy_ud_to_compact(&entry, &ud_obj);
    } else {
        PRINT_ERROR("Problem doing instruction disassembly");
    }

    ASSERT(sz == sizeInBytes);
    rawBytes = new char[sizeInBytes];
    memcpy(rawBytes, buff, sizeInBytes);

    baseAddress = baseAddr;
    cacheBaseAddress = baseAddr;
    programAddress = baseAddr;
    instructionIndex = idx;
    byteSource = src;
    container = cont;
    addressAnchor = NULL;
    liveIns = NULL;
    liveOuts = NULL;
    defUseDist = 0;

    operands = new OperandX86*[MAX_OPERANDS];

    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        ud_operand op = GET(operand)[i];
        operands[i] = NULL;
        if (op.type){
            operands[i] = new OperandX86(this, &GET(operand)[i], i);
        }
    }

    leader = false;
    defXIter = false;

    setFlags();
    verify();
}

X86Instruction::X86Instruction(TextObject* cont, uint64_t baseAddr, char* buff, uint8_t src, uint32_t idx)
    : Base(PebilClassType_X86Instruction)
{
    ud_t ud_obj;
    memcpy(&ud_obj, &ud_blank, sizeof(ud_t));
    ud_set_input_buffer(&ud_obj, (uint8_t*)buff, MAX_X86_INSTRUCTION_LENGTH);

    sizeInBytes = ud_disassemble(&ud_obj);
    if (sizeInBytes) {
        copy_ud_to_compact(&entry, &ud_obj);
    } else {
        PRINT_ERROR("Problem doing instruction disassembly");
    }
    rawBytes = new char[sizeInBytes];
    memcpy(rawBytes, buff, sizeInBytes);

    baseAddress = baseAddr;
    cacheBaseAddress = baseAddr;
    programAddress = baseAddr;
    instructionIndex = idx;
    byteSource = src;
    container = cont;
    addressAnchor = NULL;
    liveIns = NULL;
    liveOuts = NULL;
    defUseDist = 0;
    
    operands = new OperandX86*[MAX_OPERANDS];

    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        ud_operand op = GET(operand)[i];
        operands[i] = NULL;
        if (op.type){
            operands[i] = new OperandX86(this, &GET(operand)[i], i);
        }
    }

    leader = false;
    defXIter = false;

    setFlags();
    verify();
}

void X86Instruction::print(){
    char flags[11];
    flags[0] = 'r';
    if (usesRelativeAddress()){
        flags[0] = 'R';
    }
    flags[1] = 'c';
    if (isControl()){
        flags[1] = 'C';
    }
    flags[2] = 't';
    if (usesControlTarget()){
        flags[2] = 'T';
    }
    flags[3] = 'b';
    if (isJumpTableBase()){
        flags[3] = 'B';
    }
    flags[4] = 'l';
    if (isLeader()){
        flags[4] = 'L';
    }
    flags[5] = 'i';
    if (usesIndirectAddress()){
        flags[5] = 'I';
    }
    flags[6] = 'm';
    if (isMemoryOperation()){
        flags[6] = 'M';
    }
    flags[7] = 'f';
    if (isFloatPOperation()){
        flags[7] = 'F';
    }
    flags[8] = 'l';
    if (isLoad()){
        flags[8] = 'L';
    }
    flags[9] = 's';
    if (isStore()){
        flags[9] = 'S';
    }

    flags[10] = '\0';

    char hexcode[32];
    for (int32_t i = 0; i < sizeInBytes; i++){
        sprintf(hexcode + (2*i), "%02x", GET(insn_bytes)[i]);
    }

    PRINT_INFOR("%#llx:\t%16s\t%s\tflgs:[%10s]\t-> %#llx", getBaseAddress(), hexcode, GET(insn_buffer), flags, getTargetAddress());

#ifdef PRINT_INSTRUCTION_DETAIL
#ifndef NO_REG_ANALYSIS
    BitSet<uint32_t>* useRegs = getUseRegs();
    BitSet<uint32_t>* defRegs = getDefRegs();

    PRINT_INFO();
    PRINT_OUT("\t\tuses %d registers: ", (*useRegs).size());
    for (uint32_t i = 0; i < X86_ALU_REGS; i++){
        if ((*useRegs).contains(i)){
            PRINT_OUT("reg:%d ", i);
        }
    }    
    PRINT_OUT("\n");
    
    PRINT_INFO();
    PRINT_OUT("\t\tdefs %d registers: ", (*defRegs).size());
    for (uint32_t i = 0; i < X86_ALU_REGS; i++){
        if ((*defRegs).contains(i)){
            PRINT_OUT("reg:%d ", i);
        }
    }    
    PRINT_OUT("\n");

    PRINT_INFOR("Def-use distance %d", defUseDist);
    
    delete useRegs;
    delete defRegs;
#endif
    
    /*
    PRINT_INFOR("\t%s (%d,%d) (%d,%d) (%d,%d) %d", ud_lookup_mnemonic(GET(itab_entry)->mnemonic), GET(itab_entry)->operand1.type, GET(itab_entry)->operand1.size, GET(itab_entry)->operand2.type, GET(itab_entry)->operand2.size, GET(itab_entry)->operand3.type, GET(itab_entry)->operand3.size, GET(itab_entry)->prefix);

    PRINT_INFOR("%d(%s)\t%hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd %hhd", GET(mnemonic), ud_lookup_mnemonic(GET(mnemonic)),
                GET(error), GET(pfx_rex), GET(pfx_seg), GET(pfx_opr), GET(pfx_adr), GET(pfx_lock), GET(pfx_rep),
                GET(pfx_repe), GET(pfx_repne), GET(pfx_insn), GET(default64), GET(opr_mode), GET(adr_mode),
                GET(br_far), GET(br_near), GET(implicit_addr), GET(c1), GET(c2), GET(c3));
    */

    BitSet<uint32_t>* usedRegs = new BitSet<uint32_t>(X86_ALU_REGS);
    touchedRegisters(usedRegs);
    char regs[X86_ALU_REGS+1];
    regs[X86_ALU_REGS] = '\0';
    for (uint32_t i = 0; i < X86_ALU_REGS; i++){
        if (usedRegs->contains(i)){
            regs[i] = 'R';
        } else {
            regs[i] = 'r';
        }
    }
    PRINT_INFOR("General Purpose Register usage -- [%s]", regs);

    delete usedRegs;

    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        ud_operand op = GET(operand)[i];
        if (op.type){
            getOperand(i)->print();
        }
    }
#endif // PRINT_INSTRUCTION_DETAIL
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
    else if (IS_YMM_REG(base))     return RegType_YMM;
    else if (IS_PC_REG(base))      return RegType_PC;
    __SHOULD_NOT_ARRIVE;
    return 0;
}


void OperandX86::print(){
    ud_operand op = entry;

    ASSERT(op.type);

#ifdef PRINT_INSTRUCTION_DETAIL
    PRINT_INFOR("\traw operand %d: type=%d(%d), size=%hhd, position=%hhd, base=%d, index=%d, lval=%#llx, offset=%hhd, scale=%hhd", 
                operandIndex, GET(type), GET(type) - UD_OP_REG, GET(size), GET(position), GET(base), GET(index), op.lval, GET(offset), GET(scale));
#endif // PRINT_INSTRUCTION_DETAIL
    
    char typstr[32];
    char valstr[32];
    bzero(typstr, 32);
    bzero(valstr, 32);

    if (GET(type) == UD_OP_REG){
        sprintf(typstr, "%s\0", ud_regtype_str[regbase_to_type(GET(base))]);
        sprintf(valstr, "%%%s\0", UD_R_NAME_LOOKUP(GET(base)));
    } else if (GET(type) == UD_OP_MEM){
        sprintf(typstr, "%s%d\0", UD_OP_NAME_LOOKUP(GET(type)), op.size);
        if (GET(index)){
            uint8_t scale = GET(scale);
            if (!scale){
                scale++;
            }
            if (GET(base) && getValue()){
                sprintf(valstr, "%#llx + %%%s + (%%%s * %hhd)\0", op.lval, UD_R_NAME_LOOKUP(GET(base)), UD_R_NAME_LOOKUP(GET(index)), scale);
            } else {
                if (GET(base)){
                    sprintf(valstr, "%%%s + (%%%s * %hhd)\0", UD_R_NAME_LOOKUP(GET(base)), UD_R_NAME_LOOKUP(GET(index)), scale);
                } else {
                    sprintf(valstr, "%#llx + (%%%s * %hhd)\0", op.lval, UD_R_NAME_LOOKUP(GET(index)), scale);
                }
            }
        } else {
            if (GET(base)){
                sprintf(valstr, "%%%s + %#llx\0", UD_R_NAME_LOOKUP(GET(base)), op.lval);
            } else {
                if (getInstruction()->GET(pfx_seg)){
                    sprintf(valstr, "%%%s + %#llx\0", ud_reg_tab[getInstruction()->GET(pfx_seg) - UD_R_AL], op.lval);
                } else {
                    sprintf(valstr, "%#llx\0", op.lval);
                }
            }
        }
    } else if (GET(type) == UD_OP_PTR){
        __FUNCTION_NOT_IMPLEMENTED;
    } else if (GET(type) == UD_OP_IMM || GET(type) == UD_OP_JIMM){
        sprintf(typstr, "%s%d\0", UD_OP_NAME_LOOKUP(GET(type)), op.size);
        sprintf(valstr, "%#llx\0", op.lval);
    } else if (GET(type) == UD_OP_CONST){
        sprintf(typstr, "%s\0", UD_OP_NAME_LOOKUP(GET(type)));
        sprintf(valstr, "%d\0", op.lval);
    } else {
        __SHOULD_NOT_ARRIVE;
    }
    
    PRINT_INFOR("\t[op%d] %s: %s", operandIndex, typstr, valstr, GET(index));
}

bool X86Instruction::verify(){

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

    if (cacheBaseAddress != baseAddress){
        PRINT_ERROR("base address cached copy is different than orig");
        return false;
    }

    return true;
}

// TODO: get rid of this function eventually since it remains only as a verification
// that I baked these values into udis86 correctly
void X86Instruction::setFlags()
{
    uint32_t flags_usedef[2];
    bzero(flags_usedef, sizeof(uint32_t) * 2);

    if (!flags_usedef) { __SHOULD_NOT_ARRIVE; }
    __reg_define(flags_usedef, UD_Iaaa, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Iaad, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Iaam, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Iaas, __bit_shift(X86_FLAG_AF), __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Iadc, __bit_shift(X86_FLAG_CF), __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Iadd, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Iand, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Iarpl, 0, __bit_shift(X86_FLAG_ZF));
    __reg_define(flags_usedef, UD_Ibsf, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Ibt, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Ibts, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Ibtr, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Ibtc, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Iclc, 0, __bit_shift(X86_FLAG_CF));
    __reg_define(flags_usedef, UD_Icld, 0, __bit_shift(X86_FLAG_DF));
    __reg_define(flags_usedef, UD_Icli, 0, __bit_shift(X86_FLAG_IF));
    __reg_define(flags_usedef, UD_Icmc, 0, __bit_shift(X86_FLAG_CF));
    __reg_define(flags_usedef, UD_Icmovbe, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Icmovo, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Icmovno, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Icmovb, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Icmovae, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Icmovz, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Icmovnz, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Icmovbe, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Icmova, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Icmovs, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Icmovns, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Icmovp, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Icmovnp, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Icmovl, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Icmovge, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Icmovle, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Icmovg, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Icmp, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Icmpsb, __bit_shift(X86_FLAG_DF), __x86_flagset_alustd);
    //__reg_define(flags_usedef, UD_Icmpsd, __bit_shift(X86_FLAG_DF), __x86_flagset_alustd);
    //__reg_define(flags_usedef, UD_Icmpss, __bit_shift(X86_FLAG_DF), __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Icmpsw, __bit_shift(X86_FLAG_DF), __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Icmpsq, __bit_shift(X86_FLAG_DF), __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Icmpxchg, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Icmpxchg8b, 0, __bit_shift(X86_FLAG_ZF));
    __reg_define(flags_usedef, UD_Icomisd, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Icomiss, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Idaa, __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_CF), __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Idas, __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_CF), __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Idec, 0, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_PF));
    __reg_define(flags_usedef, UD_Idiv, __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_CF), __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Ifcmovb, __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ifcmove, __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ifcmovbe, __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ifcmovu, __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ifcmovnb, __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ifcmovne, __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ifcmovnbe, __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ifcmovnu, __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ifcomi, 0, __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF));
    __reg_define(flags_usedef, UD_Ifcomip, 0, __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF));
    __reg_define(flags_usedef, UD_Ifucomi, 0, __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF));
    __reg_define(flags_usedef, UD_Ifucomip, 0, __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF));
    __reg_define(flags_usedef, UD_Iidiv, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Iimul, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Iinc, 0, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_PF));
    __reg_define(flags_usedef, UD_Iinsb, __bit_shift(X86_FLAG_DF), 0);
    __reg_define(flags_usedef, UD_Iinsw, __bit_shift(X86_FLAG_DF), 0);
    __reg_define(flags_usedef, UD_Iinsd, __bit_shift(X86_FLAG_DF), 0);
    __reg_define(flags_usedef, UD_Iint, 0, __bit_shift(X86_FLAG_TF) | __bit_shift(X86_FLAG_NT));
    __reg_define(flags_usedef, UD_Iint1, 0, __bit_shift(X86_FLAG_TF) | __bit_shift(X86_FLAG_NT));
    __reg_define(flags_usedef, UD_Iint3, 0, __bit_shift(X86_FLAG_TF) | __bit_shift(X86_FLAG_NT));
    __reg_define(flags_usedef, UD_Iinto, __bit_shift(X86_FLAG_OF), __bit_shift(X86_FLAG_TF) | __bit_shift(X86_FLAG_NT));
    __reg_define(flags_usedef, UD_Iucomisd, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Iucomiss, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Iiretw, __bit_shift(X86_FLAG_NT), __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF) | __bit_shift(X86_FLAG_TF) | __bit_shift(X86_FLAG_IF) | __bit_shift(X86_FLAG_DF));
    __reg_define(flags_usedef, UD_Iiretd, __bit_shift(X86_FLAG_NT), __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF) | __bit_shift(X86_FLAG_TF) | __bit_shift(X86_FLAG_IF) | __bit_shift(X86_FLAG_DF));
    __reg_define(flags_usedef, UD_Iiretq, __bit_shift(X86_FLAG_NT), __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF) | __bit_shift(X86_FLAG_TF) | __bit_shift(X86_FLAG_IF) | __bit_shift(X86_FLAG_DF));
    __reg_define(flags_usedef, UD_Ijo, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ijno, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ijb, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ijae, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ije, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ijne, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ijbe, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ija, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ijs, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ijns, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ijp, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ijnp, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ijl, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ijge, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ijle, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ijg, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ijcxz, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ijecxz, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ijrcxz, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ilahf, 0, __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF));
    __reg_define(flags_usedef, UD_Ilar, 0, __bit_shift(X86_FLAG_ZF));
    __reg_define(flags_usedef, UD_Iloop, __bit_shift(X86_FLAG_DF), 0);
    __reg_define(flags_usedef, UD_Iloope, __bit_shift(X86_FLAG_ZF), 0);
    __reg_define(flags_usedef, UD_Iloopnz, __bit_shift(X86_FLAG_ZF), 0);
    __reg_define(flags_usedef, UD_Ilsl, 0, __bit_shift(X86_FLAG_ZF));
    __reg_define(flags_usedef, UD_Imul, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Imovsb, 0, 0);
    __reg_define(flags_usedef, UD_Imovsw, 0, 0);
    __reg_define(flags_usedef, UD_Imovsd, 0, 0);
    __reg_define(flags_usedef, UD_Imovsq, 0, 0);
    __reg_define(flags_usedef, UD_Ineg, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Ior, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Ioutsb, __bit_shift(X86_FLAG_DF), 0);
    __reg_define(flags_usedef, UD_Ioutsw, __bit_shift(X86_FLAG_DF), 0);
    __reg_define(flags_usedef, UD_Ioutsd, __bit_shift(X86_FLAG_DF), 0);
    __reg_define(flags_usedef, UD_Ipcmpestri, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Ipcmpestrm, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Ipcmpistri, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Ipcmpistrm, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Ipopfw, 0, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF) | __bit_shift(X86_FLAG_TF) | __bit_shift(X86_FLAG_IF) | __bit_shift(X86_FLAG_DF) | __bit_shift(X86_FLAG_NT));
    __reg_define(flags_usedef, UD_Ipopfd, 0, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF) | __bit_shift(X86_FLAG_TF) | __bit_shift(X86_FLAG_IF) | __bit_shift(X86_FLAG_DF) | __bit_shift(X86_FLAG_NT));
    __reg_define(flags_usedef, UD_Ipopfq, 0, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF) | __bit_shift(X86_FLAG_TF) | __bit_shift(X86_FLAG_IF) | __bit_shift(X86_FLAG_DF) | __bit_shift(X86_FLAG_NT));
    __reg_define(flags_usedef, UD_Iptest, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Ipushfw,__bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF) | __bit_shift(X86_FLAG_TF) | __bit_shift(X86_FLAG_IF) | __bit_shift(X86_FLAG_DF) | __bit_shift(X86_FLAG_NT), 0);
    __reg_define(flags_usedef, UD_Ipushfd,__bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF) | __bit_shift(X86_FLAG_TF) | __bit_shift(X86_FLAG_IF) | __bit_shift(X86_FLAG_DF) | __bit_shift(X86_FLAG_NT), 0);
    __reg_define(flags_usedef, UD_Ipushfq,__bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF) | __bit_shift(X86_FLAG_TF) | __bit_shift(X86_FLAG_IF) | __bit_shift(X86_FLAG_DF) | __bit_shift(X86_FLAG_NT), 0);
    __reg_define(flags_usedef, UD_Ircl, __bit_shift(X86_FLAG_CF), __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_CF));
    __reg_define(flags_usedef, UD_Ircr, __bit_shift(X86_FLAG_CF), __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_CF));
    __reg_define(flags_usedef, UD_Irol, __bit_shift(X86_FLAG_CF), __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_CF));
    __reg_define(flags_usedef, UD_Iror, __bit_shift(X86_FLAG_CF), __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_CF));
    __reg_define(flags_usedef, UD_Irsm, 0, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF) | __bit_shift(X86_FLAG_TF) | __bit_shift(X86_FLAG_IF) | __bit_shift(X86_FLAG_DF) | __bit_shift(X86_FLAG_NT) | __bit_shift(X86_FLAG_RF));
    __reg_define(flags_usedef, UD_Isahf, __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Isar, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Isbb, __bit_shift(X86_FLAG_CF), __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Ishr, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Iseto, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Isetno, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Isetnp, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Isetb, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Isetnb, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Isetz, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Isetnz, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Isetbe, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Iseta, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Isets, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Isetns, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Isetns, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Isetp, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Isetl, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Isetge, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Isetle, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Isetg, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ishld, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Ishrd, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Isal, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Ishl, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Istc, 0, __bit_shift(X86_FLAG_CF));
    __reg_define(flags_usedef, UD_Istd, 0, __bit_shift(X86_FLAG_DF));
    __reg_define(flags_usedef, UD_Isti, 0, __bit_shift(X86_FLAG_IF));
    __reg_define(flags_usedef, UD_Istosb, __bit_shift(X86_FLAG_DF), 0);
    __reg_define(flags_usedef, UD_Istosw, __bit_shift(X86_FLAG_DF), 0);
    __reg_define(flags_usedef, UD_Istosq, __bit_shift(X86_FLAG_DF), 0);
    __reg_define(flags_usedef, UD_Istosd, __bit_shift(X86_FLAG_DF), 0);
    __reg_define(flags_usedef, UD_Isub, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Iverr, 0, __bit_shift(X86_FLAG_ZF));
    __reg_define(flags_usedef, UD_Iverw, 0, __bit_shift(X86_FLAG_ZF));
    __reg_define(flags_usedef, UD_Ixadd, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Ixor, 0, __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Iscasb, __bit_shift(X86_FLAG_DF), 0);
    __reg_define(flags_usedef, UD_Iscasw, __bit_shift(X86_FLAG_DF), 0);
    __reg_define(flags_usedef, UD_Iscasq, __bit_shift(X86_FLAG_DF), 0);
    __reg_define(flags_usedef, UD_Iscasd, __bit_shift(X86_FLAG_DF), 0);
    __reg_define(flags_usedef, UD_Itest, 0, __x86_flagset_alustd);

    // these instructions have 2 versions: 1 is a string instruction that implicitly uses DF, the other is an SSE instruction
    if (GET(mnemonic) == UD_Imovsb || GET(mnemonic) == UD_Imovsw || GET(mnemonic) == UD_Imovsd || GET(mnemonic) == UD_Imovsq){
        // has no operands -- they must be implicit
        if (!getOperand(CMP_SRC1_OPERAND) && !getOperand(CMP_SRC2_OPERAND)){
            flags_usedef[__reg_use] = __bit_shift(X86_FLAG_DF);
        }
    }

    if (flags_usedef[__reg_use] && GET(flags_use) != flags_usedef[__reg_use]){
        print();
        PRINT_ERROR("NEW USE FLAGS (%#x) DONT MATCH OLD (%#x)", GET(flags_use), flags_usedef[__reg_use]);
    }
    if (flags_usedef[__reg_def] && GET(flags_def) != flags_usedef[__reg_def]){
        print();
        PRINT_ERROR("NEW DEF FLAGS (%#x) DONT MATCH OLD (%#x)", GET(flags_def), flags_usedef[__reg_def]);
    }
}

X86InstructionType X86Instruction::getInstructionType(){
    X86InstructionType x = X86InstructionClassifier::getInstructionType(this);
    //X86InstructionClassifier::print(this);
    return x;
}


struct x86class {
    ud_mnemonic_code mnemonic;
    X86InstructionType type;
    X86InstructionBin bin;
    X86OperandFormat format;
    uint8_t memsize;
    uint8_t location;
};

    /* macros to make the table assignment statements easier to write + more concise */
#define xbin(__bin) X86InstructionBin_ ## __bin
#define xtyp(__typ) X86InstructionType_ ## __typ
#define xfmt(__fmt) X86OperandFormat_ ## __fmt
#define xsiz(__bits) (__bits >> 3)
#define X86InstructionBin_0 X86InstructionBin_unknown
#define X86OperandFormat_0 X86OperandFormat_unknown
#define MEM_SZ_VARIABLE (0xf)
#define VRSZ (MEM_SZ_VARIABLE << 3)
#define mkclass(__mne, __typ, __bin, __fmt, __mem, __loc) \
    { UD_I ## __mne, xtyp(__typ), xbin(__bin), xfmt(__fmt), xsiz(__mem), __loc >> 8 },

static struct x86class classifications[UD_Itotaltypes] = {
    //               mnemonic,     type      bin  fmt msize  mloc
    mkclass(           3dnow,  special,   other,   0,    0,    0)
    mkclass(             aaa,      int,   other,   0,    0,    0)
    mkclass(             aad,      int,   other,   0,    0,    0)
    mkclass(             aam,      int,   other,   0,    0,    0)
    mkclass(             aas,      int,   other,   0,    0,    0)
    mkclass(             adc,      int,     int,   0, VRSZ,    0)
    mkclass(             add,      int,     int,   0, VRSZ,    0)
    mkclass(           addpd,    float,  floatv,   0,   64,    0)
    mkclass(           addps,    float,  floatv,   0,   32,    0)
    mkclass(           addsd,    float,  floats,   0,   64,    0)
    mkclass(           addss,    float,  floats,   0,   32,    0)
    mkclass(        addsubpd,    float,  floatv,   0,   64,    0)
    mkclass(        addsubps,    float,  floatv,   0,   32,    0)
    mkclass(          aesdec,      aes,       0,   0,    0,    0)
    mkclass(      aesdeclast,      aes,       0,   0,    0,    0)
    mkclass(          aesenc,      aes,       0,   0,    0,    0)
    mkclass(      aesenclast,      aes,       0,   0,    0,    0)
    mkclass(          aesimc,      aes,       0,   0,    0,    0)
    mkclass( aeskeygenassist,      aes,       0,   0,    0,    0)
    mkclass(             and,      int,     bin,   0, VRSZ,    0)
    mkclass(          andnpd,    float,    binv,   0,   64,    0)
    mkclass(          andnps,    float,    binv,   0,   32,    0)
    mkclass(           andpd,    float,    binv,   0,   64,    0)
    mkclass(           andps,    float,    binv,   0,   32,    0)
    mkclass(            arpl,  special,   other,   0,    0,    0)
    mkclass(         blendpd,      int,       0,   0,    0,    0)
    mkclass(         blendps,      int,       0,   0,    0,    0)
    mkclass(        blendvpd,      int,       0,   0,    0,    0)
    mkclass(        blendvps,      int,       0,   0,    0,    0)
    mkclass(           bound,      int,     bin,   0, VRSZ,    0)
    mkclass(             bsf,      int,     bin,   0, VRSZ,    0)
    mkclass(             bsr,      int,     bin,   0, VRSZ,    0)
    mkclass(           bswap,      int,     bin,   0, VRSZ,    0)
    mkclass(              bt,      int,     bin,   0, VRSZ,    0)
    mkclass(             btc,      int,     bin,   0, VRSZ,    0)
    mkclass(             btr,      int,     bin,   0, VRSZ,    0)
    mkclass(             bts,      int,     bin,   0, VRSZ,    0)
    mkclass(            call,     call,  uncond,   0,    0,    BinFrame)
    mkclass(             cbw,  special,     bin,   0,   16,    0)
    mkclass(             cdq,  special,     bin,   0,   64,    0)
    mkclass(            cdqe,  special,     bin,   0,   64,    0)
    mkclass(             clc,  special,   other,   0,    0,    0)
    mkclass(             cld,  special,   other,   0,    0,    0)
    mkclass(         clflush,  special,   cache,   0,    0,    0)
    mkclass(            clgi,  special,   other,   0,    0,    0)
    mkclass(             cli,  special,   other,   0,    0,    0)
    mkclass(            clts,  special,   other,   0,    0,    0)
    mkclass(             cmc,  special,   other,   0,    0,    0)
    mkclass(           cmova,     move,    move,   0, VRSZ,    0)
    mkclass(          cmovae,     move,    move,   0, VRSZ,    0)
    mkclass(           cmovb,     move,    move,   0, VRSZ,    0)
    mkclass(          cmovbe,     move,    move,   0, VRSZ,    0)
    mkclass(           cmovg,     move,    move,   0, VRSZ,    0)
    mkclass(          cmovge,     move,    move,   0, VRSZ,    0)
    mkclass(           cmovl,     move,    move,   0, VRSZ,    0)
    mkclass(          cmovle,     move,    move,   0, VRSZ,    0)
    mkclass(          cmovno,     move,    move,   0, VRSZ,    0)
    mkclass(          cmovnp,     move,    move,   0, VRSZ,    0)
    mkclass(          cmovns,     move,    move,   0, VRSZ,    0)
    mkclass(          cmovnz,     move,    move,   0, VRSZ,    0)
    mkclass(           cmovo,     move,    move,   0, VRSZ,    0)
    mkclass(           cmovp,     move,    move,   0, VRSZ,    0)
    mkclass(           cmovs,     move,    move,   0, VRSZ,    0)
    mkclass(           cmovz,     move,    move,   0, VRSZ,    0)
    mkclass(             cmp,      int,     int,   0, VRSZ,    0)
    mkclass(           cmppd,    float,  floatv,   0,   64,    0)
    mkclass(           cmpps,    float,  floatv,   0,   32,    0)
    mkclass(           cmpsb,   string,  string,  si,    8,    0)
    // TODO: 2 instructions covered by this... need to handle special
    mkclass(           cmpsd,    float,  string,  si,   32,    0)
    mkclass(           cmpsq,   string,  string,  si,   64,    0)
    mkclass(           cmpss,    float,  floats,   0,   32,    0)
    mkclass(           cmpsw,   string,  string,  si,   16,    0)
    mkclass(         cmpxchg,      int,     int,   0, VRSZ,    0)
    mkclass(       cmpxchg8b,      int,     int,   0, VRSZ,    0)
    mkclass(          comisd,    float,  floats,   0,   64,    0)
    mkclass(          comiss,    float,  floats,   0,   32,    0)
    mkclass(           cpuid,  special,   other,   0,    0,    0)
    mkclass(             cqo,  special,     bin,   0,   64,    0)
    mkclass(        cvtdq2pd,    float,  floatv,   0,   64,    0)
    mkclass(        cvtdq2ps,    float,  floatv,   0,   32,    0)
    mkclass(        cvtpd2dq,    float,  floatv,   0,   64,    0)
    mkclass(        cvtpd2pi,    float,  floatv,   0,   64,    0)
    mkclass(        cvtpd2ps,    float,  floatv,   0,   32,    0)
    mkclass(        cvtpi2pd,    float,  floatv,   0,   64,    0)
    mkclass(        cvtpi2ps,    float,  floatv,   0,   32,    0)
    mkclass(        cvtps2dq,    float,  floatv,   0,   32,    0)
    mkclass(        cvtps2pd,    float,  floatv,   0,   64,    0)
    mkclass(        cvtps2pi,    float,  floatv,   0,   32,    0)
    mkclass(        cvtsd2si,    float,  floats,   0,   64,    0)
    mkclass(        cvtsd2ss,    float,  floats,   0,   64,    0)
    mkclass(        cvtsi2sd,    float,  floats,   0,   64,    0)
    mkclass(        cvtsi2ss,    float,  floats,   0,   32,    0)
    mkclass(        cvtss2sd,    float,  floats,   0,   64,    0)
    mkclass(        cvtss2si,    float,  floats,   0,   32,    0)
    mkclass(       cvttpd2dq,    float,  floatv,   0,   64,    0)
    mkclass(       cvttpd2pi,    float,  floatv,   0,   64,    0)
    mkclass(       cvttps2dq,    float,  floatv,   0,   32,    0)
    mkclass(       cvttps2pi,    float,  floatv,   0,   32,    0)
    mkclass(       cvttsd2si,    float,  floats,   0,   32,    0)
    mkclass(       cvttss2si,    float,  floats,   0,   32,    0)
    mkclass(             cwd,  special,     bin,   0,   32,    0)
    mkclass(            cwde,  special,     bin,   0,   32,    0)
    mkclass(             daa,      int,   other,   0,    0,    0)
    mkclass(             das,      int,   other,   0,    0,    0)
    mkclass(              db,  invalid, invalid,   0,    0,    0)
    mkclass(             dec,      int,     int,   0, VRSZ,    0)
    mkclass(             div,      int,     int,   0, VRSZ,    0)
    mkclass(           divpd,    float,  floatv,   0,   64,    0)
    mkclass(           divps,    float,  floatv,   0,   32,    0)
    mkclass(           divsd,    float,  floats,   0,   64,    0)
    mkclass(           divss,    float,  floats,   0,   32,    0)
    mkclass(            dppd,     simd,       0,   0,    0,    0)
    mkclass(            dpps,     simd,       0,   0,    0,    0)
    mkclass(            emms,  special,   other,   0,    0,    0)
    mkclass(           enter,  special,   stack,   0,    0,    BinFrame)
    mkclass(       extractps,     simd,       0,   0,    0,    0)
    mkclass(           f2xm1,    float,   float,   0, VRSZ,    0)
    mkclass(            fabs,    float,   float,   0, VRSZ,    0)
    mkclass(            fadd,    float,   float,   0, VRSZ,    0)
    mkclass(           faddp,    float,   float,   0, VRSZ,    0)
    mkclass(            fbld,    float,   float,   0, VRSZ,    0)
    mkclass(           fbstp,    float,   float,   0, VRSZ,    0)
    mkclass(            fchs,    float,   float,   0, VRSZ,    0)
    mkclass(           fclex,    float,   float,   0, VRSZ,    0)
    mkclass(          fcmovb,     move,    move,   0, VRSZ,    0)
    mkclass(         fcmovbe,     move,    move,   0, VRSZ,    0)
    mkclass(          fcmove,     move,    move,   0, VRSZ,    0)
    mkclass(         fcmovnb,     move,    move,   0, VRSZ,    0)
    mkclass(        fcmovnbe,     move,    move,   0, VRSZ,    0)
    mkclass(         fcmovne,     move,    move,   0, VRSZ,    0)
    mkclass(         fcmovnu,     move,    move,   0, VRSZ,    0)
    mkclass(          fcmovu,     move,    move,   0, VRSZ,    0)
    mkclass(            fcom,    float,   float,   0, VRSZ,    0)
    mkclass(           fcom2,    float,   float,   0, VRSZ,    0)
    mkclass(           fcomi,    float,   float,   0, VRSZ,    0)
    mkclass(          fcomip,    float,   float,   0, VRSZ,    0)
    mkclass(           fcomp,    float,   float,   0, VRSZ,    0)
    mkclass(          fcomp3,    float,   float,   0, VRSZ,    0)
    mkclass(          fcomp5,    float,   float,   0, VRSZ,    0)
    mkclass(          fcompp,    float,   float,   0, VRSZ,    0)
    mkclass(            fcos,    float,   float,   0, VRSZ,    0)
    mkclass(         fdecstp,    float,   float,   0, VRSZ,    0)
    mkclass(            fdiv,    float,   float,   0, VRSZ,    0)
    mkclass(           fdivp,    float,   float,   0, VRSZ,    0)
    mkclass(           fdivr,    float,   float,   0, VRSZ,    0)
    mkclass(          fdivrp,    float,   float,   0, VRSZ,    0)
    mkclass(           femms,    float,   other,   0,    0,    0)
    mkclass(           ffree,    float,   other,   0,    0,    0)
    mkclass(          ffreep,    float,   other,   0,    0,    0)
    mkclass(           fiadd,    float,   float,   0, VRSZ,    0)
    mkclass(           ficom,    float,   float,   0, VRSZ,    0)
    mkclass(          ficomp,    float,   float,   0, VRSZ,    0)
    mkclass(           fidiv,    float,   float,   0, VRSZ,    0)
    mkclass(          fidivr,    float,   float,   0, VRSZ,    0)
    mkclass(            fild,     move,    move,  di, VRSZ,    0)
    mkclass(           fimul,    float,   float,   0, VRSZ,    0)
    mkclass(            fist,     move,    move,   0, VRSZ,    0)
    mkclass(           fistp,     move,    move,   0, VRSZ,    0)
    mkclass(          fisttp,     move,    move,   0, VRSZ,    0)
    mkclass(           fisub,    float,   float,   0, VRSZ,    0)
    mkclass(          fisubr,    float,   float,   0, VRSZ,    0)
    mkclass(             fld,     move,    move,  di, VRSZ,    0)
    mkclass(            fld1,    float,    move,   0, VRSZ,    0)
    mkclass(           fldcw,  special,   other,   0,    0,    0)
    mkclass(          fldenv,  special,   other,   0,    0,    0)
    mkclass(          fldl2e,     move,    move,  di, VRSZ,    0)
    mkclass(          fldl2t,     move,    move,  di, VRSZ,    0)
    mkclass(          fldlg2,     move,    move,  di, VRSZ,    0)
    mkclass(          fldln2,     move,    move,  di, VRSZ,    0)
    mkclass(          fldlpi,     move,    move,  di, VRSZ,    0)
    mkclass(            fldz,    float,    move,  di, VRSZ,    0)
    mkclass(            fmul,    float,   float,   0, VRSZ,    0)
    mkclass(           fmulp,    float,   float,   0, VRSZ,    0)
    mkclass(          fncstp,    float,   other,   0,    0,    0)
    mkclass(          fninit,    float,   other,   0,    0,    0)
    mkclass(            fnop,      nop,   other,   0,    0,    0)
    mkclass(          fnsave,  special,   stack,   0,    0,    BinFrame)
    mkclass(          fnstcw,  special,   stack,   0,    0,    BinFrame)
    mkclass(         fnstenv,  special,   stack,   0,    0,    BinFrame)
    mkclass(          fnstsw,  special,   stack,   0,    0,    BinFrame)
    mkclass(          fpatan,    float,   float,   0, VRSZ,    0)
    mkclass(           fprem,    float,   float,   0, VRSZ,    0)
    mkclass(          fprem1,    float,   float,   0, VRSZ,    0)
    mkclass(           fptan,    float,   float,   0, VRSZ,    0)
    mkclass(        fpxtract,    float,   float,   0, VRSZ,    0)
    mkclass(         frndint,    float,   float,   0, VRSZ,    0)
    mkclass(          frstor,  special,   stack,   0,    0,    BinFrame)
    mkclass(          fscale,    float,   float,   0, VRSZ,    0)
    mkclass(            fsin,    float,   float,   0, VRSZ,    0)
    mkclass(         fsincos,    float,   float,   0, VRSZ,    0)
    mkclass(           fsqrt,    float,   float,   0, VRSZ,    0)
    mkclass(             fst,     move,    move,   0, VRSZ,    0)
    mkclass(            fstp,     move,    move,   0, VRSZ,    0)
    mkclass(           fstp1,     move,    move,   0, VRSZ,    0)
    mkclass(           fstp8,     move,    move,   0, VRSZ,    0)
    mkclass(           fstp9,     move,    move,   0, VRSZ,    0)
    mkclass(            fsub,    float,   float,   0, VRSZ,    0)
    mkclass(           fsubp,    float,   float,   0, VRSZ,    0)
    mkclass(           fsubr,    float,   float,   0, VRSZ,    0)
    mkclass(          fsubrp,    float,   float,   0, VRSZ,    0)
    mkclass(            ftst,    float,   float,   0, VRSZ,    0)
    mkclass(           fucom,    float,   float,   0, VRSZ,    0)
    mkclass(          fucomi,    float,   float,   0, VRSZ,    0)
    mkclass(         fucomip,    float,   float,   0, VRSZ,    0)
    mkclass(          fucomp,    float,   float,   0, VRSZ,    0)
    mkclass(         fucompp,    float,   float,   0, VRSZ,    0)
    mkclass(            fxam,    float,   float,   0, VRSZ,    0)
    mkclass(            fxch,     move,    move,  di, VRSZ,    0)
    mkclass(           fxch4,     move,    move,  di, VRSZ,    0)
    mkclass(           fxch7,     move,    move,  di, VRSZ,    0)
    mkclass(         fxrstor,  special,   stack,   0,    0,    BinFrame)
    mkclass(          fxsave,  special,   stack,   0,    0,    BinFrame)
    mkclass(           fyl2x,    float,   float,   0, VRSZ,    0)
    mkclass(         fyl2xp1,    float,   float,   0, VRSZ,    0)
    mkclass(          haddpd,    float,  floatv,   0,   64,    0)
    mkclass(          haddps,    float,  floatv,   0,   32,    0)
    mkclass(             hlt,     halt,   other,   0,    0,    0)
    mkclass(          hsubpd,    float,  floatv,   0,   64,    0)
    mkclass(          hsubps,    float,  floatv,   0,   32,    0)
    mkclass(            idiv,      int,     int,   0, VRSZ,    0)
    mkclass(            imul,      int,     int,   0, VRSZ,    0)
    mkclass(              in,       io,   other,   0,    0,    0)
    mkclass(             inc,      int,     int,   0, VRSZ,    0)
    mkclass(            insb,       io,   other,  di,    0,    0)
    mkclass(            insd,       io,   other,  di,    0,    0)
    mkclass(        insertps,     simd,       0,   0,    0,    0)
    mkclass(            insw,       io,   other,  di,    0,    0)
    mkclass(             int,     trap,  system,   0,    0,    BinFrame)
    mkclass(            int1,     trap,  system,   0,    0,    BinFrame)
    mkclass(            int3,     trap,  system,   0,    0,    BinFrame)
    mkclass(            into,     trap,  system,   0,    0,    BinFrame)
    mkclass(         invalid,  invalid, invalid,   0,    0,    0)
    mkclass(            invd,  special,   cache,   0,    0,    0)
    mkclass(          invlpg,  special,   cache,   0,    0,    0)
    mkclass(         invlpga,  special,   cache,   0,    0,    0)
    mkclass(           iretd,   return,  system,   0,    0,    BinFrame)
    mkclass(           iretq,   return,  system,   0,    0,    BinFrame)
    mkclass(           iretw,   return,  system,   0,    0,    BinFrame)
    mkclass(              ja,   condbr,    cond,   0,    0,    0)
    mkclass(             jae,   condbr,    cond,   0,    0,    0)
    mkclass(              jb,   condbr,    cond,   0,    0,    0)
    mkclass(             jbe,   condbr,    cond,   0,    0,    0)
    mkclass(            jcxz,   condbr,    cond,   0,    0,    0)
    mkclass(              je,   condbr,    cond,   0,    0,    0)
    mkclass(           jecxz,   condbr,    cond,   0,    0,    0)
    mkclass(              jg,   condbr,    cond,   0,    0,    0)
    mkclass(             jge,   condbr,    cond,   0,    0,    0)
    mkclass(              jl,   condbr,    cond,   0,    0,    0)
    mkclass(             jle,   condbr,    cond,   0,    0,    0)
    mkclass(             jmp, uncondbr,  uncond,   0,    0,    0)
    mkclass(             jne,   condbr,    cond,   0,    0,    0)
    mkclass(             jno,   condbr,    cond,   0,    0,    0)
    mkclass(             jnp,   condbr,    cond,   0,    0,    0)
    mkclass(             jns,   condbr,    cond,   0,    0,    0)
    mkclass(              jo,   condbr,    cond,   0,    0,    0)
    mkclass(              jp,   condbr,    cond,   0,    0,    0)
    mkclass(           jrcxz,   condbr,    cond,   0,    0,    0)
    mkclass(              js,   condbr,    cond,   0,    0,    0)
    mkclass(            lahf,      int,   other,   0,    0,    0)
    mkclass(             lar,  special,   other,   0,    0,    0)
    mkclass(           lddqu,     move,    move,   0, VRSZ,    0)
    mkclass(         ldmxcsr,     move,    move,  di,   32,    0)
    mkclass(             lds,     move,    move,   0, VRSZ,    0)
    mkclass(             lea,     move,    move,   0, VRSZ,    0)
    mkclass(           leave,  special,   stack,   0,    0,    BinFrame)
    mkclass(             les,     move,    move,   0, VRSZ,    0)
    mkclass(          lfence,  special,   other,   0,    0,    0)
    mkclass(             lfs,     move,    move,   0, VRSZ,    0)
    mkclass(            lgdt,  special,   other,   0,    0,    0)
    mkclass(             lgs,     move,    move,   0, VRSZ,    0)
    mkclass(            lidt,  special,   other,   0,    0,    0)
    mkclass(            lldt,  special,   other,   0,    0,    0)
    mkclass(            lmsw,  special,   other,   0,    0,    0)
    mkclass(            lock,  special,   other,   0,    0,    0)
    mkclass(           lodsb,     move,    move,  di,    8,    0)
    mkclass(           lodsd,     move,    move,  di,   32,    0)
    mkclass(           lodsq,     move,    move,  di,   64,    0)
    mkclass(           lodsw,     move,    move,  di,   16,    0)
    mkclass(            loop,  special,   other,   0,    0,    0)
    mkclass(           loope,  special,   other,   0,    0,    0)
    mkclass(          loopnz,  special,   other,   0,    0,    0)
    mkclass(             lsl,  special,   other,   0,    0,    0)
    mkclass(             lss,      int,    move,   0, VRSZ,    0)
    mkclass(             ltr,  special,   other,   0,    0,    0)
    mkclass(      maskmovdqu,     move,       0,   0,    0,    0)
    mkclass(        maskmovq,     move,    move,   0,   64,    0)
    mkclass(           maxpd,    float,  floatv,   0,   64,    0)
    mkclass(           maxps,    float,  floatv,   0,   32,    0)
    mkclass(           maxsd,    float,  floats,   0,   64,    0)
    mkclass(           maxss,    float,  floats,   0,   32,    0)
    mkclass(          mfence,  special,   other,   0,    0,    0)
    mkclass(           minpd,    float,  floatv,   0,   64,    0)
    mkclass(           minps,    float,  floatv,   0,   32,    0)
    mkclass(           minsd,    float,  floats,   0,   64,    0)
    mkclass(           minss,    float,  floats,   0,   32,    0)
    mkclass(         monitor,  special,   other,   0,    0,    0)
    mkclass(             mov,     move,    move,   0, VRSZ,    0)
    mkclass(          movapd,     move,    move,   0, VRSZ,    0)
    mkclass(          movaps,     move,    move,   0, VRSZ,    0)
    mkclass(            movd,     move,    move,   0,   64,    0)
    mkclass(         movddup,     move,    move,   0, VRSZ,    0)
    mkclass(         movdq2q,     move,    move,   0,   64,    0)
    mkclass(          movdqa,     move,    move,   0, VRSZ,    0)
    mkclass(          movdqu,     move,    move,   0, VRSZ,    0)
    mkclass(         movhlps,     move,    move,   0,   32,    0)
    mkclass(          movhpd,     move,    move,   0,   64,    0)
    mkclass(          movhps,     move,    move,   0,   32,    0)
    mkclass(         movlhps,     move,    move,   0,   32,    0)
    mkclass(          movlpd,     move,    move,   0,   64,    0)
    mkclass(          movlps,     move,    move,   0,   32,    0)
    mkclass(        movmskpd,     move,    move,   0, VRSZ,    0)
    mkclass(        movmskps,     move,    move,   0, VRSZ,    0)
    mkclass(         movntdq,     move,    move,   0, VRSZ,    0)
    mkclass(        movntdqa,     move,       0,   0,    0,    0)
    mkclass(          movnti,     move,    move,   0, VRSZ,    0)
    mkclass(         movntpd,     move,    move,   0, VRSZ,    0)
    mkclass(         movntps,     move,    move,   0, VRSZ,    0)
    mkclass(          movntq,     move,    move,   0,   64,    0)
    mkclass(            movq,     move,    move,   0,   64,    0)
    mkclass(         movq2dq,     move,    move,   0,   64,    0)
    mkclass(           movsb,   string,  string, dsi,    8,    0)
    // TODO: 2 instructions covered by this... need to handle special
    mkclass(           movsd,   string,    move, dsi,   64,    0)
    mkclass(        movshdup,     move,    move,   0, VRSZ,    0)
    mkclass(        movsldup,     move,    move,   0, VRSZ,    0)
    mkclass(           movsq,   string,  string, dsi,   64,    0)
    mkclass(           movss,     move,    move,   0,   32,    0)
    mkclass(           movsw,   string,  string, dsi,   16,    0)
    mkclass(           movsx,     move,    move,   0, VRSZ,    0)
    mkclass(          movsxd,     move,    move,   0, VRSZ,    0)
    mkclass(          movupd,     move,    move,   0, VRSZ,    0)
    mkclass(          movups,     move,    move,   0, VRSZ,    0)
    mkclass(           movzx,     move,    move,   0, VRSZ,    0)
    mkclass(         mpsadbw,     simd,       0,   0,    0,    0)
    mkclass(             mul,      int,     int,   0, VRSZ,    0)
    mkclass(           mulpd,    float,  floatv,   0,   64,    0)
    mkclass(           mulps,    float,  floatv,   0,   32,    0)
    mkclass(           mulsd,    float,  floats,   0,   64,    0)
    mkclass(           mulss,    float,  floats,   0,   32,    0)
    mkclass(           mwait,  special,   other,   0,    0,    0)
    mkclass(             neg,      int,     int,   0, VRSZ,    0)
    mkclass(             nop,      nop,   other,   0,    0,    0)
    mkclass(             not,      int,     bin,   0, VRSZ,    0)
    mkclass(              or,      int,     bin,   0, VRSZ,    0)
    mkclass(            orpd,    float,    binv,   0,   64,    0)
    mkclass(            orps,    float,    binv,   0,   32,    0)
    mkclass(             out,       io,   other,   0,    0,    0)
    mkclass(           outsb,       io,   other,   0,    0,    0)
    mkclass(           outsd,       io,   other,   0,    0,    0)
    mkclass(           outsq,       io,   other,   0,    0,    0)
    mkclass(           outsw,       io,   other,   0,    0,    0)
    mkclass(           pabsb,     simd,       0,   0,    0,    0)
    mkclass(           pabsd,     simd,       0,   0,    0,    0)
    mkclass(           pabsw,     simd,       0,   0,    0,    0)
    mkclass(        packssdw,      int,    intv,   0,   32,    0)
    mkclass(        packsswb,      int,    intv,   0,    8,    0)
    mkclass(        packusdw,     simd,       0,   0,    0,    0)
    mkclass(        packuswb,      int,    intv,   0,    8,    0)
    mkclass(           paddb,      int,    intv,   0,    8,    0)
    mkclass(           paddd,      int,    intv,   0,   32,    0)
    mkclass(           paddq,      int,    intv,   0,   64,    0)
    mkclass(          paddsb,      int,    intv,   0,    8,    0)
    mkclass(          paddsw,      int,    intv,   0,   16,    0)
    mkclass(         paddusb,      int,    intv,   0,    8,    0)
    mkclass(         paddusw,      int,    intv,   0,   16,    0)
    mkclass(           paddw,      int,    intv,   0,   16,    0)
    mkclass(         palignr,     simd,    binv,   0, VRSZ,    0)
    mkclass(            pand,      int,    binv,   0, VRSZ,    0)
    mkclass(           pandn,      int,    binv,   0, VRSZ,    0)
    mkclass(           pause,  special,   other,   0,    0,    0)
    mkclass(           pavgb,      int,    intv,   0,    8,    0)
    mkclass(         pavgusb,      int,    intv,   0,   16,    0)
    mkclass(           pavgw,      int,    intv,   0,   16,    0)
    mkclass(        pblendvb,      int,       0,   0,    0,    0)
    mkclass(         pblendw,      int,       0,   0,    0,    0)
    mkclass(       pclmulqdq,     simd,       0,   0,    0,    0)
    mkclass(         pcmpeqb,      int,    intv,   0,    8,    0)
    mkclass(         pcmpeqd,      int,    intv,   0,   32,    0)
    mkclass(         pcmpeqq,     simd,       0,   0,    0,    0)
    mkclass(         pcmpeqw,      int,    intv,   0,   16,    0)
    mkclass(       pcmpestri,     simd,       0,   0,    0,    0)
    mkclass(       pcmpestrm,     simd,       0,   0,    0,    0)
    mkclass(         pcmpgtb,      int,    intv,   0,    8,    0)
    mkclass(         pcmpgtd,      int,    intv,   0,   32,    0)
    mkclass(         pcmpgtq,      int,       0,   0,    0,    0)
    mkclass(         pcmpgtw,      int,    intv,   0,   16,    0)
    mkclass(       pcmpistri,     simd,       0,   0,    0,    0)
    mkclass(       pcmpistrm,     simd,       0,   0,    0,    0)
    mkclass(          pextrb,     simd,       0,   0,    0,    0)
    mkclass(          pextrd,     simd,       0,   0,    0,    0)
    mkclass(          pextrq,     simd,       0,   0,    0,    0)
    mkclass(          pextrw,     simd,    intv,   0,   16,    0)
    mkclass(           pf2id,      int,    intv,   0,   32,    0)
    mkclass(           pf2iw,      int,    intv,   0,   16,    0)
    mkclass(           pfacc,      int,    intv,   0,   32,    0)
    mkclass(           pfadd,      int,    intv,   0,   32,    0)
    mkclass(         pfcmpeq,      int,    intv,   0,   32,    0)
    mkclass(         pfcmpge,      int,    intv,   0,   32,    0)
    mkclass(         pfcmpgt,      int,    intv,   0,   32,    0)
    mkclass(           pfmax,      int,    intv,   0,   32,    0)
    mkclass(           pfmin,      int,    intv,   0,   32,    0)
    mkclass(           pfmul,      int,    intv,   0,   32,    0)
    mkclass(          pfnacc,      int,    intv,   0,   32,    0)
    mkclass(         pfpnacc,      int,    intv,   0,   32,    0)
    mkclass(           pfrcp,      int,    intv,   0,   32,    0)
    mkclass(        pfrcpit1,      int,    intv,   0,   32,    0)
    mkclass(        pfrcpit2,      int,    intv,   0,   32,    0)
    mkclass(        pfrspit1,      int,    intv,   0,   32,    0)
    mkclass(         pfrsqrt,      int,    intv,   0,   32,    0)
    mkclass(           pfsub,      int,    intv,   0,   32,    0)
    mkclass(          pfsubr,      int,    intv,   0,   32,    0)
    mkclass(          phaddd,     simd,    intv,   0,   32,    0)
    mkclass(         phaddsw,     simd,       0,   0,    0,    0)
    mkclass(          phaddw,     simd,       0,   0,    0,    0)
    mkclass(      phminposuw,     simd,       0,   0,    0,    0)
    mkclass(          phsubd,     simd,       0,   0,    0,    0)
    mkclass(         phsubsw,     simd,       0,   0,    0,    0)
    mkclass(          phsubw,     simd,       0,   0,    0,    0)
    mkclass(           pi2fd,      int,    intv,   0,   32,    0)
    mkclass(           pi2fw,      int,    intv,   0,   16,    0)
    mkclass(          pinsrb,     simd,       0,   0,    0,    0)
    mkclass(          pinsrd,     simd,       0,   0,    0,    0)
    mkclass(          pinsrq,     simd,       0,   0,    0,    0)
    mkclass(          pinsrw,     simd,    intv,   0,   16,    0)
    mkclass(       pmaddusbw,     simd,       0,   0,    0,    0)
    mkclass(         pmaddwd,     simd,    intv,   0,   32,    0)

    // should be all binv or all intv?
    mkclass(          pmaxsb,     simd,    binv,   0,    8,    0)
    mkclass(          pmaxsd,     simd,    intv,   0,   32,    0)
    mkclass(          pmaxsw,     simd,    intv,   0,   16,    0)
    mkclass(          pmaxub,     simd,    intv,   0,    8,    0)
    mkclass(          pmaxud,     simd,    intv,   0,   32,    0)
    mkclass(          pmaxuw,     simd,    intv,   0,   16,    0)
    mkclass(          pminsb,     simd,    binv,   0,    8,    0)
    mkclass(          pminsd,     simd,    intv,   0,   32,    0)
    mkclass(          pminsw,     simd,    intv,   0,   16,    0)
    mkclass(          pminub,     simd,    intv,   0,    8,    0)
    mkclass(          pminud,     simd,    intv,   0,   32,    0)
    mkclass(          pminuw,     simd,    intv,   0,   16,    0)

    mkclass(        pmovmskb,     move,    move,   0, VRSZ,    0)
    mkclass(        pmovsxbd,     move,       0,   0,    0,    0)
    mkclass(        pmovsxbq,     move,       0,   0,    0,    0)
    mkclass(        pmovsxbw,     move,       0,   0,    0,    0)
    mkclass(        pmovsxdq,     move,       0,   0,    0,    0)
    mkclass(        pmovsxwd,     move,       0,   0,    0,    0)
    mkclass(        pmovsxwq,     move,       0,   0,    0,    0)
    mkclass(        pmovzxbd,     move,       0,   0,    0,    0)
    mkclass(        pmovzxbq,     move,       0,   0,    0,    0)
    mkclass(        pmovzxbw,     move,       0,   0,    0,    0)
    mkclass(        pmovzxdq,     move,       0,   0,    0,    0)
    mkclass(        pmovzxwd,     move,       0,   0,    0,    0)
    mkclass(        pmovzxwq,     move,       0,   0,    0,    0)
    mkclass(          pmuldq,     simd,       0,   0,    0,    0)
    mkclass(        pmulhrsw,     simd,       0,   0,    0,    0)
    mkclass(         pmulhrw,      int,    intv,   0,   16,    0)
    mkclass(         pmulhuw,      int,    intv,   0,   16,    0)
    mkclass(          pmulhw,      int,    intv,   0,   16,    0)
    mkclass(          pmulld,     simd,       0,   0,    0,    0)
    mkclass(          pmullw,      int,    intv,   0,   16,    0)
    mkclass(         pmuludq,      int,    intv,   0,   32,    0)
    mkclass(             pop,      int,   stack,   0, VRSZ,    BinStack)
    mkclass(            popa,  special,   stack,   0,   16,    BinFrame)
    mkclass(           popad,  special,   stack,   0,   32,    BinFrame)
    mkclass(           popfd,      int,   stack,   0,   32,    BinStack)
    mkclass(           popfq,      int,   stack,   0,   64,    BinStack)
    mkclass(           popfw,      int,   stack,   0,   16,    BinStack)
    mkclass(             por,      int,    binv,   0, VRSZ,    0)
    mkclass(        prefetch, prefetch,   cache,   0,    0,    0)
    mkclass(     prefetchnta, prefetch,   cache,   0,    0,    0)
    mkclass(      prefetcht0, prefetch,   cache,   0,    0,    0)
    mkclass(      prefetcht1, prefetch,   cache,   0,    0,    0)
    mkclass(      prefetcht2, prefetch,   cache,   0,    0,    0)
    mkclass(          psadbw,      int,    intv,   0,   16,    0)
    mkclass(          pshufb,     simd,    binv,   0,    8,    0)
    mkclass(          pshufd,      int,    binv,   0,   32,    0)
    mkclass(         pshufhw,      int,    binv,   0,   16,    0)
    mkclass(         pshuflw,      int,    binv,   0,   16,    0)
    mkclass(          pshufw,      int,    binv,   0,   16,    0)
    mkclass(          psignb,     simd,       0,   0,    0,    0)
    mkclass(          psignd,     simd,       0,   0,    0,    0)
    mkclass(          psignw,     simd,       0,   0,    0,    0)
    mkclass(           pslld,      int,    binv,   0,   32,    0)
    mkclass(          pslldq,      int,    binv,   0,   64,    0)
    mkclass(           psllq,      int,    binv,   0,   64,    0)
    mkclass(           psllw,      int,    binv,   0,   16,    0)
    mkclass(           psrad,      int,    binv,   0,   32,    0)
    mkclass(           psraw,      int,    binv,   0,   16,    0)
    mkclass(           psrld,      int,    binv,   0,   32,    0)
    mkclass(          psrldq,      int,    binv,   0,   64,    0)
    mkclass(           psrlq,      int,    binv,   0,   64,    0)
    mkclass(           psrlw,      int,    binv,   0,   16,    0)
    mkclass(           psubb,      int,    intv,   0,   16,    0)
    mkclass(           psubd,      int,    intv,   0,   32,    0)
    mkclass(           psubq,      int,    intv,   0,   64,    0)

    // the byte forms of these are 16-bit?
    mkclass(          psubsb,      int,    intv,   0,   16,    0)
    mkclass(          psubsw,      int,    intv,   0,   16,    0)
    mkclass(         psubusb,      int,    intv,   0,   16,    0)
    mkclass(         psubusw,      int,    intv,   0,   16,    0)
    mkclass(           psubw,      int,    intv,   0,   16,    0)

    mkclass(          pswapd,      int,    intv,   0,   32,    0)
    mkclass(           ptest,      int,       0,   0,    0,    0)
    mkclass(       punpckhbw,      int,    binv,   0,   16,    0)
    mkclass(       punpckhdq,      int,    binv,   0,   64,    0)
    mkclass(      punpckhqdq,      int,    binv,   0,   64,    0)
    mkclass(       punpckhwd,      int,    binv,   0,   32,    0)
    mkclass(       punpcklbw,      int,    binv,   0,   16,    0)
    mkclass(       punpckldq,      int,    binv,   0,   64,    0)
    mkclass(      punpcklqdq,      int,    binv,   0,   64,    0)
    mkclass(       punpcklwd,      int,    binv,   0,   32,    0)
    mkclass(            push,      int,   stack,   0, VRSZ,    BinStack)
    mkclass(           pusha,  special,   stack,   0,   16,    BinFrame)
    mkclass(          pushad,  special,   stack,   0,   32,    BinFrame)
    mkclass(          pushfd,      int,   stack,   0,   32,    BinStack)
    mkclass(          pushfq,      int,   stack,   0,   64,    BinStack)
    mkclass(          pushfw,      int,   stack,   0,   16,    BinStack)
    mkclass(            pxor,      int,    binv,   0, VRSZ,    0)
    mkclass(             rcl,      int,     bin,   0, VRSZ,    0)
    mkclass(           rcpps,    float,  floatv,   0,   32,    0)
    mkclass(           rcpss,    float,  floats,   0,   32,    0)
    mkclass(             rcr,      int,     bin,   0, VRSZ,    0)
    mkclass(           rdmsr,      int,   other,   0,    0,    0)
    mkclass(           rdpmc,  hwcount,   other,   0,    0,    0)
    mkclass(           rdtsc,  hwcount,   other,   0,    0,    0)
    mkclass(          rdtscp,  hwcount,   other,   0,    0,    0)
    mkclass(             rep,   string,  string,   0,    0,    0)
    mkclass(           repne,   string,  string,   0,    0,    0)
    mkclass(             ret,   return,  uncond,   0,    0,    BinFrame)
    mkclass(            retf,   return,  uncond,   0,    0,    BinFrame)
    mkclass(             rol,      int,     bin,   0, VRSZ,    0)
    mkclass(             ror,      int,     bin,   0, VRSZ,    0)
    mkclass(         roundpd,     simd,  floatv,   0,   64,    0)
    mkclass(         roundps,     simd,  floatv,   0,   32,    0)
    mkclass(         roundsd,     simd,  floats,   0,   64,    0)
    mkclass(         roundss,     simd,  floats,   0,   32,    0)
    mkclass(             rsm,  special,   other,   0,    0,    0)
    mkclass(         rsqrtps,    float,  floatv,   0,   32,    0)
    mkclass(         rsqrtss,    float,  floats,   0,   32,    0)
    mkclass(            sahf,      int,   other,   0,    0,    0)
    mkclass(             sal,      int,     bin,   0, VRSZ,    0)
    mkclass(            salc,      int,     bin,   0, VRSZ,    0)
    mkclass(             sar,      int,     bin,   0, VRSZ,    0)
    mkclass(             sbb,      int,     int,   0, VRSZ,    0)
    mkclass(           scasb,   string,  string,  si,    8,    0)
    mkclass(           scasd,   string,  string,  si,   32,    0)
    mkclass(           scasq,   string,  string,  si,   64,    0)
    mkclass(           scasw,   string,  string,  si,   16,    0)
    mkclass(            seta,      int,     bin,   0,    8,    0)
    mkclass(            setb,      int,     bin,   0,    8,    0)
    mkclass(           setbe,      int,     bin,   0,    8,    0)
    mkclass(            setg,      int,     bin,   0,    8,    0)
    mkclass(           setge,      int,     bin,   0,    8,    0)
    mkclass(            setl,      int,     bin,   0,    8,    0)
    mkclass(           setle,      int,     bin,   0,    8,    0)
    mkclass(           setnb,      int,     bin,   0,    8,    0)
    mkclass(           setno,      int,     bin,   0,    8,    0)
    mkclass(           setnp,      int,     bin,   0,    8,    0)
    mkclass(           setns,      int,     bin,   0,    8,    0)
    mkclass(           setnz,      int,     bin,   0,    8,    0)
    mkclass(            seto,      int,     bin,   0,    8,    0)
    mkclass(            setp,      int,     bin,   0,    8,    0)
    mkclass(            sets,      int,     bin,   0,    8,    0)
    mkclass(            setz,      int,     bin,   0,    8,    0)
    mkclass(          sfence,  special,   other,   0,    0,    0)
    mkclass(            sgdt,  special,   other,   0,    0,    0)
    mkclass(             shl,      int,     bin,   0, VRSZ,    0)
    mkclass(            shld,      int,     bin,   0, VRSZ,    0)
    mkclass(             shr,      int,     bin,   0, VRSZ,    0)
    mkclass(            shrd,      int,     bin,   0, VRSZ,    0)
    mkclass(          shufpd,    float,    binv,   0,   64,    0)
    mkclass(          shufps,    float,    binv,   0,   32,    0)
    mkclass(            sidt,  special,   other,   0,    0,    0)
    mkclass(          skinit,  special,   other,   0,    0,    0)
    mkclass(            sldt,  special,   other,   0,    0,    0)
    mkclass(            smsw,  special,   other,   0,    0,    0)
    mkclass(          sqrtpd,    float,  floatv,   0,   64,    0)
    mkclass(          sqrtps,    float,  floatv,   0,   32,    0)
    mkclass(          sqrtsd,    float,  floats,   0,   64,    0)
    mkclass(          sqrtss,    float,  floats,   0,   32,    0)
    mkclass(             stc,  special,   other,   0,    0,    0)
    mkclass(             std,  special,   other,   0,    0,    0)
    mkclass(            stgi,  special,   other,   0,    0,    0)
    mkclass(             sti,  special,   other,   0,    0,    0)
    mkclass(         stmxcsr,     move,   other,   0,    0,    0)
    mkclass(           stosb,   string,    move, dsi,    8,    0)
    mkclass(           stosd,   string,    move, dsi,   32,    0)
    mkclass(           stosq,   string,    move, dsi,   64,    0)
    mkclass(           stosw,   string,    move, dsi,   16,    0)
    mkclass(             str,     move,    move,   0, VRSZ,    0)
    mkclass(             sub,      int,     int,   0, VRSZ,    0)
    mkclass(           subpd,    float,  floatv,   0,   64,    0)
    mkclass(           subps,    float,  floatv,   0,   32,    0)
    mkclass(           subsd,    float,  floats,   0,   64,    0)
    mkclass(           subss,    float,  floats,   0,   32,    0)
    mkclass(          swapgs,  special,   other,   0,    0,    0)
    mkclass(         syscall,  syscall,  system,   0,    0,    BinFrame)
    mkclass(        sysenter,  syscall,  system,   0,    0,    BinFrame)
    mkclass(         sysexit,  syscall,  system,   0,    0,    BinFrame)
    mkclass(          sysret,  syscall,  system,   0,    0,    BinFrame)
    mkclass(            test,      int,     bin,   0, VRSZ,    0)
    mkclass(         ucomisd,    float,  floats,   0,   64,    0)
    mkclass(         ucomiss,    float,  floats,   0,   32,    0)
    mkclass(             ud2,  invalid, invalid,   0,    0,    0)
    mkclass(        unpckhpd,    float,    binv,   0,   64,    0)
    mkclass(        unpckhps,    float,    binv,   0,   32,    0)
    mkclass(        unpcklpd,    float,    binv,   0,   64,    0)
    mkclass(        unpcklps,    float,    binv,   0,   32,    0)
    mkclass(          vaddpd,      avx,       0,   0,    0,    0)
    mkclass(          vaddps,      avx,       0,   0,    0,    0)
    mkclass(          vaddsd,      avx,       0,   0,    0,    0)
    mkclass(          vaddss,      avx,       0,   0,    0,    0)
    mkclass(       vaddsubpd,      avx,       0,   0,    0,    0)
    mkclass(       vaddsubps,      avx,       0,   0,    0,    0)
    mkclass(         vaesdec,      aes,       0,   0,    0,    0)
    mkclass(     vaesdeclast,      aes,       0,   0,    0,    0)
    mkclass(         vaesenc,      aes,       0,   0,    0,    0)
    mkclass(     vaesenclast,      aes,       0,   0,    0,    0)
    mkclass(         vaesimc,      aes,       0,   0,    0,    0)
    mkclass(vaeskeygenassist,      aes,       0,   0,    0,    0)
    mkclass(         vandnpd,      avx,       0,   0,    0,    0)
    mkclass(         vandnps,      avx,       0,   0,    0,    0)
    mkclass(          vandpd,      avx,       0,   0,    0,    0)
    mkclass(          vandps,      avx,       0,   0,    0,    0)
    mkclass(        vblendpd,      avx,       0,   0,    0,    0)
    mkclass(        vblendps,      avx,       0,   0,    0,    0)
    mkclass(       vblendvpd,      avx,       0,   0,    0,    0)
    mkclass(       vblendvps,      avx,       0,   0,    0,    0)
    mkclass(      vbroadcast,     move,       0,   0,    0,    0)
    mkclass(          vcmppd,      avx,       0,   0,    0,    0)
    mkclass(          vcmpps,      avx,       0,   0,    0,    0)
    mkclass(          vcmpsd,      avx,       0,   0,    0,    0)
    mkclass(          vcmpss,      avx,       0,   0,    0,    0)
    mkclass(         vcomisd,      avx,       0,   0,    0,    0)
    mkclass(         vcomiss,      avx,       0,   0,    0,    0)
    mkclass(       vcvtdq2pd,      avx,       0,   0,    0,    0)
    mkclass(       vcvtdq2ps,      avx,       0,   0,    0,    0)
    mkclass(       vcvtpd2dq,      avx,       0,   0,    0,    0)
    mkclass(       vcvtpd2ps,      avx,       0,   0,    0,    0)
    mkclass(       vcvtph2ps,      avx,       0,   0,    0,    0)
    mkclass(       vcvtps2dq,      avx,       0,   0,    0,    0)
    mkclass(       vcvtps2pd,      avx,       0,   0,    0,    0)
    mkclass(       vcvtps2ph,      avx,       0,   0,    0,    0)
    mkclass(       vcvtsd2si,      avx,       0,   0,    0,    0)
    mkclass(       vcvtsd2ss,      avx,       0,   0,    0,    0)
    mkclass(       vcvtsi2sd,      avx,       0,   0,    0,    0)
    mkclass(       vcvtsi2ss,      avx,       0,   0,    0,    0)
    mkclass(       vcvtss2sd,      avx,       0,   0,    0,    0)
    mkclass(       vcvtss2si,      avx,       0,   0,    0,    0)
    mkclass(      vcvttpd2dq,      avx,       0,   0,    0,    0)
    mkclass(      vcvttps2dq,      avx,       0,   0,    0,    0)
    mkclass(      vcvttsd2si,      avx,       0,   0,    0,    0)
    mkclass(      vcvttss2si,      avx,       0,   0,    0,    0)
    mkclass(          vdivpd,      avx,       0,   0,    0,    0)
    mkclass(          vdivps,      avx,       0,   0,    0,    0)
    mkclass(          vdivsd,      avx,       0,   0,    0,    0)
    mkclass(          vdivss,      avx,       0,   0,    0,    0)
    mkclass(           vdppd,      avx,       0,   0,    0,    0)
    mkclass(           vdpps,      avx,       0,   0,    0,    0)
    mkclass(            verr,  special,   other,   0,    0,    0)
    mkclass(            verw,  special,   other,   0,    0,    0)
    mkclass(    vextractf128,      avx,       0,   0,    0,    0)
    mkclass(      vextractps,      avx,       0,   0,    0,    0)
    mkclass(         vfmaddp,      avx,       0,   0,    0,    0)
    mkclass(         vfmadds,      avx,       0,   0,    0,    0)
    mkclass(      vfmaddsubp,      avx,       0,   0,    0,    0)
    mkclass(      vfmsubaddp,      avx,       0,   0,    0,    0)
    mkclass(         vfmsubp,      avx,       0,   0,    0,    0)
    mkclass(         vfmsubs,      avx,       0,   0,    0,    0)
    mkclass(        vfnmaddp,      avx,       0,   0,    0,    0)
    mkclass(        vfnmadds,      avx,       0,   0,    0,    0)
    mkclass(        vfnmsubp,      avx,       0,   0,    0,    0)
    mkclass(        vfnmsubs,      avx,       0,   0,    0,    0)
    mkclass(         vhaddpd,      avx,       0,   0,    0,    0)
    mkclass(         vhaddps,      avx,       0,   0,    0,    0)
    mkclass(         vhsubpd,      avx,       0,   0,    0,    0)
    mkclass(         vhsubps,      avx,       0,   0,    0,    0)
    mkclass(     vinsertf128,      avx,       0,   0,    0,    0)
    mkclass(       vinsertps,      avx,       0,   0,    0,    0)
    mkclass(          vlddqu,     move,       0,   0, VRSZ,    0)
    mkclass(        vldmxcsr,     move,       0,   0,    0,    0)
    mkclass(        vmaskmov,     move,       0,   0,    0,    0)
    mkclass(     vmaskmovdqu,     move,       0,   0,    0,    0)
    mkclass(          vmaxpd,      avx,       0,   0,    0,    0)
    mkclass(          vmaxps,      avx,       0,   0,    0,    0)
    mkclass(          vmaxsd,      avx,       0,   0,    0,    0)
    mkclass(          vmaxss,      avx,       0,   0,    0,    0)
    mkclass(          vmcall,      vmx,   other,   0,    0,    0)
    mkclass(         vmclear,      vmx,   other,   0,    0,    0)
    mkclass(          vminpd,      avx,       0,   0,    0,    0)
    mkclass(          vminps,      avx,       0,   0,    0,    0)
    mkclass(          vminsd,      avx,       0,   0,    0,    0)
    mkclass(          vminss,      avx,       0,   0,    0,    0)
    mkclass(          vmload,      vmx,   other,   0,    0,    0)
    mkclass(         vmmcall,      vmx,   other,   0,    0,    0)
    mkclass(         vmovapd,     move,       0,   0,    0,    0)
    mkclass(         vmovaps,     move,       0,   0,    0,    0)
    mkclass(           vmovd,     move,       0,   0,    0,    0)
    mkclass(        vmovddup,     move,       0,   0,    0,    0)
    mkclass(         vmovdqa,     move,       0,   0,    0,    0)
    mkclass(         vmovdqu,     move,       0,   0,    0,    0)
    mkclass(         vmovhpd,     move,       0,   0,    0,    0)
    mkclass(         vmovhps,     move,       0,   0,    0,    0)
    mkclass(         vmovlpd,     move,       0,   0,    0,    0)
    mkclass(         vmovlps,     move,       0,   0,    0,    0)
    mkclass(       vmovmskpd,     move,       0,   0,    0,    0)
    mkclass(       vmovmskps,     move,       0,   0,    0,    0)
    mkclass(        vmovntdq,     move,       0,   0,    0,    0)
    mkclass(       vmovntdqa,     move,       0,   0,    0,    0)
    mkclass(        vmovntpd,     move,       0,   0,    0,    0)
    mkclass(        vmovntps,     move,       0,   0,    0,    0)
    mkclass(           vmovq,     move,       0,   0,    0,    0)
    mkclass(          vmovsd,     move,       0,   0,    0,    0)
    mkclass(       vmovshdup,     move,       0,   0,    0,    0)
    mkclass(       vmovsldup,     move,       0,   0,    0,    0)
    mkclass(          vmovss,     move,       0,   0,    0,    0)
    mkclass(         vmovupd,     move,       0,   0,    0,    0)
    mkclass(         vmovups,     move,       0,   0,    0,    0)
    mkclass(        vmpsadbw,      avx,       0,   0,    0,    0)
    mkclass(         vmptrld,      vmx,   other,   0,    0,    0)
    mkclass(         vmptrst,      vmx,   other,   0,    0,    0)
    mkclass(        vmresume,      vmx,   other,   0,    0,    0)
    mkclass(           vmrun,      vmx,   other,   0,    0,    0)
    mkclass(          vmsave,      vmx,   other,   0,    0,    0)
    mkclass(          vmulpd,      avx,       0,   0,    0,    0)
    mkclass(          vmulps,      avx,       0,   0,    0,    0)
    mkclass(          vmulsd,      avx,       0,   0,    0,    0)
    mkclass(          vmulss,      avx,       0,   0,    0,    0)
    mkclass(          vmxoff,      vmx,   other,   0,    0,    0)
    mkclass(           vmxon,      vmx,   other,   0,    0,    0)
    mkclass(           vorpd,      avx,       0,   0,    0,    0)
    mkclass(           vorps,      avx,       0,   0,    0,    0)
    mkclass(          vpabsb,      avx,       0,   0,    0,    0)
    mkclass(          vpabsd,      avx,       0,   0,    0,    0)
    mkclass(          vpabsw,      avx,       0,   0,    0,    0)
    mkclass(       vpackssdw,      avx,       0,   0,    0,    0)
    mkclass(       vpacksswb,      avx,       0,   0,    0,    0)
    mkclass(       vpackusdw,      avx,       0,   0,    0,    0)
    mkclass(       vpackuswb,      avx,       0,   0,    0,    0)
    mkclass(          vpaddb,      avx,       0,   0,    0,    0)
    mkclass(          vpaddd,      avx,       0,   0,    0,    0)
    mkclass(          vpaddq,      avx,       0,   0,    0,    0)
    mkclass(         vpaddsb,      avx,       0,   0,    0,    0)
    mkclass(         vpaddsw,      avx,       0,   0,    0,    0)
    mkclass(        vpaddusb,      avx,       0,   0,    0,    0)
    mkclass(        vpaddusw,      avx,       0,   0,    0,    0)
    mkclass(          vpaddw,      avx,       0,   0,    0,    0)
    mkclass(        vpalignr,      avx,       0,   0,    0,    0)
    mkclass(           vpand,      avx,       0,   0,    0,    0)
    mkclass(          vpandn,      avx,       0,   0,    0,    0)
    mkclass(          vpavgb,      avx,       0,   0,    0,    0)
    mkclass(          vpavgw,      avx,       0,   0,    0,    0)
    mkclass(       vpblendvb,      avx,       0,   0,    0,    0)
    mkclass(        vpblendw,      avx,       0,   0,    0,    0)
    mkclass(      vpclmulqdq,      avx,       0,   0,    0,    0)
    mkclass(        vpcmpeqb,      avx,       0,   0,    0,    0)
    mkclass(        vpcmpeqd,      avx,       0,   0,    0,    0)
    mkclass(        vpcmpeqq,      avx,       0,   0,    0,    0)
    mkclass(        vpcmpeqw,      avx,       0,   0,    0,    0)
    mkclass(      vpcmpestri,      avx,       0,   0,    0,    0)
    mkclass(      vpcmpestrm,      avx,       0,   0,    0,    0)
    mkclass(        vpcmpgtb,      avx,       0,   0,    0,    0)
    mkclass(        vpcmpgtd,      avx,       0,   0,    0,    0)
    mkclass(        vpcmpgtq,      avx,       0,   0,    0,    0)
    mkclass(        vpcmpgtw,      avx,       0,   0,    0,    0)
    mkclass(      vpcmpistri,      avx,       0,   0,    0,    0)
    mkclass(      vpcmpistrm,      avx,       0,   0,    0,    0)
    mkclass(       vpermf128,      avx,       0,   0,    0,    0)
    mkclass(       vpermilpd,      avx,       0,   0,    0,    0)
    mkclass(       vpermilps,      avx,       0,   0,    0,    0)
    mkclass(         vpextrb,      avx,       0,   0,    0,    0)
    mkclass(         vpextrd,      avx,       0,   0,    0,    0)
    mkclass(         vpextrq,      avx,       0,   0,    0,    0)
    mkclass(         vpextrw,      avx,       0,   0,    0,    0)
    mkclass(         vphaddd,      avx,       0,   0,    0,    0)
    mkclass(        vphaddsw,      avx,       0,   0,    0,    0)
    mkclass(         vphaddw,      avx,       0,   0,    0,    0)
    mkclass(     vphminposuw,      avx,       0,   0,    0,    0)
    mkclass(         vphsubd,      avx,       0,   0,    0,    0)
    mkclass(        vphsubsw,      avx,       0,   0,    0,    0)
    mkclass(         vphsubw,      avx,       0,   0,    0,    0)
    mkclass(         vpinsrb,      avx,       0,   0,    0,    0)
    mkclass(         vpinsrd,      avx,       0,   0,    0,    0)
    mkclass(         vpinsrq,      avx,       0,   0,    0,    0)
    mkclass(         vpinsrw,      avx,       0,   0,    0,    0)
    mkclass(      vpmaddusbw,      avx,       0,   0,    0,    0)
    mkclass(        vpmaddwd,      avx,       0,   0,    0,    0)
    mkclass(         vpmaxsb,      avx,       0,   0,    0,    0)
    mkclass(         vpmaxsd,      avx,       0,   0,    0,    0)
    mkclass(         vpmaxsw,      avx,       0,   0,    0,    0)
    mkclass(         vpmaxub,      avx,       0,   0,    0,    0)
    mkclass(         vpmaxud,      avx,       0,   0,    0,    0)
    mkclass(         vpmaxuw,      avx,       0,   0,    0,    0)
    mkclass(         vpminsb,      avx,       0,   0,    0,    0)
    mkclass(         vpminsd,      avx,       0,   0,    0,    0)
    mkclass(         vpminsw,      avx,       0,   0,    0,    0)
    mkclass(         vpminub,      avx,       0,   0,    0,    0)
    mkclass(         vpminud,      avx,       0,   0,    0,    0)
    mkclass(         vpminuw,      avx,       0,   0,    0,    0)
    mkclass(       vpmovmskb,     move,       0,   0,    0,    0)
    mkclass(       vpmovsxbd,     move,       0,   0,    0,    0)
    mkclass(       vpmovsxbq,     move,       0,   0,    0,    0)
    mkclass(       vpmovsxbw,     move,       0,   0,    0,    0)
    mkclass(       vpmovsxdq,     move,       0,   0,    0,    0)
    mkclass(       vpmovsxwd,     move,       0,   0,    0,    0)
    mkclass(       vpmovsxwq,     move,       0,   0,    0,    0)
    mkclass(       vpmovzxbd,     move,       0,   0,    0,    0)
    mkclass(       vpmovzxbq,     move,       0,   0,    0,    0)
    mkclass(       vpmovzxbw,     move,       0,   0,    0,    0)
    mkclass(       vpmovzxdq,     move,       0,   0,    0,    0)
    mkclass(       vpmovzxwd,     move,       0,   0,    0,    0)
    mkclass(       vpmovzxwq,     move,       0,   0,    0,    0)
    mkclass(         vpmuldq,      avx,       0,   0,    0,    0)
    mkclass(       vpmulhrsw,      avx,       0,   0,    0,    0)
    mkclass(        vpmulhuw,      avx,       0,   0,    0,    0)
    mkclass(         vpmulhw,      avx,       0,   0,    0,    0)
    mkclass(         vpmulld,      avx,       0,   0,    0,    0)
    mkclass(         vpmullw,      avx,       0,   0,    0,    0)
    mkclass(        vpmuludq,      avx,       0,   0,    0,    0)
    mkclass(            vpor,      avx,       0,   0,    0,    0)
    mkclass(         vpsadbw,      avx,       0,   0,    0,    0)
    mkclass(         vpshufb,      avx,       0,   0,    0,    0)
    mkclass(         vpshufd,      avx,       0,   0,    0,    0)
    mkclass(        vpshufhw,      avx,       0,   0,    0,    0)
    mkclass(        vpshuflw,      avx,       0,   0,    0,    0)
    mkclass(         vpsignb,      avx,       0,   0,    0,    0)
    mkclass(         vpsignd,      avx,       0,   0,    0,    0)
    mkclass(         vpsignw,      avx,       0,   0,    0,    0)
    mkclass(          vpslld,      avx,       0,   0,    0,    0)
    mkclass(         vpslldq,      avx,       0,   0,    0,    0)
    mkclass(          vpsllq,      avx,       0,   0,    0,    0)
    mkclass(          vpsllw,      avx,       0,   0,    0,    0)
    mkclass(          vpsrad,      avx,       0,   0,    0,    0)
    mkclass(          vpsraw,      avx,       0,   0,    0,    0)
    mkclass(          vpsrld,      avx,       0,   0,    0,    0)
    mkclass(         vpsrldq,      avx,       0,   0,    0,    0)
    mkclass(          vpsrlq,      avx,       0,   0,    0,    0)
    mkclass(          vpsrlw,      avx,       0,   0,    0,    0)
    mkclass(          vpsubb,      avx,       0,   0,    0,    0)
    mkclass(          vpsubd,      avx,       0,   0,    0,    0)
    mkclass(          vpsubq,      avx,       0,   0,    0,    0)
    mkclass(         vpsubsb,      avx,       0,   0,    0,    0)
    mkclass(         vpsubsw,      avx,       0,   0,    0,    0)
    mkclass(        vpsubusb,      avx,       0,   0,    0,    0)
    mkclass(        vpsubusw,      avx,       0,   0,    0,    0)
    mkclass(          vpsubw,      avx,       0,   0,    0,    0)
    mkclass(      vpunpckhbw,      avx,       0,   0,    0,    0)
    mkclass(      vpunpckhdq,      avx,       0,   0,    0,    0)
    mkclass(     vpunpckhqdq,      avx,       0,   0,    0,    0)
    mkclass(      vpunpckhwd,      avx,       0,   0,    0,    0)
    mkclass(      vpunpcklbw,      avx,       0,   0,    0,    0)
    mkclass(      vpunpckldq,      avx,       0,   0,    0,    0)
    mkclass(     vpunpcklqdq,      avx,       0,   0,    0,    0)
    mkclass(      vpunpcklwd,      avx,       0,   0,    0,    0)
    mkclass(           vpxor,      avx,       0,   0,    0,    0)
    mkclass(          vrcpps,      avx,       0,   0,    0,    0)
    mkclass(          vrcpss,      avx,       0,   0,    0,    0)
    mkclass(        vroundpd,      avx,       0,   0,    0,    0)
    mkclass(        vroundps,      avx,       0,   0,    0,    0)
    mkclass(        vroundsd,      avx,       0,   0,    0,    0)
    mkclass(        vroundss,      avx,       0,   0,    0,    0)
    mkclass(        vrsqrtps,      avx,       0,   0,    0,    0)
    mkclass(        vrsqrtss,      avx,       0,   0,    0,    0)
    mkclass(         vshufpd,      avx,       0,   0,    0,    0)
    mkclass(         vshufps,      avx,       0,   0,    0,    0)
    mkclass(         vsqrtpd,      avx,       0,   0,    0,    0)
    mkclass(         vsqrtps,      avx,       0,   0,    0,    0)
    mkclass(         vsqrtsd,      avx,       0,   0,    0,    0)
    mkclass(         vsqrtss,      avx,       0,   0,    0,    0)
    mkclass(        vstmxcsr,     move,       0,   0,    0,    0)
    mkclass(          vsubpd,      avx,       0,   0,    0,    0)
    mkclass(          vsubps,      avx,       0,   0,    0,    0)
    mkclass(          vsubsd,      avx,       0,   0,    0,    0)
    mkclass(          vsubss,      avx,       0,   0,    0,    0)
    mkclass(         vtestpd,      avx,       0,   0,    0,    0)
    mkclass(         vtestps,      avx,       0,   0,    0,    0)
    mkclass(        vucomisd,      avx,       0,   0,    0,    0)
    mkclass(        vucomiss,      avx,       0,   0,    0,    0)
    mkclass(       vunpckhpd,      avx,       0,   0,    0,    0)
    mkclass(       vunpckhps,      avx,       0,   0,    0,    0)
    mkclass(       vunpcklpd,      avx,       0,   0,    0,    0)
    mkclass(       vunpcklps,      avx,       0,   0,    0,    0)
    mkclass(          vxorpd,      avx,       0,   0,    0,    0)
    mkclass(          vxorps,      avx,       0,   0,    0,    0)
    mkclass(        vzeroall,      avx,       0,   0,    0,    0)
    mkclass(            wait,  special,   other,   0,    0,    0)
    mkclass(          wbinvd,  special,   other,   0,    0,    0)
    mkclass(           wrmsr,  special,   other,   0,    0,    0)
    mkclass(            xadd,      int,     int,   0, VRSZ,    0)
    mkclass(            xchg,      int,     int,   0, VRSZ,    0)
    mkclass(           xgetbv, special,   other,   0,    0,    0)
    mkclass(           xlatb,  special,   other,   0,    0,    0)
    mkclass(             xor,      int,     bin,   0, VRSZ,    0)
    mkclass(           xorpd,    float,    binv,   0,   64,    0)
    mkclass(           xorps,    float,    binv,   0,   32,    0)    
};



X86InstructionBin X86InstructionClassifier::getInstructionBin(X86Instruction* x){ 
    return classifications[x->GET(mnemonic)].bin;
}

uint8_t X86InstructionClassifier::getInstructionMemLocation(X86Instruction* x){
    uint8_t loc = classifications[x->GET(mnemonic)].location;
    if (x->isLoad()){
        loc |= (BinLoad >> 8);
    }
    if (x->isStore()){
        loc |= (BinStore >> 8);
    }    
    return loc;
}

uint8_t X86InstructionClassifier::getInstructionMemSize(X86Instruction* x){
    uint8_t mem = classifications[x->GET(mnemonic)].memsize;
    if (mem & MEM_SZ_VARIABLE){
        mem = x->getDstSizeInBytes();
    }
    return mem;
}

X86InstructionType X86InstructionClassifier::getInstructionType(X86Instruction* x){
    return classifications[x->GET(mnemonic)].type;
}

X86OperandFormat X86InstructionClassifier::getInstructionFormat(X86Instruction* x){ 
    return classifications[x->GET(mnemonic)].format;
}

void X86InstructionClassifier::print(X86Instruction* x){
    PRINT_INFOR("Instruciton %s: %hhd %hhd %hhd %hhd %hhd", ud_mnemonics_str[x->GET(mnemonic)], getInstructionBin(x), getInstructionMemLocation(x), getInstructionMemSize(x), getInstructionType(x), getInstructionFormat(x));
}

bool X86InstructionClassifier::verify(){
    bool err = false;
    for (uint32_t i = 0; i < UD_Itotaltypes; i++){
        if (classifications[i].mnemonic < UD_Itotaltypes){
            if (classifications[i].mnemonic != i){
                err = true;
                PRINT_WARN(20, "Instruction classification definition slot %s contains info for %s", ud_mnemonics_str[i], ud_mnemonics_str[classifications[i].mnemonic]);
            }
        } else {
            PRINT_WARN(20, "Invalid mnemonic %d in slot (%d) %s", classifications[i].mnemonic, i, ud_mnemonics_str[i]);
            err = true;
        }
    }
    return !err;
}
