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
    memcpy(comp->insn_hexcode, reg->insn_hexcode, sizeof(char) * 32);
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
    if (isExplicitMemoryOperation()){
        OperandX86* op = getMemoryOperand();
        ASSERT(op);
        if (op->getOperandIndex() == COMP_SRC_OPERAND){
            return true;
        }
        if (!isMoveOperation() && op->getOperandIndex() == ALU_SRC2_OPERAND){
            return true;
        }
    }
    return false;
}

bool X86Instruction::isStore(){
    if (isStackPush()){
        return true;
    }
    if (isExplicitMemoryOperation()){
        OperandX86* op = getMemoryOperand();
        ASSERT(op);
        if (op->getOperandIndex() == COMP_DEST_OPERAND){
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

bool X86Instruction::usesFlag(uint32_t flg) { 
    return (GET(flags_use) & (1 << flg));
}

bool X86Instruction::defsFlag(uint32_t flg) { 
    return (GET(flags_def) & (1 << flg));
}

bool X86Instruction::usesAluReg(uint32_t alu){
    return (GET(impreg_use) & (1 << alu));
}

bool X86Instruction::defsAluReg(uint32_t alu){
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

bool X86Instruction::isConditionCompare(){
    int32_t m = GET(mnemonic);

    if ((m == UD_Icmp) ||
        (m == UD_Itest) ||
        (m == UD_Iptest) ||
        (m == UD_Iftst) ||
        (m == UD_Ibt) ||
        (m == UD_Ibtc) ||
        (m == UD_Ibtr) ||
        (m == UD_Ibts) ||
        (m == UD_Icmppd) ||
        (m == UD_Icmpps) ||
        (m == UD_Icmpsb) ||
        (m == UD_Icmpsw) ||
        (m == UD_Icmpsd) ||
        (m == UD_Icmpsq) ||
        (m == UD_Icmpss) ||
        (m == UD_Icmpxchg) ||
        (m == UD_Icmpxchg8b) ||
        (m == UD_Ipcmpeqb) ||
        (m == UD_Ipcmpeqw) ||
        (m == UD_Ipcmpeqd) ||
        (m == UD_Ipcmpgtb) ||
        (m == UD_Ipcmpgtw) ||
        (m == UD_Ipcmpgtd) ||
        (m == UD_Ipcmpgtq) ||
        (m == UD_Ipfcmpge) ||
        (m == UD_Ipfcmpgt) ||
        (m == UD_Ipfcmpeq)){
        return true;
    }
    return false;
}


uint32_t convertUdXMMReg(uint32_t reg){
    ASSERT(reg && IS_XMM_REG(reg));
    return reg - UD_R_XMM0 + X86_FPREG_XMM0;
}

uint32_t OperandX86::getBaseRegister(){
    ASSERT(GET(base) && IS_ALU_REG(GET(base)));
    if (IS_GPR(GET(base))){
        return convertUdGPReg(GET(base));
    } else if (IS_XMM_REG(GET(base))){
        return convertUdXMMReg(GET(base));
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

void X86Instruction::usesRegisters(BitSet<uint32_t>* regs){
    if (isMoveOperation()){
        if (operands[MOV_SRC_OPERAND]){
            operands[MOV_SRC_OPERAND]->touchedRegisters(regs);
        }
        if (operands[MOV_DEST_OPERAND]){
            if (operands[MOV_DEST_OPERAND]->getType() == UD_OP_MEM || 
                operands[MOV_DEST_OPERAND]->getType() == UD_OP_PTR){
                operands[MOV_DEST_OPERAND]->touchedRegisters(regs);
            }
        }
    }
    if (isIntegerOperation() || isFloatPOperation()){
        if (countExplicitOperands() > 1){
            if (operands[ALU_SRC1_OPERAND]){
                operands[ALU_SRC1_OPERAND]->touchedRegisters(regs);
            }
        }
        if (countExplicitOperands() > 0){
            if (operands[ALU_SRC2_OPERAND]){
                operands[ALU_SRC2_OPERAND]->touchedRegisters(regs);
            }
        }
    }
    impliedUses(regs);
    // TODO: implement this for branches?
}

void X86Instruction::defsRegisters(BitSet<uint32_t>* regs){
    if (isMoveOperation()){
        if (operands[MOV_DEST_OPERAND] && operands[MOV_DEST_OPERAND]->getType() == UD_OP_REG){
            operands[MOV_DEST_OPERAND]->touchedRegisters(regs);
        }
    }
    if (isIntegerOperation() || isFloatPOperation()){
        if (!isConditionCompare() && countExplicitOperands() > 1){
            if (operands[ALU_DEST_OPERAND] && operands[ALU_DEST_OPERAND]->getType() == UD_OP_REG){
                operands[ALU_DEST_OPERAND]->touchedRegisters(regs);
            }
        }
    }
    impliedDefs(regs);
    // TODO: implement this for branches?
}

#define has(reg) (op->GET(reg) && IS_ALU_REG(op->GET(reg)))

LinkedList<X86Instruction::ReachingDefinition*>* X86Instruction::getDefs(){

    LinkedList<ReachingDefinition*>* defs =
        new LinkedList<ReachingDefinition*>();

    OperandX86* op;

    if (isMoveOperation()) {
        op = operands[MOV_DEST_OPERAND];
        if (op) {
            DefLocation loc;
            loc.value = op->getValue();
            loc.base = has(base) ? op->getBaseRegister() : X86_ALU_REGS;
            loc.index = has(index) ? op->getIndexRegister() : X86_ALU_REGS;
            loc.offset = op->GET(offset);
            loc.scale = op->GET(scale);
            loc.type = op->GET(type);
            defs->insert(new ReachingDefinition(this, loc));
        } // else implied?


    } else if ((isIntegerOperation() || isFloatPOperation()) && !isConditionCompare()) {
        op = operands[ALU_DEST_OPERAND];
        if (op) {
            DefLocation loc;
            loc.value = op->getValue();
            loc.base = has(base) ? op->getBaseRegister() : X86_ALU_REGS;
            loc.index = has(index) ? op->getIndexRegister() : X86_ALU_REGS;
            loc.offset = op->GET(offset);
            loc.scale = op->GET(scale);
            loc.type = op->GET(type);
            defs->insert(new ReachingDefinition(this, loc));
        } // else implied?
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
            defs->insert(new ReachingDefinition(this, loc));
        }
    } 

    return defs;
}

LinkedList<X86Instruction::ReachingDefinition*>* X86Instruction::getUses(){

    LinkedList<ReachingDefinition*>* uses =
        new LinkedList<ReachingDefinition*>();

    OperandX86* op;

    if (isMoveOperation()) {
        op = operands[MOV_SRC_OPERAND];
        if (op) {
            DefLocation loc;
            loc.value = op->getValue();
            loc.base = has(base) ? op->getBaseRegister() : X86_ALU_REGS;
            loc.index = has(index) ? op->getIndexRegister() : X86_ALU_REGS;
            loc.offset = op->GET(offset);
            loc.scale = op->GET(scale);
            loc.type = op->GET(type);
            uses->insert(new ReachingDefinition(this, loc));
        } // else implied?


    } else if ((isIntegerOperation() || isFloatPOperation())) {
        op = operands[ALU_SRC1_OPERAND];
        if (op) {
            DefLocation loc;
            loc.value = op->getValue();
            loc.base = has(base) ? op->getBaseRegister() : X86_ALU_REGS;
            loc.index = has(index) ? op->getIndexRegister() : X86_ALU_REGS;
            loc.offset = op->GET(offset);
            loc.scale = op->GET(scale);
            loc.type = op->GET(type);
            uses->insert(new ReachingDefinition(this, loc));
        } // else implied?

        op = operands[ALU_SRC2_OPERAND];
        if (op) {
            DefLocation loc;
            loc.value = op->getValue();
            loc.base = has(base) ? op->getBaseRegister() : X86_ALU_REGS;
            loc.index = has(index) ? op->getIndexRegister() : X86_ALU_REGS;
            loc.offset = op->GET(offset);
            loc.scale = op->GET(scale);
            loc.type = op->GET(type);
            uses->insert(new ReachingDefinition(this, loc));
        }
    }

    // Get the implied register uses
    BitSet<uint32_t> imp_regs(X86_ALU_REGS);
    impliedUses(&imp_regs);

    for (uint32_t i = 0; i < X86_ALU_REGS; ++i) {
        if (imp_regs.contains(i)) {
            struct DefLocation loc;
            bzero(&loc, sizeof(loc));
            loc.base = i;
            loc.type = UD_OP_REG;
            uses->insert(new ReachingDefinition(this, loc));
        }
    } 

    return uses;
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
    if (getInstructionType() == X86InstructionType_simd){
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
    if (getInstructionType() == X86InstructionType_uncond_branch ||
        getInstructionType() == X86InstructionType_cond_branch){
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
    else if (getInstructionType() == X86InstructionType_system_call){
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
    uint32_t currByte = 0;
    for (uint32_t i = 0; i < sizeInBytes; i++){
        rawBytes[currByte++] = mapCharsToByte(GET(insn_hexcode)[2*i], GET(insn_hexcode)[2*i+1]);
    }


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

    ASSERT(strlen(GET(insn_hexcode)) % 2 == 0);

    for (int32_t i = 0; i < strlen(GET(insn_hexcode)); i += 2){
        fprintf(stream, "%c%c ", GET(insn_hexcode)[i], GET(insn_hexcode)[i+1]);
    }

    if (strlen(GET(insn_hexcode)) < 16){
        for (int32_t i = 16 - strlen(GET(insn_hexcode)); i > 0; i -= 2){
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

    for (uint32_t i = 0; i < sizeInBytes; i++){
        char byt = mapCharsToByte(GET(insn_hexcode)[2*i], GET(insn_hexcode)[2*i+1]);
        binaryOutputFile->copyBytes(&byt, 1, offset + i);
    }

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

uint16_t X86Instruction::getInstructionBin(){
    if (instructionBin){
        return instructionBin;
    }
    return setInstructionBin();
}

uint16_t X86Instruction::setInstructionBin(){
    uint16_t bin = X86InstructionBin_unknown;
    if(isLoad()) bin += BinLoad;
    if(isStore()) bin += BinStore;
    switch(GET(mnemonic)){
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
            bin += X86InstructionBin_bin;
            bin += INSTBIN_DATATYPE(sizeof(int8_t));
            break;
        case UD_Icbw:
            bin += X86InstructionBin_bin;
            bin += INSTBIN_DATATYPE(sizeof(int16_t));
            break;
        case UD_Icwd:
        case UD_Icwde:
            bin += X86InstructionBin_bin;
            bin += INSTBIN_DATATYPE(sizeof(int32_t));
            break;
        case UD_Icdqe:
        case UD_Icdq:
        case UD_Icqo:
            bin += X86InstructionBin_bin;
            bin += INSTBIN_DATATYPE(sizeof(int64_t));
            break;
        case UD_Iand:
        case UD_Ibound:
        case UD_Ibsf:
        case UD_Ibsr:
        case UD_Ibt:
        case UD_Ibtc:
        case UD_Ibtr:
        case UD_Ibts:
        case UD_Ibswap:
        case UD_Inot:
        case UD_Ior:
        case UD_Ircl:
        case UD_Ircr:
        case UD_Irol:
        case UD_Iror:
        case UD_Isal:
        case UD_Isalc:
        case UD_Isar:
        case UD_Ishl:
        case UD_Ishr:
        case UD_Ishld:
        case UD_Ishrd:
        case UD_Itest:
        case UD_Ixor:
            bin += X86InstructionBin_bin;
            bin += INSTBIN_DATATYPE(getDstSizeInBytes());
            break;
        case UD_Ipshufb:
        case UD_Ipminsb:
        case UD_Ipmaxsb:
            bin += X86InstructionBin_binv;
            bin += INSTBIN_DATATYPE(sizeof(int8_t));
            break;
        case UD_Ipshufhw:
        case UD_Ipshuflw:
        case UD_Ipshufw:
        case UD_Ipsllw:
        case UD_Ipsraw:
        case UD_Ipsrlw:
        case UD_Ipunpckhbw:
        case UD_Ipunpcklbw:
            bin += X86InstructionBin_binv;
            bin += INSTBIN_DATATYPE(sizeof(int16_t));
            break;
        case UD_Ipshufd:
        case UD_Ipslld:
        case UD_Ipsrad:
        case UD_Ipsrld:
        case UD_Ipunpckhwd:
        case UD_Ipunpcklwd:
            bin += X86InstructionBin_binv;
            bin += INSTBIN_DATATYPE(sizeof(int32_t));
            break;
        case UD_Ipslldq:
        case UD_Ipsllq:
        case UD_Ipsrlq:
        case UD_Ipsrldq:
        case UD_Ipunpckhdq:
        case UD_Ipunpckhqdq:
        case UD_Ipunpckldq:
        case UD_Ipunpcklqdq:
            bin += X86InstructionBin_binv;
            bin += INSTBIN_DATATYPE(sizeof(int64_t));
            break;
        case UD_Iandpd:
        case UD_Iandnpd:
        case UD_Iorpd:
        case UD_Ishufpd:
        case UD_Ixorpd:
        case UD_Iunpckhpd:
        case UD_Iunpcklpd:
            bin += X86InstructionBin_binv;
            bin += INSTBIN_DATATYPE(sizeof(double));
            break;
        case UD_Iandps:
        case UD_Iandnps:
        case UD_Iorps:
        case UD_Ishufps:
        case UD_Ixorps:
        case UD_Iunpckhps:
        case UD_Iunpcklps:
            bin += X86InstructionBin_binv;
            bin += INSTBIN_DATATYPE(sizeof(float));
            break;
        case UD_Ipalignr:
        case UD_Ipand:
        case UD_Ipandn:
        case UD_Ipor:
        case UD_Ipxor:
            bin += X86InstructionBin_binv;
            bin += INSTBIN_DATATYPE(getDstSizeInBytes());
            break;
        case UD_Iadc:
        case UD_Iadd:
        case UD_Icmp:
        case UD_Icmpxchg:
        case UD_Icmpxchg8b:
        case UD_Idec:
        case UD_Idiv:
        case UD_Iidiv:
        case UD_Iimul:
        case UD_Iinc:
        case UD_Imul:
        case UD_Ineg:
        case UD_Isbb:
        case UD_Isub:
        case UD_Ixadd:
        case UD_Ixchg:
            bin += X86InstructionBin_int;
            bin += INSTBIN_DATATYPE(getDstSizeInBytes());
            break;
        case UD_Ipacksswb:
        case UD_Ipackuswb:
        case UD_Ipaddb:
        case UD_Ipaddsb:
        case UD_Ipaddusb:
        case UD_Ipavgb:
        case UD_Ipcmpeqb:
        case UD_Ipcmpgtb:
        case UD_Ipmaxub:
        case UD_Ipminub:
            bin += X86InstructionBin_intv;
            bin += INSTBIN_DATATYPE(sizeof(int8_t));
            break;
        case UD_Ipavgw:
        case UD_Ipcmpeqw:
        case UD_Ipcmpgtw:
        case UD_Ipextrw:
        case UD_Ipinsrw:
        case UD_Ipmaxsw:
        case UD_Ipminsw:
        case UD_Ipmulhuw:
        case UD_Ipmulhw:
        case UD_Ipmullw:
        case UD_Ipavgusb:
        case UD_Ipsubb:
        case UD_Ipsubsb:
        case UD_Ipsubusb:
        case UD_Ipsadbw:
        case UD_Ipsubw:
        case UD_Ipsubsw:
        case UD_Ipsubusw:
        case UD_Ipi2fw:
        case UD_Ipf2iw:
        case UD_Ipaddw:
        case UD_Ipaddsw:
        case UD_Ipaddusw:
        case UD_Ipminuw:
        case UD_Ipmaxuw:
        case UD_Ipmulhrw:
            bin += X86InstructionBin_intv;
            bin += INSTBIN_DATATYPE(sizeof(int16_t));
            break;
        case UD_Ipcmpeqd:
        case UD_Ipcmpgtd:
        case UD_Ipf2id:
        case UD_Ipfacc:
        case UD_Ipfadd:
        case UD_Ipfcmpeq:
        case UD_Ipfcmpge:
        case UD_Ipfcmpgt:
        case UD_Ipfmax:
        case UD_Ipfmin:
        case UD_Ipfmul:
        case UD_Ipfnacc:
        case UD_Ipfpnacc:
        case UD_Ipfrcp:
        case UD_Ipfrcpit1:
        case UD_Ipfrcpit2:
        case UD_Ipfrspit1:
        case UD_Ipfrsqrt:
        case UD_Ipfsub:
        case UD_Ipfsubr:
        case UD_Ipi2fd:
        case UD_Ipaddd:
        case UD_Ipackssdw:
        case UD_Iphaddd:
        case UD_Ipmaddwd:
        case UD_Ipminsd:
        case UD_Ipminud:
        case UD_Ipmaxsd:
        case UD_Ipmaxud:
        case UD_Ipmuludq:
        case UD_Ipsubd:
        case UD_Ipswapd:
            bin += X86InstructionBin_intv;
            bin += INSTBIN_DATATYPE(sizeof(int32_t));
            break;
        case UD_Ipaddq:
        case UD_Ipsubq:
            bin += X86InstructionBin_intv;
            bin += INSTBIN_DATATYPE(sizeof(int64_t));
            break;
        case UD_Iaddpd:
        case UD_Iaddsubpd:
        case UD_Icmppd:
        case UD_Icvtpd2dq:
        case UD_Icvtpd2pi:
        case UD_Icvttpd2pi:
        case UD_Icvtdq2pd:
        case UD_Icvtpi2pd:
        case UD_Icvtps2pd:
        case UD_Icvttpd2dq:
        case UD_Idivpd:
        case UD_Ihaddpd:
        case UD_Ihsubpd:
        case UD_Imaxpd:
        case UD_Iminpd:
        case UD_Imulpd:
        case UD_Iroundpd:
        case UD_Isqrtpd:
        case UD_Isubpd:
            bin += X86InstructionBin_floatv;
            bin += INSTBIN_DATATYPE(sizeof(double));
            break;
        case UD_Iaddsd:
        case UD_Icomisd:
        case UD_Icvtsd2si:
        case UD_Icvtsd2ss:
        case UD_Icvtss2sd:
        case UD_Icvtsi2sd:
        case UD_Idivsd:
        case UD_Imaxsd:
        case UD_Iminsd:
        case UD_Imulsd:
        case UD_Iroundsd:
        case UD_Isqrtsd:
        case UD_Isubsd:
        case UD_Iucomisd:
            bin += X86InstructionBin_floats;
            bin += INSTBIN_DATATYPE(sizeof(double));
            break;
        case UD_Iaddps:
        case UD_Iaddsubps:
        case UD_Icmpps:
        case UD_Icvtps2dq:
        case UD_Icvtps2pi:
        case UD_Icvtdq2ps:
        case UD_Icvtpd2ps:
        case UD_Icvtpi2ps:
        case UD_Icvttps2dq:
        case UD_Icvttps2pi:
        case UD_Idivps:
        case UD_Ihaddps:
        case UD_Ihsubps:
        case UD_Imaxps:
        case UD_Iminps:
        case UD_Imulps:
        case UD_Ircpps:
        case UD_Iroundps:
        case UD_Irsqrtps:
        case UD_Isqrtps:
        case UD_Isubps:
            bin += X86InstructionBin_floatv;
            bin += INSTBIN_DATATYPE(sizeof(float));
            break;
        case UD_Iaddss:
        case UD_Icmpss:
        case UD_Icomiss:
        case UD_Icvtsi2ss:
        case UD_Icvtss2si:
        case UD_Icvttsd2si:
        case UD_Icvttss2si:
        case UD_Idivss:
        case UD_Imaxss:
        case UD_Iminss:
        case UD_Imulss:
        case UD_Ircpss:
        case UD_Iroundss:
        case UD_Irsqrtss:
        case UD_Isqrtss:
        case UD_Isubss:
        case UD_Iucomiss:
            bin += X86InstructionBin_floats;
            bin += INSTBIN_DATATYPE(sizeof(float));
            break;
        case UD_If2xm1:
        case UD_Ifabs:
        case UD_Ifadd:
        case UD_Ifaddp:
        case UD_Ifbld:
        case UD_Ifbstp:
        case UD_Ifchs:
        case UD_Ifclex:
        case UD_Ifucomi:
        case UD_Ifcomi:
        case UD_Ifucomip:
        case UD_Ifcomip:
        case UD_Ifcom:
        case UD_Ifcom2:
        case UD_Ifcomp3:
        case UD_Ifcomp:
        case UD_Ifcomp5:
        case UD_Ifcompp:
        case UD_Ifcos:
        case UD_Ifdecstp:
        case UD_Ifdiv:
        case UD_Ifdivp:
        case UD_Ifdivr:
        case UD_Ifdivrp:
        case UD_Ifiadd:
        case UD_Ifidivr:
        case UD_Ifidiv:
        case UD_Ifisub:
        case UD_Ifisubr:
        case UD_Ificom:
        case UD_Ificomp:
        case UD_Ifmul:
        case UD_Ifmulp:
        case UD_Ifimul:
        case UD_Ifpatan:
        case UD_Ifprem:
        case UD_Ifprem1:
        case UD_Ifptan:
        case UD_Ifrndint:
        case UD_Ifscale:
        case UD_Ifsin:
        case UD_Ifsincos:
        case UD_Ifsqrt:
        case UD_Ifsub:
        case UD_Ifsubp:
        case UD_Ifsubr:
        case UD_Ifsubrp:
        case UD_Iftst:
        case UD_Ifucom:
        case UD_Ifucomp:
        case UD_Ifucompp:
        case UD_Ifxam:
        case UD_Ifpxtract:
        case UD_Ifyl2x:
        case UD_Ifyl2xp1:
            bin += X86InstructionBin_float;
            bin += INSTBIN_DATATYPE(getDstSizeInBytes());
            break;
        case UD_Ifild:
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
        case UD_Ifcmovb:
        case UD_Ifcmove:
        case UD_Ifcmovbe:
        case UD_Ifcmovu:
        case UD_Ifcmovnb:
        case UD_Ifcmovne:
        case UD_Ifcmovnbe:
        case UD_Ifcmovnu:
        case UD_Ifxch:
        case UD_Ifxch4:
        case UD_Ifxch7:
        case UD_Ifstp:
        case UD_Ifstp1:
        case UD_Ifstp8:
        case UD_Ifstp9:
        case UD_Ifst:
        case UD_Ilddqu:
        case UD_Ilds:
        case UD_Ilea:
        case UD_Iles:
        case UD_Ilfs:
        case UD_Ilgs:
        case UD_Ilss:
        case UD_Istr:
        case UD_Imov:
        case UD_Imovapd:
        case UD_Imovaps:
        case UD_Imovddup:
        case UD_Imovdqa:
        case UD_Imovdqu:
        case UD_Imovmskpd:
        case UD_Imovmskps:
        case UD_Imovntdq:
        case UD_Imovnti:
        case UD_Imovntpd:
        case UD_Imovntps:
        case UD_Imovsldup:
        case UD_Imovshdup:
        case UD_Imovsx:
        case UD_Imovupd:
        case UD_Imovups:
        case UD_Imovzx:
        case UD_Imovsxd:
        case UD_Ipmovmskb:
            bin += X86InstructionBin_move;
            bin += INSTBIN_DATATYPE(getDstSizeInBytes());
            break;
        case UD_Ilodsb:
        case UD_Istosb:
            bin += X86InstructionBin_move;
            bin += INSTBIN_DATATYPE(sizeof(int8_t));
            break;
        case UD_Ilodsw:
        case UD_Istosw:
            bin += X86InstructionBin_move;
            bin += INSTBIN_DATATYPE(sizeof(int16_t));
            break;
        case UD_Ildmxcsr:
        case UD_Ilodsd:
        case UD_Istosd:
            bin += X86InstructionBin_move;
            bin += INSTBIN_DATATYPE(sizeof(int32_t));
            break;
        case UD_Ilodsq:
        case UD_Imaskmovq:
        case UD_Istosq:
        case UD_Imovq:
        case UD_Imovqa:
        case UD_Imovq2dq:
        case UD_Imovdq2q:
            bin += X86InstructionBin_move;
            bin += INSTBIN_DATATYPE(sizeof(int64_t));
            break;
        case UD_Imovss:
        case UD_Imovhps:
        case UD_Imovlps:
        case UD_Imovlhps:
        case UD_Imovhlps:
            bin += X86InstructionBin_move;
            bin += INSTBIN_DATATYPE(sizeof(float));
            break;
        case UD_Imovd:
        case UD_Imovhpd:
        case UD_Imovlpd:
        case UD_Imovntq:
        case UD_Imovsd:
            bin += X86InstructionBin_move;
            bin += INSTBIN_DATATYPE(sizeof(double));
            break;
        case UD_Imovsb:
        case UD_Icmpsb:
        case UD_Iscasb:
            bin += X86InstructionBin_string;
            bin += INSTBIN_DATATYPE(sizeof(int8_t));
            break;
        case UD_Icmpsw:
        case UD_Imovsw:
        case UD_Iscasw:
            bin += X86InstructionBin_string;
            bin += INSTBIN_DATATYPE(sizeof(int16_t));
            break;
        case UD_Icmpsd:
        case UD_Iscasd:
            bin += X86InstructionBin_string;
            bin += INSTBIN_DATATYPE(sizeof(int32_t));
            break;
        case UD_Icmpsq:
        case UD_Imovsq:
        case UD_Iscasq:
            bin += X86InstructionBin_string;
            bin += INSTBIN_DATATYPE(sizeof(int64_t));
            break;
        case UD_Irepne: //FIXME variable size
        case UD_Irep: //FIXME variable size
            bin += X86InstructionBin_string;
            break;
        case UD_Ija:
        case UD_Ijae:
        case UD_Ijb:
        case UD_Ijbe:
        case UD_Ijcxz:
        case UD_Ijecxz:
        case UD_Ijg:
        case UD_Ijge:
        case UD_Ijl:
        case UD_Ijle:
        case UD_Ijno:
        case UD_Ijnp:
        case UD_Ijns:
        case UD_Ijnz:
        case UD_Ijo:
        case UD_Ijp:
        case UD_Ijrcxz:
        case UD_Ijs:
        case UD_Ijz:
            bin += X86InstructionBin_cond;
            break;
        case UD_Icall:
        case UD_Iret:
        case UD_Iretf:
            bin += BinFrame;
        case UD_Ijmp:
            bin += X86InstructionBin_uncond;
            break;
        case UD_Ienter:
        case UD_Ileave:
            bin += X86InstructionBin_stack;
            bin += BinFrame;
            break;
        case UD_Ifnsave:
        case UD_Ifnstcw:
        case UD_Ifnstenv:
        case UD_Ifnstsw:
        case UD_Ifrstor:
        case UD_Ifxrstor:
        case UD_Ifxsave:
            bin += X86InstructionBin_stack;
            bin += BinFrame;
            break;
        case UD_Ipop:
        case UD_Ipush:
            bin += X86InstructionBin_stack;
            bin += BinStack;
            bin += INSTBIN_DATATYPE(getDstSizeInBytes());
            break;
        case UD_Ipopa:
        case UD_Ipusha:
            bin += X86InstructionBin_stack;
            bin += BinFrame;
            bin += INSTBIN_DATATYPE(sizeof(int16_t));
            break;
        case UD_Ipopad:
        case UD_Ipushad:
            bin += X86InstructionBin_stack;
            bin += BinFrame;
            bin += INSTBIN_DATATYPE(sizeof(int32_t));
            break;
        case UD_Ipopfw:
        case UD_Ipushfw:
            bin += X86InstructionBin_stack;
            bin += BinStack;
            bin += INSTBIN_DATATYPE(sizeof(int16_t));
            break;
        case UD_Ipopfd:
        case UD_Ipushfd:
            bin += X86InstructionBin_stack;
            bin += BinStack;
            bin += INSTBIN_DATATYPE(sizeof(int32_t));
            break;
        case UD_Ipopfq:
        case UD_Ipushfq:
            bin += X86InstructionBin_stack;
            bin += BinStack;
            bin += INSTBIN_DATATYPE(sizeof(int64_t));
            break;
        case UD_Iclflush:
        case UD_Iinvd:
        case UD_Iinvlpg:
        case UD_Iinvlpga:
        case UD_Iprefetch:
        case UD_Iprefetchnta:
        case UD_Iprefetcht0:
        case UD_Iprefetcht1:
        case UD_Iprefetcht2:
            bin += X86InstructionBin_cache;
            break;
        case UD_Iint:
        case UD_Iint1:
        case UD_Iint3:
        case UD_Iinto:
        case UD_Iiretd:
        case UD_Iiretq:
        case UD_Iiretw:
        case UD_Isyscall:
        case UD_Isysenter:
        case UD_Isysexit:
        case UD_Isysret:
            bin += X86InstructionBin_system;
            bin += BinFrame;
            break;
        case UD_Id3vil:
        case UD_Idb:
        case UD_Igrp_asize:
        case UD_Igrp_mod:
        case UD_Igrp_mode:
        case UD_Igrp_osize:
        case UD_Igrp_reg:
        case UD_Igrp_rm:
        case UD_Igrp_vendor:
        case UD_Igrp_x87:
        case UD_Iinvalid:
        case UD_Ina:
        case UD_Inone:
        case UD_Iud2:
            bin += X86InstructionBin_invalid;
            break;
        case UD_I3dnow:
        case UD_Iaaa:
        case UD_Iaad:
        case UD_Iaam:
        case UD_Iaas:
        case UD_Iarpl:
        case UD_Iclc:
        case UD_Icld:
        case UD_Iclgi:
        case UD_Icli:
        case UD_Iclts:
        case UD_Icmc:
        case UD_Icpuid:
        case UD_Idaa:
        case UD_Idas:
        case UD_Iemms:
        case UD_Ifemms:
        case UD_Iffree:
        case UD_Iffreep:
        case UD_Ifldcw:
        case UD_Ifldenv:
        case UD_Ifncstp:
        case UD_Ifninit:
        case UD_Ifnop:
        case UD_Ihlt:
        case UD_Iin:
        case UD_Iinsb:
        case UD_Iinsd:
        case UD_Iinsw:
        case UD_Ilahf:
        case UD_Ilar:
        case UD_Ilfence:
        case UD_Ilgdt:
        case UD_Ilidt:
        case UD_Illdt:
        case UD_Ilmsw:
        case UD_Ilock:
        case UD_Iloop:
        case UD_Iloope:
        case UD_Iloopnz:
        case UD_Ilsl:
        case UD_Iltr:
        case UD_Imfence:
        case UD_Imonitor:
        case UD_Imwait:
        case UD_Inop:
        case UD_Iout:
        case UD_Ioutsb:
        case UD_Ioutsd:
        case UD_Ioutsq:
        case UD_Ioutsw:
        case UD_Ipause:
        case UD_Irdmsr:
        case UD_Irdpmc:
        case UD_Irdtsc:
        case UD_Irdtscp:
        case UD_Irsm:
        case UD_Isahf:
        case UD_Isfence:
        case UD_Isgdt:
        case UD_Isidt:
        case UD_Iskinit:
        case UD_Isldt:
        case UD_Ismsw:
        case UD_Istc:
        case UD_Istd:
        case UD_Istgi:
        case UD_Isti:
        case UD_Istmxcsr:
        case UD_Iswapgs:
        case UD_Iverr:
        case UD_Iverw:
        case UD_Ivmcall:
        case UD_Ivmclear:
        case UD_Ivmload:
        case UD_Ivmmcall:
        case UD_Ivmptrld:
        case UD_Ivmptrst:
        case UD_Ivmresume:
        case UD_Ivmrun:
        case UD_Ivmsave:
        case UD_Ivmxoff:
        case UD_Ivmxon:
        case UD_Iwait:
        case UD_Iwbinvd:
        case UD_Iwrmsr:
        case UD_Ixlatb:
            bin += X86InstructionBin_other;
            break;
        default:
            bin = X86InstructionBin_unknown;
            break;
    };
    instructionBin = bin;
    return instructionBin;
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
    instructionBin = X86InstructionBin_unknown;
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

    setFlags();

    defXIter = false;

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
    instructionBin = X86InstructionBin_unknown;
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

    setFlags();

    defXIter = false;

    verify();
}

void X86Instruction::print(){
    char flags[9];
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

    flags[8] = '\0';

    PRINT_INFOR("%#llx:\t%16s\t%s\tflgs:[%8s]\t-> %#llx %hx", getBaseAddress(), GET(insn_hexcode), GET(insn_buffer), flags, getTargetAddress(), getInstructionBin());

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
    __reg_define(flags_usedef, UD_Icmpsd, __bit_shift(X86_FLAG_DF), __x86_flagset_alustd);
    __reg_define(flags_usedef, UD_Icmpss, __bit_shift(X86_FLAG_DF), __x86_flagset_alustd);
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
    __reg_define(flags_usedef, UD_Ijz, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
    __reg_define(flags_usedef, UD_Ijnz, __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_CF), 0);
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
        if (!getOperand(COMP_DEST_OPERAND) && !getOperand(COMP_SRC_OPERAND)){
            flags_usedef[__reg_use] = __bit_shift(X86_FLAG_DF);
        }
    }

    if (GET(flags_use) != flags_usedef[__reg_use]){
        print();
        PRINT_ERROR("NEW USE FLAGS (%#x) DONT MATCH OLD (%#x)", GET(flags_use), flags_usedef[__reg_use]);
    }
    if (GET(flags_def) != flags_usedef[__reg_def]){
        print();
        PRINT_ERROR("NEW DEF FLAGS (%#x) DONT MATCH OLD (%#x)", GET(flags_def), flags_usedef[__reg_def]);
    }
}

#include <map>
using namespace std;
static map<int, X86InstructionType> classMap;

X86InstructionType X86InstructionClassifier::getClass(int mnemonic){
    ASSERT(mnemonic >= 0 && mnemonic < UD_Itotaltypes && "invalid mnemonic");
    if (classMap.size() == 0){
        fillClassDefinitions();
    }
    ASSERT(classMap.count(mnemonic) == 1);
    return classMap[mnemonic];
}

bool X86InstructionClassifier::verify(){
    for (uint32_t i = 0; i < UD_Itotaltypes; i++){
        if (classMap.count(i) != 1){
            PRINT_ERROR("Instruction classification definition missing for %s", ud_mnemonics_str[i]);
            return false;
        }
    }
    for (map<int, X86InstructionType>::iterator ii = classMap.begin(); ii != classMap.end(); ii++){
        int mnemonic = (*ii).first;
        X86InstructionType typ = (*ii).second;
        if (mnemonic < 0 || mnemonic >= UD_Itotaltypes){
            PRINT_ERROR("Invalid mnemonic found: %d", mnemonic);
            return false;
        }
    }
    if (classMap.size() != UD_Itotaltypes){
        PRINT_ERROR("number of elements in classification table is incorrect");
        return false;
    }
    return true;
}

X86InstructionType X86Instruction::getInstructionType(){
    return X86InstructionClassifier::getClass(GET(mnemonic));
}

void X86InstructionClassifier::fillClassDefinitions(){
    classMap[UD_I3dnow] = X86InstructionType_special;
    classMap[UD_Iaaa] = X86InstructionType_int;
    classMap[UD_Iaad] = X86InstructionType_int;
    classMap[UD_Iaam] = X86InstructionType_int;
    classMap[UD_Iaas] = X86InstructionType_int;
    classMap[UD_Iadc] = X86InstructionType_int;
    classMap[UD_Iadd] = X86InstructionType_int;
    classMap[UD_Iaddpd] = X86InstructionType_float;
    classMap[UD_Iaddps] = X86InstructionType_float;
    classMap[UD_Iaddsd] = X86InstructionType_float;
    classMap[UD_Iaddss] = X86InstructionType_float;
    classMap[UD_Iaddsubpd] = X86InstructionType_float;
    classMap[UD_Iaddsubps] = X86InstructionType_float;
    classMap[UD_Iand] = X86InstructionType_int;
    classMap[UD_Iandnpd] = X86InstructionType_float;
    classMap[UD_Iandnps] = X86InstructionType_float;
    classMap[UD_Iandpd] = X86InstructionType_float;
    classMap[UD_Iandps] = X86InstructionType_float;
    classMap[UD_Iarpl] = X86InstructionType_special;
    classMap[UD_Iblendpd] = X86InstructionType_int;
    classMap[UD_Iblendps] = X86InstructionType_int;
    classMap[UD_Iblendvpd] = X86InstructionType_int;
    classMap[UD_Iblendvps] = X86InstructionType_int;
    classMap[UD_Ibound] = X86InstructionType_int;
    classMap[UD_Ibsf] = X86InstructionType_int;
    classMap[UD_Ibsr] = X86InstructionType_int;
    classMap[UD_Ibswap] = X86InstructionType_int;
    classMap[UD_Ibt] = X86InstructionType_int;
    classMap[UD_Ibtc] = X86InstructionType_int;
    classMap[UD_Ibtr] = X86InstructionType_int;
    classMap[UD_Ibts] = X86InstructionType_int;
    classMap[UD_Icall] = X86InstructionType_call;
    classMap[UD_Icbw] = X86InstructionType_special;
    classMap[UD_Icdq] = X86InstructionType_special;
    classMap[UD_Icdqe] = X86InstructionType_special;
    classMap[UD_Iclc] = X86InstructionType_special;
    classMap[UD_Icld] = X86InstructionType_special;
    classMap[UD_Iclflush] = X86InstructionType_special;
    classMap[UD_Iclgi] = X86InstructionType_special;
    classMap[UD_Icli] = X86InstructionType_special;
    classMap[UD_Iclts] = X86InstructionType_special;
    classMap[UD_Icmc] = X86InstructionType_special;
    classMap[UD_Icmova] = X86InstructionType_move;
    classMap[UD_Icmovae] = X86InstructionType_move;
    classMap[UD_Icmovb] = X86InstructionType_move;
    classMap[UD_Icmovbe] = X86InstructionType_move;
    classMap[UD_Icmovg] = X86InstructionType_move;
    classMap[UD_Icmovge] = X86InstructionType_move;
    classMap[UD_Icmovl] = X86InstructionType_move;
    classMap[UD_Icmovle] = X86InstructionType_move;
    classMap[UD_Icmovno] = X86InstructionType_move;
    classMap[UD_Icmovnp] = X86InstructionType_move;
    classMap[UD_Icmovns] = X86InstructionType_move;
    classMap[UD_Icmovnz] = X86InstructionType_move;
    classMap[UD_Icmovo] = X86InstructionType_move;
    classMap[UD_Icmovp] = X86InstructionType_move;
    classMap[UD_Icmovs] = X86InstructionType_move;
    classMap[UD_Icmovz] = X86InstructionType_move;
    classMap[UD_Icmp] = X86InstructionType_int;
    classMap[UD_Icmppd] = X86InstructionType_float;
    classMap[UD_Icmpps] = X86InstructionType_float;
    classMap[UD_Icmpsb] = X86InstructionType_string;
    classMap[UD_Icmpsd] = X86InstructionType_float;
    classMap[UD_Icmpsq] = X86InstructionType_string;
    classMap[UD_Icmpss] = X86InstructionType_float;
    classMap[UD_Icmpsw] = X86InstructionType_string;
    classMap[UD_Icmpxchg] = X86InstructionType_int;
    classMap[UD_Icmpxchg8b] = X86InstructionType_int;
    classMap[UD_Icomisd] = X86InstructionType_float;
    classMap[UD_Icomiss] = X86InstructionType_float;
    classMap[UD_Icpuid] = X86InstructionType_special;
    classMap[UD_Icqo] = X86InstructionType_special;
    classMap[UD_Icvtdq2pd] = X86InstructionType_float;
    classMap[UD_Icvtdq2ps] = X86InstructionType_float;
    classMap[UD_Icvtpd2dq] = X86InstructionType_float;
    classMap[UD_Icvtpd2pi] = X86InstructionType_float;
    classMap[UD_Icvtpd2ps] = X86InstructionType_float;
    classMap[UD_Icvtpi2pd] = X86InstructionType_float;
    classMap[UD_Icvtpi2ps] = X86InstructionType_float;
    classMap[UD_Icvtps2dq] = X86InstructionType_float;
    classMap[UD_Icvtps2pd] = X86InstructionType_float;
    classMap[UD_Icvtps2pi] = X86InstructionType_float;
    classMap[UD_Icvtsd2si] = X86InstructionType_float;
    classMap[UD_Icvtsd2ss] = X86InstructionType_float;
    classMap[UD_Icvtsi2sd] = X86InstructionType_float;
    classMap[UD_Icvtsi2ss] = X86InstructionType_float;
    classMap[UD_Icvtss2sd] = X86InstructionType_float;
    classMap[UD_Icvtss2si] = X86InstructionType_float;
    classMap[UD_Icvttpd2dq] = X86InstructionType_float;
    classMap[UD_Icvttpd2pi] = X86InstructionType_float;
    classMap[UD_Icvttps2dq] = X86InstructionType_float;
    classMap[UD_Icvttps2pi] = X86InstructionType_float;
    classMap[UD_Icvttsd2si] = X86InstructionType_float;
    classMap[UD_Icvttss2si] = X86InstructionType_float;
    classMap[UD_Icwd] = X86InstructionType_special;
    classMap[UD_Icwde] = X86InstructionType_special;
    classMap[UD_Idaa] = X86InstructionType_int;
    classMap[UD_Idas] = X86InstructionType_int;
    classMap[UD_Idb] = X86InstructionType_invalid;
    classMap[UD_Idec] = X86InstructionType_int;
    classMap[UD_Idiv] = X86InstructionType_int;
    classMap[UD_Idivpd] = X86InstructionType_float;
    classMap[UD_Idivps] = X86InstructionType_float;
    classMap[UD_Idivsd] = X86InstructionType_float;
    classMap[UD_Idivss] = X86InstructionType_float;
    classMap[UD_Idppd] = X86InstructionType_simd;
    classMap[UD_Idpps] = X86InstructionType_simd;
    classMap[UD_Iemms] = X86InstructionType_special;
    classMap[UD_Ienter] = X86InstructionType_special;
    classMap[UD_Iextractps] = X86InstructionType_simd;
    classMap[UD_If2xm1] = X86InstructionType_float;
    classMap[UD_Ifabs] = X86InstructionType_float;
    classMap[UD_Ifadd] = X86InstructionType_float;
    classMap[UD_Ifaddp] = X86InstructionType_float;
    classMap[UD_Ifbld] = X86InstructionType_float;
    classMap[UD_Ifbstp] = X86InstructionType_float;
    classMap[UD_Ifchs] = X86InstructionType_float;
    classMap[UD_Ifclex] = X86InstructionType_float;
    classMap[UD_Ifcmovb] = X86InstructionType_move;
    classMap[UD_Ifcmovbe] = X86InstructionType_move;
    classMap[UD_Ifcmove] = X86InstructionType_move;
    classMap[UD_Ifcmovnb] = X86InstructionType_move;
    classMap[UD_Ifcmovnbe] = X86InstructionType_move;
    classMap[UD_Ifcmovne] = X86InstructionType_move;
    classMap[UD_Ifcmovnu] = X86InstructionType_move;
    classMap[UD_Ifcmovu] = X86InstructionType_move;
    classMap[UD_Ifcom] = X86InstructionType_float;
    classMap[UD_Ifcom2] = X86InstructionType_float;
    classMap[UD_Ifcomi] = X86InstructionType_float;
    classMap[UD_Ifcomip] = X86InstructionType_float;
    classMap[UD_Ifcomp] = X86InstructionType_float;
    classMap[UD_Ifcomp3] = X86InstructionType_float;
    classMap[UD_Ifcomp5] = X86InstructionType_float;
    classMap[UD_Ifcompp] = X86InstructionType_float;
    classMap[UD_Ifcos] = X86InstructionType_float;
    classMap[UD_Ifdecstp] = X86InstructionType_float;
    classMap[UD_Ifdiv] = X86InstructionType_float;
    classMap[UD_Ifdivp] = X86InstructionType_float;
    classMap[UD_Ifdivr] = X86InstructionType_float;
    classMap[UD_Ifdivrp] = X86InstructionType_float;
    classMap[UD_Ifemms] = X86InstructionType_float;
    classMap[UD_Iffree] = X86InstructionType_float;
    classMap[UD_Iffreep] = X86InstructionType_float;
    classMap[UD_Ifiadd] = X86InstructionType_float;
    classMap[UD_Ificom] = X86InstructionType_float;
    classMap[UD_Ificomp] = X86InstructionType_float;
    classMap[UD_Ifidiv] = X86InstructionType_float;
    classMap[UD_Ifidivr] = X86InstructionType_float;
    classMap[UD_Ifild] = X86InstructionType_float;
    classMap[UD_Ifimul] = X86InstructionType_float;
    classMap[UD_Ifist] = X86InstructionType_float;
    classMap[UD_Ifistp] = X86InstructionType_float;
    classMap[UD_Ifisttp] = X86InstructionType_float;
    classMap[UD_Ifisub] = X86InstructionType_float;
    classMap[UD_Ifisubr] = X86InstructionType_float;
    classMap[UD_Ifld] = X86InstructionType_float;
    classMap[UD_Ifld1] = X86InstructionType_float;
    classMap[UD_Ifldcw] = X86InstructionType_special;
    classMap[UD_Ifldenv] = X86InstructionType_special;
    classMap[UD_Ifldl2e] = X86InstructionType_float;
    classMap[UD_Ifldl2t] = X86InstructionType_float;
    classMap[UD_Ifldlg2] = X86InstructionType_float;
    classMap[UD_Ifldln2] = X86InstructionType_float;
    classMap[UD_Ifldlpi] = X86InstructionType_float;
    classMap[UD_Ifldz] = X86InstructionType_float;
    classMap[UD_Ifmul] = X86InstructionType_float;
    classMap[UD_Ifmulp] = X86InstructionType_float;
    classMap[UD_Ifncstp] = X86InstructionType_float;
    classMap[UD_Ifninit] = X86InstructionType_float;
    classMap[UD_Ifnop] = X86InstructionType_nop;
    classMap[UD_Ifnsave] = X86InstructionType_special;
    classMap[UD_Ifnstcw] = X86InstructionType_special;
    classMap[UD_Ifnstenv] = X86InstructionType_special;
    classMap[UD_Ifnstsw] = X86InstructionType_special;
    classMap[UD_Ifpatan] = X86InstructionType_float;
    classMap[UD_Ifprem] = X86InstructionType_float;
    classMap[UD_Ifprem1] = X86InstructionType_float;
    classMap[UD_Ifptan] = X86InstructionType_float;
    classMap[UD_Ifpxtract] = X86InstructionType_float;
    classMap[UD_Ifrndint] = X86InstructionType_float;
    classMap[UD_Ifrstor] = X86InstructionType_special;
    classMap[UD_Ifscale] = X86InstructionType_float;
    classMap[UD_Ifsin] = X86InstructionType_float;
    classMap[UD_Ifsincos] = X86InstructionType_float;
    classMap[UD_Ifsqrt] = X86InstructionType_float;
    classMap[UD_Ifst] = X86InstructionType_float;
    classMap[UD_Ifstp] = X86InstructionType_float;
    classMap[UD_Ifstp1] = X86InstructionType_float;
    classMap[UD_Ifstp8] = X86InstructionType_float;
    classMap[UD_Ifstp9] = X86InstructionType_float;
    classMap[UD_Ifsub] = X86InstructionType_float;
    classMap[UD_Ifsubp] = X86InstructionType_float;
    classMap[UD_Ifsubr] = X86InstructionType_float;
    classMap[UD_Ifsubrp] = X86InstructionType_float;
    classMap[UD_Iftst] = X86InstructionType_float;
    classMap[UD_Ifucom] = X86InstructionType_float;
    classMap[UD_Ifucomi] = X86InstructionType_float;
    classMap[UD_Ifucomip] = X86InstructionType_float;
    classMap[UD_Ifucomp] = X86InstructionType_float;
    classMap[UD_Ifucompp] = X86InstructionType_float;
    classMap[UD_Ifxam] = X86InstructionType_float;
    classMap[UD_Ifxch] = X86InstructionType_int;
    classMap[UD_Ifxch4] = X86InstructionType_int;
    classMap[UD_Ifxch7] = X86InstructionType_int;
    classMap[UD_Ifxrstor] = X86InstructionType_special;
    classMap[UD_Ifxsave] = X86InstructionType_special;
    classMap[UD_Ifyl2x] = X86InstructionType_float;
    classMap[UD_Ifyl2xp1] = X86InstructionType_float;
    classMap[UD_Ihaddpd] = X86InstructionType_float;
    classMap[UD_Ihaddps] = X86InstructionType_float;
    classMap[UD_Ihlt] = X86InstructionType_halt;
    classMap[UD_Ihsubpd] = X86InstructionType_float;
    classMap[UD_Ihsubps] = X86InstructionType_float;
    classMap[UD_Iidiv] = X86InstructionType_int;
    classMap[UD_Iimul] = X86InstructionType_int;
    classMap[UD_Iin] = X86InstructionType_io;
    classMap[UD_Iinc] = X86InstructionType_int;
    classMap[UD_Iinsb] = X86InstructionType_io;
    classMap[UD_Iinsd] = X86InstructionType_io;
    classMap[UD_Iinsertps] = X86InstructionType_simd;
    classMap[UD_Iinsw] = X86InstructionType_io;
    classMap[UD_Iint] = X86InstructionType_trap;
    classMap[UD_Iint1] = X86InstructionType_trap;
    classMap[UD_Iint3] = X86InstructionType_trap;
    classMap[UD_Iinto] = X86InstructionType_trap;
    classMap[UD_Iinvalid] = X86InstructionType_invalid;
    classMap[UD_Iinvd] = X86InstructionType_special;
    classMap[UD_Iinvlpg] = X86InstructionType_special;
    classMap[UD_Iinvlpga] = X86InstructionType_special;
    classMap[UD_Iiretd] = X86InstructionType_return;
    classMap[UD_Iiretq] = X86InstructionType_return;
    classMap[UD_Iiretw] = X86InstructionType_return;
    classMap[UD_Ija] = X86InstructionType_cond_branch;
    classMap[UD_Ijae] = X86InstructionType_cond_branch;
    classMap[UD_Ijb] = X86InstructionType_cond_branch;
    classMap[UD_Ijbe] = X86InstructionType_cond_branch;
    classMap[UD_Ijcxz] = X86InstructionType_cond_branch;
    classMap[UD_Ijecxz] = X86InstructionType_cond_branch;
    classMap[UD_Ijg] = X86InstructionType_cond_branch;
    classMap[UD_Ijge] = X86InstructionType_cond_branch;
    classMap[UD_Ijl] = X86InstructionType_cond_branch;
    classMap[UD_Ijle] = X86InstructionType_cond_branch;
    classMap[UD_Ijmp] = X86InstructionType_uncond_branch;
    classMap[UD_Ijno] = X86InstructionType_cond_branch;
    classMap[UD_Ijnp] = X86InstructionType_cond_branch;
    classMap[UD_Ijns] = X86InstructionType_cond_branch;
    classMap[UD_Ijnz] = X86InstructionType_cond_branch;
    classMap[UD_Ijo] = X86InstructionType_cond_branch;
    classMap[UD_Ijp] = X86InstructionType_cond_branch;
    classMap[UD_Ijrcxz] = X86InstructionType_cond_branch;
    classMap[UD_Ijs] = X86InstructionType_cond_branch;
    classMap[UD_Ijz] = X86InstructionType_cond_branch;
    classMap[UD_Ilahf] = X86InstructionType_int;
    classMap[UD_Ilar] = X86InstructionType_special;
    classMap[UD_Ilddqu] = X86InstructionType_int;
    classMap[UD_Ildmxcsr] = X86InstructionType_special;
    classMap[UD_Ilds] = X86InstructionType_move;
    classMap[UD_Ilea] = X86InstructionType_move;
    classMap[UD_Ileave] = X86InstructionType_special;
    classMap[UD_Iles] = X86InstructionType_int;
    classMap[UD_Ilfence] = X86InstructionType_special;
    classMap[UD_Ilfs] = X86InstructionType_int;
    classMap[UD_Ilgdt] = X86InstructionType_special;
    classMap[UD_Ilgs] = X86InstructionType_int;
    classMap[UD_Ilidt] = X86InstructionType_special;
    classMap[UD_Illdt] = X86InstructionType_special;
    classMap[UD_Ilmsw] = X86InstructionType_special;
    classMap[UD_Ilock] = X86InstructionType_special;
    classMap[UD_Ilodsb] = X86InstructionType_string;
    classMap[UD_Ilodsd] = X86InstructionType_string;
    classMap[UD_Ilodsq] = X86InstructionType_string;
    classMap[UD_Ilodsw] = X86InstructionType_string;
    classMap[UD_Iloop] = X86InstructionType_special;
    classMap[UD_Iloope] = X86InstructionType_special;
    classMap[UD_Iloopnz] = X86InstructionType_special;
    classMap[UD_Ilsl] = X86InstructionType_special;
    classMap[UD_Ilss] = X86InstructionType_int;
    classMap[UD_Iltr] = X86InstructionType_special;
    classMap[UD_Imaskmovq] = X86InstructionType_move;
    classMap[UD_Imaxpd] = X86InstructionType_float;
    classMap[UD_Imaxps] = X86InstructionType_float;
    classMap[UD_Imaxsd] = X86InstructionType_float;
    classMap[UD_Imaxss] = X86InstructionType_float;
    classMap[UD_Imfence] = X86InstructionType_special;
    classMap[UD_Iminpd] = X86InstructionType_float;
    classMap[UD_Iminps] = X86InstructionType_float;
    classMap[UD_Iminsd] = X86InstructionType_float;
    classMap[UD_Iminss] = X86InstructionType_float;
    classMap[UD_Imonitor] = X86InstructionType_special;
    classMap[UD_Imov] = X86InstructionType_move;
    classMap[UD_Imovapd] = X86InstructionType_move;
    classMap[UD_Imovaps] = X86InstructionType_move;
    classMap[UD_Imovd] = X86InstructionType_move;
    classMap[UD_Imovddup] = X86InstructionType_move;
    classMap[UD_Imovdq2q] = X86InstructionType_move;
    classMap[UD_Imovdqa] = X86InstructionType_move;
    classMap[UD_Imovdqu] = X86InstructionType_move;
    classMap[UD_Imovhlps] = X86InstructionType_move;
    classMap[UD_Imovhpd] = X86InstructionType_move;
    classMap[UD_Imovhps] = X86InstructionType_move;
    classMap[UD_Imovlhps] = X86InstructionType_move;
    classMap[UD_Imovlpd] = X86InstructionType_move;
    classMap[UD_Imovlps] = X86InstructionType_move;
    classMap[UD_Imovmskpd] = X86InstructionType_move;
    classMap[UD_Imovmskps] = X86InstructionType_move;
    classMap[UD_Imovntdq] = X86InstructionType_move;
    classMap[UD_Imovntdqa] = X86InstructionType_move;
    classMap[UD_Imovnti] = X86InstructionType_move;
    classMap[UD_Imovntpd] = X86InstructionType_move;
    classMap[UD_Imovntps] = X86InstructionType_move;
    classMap[UD_Imovntq] = X86InstructionType_move;
    classMap[UD_Imovq] = X86InstructionType_move;
    classMap[UD_Imovq2dq] = X86InstructionType_move;
    classMap[UD_Imovqa] = X86InstructionType_move;
    classMap[UD_Imovsb] = X86InstructionType_move;
    classMap[UD_Imovsd] = X86InstructionType_move;
    classMap[UD_Imovshdup] = X86InstructionType_move;
    classMap[UD_Imovsldup] = X86InstructionType_move;
    classMap[UD_Imovsq] = X86InstructionType_move;
    classMap[UD_Imovss] = X86InstructionType_move;
    classMap[UD_Imovsw] = X86InstructionType_move;
    classMap[UD_Imovsx] = X86InstructionType_move;
    classMap[UD_Imovsxd] = X86InstructionType_move;
    classMap[UD_Imovupd] = X86InstructionType_move;
    classMap[UD_Imovups] = X86InstructionType_move;
    classMap[UD_Imovzx] = X86InstructionType_move;
    classMap[UD_Impsadbw] = X86InstructionType_simd;
    classMap[UD_Imul] = X86InstructionType_int;
    classMap[UD_Imulpd] = X86InstructionType_float;
    classMap[UD_Imulps] = X86InstructionType_float;
    classMap[UD_Imulsd] = X86InstructionType_float;
    classMap[UD_Imulss] = X86InstructionType_float;
    classMap[UD_Imwait] = X86InstructionType_special;
    classMap[UD_Ineg] = X86InstructionType_int;
    classMap[UD_Inop] = X86InstructionType_nop;
    classMap[UD_Inot] = X86InstructionType_int;
    classMap[UD_Ior] = X86InstructionType_int;
    classMap[UD_Iorpd] = X86InstructionType_float;
    classMap[UD_Iorps] = X86InstructionType_float;
    classMap[UD_Iout] = X86InstructionType_io;
    classMap[UD_Ioutsb] = X86InstructionType_io;
    classMap[UD_Ioutsd] = X86InstructionType_io;
    classMap[UD_Ioutsq] = X86InstructionType_io;
    classMap[UD_Ioutsw] = X86InstructionType_io;
    classMap[UD_Ipackssdw] = X86InstructionType_int;
    classMap[UD_Ipacksswb] = X86InstructionType_int;
    classMap[UD_Ipackusdw] = X86InstructionType_simd;
    classMap[UD_Ipackuswb] = X86InstructionType_int;
    classMap[UD_Ipaddb] = X86InstructionType_int;
    classMap[UD_Ipaddd] = X86InstructionType_int;
    classMap[UD_Ipaddq] = X86InstructionType_int;
    classMap[UD_Ipaddsb] = X86InstructionType_int;
    classMap[UD_Ipaddsw] = X86InstructionType_int;
    classMap[UD_Ipaddusb] = X86InstructionType_int;
    classMap[UD_Ipaddusw] = X86InstructionType_int;
    classMap[UD_Ipaddw] = X86InstructionType_int;
    classMap[UD_Ipalignr] = X86InstructionType_simd;
    classMap[UD_Ipand] = X86InstructionType_int;
    classMap[UD_Ipandn] = X86InstructionType_int;
    classMap[UD_Ipause] = X86InstructionType_special;
    classMap[UD_Ipavgb] = X86InstructionType_int;
    classMap[UD_Ipavgusb] = X86InstructionType_int;
    classMap[UD_Ipavgw] = X86InstructionType_int;
    classMap[UD_Ipblendvb] = X86InstructionType_int;
    classMap[UD_Ipblendw] = X86InstructionType_int;
    classMap[UD_Ipclmulqdq] = X86InstructionType_simd;
    classMap[UD_Ipcmpeqb] = X86InstructionType_int;
    classMap[UD_Ipcmpeqd] = X86InstructionType_int;
    classMap[UD_Ipcmpeqq] = X86InstructionType_simd;
    classMap[UD_Ipcmpeqw] = X86InstructionType_int;
    classMap[UD_Ipcmpestri] = X86InstructionType_simd;
    classMap[UD_Ipcmpestrm] = X86InstructionType_simd;
    classMap[UD_Ipcmpgtb] = X86InstructionType_int;
    classMap[UD_Ipcmpgtd] = X86InstructionType_int;
    classMap[UD_Ipcmpgtq] = X86InstructionType_int;
    classMap[UD_Ipcmpgtw] = X86InstructionType_int;
    classMap[UD_Ipcmpistri] = X86InstructionType_simd;
    classMap[UD_Ipcmpistrm] = X86InstructionType_simd;
    classMap[UD_Ipextrb] = X86InstructionType_simd;
    classMap[UD_Ipextrd] = X86InstructionType_simd;
    classMap[UD_Ipextrq] = X86InstructionType_simd;
    classMap[UD_Ipextrw] = X86InstructionType_simd;
    classMap[UD_Ipf2id] = X86InstructionType_int;
    classMap[UD_Ipf2iw] = X86InstructionType_int;
    classMap[UD_Ipfacc] = X86InstructionType_int;
    classMap[UD_Ipfadd] = X86InstructionType_int;
    classMap[UD_Ipfcmpeq] = X86InstructionType_int;
    classMap[UD_Ipfcmpge] = X86InstructionType_int;
    classMap[UD_Ipfcmpgt] = X86InstructionType_int;
    classMap[UD_Ipfmax] = X86InstructionType_int;
    classMap[UD_Ipfmin] = X86InstructionType_int;
    classMap[UD_Ipfmul] = X86InstructionType_int;
    classMap[UD_Ipfnacc] = X86InstructionType_int;
    classMap[UD_Ipfpnacc] = X86InstructionType_int;
    classMap[UD_Ipfrcp] = X86InstructionType_int;
    classMap[UD_Ipfrcpit1] = X86InstructionType_int;
    classMap[UD_Ipfrcpit2] = X86InstructionType_int;
    classMap[UD_Ipfrspit1] = X86InstructionType_int;
    classMap[UD_Ipfrsqrt] = X86InstructionType_int;
    classMap[UD_Ipfsub] = X86InstructionType_int;
    classMap[UD_Ipfsubr] = X86InstructionType_int;
    classMap[UD_Iphaddd] = X86InstructionType_simd;
    classMap[UD_Iphminposuw] = X86InstructionType_simd;
    classMap[UD_Ipi2fd] = X86InstructionType_int;
    classMap[UD_Ipi2fw] = X86InstructionType_int;
    classMap[UD_Ipinsrb] = X86InstructionType_simd;
    classMap[UD_Ipinsrd] = X86InstructionType_simd;
    classMap[UD_Ipinsrq] = X86InstructionType_simd;
    classMap[UD_Ipinsrw] = X86InstructionType_simd;
    classMap[UD_Ipmaddwd] = X86InstructionType_simd;
    classMap[UD_Ipmaxsb] = X86InstructionType_simd;
    classMap[UD_Ipmaxsd] = X86InstructionType_simd;
    classMap[UD_Ipmaxsw] = X86InstructionType_simd;
    classMap[UD_Ipmaxub] = X86InstructionType_simd;
    classMap[UD_Ipmaxud] = X86InstructionType_simd;
    classMap[UD_Ipmaxuw] = X86InstructionType_simd;
    classMap[UD_Ipminsb] = X86InstructionType_simd;
    classMap[UD_Ipminsd] = X86InstructionType_simd;
    classMap[UD_Ipminsw] = X86InstructionType_simd;
    classMap[UD_Ipminub] = X86InstructionType_simd;
    classMap[UD_Ipminud] = X86InstructionType_simd;
    classMap[UD_Ipminuw] = X86InstructionType_simd;
    classMap[UD_Ipmovmskb] = X86InstructionType_move;
    classMap[UD_Ipmovsxbd] = X86InstructionType_move;
    classMap[UD_Ipmovsxbq] = X86InstructionType_move;
    classMap[UD_Ipmovsxbw] = X86InstructionType_move;
    classMap[UD_Ipmovsxdq] = X86InstructionType_move;
    classMap[UD_Ipmovsxwd] = X86InstructionType_move;
    classMap[UD_Ipmovsxwq] = X86InstructionType_move;
    classMap[UD_Ipmovzxbd] = X86InstructionType_move;
    classMap[UD_Ipmovzxbq] = X86InstructionType_move;
    classMap[UD_Ipmovzxbw] = X86InstructionType_move;
    classMap[UD_Ipmovzxdq] = X86InstructionType_move;
    classMap[UD_Ipmovzxwd] = X86InstructionType_move;
    classMap[UD_Ipmovzxwq] = X86InstructionType_move;
    classMap[UD_Ipmuldq] = X86InstructionType_simd;
    classMap[UD_Ipmulhrw] = X86InstructionType_int;
    classMap[UD_Ipmulhuw] = X86InstructionType_int;
    classMap[UD_Ipmulhw] = X86InstructionType_int;
    classMap[UD_Ipmullw] = X86InstructionType_int;
    classMap[UD_Ipmuludq] = X86InstructionType_int;
    classMap[UD_Ipop] = X86InstructionType_int;
    classMap[UD_Ipopa] = X86InstructionType_special;
    classMap[UD_Ipopad] = X86InstructionType_special;
    classMap[UD_Ipopfd] = X86InstructionType_int;
    classMap[UD_Ipopfq] = X86InstructionType_int;
    classMap[UD_Ipopfw] = X86InstructionType_int;
    classMap[UD_Ipor] = X86InstructionType_int;
    classMap[UD_Iprefetch] = X86InstructionType_prefetch;
    classMap[UD_Iprefetchnta] = X86InstructionType_prefetch;
    classMap[UD_Iprefetcht0] = X86InstructionType_prefetch;
    classMap[UD_Iprefetcht1] = X86InstructionType_prefetch;
    classMap[UD_Iprefetcht2] = X86InstructionType_prefetch;
    classMap[UD_Ipsadbw] = X86InstructionType_int;
    classMap[UD_Ipshufb] = X86InstructionType_simd;
    classMap[UD_Ipshufd] = X86InstructionType_int;
    classMap[UD_Ipshufhw] = X86InstructionType_int;
    classMap[UD_Ipshuflw] = X86InstructionType_int;
    classMap[UD_Ipshufw] = X86InstructionType_int;
    classMap[UD_Ipslld] = X86InstructionType_int;
    classMap[UD_Ipslldq] = X86InstructionType_int;
    classMap[UD_Ipsllq] = X86InstructionType_int;
    classMap[UD_Ipsllw] = X86InstructionType_int;
    classMap[UD_Ipsrad] = X86InstructionType_int;
    classMap[UD_Ipsraw] = X86InstructionType_int;
    classMap[UD_Ipsrld] = X86InstructionType_int;
    classMap[UD_Ipsrldq] = X86InstructionType_int;
    classMap[UD_Ipsrlq] = X86InstructionType_int;
    classMap[UD_Ipsrlw] = X86InstructionType_int;
    classMap[UD_Ipsubb] = X86InstructionType_int;
    classMap[UD_Ipsubd] = X86InstructionType_int;
    classMap[UD_Ipsubq] = X86InstructionType_int;
    classMap[UD_Ipsubsb] = X86InstructionType_int;
    classMap[UD_Ipsubsw] = X86InstructionType_int;
    classMap[UD_Ipsubusb] = X86InstructionType_int;
    classMap[UD_Ipsubusw] = X86InstructionType_int;
    classMap[UD_Ipsubw] = X86InstructionType_int;
    classMap[UD_Ipswapd] = X86InstructionType_int;
    classMap[UD_Iptest] = X86InstructionType_int;
    classMap[UD_Ipunpckhbw] = X86InstructionType_int;
    classMap[UD_Ipunpckhdq] = X86InstructionType_int;
    classMap[UD_Ipunpckhqdq] = X86InstructionType_int;
    classMap[UD_Ipunpckhwd] = X86InstructionType_int;
    classMap[UD_Ipunpcklbw] = X86InstructionType_int;
    classMap[UD_Ipunpckldq] = X86InstructionType_int;
    classMap[UD_Ipunpcklqdq] = X86InstructionType_int;
    classMap[UD_Ipunpcklwd] = X86InstructionType_int;
    classMap[UD_Ipush] = X86InstructionType_int;
    classMap[UD_Ipusha] = X86InstructionType_special;
    classMap[UD_Ipushad] = X86InstructionType_special;
    classMap[UD_Ipushfd] = X86InstructionType_int;
    classMap[UD_Ipushfq] = X86InstructionType_int;
    classMap[UD_Ipushfw] = X86InstructionType_int;
    classMap[UD_Ipxor] = X86InstructionType_int;
    classMap[UD_Ircl] = X86InstructionType_int;
    classMap[UD_Ircpps] = X86InstructionType_float;
    classMap[UD_Ircpss] = X86InstructionType_float;
    classMap[UD_Ircr] = X86InstructionType_int;
    classMap[UD_Irdmsr] = X86InstructionType_int;
    classMap[UD_Irdpmc] = X86InstructionType_hwcount;
    classMap[UD_Irdtsc] = X86InstructionType_hwcount;
    classMap[UD_Irdtscp] = X86InstructionType_hwcount;
    classMap[UD_Irep] = X86InstructionType_string;
    classMap[UD_Irepne] = X86InstructionType_string;
    classMap[UD_Iret] = X86InstructionType_return;
    classMap[UD_Iretf] = X86InstructionType_return;
    classMap[UD_Irol] = X86InstructionType_int;
    classMap[UD_Iror] = X86InstructionType_int;
    classMap[UD_Iroundpd] = X86InstructionType_simd;
    classMap[UD_Iroundps] = X86InstructionType_simd;
    classMap[UD_Iroundsd] = X86InstructionType_simd;
    classMap[UD_Iroundss] = X86InstructionType_simd;
    classMap[UD_Irsm] = X86InstructionType_special;
    classMap[UD_Irsqrtps] = X86InstructionType_float;
    classMap[UD_Irsqrtss] = X86InstructionType_float;
    classMap[UD_Isahf] = X86InstructionType_int;
    classMap[UD_Isal] = X86InstructionType_int;
    classMap[UD_Isalc] = X86InstructionType_int;
    classMap[UD_Isar] = X86InstructionType_int;
    classMap[UD_Isbb] = X86InstructionType_int;
    classMap[UD_Iscasb] = X86InstructionType_string;
    classMap[UD_Iscasd] = X86InstructionType_string;
    classMap[UD_Iscasq] = X86InstructionType_string;
    classMap[UD_Iscasw] = X86InstructionType_string;
    classMap[UD_Iseta] = X86InstructionType_int;
    classMap[UD_Isetb] = X86InstructionType_int;
    classMap[UD_Isetbe] = X86InstructionType_int;
    classMap[UD_Isetg] = X86InstructionType_int;
    classMap[UD_Isetge] = X86InstructionType_int;
    classMap[UD_Isetl] = X86InstructionType_int;
    classMap[UD_Isetle] = X86InstructionType_int;
    classMap[UD_Isetnb] = X86InstructionType_int;
    classMap[UD_Isetno] = X86InstructionType_int;
    classMap[UD_Isetnp] = X86InstructionType_int;
    classMap[UD_Isetns] = X86InstructionType_int;
    classMap[UD_Isetnz] = X86InstructionType_int;
    classMap[UD_Iseto] = X86InstructionType_int;
    classMap[UD_Isetp] = X86InstructionType_int;
    classMap[UD_Isets] = X86InstructionType_int;
    classMap[UD_Isetz] = X86InstructionType_int;
    classMap[UD_Isfence] = X86InstructionType_special;
    classMap[UD_Isgdt] = X86InstructionType_special;
    classMap[UD_Ishl] = X86InstructionType_int;
    classMap[UD_Ishld] = X86InstructionType_int;
    classMap[UD_Ishr] = X86InstructionType_int;
    classMap[UD_Ishrd] = X86InstructionType_int;
    classMap[UD_Ishufpd] = X86InstructionType_float;
    classMap[UD_Ishufps] = X86InstructionType_float;
    classMap[UD_Isidt] = X86InstructionType_special;
    classMap[UD_Iskinit] = X86InstructionType_special;
    classMap[UD_Isldt] = X86InstructionType_special;
    classMap[UD_Ismsw] = X86InstructionType_special;
    classMap[UD_Isqrtpd] = X86InstructionType_float;
    classMap[UD_Isqrtps] = X86InstructionType_float;
    classMap[UD_Isqrtsd] = X86InstructionType_float;
    classMap[UD_Isqrtss] = X86InstructionType_float;
    classMap[UD_Istc] = X86InstructionType_special;
    classMap[UD_Istd] = X86InstructionType_special;
    classMap[UD_Istgi] = X86InstructionType_special;
    classMap[UD_Isti] = X86InstructionType_special;
    classMap[UD_Istmxcsr] = X86InstructionType_special;
    classMap[UD_Istosb] = X86InstructionType_string;
    classMap[UD_Istosd] = X86InstructionType_string;
    classMap[UD_Istosq] = X86InstructionType_string;
    classMap[UD_Istosw] = X86InstructionType_string;
    classMap[UD_Istr] = X86InstructionType_special;
    classMap[UD_Isub] = X86InstructionType_int;
    classMap[UD_Isubpd] = X86InstructionType_float;
    classMap[UD_Isubps] = X86InstructionType_float;
    classMap[UD_Isubsd] = X86InstructionType_float;
    classMap[UD_Isubss] = X86InstructionType_float;
    classMap[UD_Iswapgs] = X86InstructionType_special;
    classMap[UD_Isyscall] = X86InstructionType_system_call;
    classMap[UD_Isysenter] = X86InstructionType_system_call;
    classMap[UD_Isysexit] = X86InstructionType_system_call;
    classMap[UD_Isysret] = X86InstructionType_system_call;
    classMap[UD_Itest] = X86InstructionType_int;
    classMap[UD_Iucomisd] = X86InstructionType_float;
    classMap[UD_Iucomiss] = X86InstructionType_float;
    classMap[UD_Iud2] = X86InstructionType_invalid;
    classMap[UD_Iunpckhpd] = X86InstructionType_float;
    classMap[UD_Iunpckhps] = X86InstructionType_float;
    classMap[UD_Iunpcklpd] = X86InstructionType_float;
    classMap[UD_Iunpcklps] = X86InstructionType_float;
    classMap[UD_Ivaddpd] = X86InstructionType_avx;
    classMap[UD_Ivaddps] = X86InstructionType_avx;
    classMap[UD_Ivaddsd] = X86InstructionType_avx;
    classMap[UD_Ivaddss] = X86InstructionType_avx;
    classMap[UD_Ivblendpd] = X86InstructionType_avx;
    classMap[UD_Iverr] = X86InstructionType_special;
    classMap[UD_Iverw] = X86InstructionType_special;
    classMap[UD_Ivmcall] = X86InstructionType_vmx;
    classMap[UD_Ivmclear] = X86InstructionType_vmx;
    classMap[UD_Ivmload] = X86InstructionType_vmx;
    classMap[UD_Ivmmcall] = X86InstructionType_vmx;
    classMap[UD_Ivmovapd] = X86InstructionType_move;
    classMap[UD_Ivmptrld] = X86InstructionType_vmx;
    classMap[UD_Ivmptrst] = X86InstructionType_vmx;
    classMap[UD_Ivmresume] = X86InstructionType_vmx;
    classMap[UD_Ivmrun] = X86InstructionType_vmx;
    classMap[UD_Ivmsave] = X86InstructionType_vmx;
    classMap[UD_Ivmulpd] = X86InstructionType_avx;
    classMap[UD_Ivmxoff] = X86InstructionType_vmx;
    classMap[UD_Ivmxon] = X86InstructionType_vmx;
    classMap[UD_Ivpshufd] = X86InstructionType_avx;
    classMap[UD_Iwait] = X86InstructionType_special;
    classMap[UD_Iwbinvd] = X86InstructionType_special;
    classMap[UD_Iwrmsr] = X86InstructionType_special;
    classMap[UD_Ixadd] = X86InstructionType_int;
    classMap[UD_Ixchg] = X86InstructionType_int;
    classMap[UD_Ixlatb] = X86InstructionType_special;
    classMap[UD_Ixor] = X86InstructionType_int;
    classMap[UD_Ixorpd] = X86InstructionType_float;
    classMap[UD_Ixorps] = X86InstructionType_float;

    verify();
}

