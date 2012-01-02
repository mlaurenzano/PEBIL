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

    PRINT_INFOR("%#llx:\t%16s\t%s\tflgs:[%10s]\t-> %#llx", getBaseAddress(), GET(insn_hexcode), GET(insn_buffer), flags, getTargetAddress());

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

#include <map>
using namespace std;
static map<uint32_t, uint32_t> classMap;

void X86InstructionClassifier::initialize(){
    if (classMap.size() == 0){
        fillClassDefinitions();
    }
    ASSERT(classMap.size());
}

uint32_t X86InstructionClassifier::getClass(uint32_t mnemonic){
    ASSERT(mnemonic >= 0 && mnemonic < UD_Itotaltypes && "invalid mnemonic");
    initialize();
    ASSERT(classMap.count(mnemonic) == 1);
    return classMap[mnemonic];
}

X86InstructionBin X86InstructionClassifier::getInstructionBin(X86Instruction* x){ 
    return (X86InstructionBin)rawClassBits(x->GET(mnemonic), 8, 8);  
}

uint8_t X86InstructionClassifier::getInstructionMemLocation(X86Instruction* x){
    uint8_t loc = (uint8_t)rawClassBits(x->GET(mnemonic), 4, 28);
    if (x->isLoad()){
        loc |= (BinLoad >> 8);
    }
    if (x->isStore()){
        loc |= (BinStore >> 8);
    }    
    return loc;
}

uint8_t X86InstructionClassifier::getInstructionMemSize(X86Instruction* x){
    uint8_t mem = (uint8_t)rawClassBits(x->GET(mnemonic), 8, 20);
    if (mem & MEM_SZ_VARIABLE){
        mem = x->getDstSizeInBytes();
    }
    return mem;
}

X86InstructionType X86InstructionClassifier::getInstructionType(X86Instruction* x){
    return (X86InstructionType)rawClassBits(x->GET(mnemonic), 8, 0);
}

X86OperandFormat X86InstructionClassifier::getInstructionFormat(X86Instruction* x){ 
    return (X86OperandFormat)rawClassBits(x->GET(mnemonic), 4, 16);
}

uint32_t X86InstructionClassifier::packFields(uint8_t type, uint8_t bin, uint8_t format, uint8_t memsize, uint8_t location){
    return (
            ( (type     & 0xff ) << 0 )  |
            ( (bin      & 0xff ) << 8  ) |
            ( (format   & 0xf  ) << 16 ) |
            ( (memsize  & 0xff ) << 20 ) |
            ( (location & 0xf  ) << 28 ));
}

void X86InstructionClassifier::print(X86Instruction* x){
    PRINT_INFOR("Instruciton %s: %#08x %hhd %hhd %hhd %hhd %hhd", ud_mnemonics_str[x->GET(mnemonic)], getClass(x->GET(mnemonic)), getInstructionBin(x), getInstructionMemLocation(x), getInstructionMemSize(x), getInstructionType(x), getInstructionFormat(x));
}

bool X86InstructionClassifier::verify(){
    bool err = false;
    for (uint32_t i = 0; i < UD_Itotaltypes; i++){
        if (classMap.count(i) != 1){
            PRINT_WARN(20, "Instruction classification definition missing for %s", ud_mnemonics_str[i]);
            err = true;
        }
    }
    if (err){
        PRINT_ERROR("Missing instruction classiciation definition(s)");
        return false;
    }
    for (map<uint32_t, uint32_t>::iterator ii = classMap.begin(); ii != classMap.end(); ii++){
        uint32_t mnemonic = (*ii).first;
        uint32_t clss = (*ii).second;
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

void X86InstructionClassifier::fillClassDefinitions(){

    /* macros to make the table assignment statements easier to write + more concise */
#define xbin(__bin) X86InstructionBin_ ## __bin
#define xtyp(__typ) X86InstructionType_ ## __typ
#define xfmt(__fmt) X86OperandFormat_ ## __fmt
#define xsiz(__bits) (__bits >> 3)
#define X86InstructionBin_0 0
#define X86OperandFormat_0 0
#define VRSZ (MEM_SZ_VARIABLE << 3)
#define mkassign(__mne, __typ, __bin, __fmt, __mem, __loc) \
    classMap[UD_I ## __mne] = packFields(xtyp(__typ), xbin(__bin), xfmt(__fmt), xsiz(__mem), __loc >> 8)

    //               mnemonic,     type      bin  fmt msize  mloc
    mkassign(           3dnow,  special,   other,   0,    0,    0);
    mkassign(             aaa,      int,   other,   0,    0,    0);
    mkassign(             aad,      int,   other,   0,    0,    0);
    mkassign(             aam,      int,   other,   0,    0,    0);
    mkassign(             aas,      int,   other,   0,    0,    0);
    mkassign(             adc,      int,     int,   0, VRSZ,    0);
    mkassign(             add,      int,     int,   0, VRSZ,    0);
    mkassign(           addpd,    float,  floatv,   0,   64,    0);
    mkassign(           addps,    float,  floatv,   0,   32,    0);
    mkassign(           addsd,    float,  floats,   0,   64,    0);
    mkassign(           addss,    float,  floats,   0,   32,    0);
    mkassign(        addsubpd,    float,  floatv,   0,   64,    0);
    mkassign(        addsubps,    float,  floatv,   0,   32,    0);
    mkassign(      aesdeclast,      aes,       0,   0,    0,    0);
    mkassign(          aesdec,      aes,       0,   0,    0,    0);
    mkassign(      aesenclast,      aes,       0,   0,    0,    0);
    mkassign(          aesenc,      aes,       0,   0,    0,    0);
    mkassign(          aesimc,      aes,       0,   0,    0,    0);
    mkassign( aeskeygenassist,      aes,       0,   0,    0,    0);
    mkassign(          andnpd,    float,    binv,   0,   64,    0);
    mkassign(          andnps,    float,    binv,   0,   32,    0);
    mkassign(             and,      int,     bin,   0, VRSZ,    0);
    mkassign(           andpd,    float,    binv,   0,   64,    0);
    mkassign(           andps,    float,    binv,   0,   32,    0);
    mkassign(            arpl,  special,   other,   0,    0,    0);
    mkassign(         blendpd,      int,       0,   0,    0,    0);
    mkassign(         blendps,      int,       0,   0,    0,    0);
    mkassign(        blendvpd,      int,       0,   0,    0,    0);
    mkassign(        blendvps,      int,       0,   0,    0,    0);
    mkassign(           bound,      int,     bin,   0, VRSZ,    0);
    mkassign(             bsf,      int,     bin,   0, VRSZ,    0);
    mkassign(             bsr,      int,     bin,   0, VRSZ,    0);
    mkassign(           bswap,      int,     bin,   0, VRSZ,    0);
    mkassign(             btc,      int,     bin,   0, VRSZ,    0);
    mkassign(              bt,      int,     bin,   0, VRSZ,    0);
    mkassign(             btr,      int,     bin,   0, VRSZ,    0);
    mkassign(             bts,      int,     bin,   0, VRSZ,    0);
    mkassign(            call,     call,  uncond,   0,    0,    BinFrame);
    mkassign(             cbw,  special,     bin,   0,   16,    0);
    mkassign(            cdqe,  special,     bin,   0,   64,    0);
    mkassign(             cdq,  special,     bin,   0,   64,    0);
    mkassign(             clc,  special,   other,   0,    0,    0);
    mkassign(             cld,  special,   other,   0,    0,    0);
    mkassign(         clflush,  special,   cache,   0,    0,    0);
    mkassign(            clgi,  special,   other,   0,    0,    0);
    mkassign(             cli,  special,   other,   0,    0,    0);
    mkassign(            clts,  special,   other,   0,    0,    0);
    mkassign(             cmc,  special,   other,   0,    0,    0);
    mkassign(          cmovae,     move,    move,   0, VRSZ,    0);
    mkassign(           cmova,     move,    move,   0, VRSZ,    0);
    mkassign(          cmovbe,     move,    move,   0, VRSZ,    0);
    mkassign(           cmovb,     move,    move,   0, VRSZ,    0);
    mkassign(          cmovge,     move,    move,   0, VRSZ,    0);
    mkassign(           cmovg,     move,    move,   0, VRSZ,    0);
    mkassign(          cmovle,     move,    move,   0, VRSZ,    0);
    mkassign(           cmovl,     move,    move,   0, VRSZ,    0);
    mkassign(          cmovno,     move,    move,   0, VRSZ,    0);
    mkassign(          cmovnp,     move,    move,   0, VRSZ,    0);
    mkassign(          cmovns,     move,    move,   0, VRSZ,    0);
    mkassign(          cmovnz,     move,    move,   0, VRSZ,    0);
    mkassign(           cmovo,     move,    move,   0, VRSZ,    0);
    mkassign(           cmovp,     move,    move,   0, VRSZ,    0);
    mkassign(           cmovs,     move,    move,   0, VRSZ,    0);
    mkassign(           cmovz,     move,    move,   0, VRSZ,    0);
    mkassign(             cmp,      int,     int,   0, VRSZ,    0);
    mkassign(           cmppd,    float,  floatv,   0,   64,    0);
    mkassign(           cmpps,    float,  floatv,   0,   32,    0);
    mkassign(           cmpsb,   string,  string,  si,    8,    0);
    // TODO: 2 instructions covered by this... need to handle special
    mkassign(           cmpsd,    float,  string,  si,   32,    0);
    mkassign(           cmpsq,   string,  string,  si,   64,    0);
    mkassign(           cmpss,    float,  floats,   0,   32,    0);
    mkassign(           cmpsw,   string,  string,  si,   16,    0);
    mkassign(       cmpxchg8b,      int,     int,   0, VRSZ,    0);
    mkassign(         cmpxchg,      int,     int,   0, VRSZ,    0);
    mkassign(          comisd,    float,  floats,   0,   64,    0);
    mkassign(          comiss,    float,  floats,   0,   32,    0);
    mkassign(           cpuid,  special,   other,   0,    0,    0);
    mkassign(             cqo,  special,     bin,   0,   64,    0);
    mkassign(        cvtdq2pd,    float,  floatv,   0,   64,    0);
    mkassign(        cvtdq2ps,    float,  floatv,   0,   32,    0);
    mkassign(        cvtpd2dq,    float,  floatv,   0,   64,    0);
    mkassign(        cvtpd2pi,    float,  floatv,   0,   64,    0);
    mkassign(        cvtpd2ps,    float,  floatv,   0,   32,    0);
    mkassign(        cvtpi2pd,    float,  floatv,   0,   64,    0);
    mkassign(        cvtpi2ps,    float,  floatv,   0,   32,    0);
    mkassign(        cvtps2dq,    float,  floatv,   0,   32,    0);
    mkassign(        cvtps2pd,    float,  floatv,   0,   64,    0);
    mkassign(        cvtps2pi,    float,  floatv,   0,   32,    0);
    mkassign(        cvtsd2si,    float,  floats,   0,   64,    0);
    mkassign(        cvtsd2ss,    float,  floats,   0,   64,    0);
    mkassign(        cvtsi2sd,    float,  floats,   0,   64,    0);
    mkassign(        cvtsi2ss,    float,  floats,   0,   32,    0);
    mkassign(        cvtss2sd,    float,  floats,   0,   64,    0);
    mkassign(        cvtss2si,    float,  floats,   0,   32,    0);
    mkassign(       cvttpd2dq,    float,  floatv,   0,   64,    0);
    mkassign(       cvttpd2pi,    float,  floatv,   0,   64,    0);
    mkassign(       cvttps2dq,    float,  floatv,   0,   32,    0);
    mkassign(       cvttps2pi,    float,  floatv,   0,   32,    0);
    mkassign(       cvttsd2si,    float,  floats,   0,   32,    0);
    mkassign(       cvttss2si,    float,  floats,   0,   32,    0);
    mkassign(            cwde,  special,     bin,   0,   32,    0);
    mkassign(             cwd,  special,     bin,   0,   32,    0);
    mkassign(             daa,      int,   other,   0,    0,    0);
    mkassign(             das,      int,   other,   0,    0,    0);
    mkassign(              db,  invalid, invalid,   0,    0,    0);
    mkassign(             dec,      int,     int,   0, VRSZ,    0);
    mkassign(             div,      int,     int,   0, VRSZ,    0);
    mkassign(           divpd,    float,  floatv,   0,   64,    0);
    mkassign(           divps,    float,  floatv,   0,   32,    0);
    mkassign(           divsd,    float,  floats,   0,   64,    0);
    mkassign(           divss,    float,  floats,   0,   32,    0);
    mkassign(            dppd,     simd,       0,   0,    0,    0);
    mkassign(            dpps,     simd,       0,   0,    0,    0);
    mkassign(            emms,  special,   other,   0,    0,    0);
    mkassign(           enter,  special,   stack,   0,    0,    BinFrame);
    mkassign(       extractps,     simd,       0,   0,    0,    0);
    mkassign(           f2xm1,    float,   float,   0, VRSZ,    0);
    mkassign(            fabs,    float,   float,   0, VRSZ,    0);
    mkassign(            fadd,    float,   float,   0, VRSZ,    0);
    mkassign(           faddp,    float,   float,   0, VRSZ,    0);
    mkassign(            fbld,    float,   float,   0, VRSZ,    0);
    mkassign(           fbstp,    float,   float,   0, VRSZ,    0);
    mkassign(            fchs,    float,   float,   0, VRSZ,    0);
    mkassign(           fclex,    float,   float,   0, VRSZ,    0);
    mkassign(         fcmovbe,     move,    move,   0, VRSZ,    0);
    mkassign(          fcmovb,     move,    move,   0, VRSZ,    0);
    mkassign(          fcmove,     move,    move,   0, VRSZ,    0);
    mkassign(        fcmovnbe,     move,    move,   0, VRSZ,    0);
    mkassign(         fcmovnb,     move,    move,   0, VRSZ,    0);
    mkassign(         fcmovne,     move,    move,   0, VRSZ,    0);
    mkassign(         fcmovnu,     move,    move,   0, VRSZ,    0);
    mkassign(          fcmovu,     move,    move,   0, VRSZ,    0);
    mkassign(           fcom2,    float,   float,   0, VRSZ,    0);
    mkassign(           fcomi,    float,   float,   0, VRSZ,    0);
    mkassign(          fcomip,    float,   float,   0, VRSZ,    0);
    mkassign(          fcomp3,    float,   float,   0, VRSZ,    0);
    mkassign(          fcomp5,    float,   float,   0, VRSZ,    0);
    mkassign(            fcom,    float,   float,   0, VRSZ,    0);
    mkassign(           fcomp,    float,   float,   0, VRSZ,    0);
    mkassign(          fcompp,    float,   float,   0, VRSZ,    0);
    mkassign(            fcos,    float,   float,   0, VRSZ,    0);
    mkassign(         fdecstp,    float,   float,   0, VRSZ,    0);
    mkassign(            fdiv,    float,   float,   0, VRSZ,    0);
    mkassign(           fdivp,    float,   float,   0, VRSZ,    0);
    mkassign(           fdivr,    float,   float,   0, VRSZ,    0);
    mkassign(          fdivrp,    float,   float,   0, VRSZ,    0);
    mkassign(           femms,    float,   other,   0,    0,    0);
    mkassign(           ffree,    float,   other,   0,    0,    0);
    mkassign(          ffreep,    float,   other,   0,    0,    0);
    mkassign(           fiadd,    float,   float,   0, VRSZ,    0);
    mkassign(           ficom,    float,   float,   0, VRSZ,    0);
    mkassign(          ficomp,    float,   float,   0, VRSZ,    0);
    mkassign(           fidiv,    float,   float,   0, VRSZ,    0);
    mkassign(          fidivr,    float,   float,   0, VRSZ,    0);
    mkassign(            fild,     move,    move,  di, VRSZ,    0);
    mkassign(           fimul,    float,   float,   0, VRSZ,    0);
    mkassign(            fist,     move,    move,   0, VRSZ,    0);
    mkassign(           fistp,     move,    move,   0, VRSZ,    0);
    mkassign(          fisttp,     move,    move,   0, VRSZ,    0);
    mkassign(           fisub,    float,   float,   0, VRSZ,    0);
    mkassign(          fisubr,    float,   float,   0, VRSZ,    0);
    mkassign(            fld1,    float,    move,   0, VRSZ,    0);
    mkassign(           fldcw,  special,   other,   0,    0,    0);
    mkassign(          fldenv,  special,   other,   0,    0,    0);
    mkassign(          fldl2e,     move,    move,  di, VRSZ,    0);
    mkassign(          fldl2t,     move,    move,  di, VRSZ,    0);
    mkassign(          fldlg2,     move,    move,  di, VRSZ,    0);
    mkassign(          fldln2,     move,    move,  di, VRSZ,    0);
    mkassign(          fldlpi,     move,    move,  di, VRSZ,    0);
    mkassign(             fld,     move,    move,  di, VRSZ,    0);
    mkassign(            fldz,    float,    move,  di, VRSZ,    0);
    mkassign(            fmul,    float,   float,   0, VRSZ,    0);
    mkassign(           fmulp,    float,   float,   0, VRSZ,    0);
    mkassign(          fncstp,    float,   other,   0,    0,    0);
    mkassign(          fninit,    float,   other,   0,    0,    0);
    mkassign(            fnop,      nop,   other,   0,    0,    0);
    mkassign(          fnsave,  special,   stack,   0,    0,    BinFrame);
    mkassign(          fnstcw,  special,   stack,   0,    0,    BinFrame);
    mkassign(         fnstenv,  special,   stack,   0,    0,    BinFrame);
    mkassign(          fnstsw,  special,   stack,   0,    0,    BinFrame);
    mkassign(          fpatan,    float,   float,   0, VRSZ,    0);
    mkassign(          fprem1,    float,   float,   0, VRSZ,    0);
    mkassign(           fprem,    float,   float,   0, VRSZ,    0);
    mkassign(           fptan,    float,   float,   0, VRSZ,    0);
    mkassign(        fpxtract,    float,   float,   0, VRSZ,    0);
    mkassign(         frndint,    float,   float,   0, VRSZ,    0);
    mkassign(          frstor,  special,   stack,   0,    0,    BinFrame);
    mkassign(          fscale,    float,   float,   0, VRSZ,    0);
    mkassign(         fsincos,    float,   float,   0, VRSZ,    0);
    mkassign(            fsin,    float,   float,   0, VRSZ,    0);
    mkassign(           fsqrt,    float,   float,   0, VRSZ,    0);
    mkassign(           fstp1,     move,    move,   0, VRSZ,    0);
    mkassign(           fstp8,     move,    move,   0, VRSZ,    0);
    mkassign(           fstp9,     move,    move,   0, VRSZ,    0);
    mkassign(             fst,     move,    move,   0, VRSZ,    0);
    mkassign(            fstp,     move,    move,   0, VRSZ,    0);
    mkassign(            fsub,    float,   float,   0, VRSZ,    0);
    mkassign(           fsubp,    float,   float,   0, VRSZ,    0);
    mkassign(           fsubr,    float,   float,   0, VRSZ,    0);
    mkassign(          fsubrp,    float,   float,   0, VRSZ,    0);
    mkassign(            ftst,    float,   float,   0, VRSZ,    0);
    mkassign(          fucomi,    float,   float,   0, VRSZ,    0);
    mkassign(         fucomip,    float,   float,   0, VRSZ,    0);
    mkassign(           fucom,    float,   float,   0, VRSZ,    0);
    mkassign(          fucomp,    float,   float,   0, VRSZ,    0);
    mkassign(         fucompp,    float,   float,   0, VRSZ,    0);
    mkassign(            fxam,    float,   float,   0, VRSZ,    0);
    mkassign(           fxch4,     move,    move,  di, VRSZ,    0);
    mkassign(           fxch7,     move,    move,  di, VRSZ,    0);
    mkassign(            fxch,     move,    move,  di, VRSZ,    0);
    mkassign(         fxrstor,  special,   stack,   0,    0,    BinFrame);
    mkassign(          fxsave,  special,   stack,   0,    0,    BinFrame);
    mkassign(         fyl2xp1,    float,   float,   0, VRSZ,    0);
    mkassign(           fyl2x,    float,   float,   0, VRSZ,    0);
    mkassign(          haddpd,    float,  floatv,   0,   64,    0);
    mkassign(          haddps,    float,  floatv,   0,   32,    0);
    mkassign(             hlt,     halt,   other,   0,    0,    0);
    mkassign(          hsubpd,    float,  floatv,   0,   64,    0);
    mkassign(          hsubps,    float,  floatv,   0,   32,    0);
    mkassign(            idiv,      int,     int,   0, VRSZ,    0);
    mkassign(            imul,      int,     int,   0, VRSZ,    0);
    mkassign(             inc,      int,     int,   0, VRSZ,    0);
    mkassign(              in,       io,   other,   0,    0,    0);
    mkassign(            insb,       io,   other,  di,    0,    0);
    mkassign(            insd,       io,   other,  di,    0,    0);
    mkassign(        insertps,     simd,       0,   0,    0,    0);
    mkassign(            insw,       io,   other,  di,    0,    0);
    mkassign(            int1,     trap,  system,   0,    0,    BinFrame);
    mkassign(            int3,     trap,  system,   0,    0,    BinFrame);
    mkassign(            into,     trap,  system,   0,    0,    BinFrame);
    mkassign(             int,     trap,  system,   0,    0,    BinFrame);
    mkassign(         invalid,  invalid, invalid,   0,    0,    0);
    mkassign(            invd,  special,   cache,   0,    0,    0);
    mkassign(         invlpga,  special,   cache,   0,    0,    0);
    mkassign(          invlpg,  special,   cache,   0,    0,    0);
    mkassign(           iretd,   return,  system,   0,    0,    BinFrame);
    mkassign(           iretq,   return,  system,   0,    0,    BinFrame);
    mkassign(           iretw,   return,  system,   0,    0,    BinFrame);
    mkassign(             jae,   condbr,    cond,   0,    0,    0);
    mkassign(              ja,   condbr,    cond,   0,    0,    0);
    mkassign(             jbe,   condbr,    cond,   0,    0,    0);
    mkassign(              jb,   condbr,    cond,   0,    0,    0);
    mkassign(            jcxz,   condbr,    cond,   0,    0,    0);
    mkassign(           jecxz,   condbr,    cond,   0,    0,    0);
    mkassign(              je,   condbr,    cond,   0,    0,    0);
    mkassign(             jge,   condbr,    cond,   0,    0,    0);
    mkassign(              jg,   condbr,    cond,   0,    0,    0);
    mkassign(             jle,   condbr,    cond,   0,    0,    0);
    mkassign(              jl,   condbr,    cond,   0,    0,    0);
    mkassign(             jmp, uncondbr,  uncond,   0,    0,    0);
    mkassign(             jne,   condbr,    cond,   0,    0,    0);
    mkassign(             jno,   condbr,    cond,   0,    0,    0);
    mkassign(             jnp,   condbr,    cond,   0,    0,    0);
    mkassign(             jns,   condbr,    cond,   0,    0,    0);
    mkassign(              jo,   condbr,    cond,   0,    0,    0);
    mkassign(              jp,   condbr,    cond,   0,    0,    0);
    mkassign(           jrcxz,   condbr,    cond,   0,    0,    0);
    mkassign(              js,   condbr,    cond,   0,    0,    0);
    mkassign(            lahf,      int,   other,   0,    0,    0);
    mkassign(             lar,  special,   other,   0,    0,    0);
    mkassign(           lddqu,     move,    move,   0, VRSZ,    0);
    mkassign(         ldmxcsr,     move,    move,  di,   32,    0);
    mkassign(             lds,     move,    move,   0, VRSZ,    0);
    mkassign(             lea,     move,    move,   0, VRSZ,    0);
    mkassign(           leave,  special,   stack,   0,    0,    BinFrame);
    mkassign(             les,     move,    move,   0, VRSZ,    0);
    mkassign(          lfence,  special,   other,   0,    0,    0);
    mkassign(             lfs,     move,    move,   0, VRSZ,    0);
    mkassign(            lgdt,  special,   other,   0,    0,    0);
    mkassign(             lgs,     move,    move,   0, VRSZ,    0);
    mkassign(            lidt,  special,   other,   0,    0,    0);
    mkassign(            lldt,  special,   other,   0,    0,    0);
    mkassign(            lmsw,  special,   other,   0,    0,    0);
    mkassign(            lock,  special,   other,   0,    0,    0);
    mkassign(           lodsb,     move,    move,  di,    8,    0);
    mkassign(           lodsd,     move,    move,  di,   32,    0);
    mkassign(           lodsq,     move,    move,  di,   64,    0);
    mkassign(           lodsw,     move,    move,  di,   16,    0);
    mkassign(           loope,  special,   other,   0,    0,    0);
    mkassign(          loopnz,  special,   other,   0,    0,    0);
    mkassign(            loop,  special,   other,   0,    0,    0);
    mkassign(             lsl,  special,   other,   0,    0,    0);
    mkassign(             lss,      int,    move,   0, VRSZ,    0);
    mkassign(             ltr,  special,   other,   0,    0,    0);
    mkassign(      maskmovdqu,     move,       0,   0,    0,    0);
    mkassign(        maskmovq,     move,    move,   0,   64,    0);
    mkassign(           maxpd,    float,  floatv,   0,   64,    0);
    mkassign(           maxps,    float,  floatv,   0,   32,    0);
    mkassign(           maxsd,    float,  floats,   0,   64,    0);
    mkassign(           maxss,    float,  floats,   0,   32,    0);
    mkassign(          mfence,  special,   other,   0,    0,    0);
    mkassign(           minpd,    float,  floatv,   0,   64,    0);
    mkassign(           minps,    float,  floatv,   0,   32,    0);
    mkassign(           minsd,    float,  floats,   0,   64,    0);
    mkassign(           minss,    float,  floats,   0,   32,    0);
    mkassign(         monitor,  special,   other,   0,    0,    0);
    mkassign(          movapd,     move,    move,   0, VRSZ,    0);
    mkassign(          movaps,     move,    move,   0, VRSZ,    0);
    mkassign(         movddup,     move,    move,   0, VRSZ,    0);
    mkassign(            movd,     move,    move,   0,   64,    0);
    mkassign(         movdq2q,     move,    move,   0,   64,    0);
    mkassign(          movdqa,     move,    move,   0, VRSZ,    0);
    mkassign(          movdqu,     move,    move,   0, VRSZ,    0);
    mkassign(         movhlps,     move,    move,   0,   32,    0);
    mkassign(          movhpd,     move,    move,   0,   64,    0);
    mkassign(          movhps,     move,    move,   0,   32,    0);
    mkassign(         movlhps,     move,    move,   0,   32,    0);
    mkassign(          movlpd,     move,    move,   0,   64,    0);
    mkassign(          movlps,     move,    move,   0,   32,    0);
    mkassign(        movmskpd,     move,    move,   0, VRSZ,    0);
    mkassign(        movmskps,     move,    move,   0, VRSZ,    0);
    mkassign(        movntdqa,     move,       0,   0,    0,    0);
    mkassign(         movntdq,     move,    move,   0, VRSZ,    0);
    mkassign(          movnti,     move,    move,   0, VRSZ,    0);
    mkassign(         movntpd,     move,    move,   0, VRSZ,    0);
    mkassign(         movntps,     move,    move,   0, VRSZ,    0);
    mkassign(          movntq,     move,    move,   0,   64,    0);
    mkassign(             mov,     move,    move,   0, VRSZ,    0);
    mkassign(         movq2dq,     move,    move,   0,   64,    0);
    mkassign(            movq,     move,    move,   0,   64,    0);
    mkassign(           movsb,   string,  string, dsi,    8,    0);
    // TODO: 2 instructions covered by this... need to handle special
    mkassign(           movsd,   string,    move, dsi,   64,    0);
    mkassign(        movshdup,     move,    move,   0, VRSZ,    0);
    mkassign(        movsldup,     move,    move,   0, VRSZ,    0);
    mkassign(           movsq,   string,  string, dsi,   64,    0);
    mkassign(           movss,     move,    move,   0,   32,    0);
    mkassign(           movsw,   string,  string, dsi,   16,    0);
    mkassign(          movsxd,     move,    move,   0, VRSZ,    0);
    mkassign(           movsx,     move,    move,   0, VRSZ,    0);
    mkassign(          movupd,     move,    move,   0, VRSZ,    0);
    mkassign(          movups,     move,    move,   0, VRSZ,    0);
    mkassign(           movzx,     move,    move,   0, VRSZ,    0);
    mkassign(         mpsadbw,     simd,       0,   0,    0,    0);
    mkassign(             mul,      int,     int,   0, VRSZ,    0);
    mkassign(           mulpd,    float,  floatv,   0,   64,    0);
    mkassign(           mulps,    float,  floatv,   0,   32,    0);
    mkassign(           mulsd,    float,  floats,   0,   64,    0);
    mkassign(           mulss,    float,  floats,   0,   32,    0);
    mkassign(           mwait,  special,   other,   0,    0,    0);
    mkassign(             neg,      int,     int,   0, VRSZ,    0);
    mkassign(             nop,      nop,   other,   0,    0,    0);
    mkassign(             not,      int,     bin,   0, VRSZ,    0);
    mkassign(              or,      int,     bin,   0, VRSZ,    0);
    mkassign(            orpd,    float,    binv,   0,   64,    0);
    mkassign(            orps,    float,    binv,   0,   32,    0);
    mkassign(             out,       io,   other,   0,    0,    0);
    mkassign(           outsb,       io,   other,   0,    0,    0);
    mkassign(           outsd,       io,   other,   0,    0,    0);
    mkassign(           outsq,       io,   other,   0,    0,    0);
    mkassign(           outsw,       io,   other,   0,    0,    0);
    mkassign(           pabsb,     simd,       0,   0,    0,    0);
    mkassign(           pabsd,     simd,       0,   0,    0,    0);
    mkassign(           pabsw,     simd,       0,   0,    0,    0);
    mkassign(        packssdw,      int,    intv,   0,   32,    0);
    mkassign(        packsswb,      int,    intv,   0,    8,    0);
    mkassign(        packusdw,     simd,       0,   0,    0,    0);
    mkassign(        packuswb,      int,    intv,   0,    8,    0);
    mkassign(           paddb,      int,    intv,   0,    8,    0);
    mkassign(           paddd,      int,    intv,   0,   32,    0);
    mkassign(           paddq,      int,    intv,   0,   64,    0);
    mkassign(          paddsb,      int,    intv,   0,    8,    0);
    mkassign(          paddsw,      int,    intv,   0,   16,    0);
    mkassign(         paddusb,      int,    intv,   0,    8,    0);
    mkassign(         paddusw,      int,    intv,   0,   16,    0);
    mkassign(           paddw,      int,    intv,   0,   16,    0);
    mkassign(         palignr,     simd,    binv,   0, VRSZ,    0);
    mkassign(           pandn,      int,    binv,   0, VRSZ,    0);
    mkassign(            pand,      int,    binv,   0, VRSZ,    0);
    mkassign(           pause,  special,   other,   0,    0,    0);
    mkassign(           pavgb,      int,    intv,   0,    8,    0);
    mkassign(         pavgusb,      int,    intv,   0,   16,    0);
    mkassign(           pavgw,      int,    intv,   0,   16,    0);
    mkassign(        pblendvb,      int,       0,   0,    0,    0);
    mkassign(         pblendw,      int,       0,   0,    0,    0);
    mkassign(       pclmulqdq,     simd,       0,   0,    0,    0);
    mkassign(         pcmpeqb,      int,    intv,   0,    8,    0);
    mkassign(         pcmpeqd,      int,    intv,   0,   32,    0);
    mkassign(         pcmpeqq,     simd,       0,   0,    0,    0);
    mkassign(         pcmpeqw,      int,    intv,   0,   16,    0);
    mkassign(       pcmpestri,     simd,       0,   0,    0,    0);
    mkassign(       pcmpestrm,     simd,       0,   0,    0,    0);
    mkassign(         pcmpgtb,      int,    intv,   0,    8,    0);
    mkassign(         pcmpgtd,      int,    intv,   0,   32,    0);
    mkassign(         pcmpgtq,      int,       0,   0,    0,    0);
    mkassign(         pcmpgtw,      int,    intv,   0,   16,    0);
    mkassign(       pcmpistri,     simd,       0,   0,    0,    0);
    mkassign(       pcmpistrm,     simd,       0,   0,    0,    0);
    mkassign(          pextrb,     simd,       0,   0,    0,    0);
    mkassign(          pextrd,     simd,       0,   0,    0,    0);
    mkassign(          pextrq,     simd,       0,   0,    0,    0);
    mkassign(          pextrw,     simd,    intv,   0,   16,    0);
    mkassign(           pf2id,      int,    intv,   0,   32,    0);
    mkassign(           pf2iw,      int,    intv,   0,   16,    0);
    mkassign(           pfacc,      int,    intv,   0,   32,    0);
    mkassign(           pfadd,      int,    intv,   0,   32,    0);
    mkassign(         pfcmpeq,      int,    intv,   0,   32,    0);
    mkassign(         pfcmpge,      int,    intv,   0,   32,    0);
    mkassign(         pfcmpgt,      int,    intv,   0,   32,    0);
    mkassign(           pfmax,      int,    intv,   0,   32,    0);
    mkassign(           pfmin,      int,    intv,   0,   32,    0);
    mkassign(           pfmul,      int,    intv,   0,   32,    0);
    mkassign(          pfnacc,      int,    intv,   0,   32,    0);
    mkassign(         pfpnacc,      int,    intv,   0,   32,    0);
    mkassign(        pfrcpit1,      int,    intv,   0,   32,    0);
    mkassign(        pfrcpit2,      int,    intv,   0,   32,    0);
    mkassign(           pfrcp,      int,    intv,   0,   32,    0);
    mkassign(        pfrspit1,      int,    intv,   0,   32,    0);
    mkassign(         pfrsqrt,      int,    intv,   0,   32,    0);
    mkassign(           pfsub,      int,    intv,   0,   32,    0);
    mkassign(          pfsubr,      int,    intv,   0,   32,    0);
    mkassign(          phaddd,     simd,    intv,   0,   32,    0);
    mkassign(         phaddsw,     simd,       0,   0,    0,    0);
    mkassign(          phaddw,     simd,       0,   0,    0,    0);
    mkassign(      phminposuw,     simd,       0,   0,    0,    0);
    mkassign(          phsubd,     simd,       0,   0,    0,    0);
    mkassign(         phsubsw,     simd,       0,   0,    0,    0);
    mkassign(          phsubw,     simd,       0,   0,    0,    0);
    mkassign(           pi2fd,      int,    intv,   0,   32,    0);
    mkassign(           pi2fw,      int,    intv,   0,   16,    0);
    mkassign(          pinsrb,     simd,       0,   0,    0,    0);
    mkassign(          pinsrd,     simd,       0,   0,    0,    0);
    mkassign(          pinsrq,     simd,       0,   0,    0,    0);
    mkassign(          pinsrw,     simd,    intv,   0,   16,    0);
    mkassign(       pmaddusbw,     simd,       0,   0,    0,    0);
    mkassign(         pmaddwd,     simd,    intv,   0,   32,    0);

    // should be all binv or all intv?
    mkassign(          pmaxsb,     simd,    binv,   0,    8,    0);
    mkassign(          pmaxsd,     simd,    intv,   0,   32,    0);
    mkassign(          pmaxsw,     simd,    intv,   0,   16,    0);
    mkassign(          pmaxub,     simd,    intv,   0,    8,    0);
    mkassign(          pmaxud,     simd,    intv,   0,   32,    0);
    mkassign(          pmaxuw,     simd,    intv,   0,   16,    0);
    mkassign(          pminsb,     simd,    binv,   0,    8,    0);
    mkassign(          pminsd,     simd,    intv,   0,   32,    0);
    mkassign(          pminsw,     simd,    intv,   0,   16,    0);
    mkassign(          pminub,     simd,    intv,   0,    8,    0);
    mkassign(          pminud,     simd,    intv,   0,   32,    0);
    mkassign(          pminuw,     simd,    intv,   0,   16,    0);

    mkassign(        pmovmskb,     move,    move,   0, VRSZ,    0);
    mkassign(        pmovsxbd,     move,       0,   0,    0,    0);
    mkassign(        pmovsxbq,     move,       0,   0,    0,    0);
    mkassign(        pmovsxbw,     move,       0,   0,    0,    0);
    mkassign(        pmovsxdq,     move,       0,   0,    0,    0);
    mkassign(        pmovsxwd,     move,       0,   0,    0,    0);
    mkassign(        pmovsxwq,     move,       0,   0,    0,    0);
    mkassign(        pmovzxbd,     move,       0,   0,    0,    0);
    mkassign(        pmovzxbq,     move,       0,   0,    0,    0);
    mkassign(        pmovzxbw,     move,       0,   0,    0,    0);
    mkassign(        pmovzxdq,     move,       0,   0,    0,    0);
    mkassign(        pmovzxwd,     move,       0,   0,    0,    0);
    mkassign(        pmovzxwq,     move,       0,   0,    0,    0);
    mkassign(          pmuldq,     simd,       0,   0,    0,    0);
    mkassign(        pmulhrsw,     simd,       0,   0,    0,    0);
    mkassign(         pmulhrw,      int,    intv,   0,   16,    0);
    mkassign(         pmulhuw,      int,    intv,   0,   16,    0);
    mkassign(          pmulhw,      int,    intv,   0,   16,    0);
    mkassign(          pmulld,     simd,       0,   0,    0,    0);
    mkassign(          pmullw,      int,    intv,   0,   16,    0);
    mkassign(         pmuludq,      int,    intv,   0,   32,    0);
    mkassign(           popad,  special,   stack,   0,   32,    BinFrame);
    mkassign(            popa,  special,   stack,   0,   16,    BinFrame);
    mkassign(           popfd,      int,   stack,   0,   32,    BinStack);
    mkassign(           popfq,      int,   stack,   0,   64,    BinStack);
    mkassign(           popfw,      int,   stack,   0,   16,    BinStack);
    mkassign(             pop,      int,   stack,   0, VRSZ,    BinStack);
    mkassign(             por,      int,    binv,   0, VRSZ,    0);
    mkassign(     prefetchnta, prefetch,   cache,   0,    0,    0);
    mkassign(        prefetch, prefetch,   cache,   0,    0,    0);
    mkassign(      prefetcht0, prefetch,   cache,   0,    0,    0);
    mkassign(      prefetcht1, prefetch,   cache,   0,    0,    0);
    mkassign(      prefetcht2, prefetch,   cache,   0,    0,    0);
    mkassign(          psadbw,      int,    intv,   0,   16,    0);
    mkassign(          pshufb,     simd,    binv,   0,    8,    0);
    mkassign(          pshufd,      int,    binv,   0,   32,    0);
    mkassign(         pshufhw,      int,    binv,   0,   16,    0);
    mkassign(         pshuflw,      int,    binv,   0,   16,    0);
    mkassign(          pshufw,      int,    binv,   0,   16,    0);
    mkassign(          psignb,     simd,       0,   0,    0,    0);
    mkassign(          psignd,     simd,       0,   0,    0,    0);
    mkassign(          psignw,     simd,       0,   0,    0,    0);
    mkassign(           pslld,      int,    binv,   0,   32,    0);
    mkassign(          pslldq,      int,    binv,   0,   64,    0);
    mkassign(           psllq,      int,    binv,   0,   64,    0);
    mkassign(           psllw,      int,    binv,   0,   16,    0);
    mkassign(           psrad,      int,    binv,   0,   32,    0);
    mkassign(           psraw,      int,    binv,   0,   16,    0);
    mkassign(           psrld,      int,    binv,   0,   32,    0);
    mkassign(          psrldq,      int,    binv,   0,   64,    0);
    mkassign(           psrlq,      int,    binv,   0,   64,    0);
    mkassign(           psrlw,      int,    binv,   0,   16,    0);
    mkassign(           psubb,      int,    intv,   0,   16,    0);
    mkassign(           psubd,      int,    intv,   0,   32,    0);
    mkassign(           psubq,      int,    intv,   0,   64,    0);

    // the byte forms of these are 16-bit?
    mkassign(          psubsb,      int,    intv,   0,   16,    0);
    mkassign(          psubsw,      int,    intv,   0,   16,    0);
    mkassign(         psubusb,      int,    intv,   0,   16,    0);
    mkassign(         psubusw,      int,    intv,   0,   16,    0);
    mkassign(           psubw,      int,    intv,   0,   16,    0);

    mkassign(          pswapd,      int,    intv,   0,   32,    0);
    mkassign(           ptest,      int,       0,   0,    0,    0);
    mkassign(       punpckhbw,      int,    binv,   0,   16,    0);
    mkassign(       punpckhdq,      int,    binv,   0,   64,    0);
    mkassign(      punpckhqdq,      int,    binv,   0,   64,    0);
    mkassign(       punpckhwd,      int,    binv,   0,   32,    0);
    mkassign(       punpcklbw,      int,    binv,   0,   16,    0);
    mkassign(       punpckldq,      int,    binv,   0,   64,    0);
    mkassign(      punpcklqdq,      int,    binv,   0,   64,    0);
    mkassign(       punpcklwd,      int,    binv,   0,   32,    0);
    mkassign(          pushad,  special,   stack,   0,   32,    BinFrame);
    mkassign(           pusha,  special,   stack,   0,   16,    BinFrame);
    mkassign(          pushfd,      int,   stack,   0,   32,    BinStack);
    mkassign(          pushfq,      int,   stack,   0,   64,    BinStack);
    mkassign(          pushfw,      int,   stack,   0,   16,    BinStack);
    mkassign(            push,      int,   stack,   0, VRSZ,    BinStack);
    mkassign(            pxor,      int,    binv,   0, VRSZ,    0);
    mkassign(             rcl,      int,     bin,   0, VRSZ,    0);
    mkassign(           rcpps,    float,  floatv,   0,   32,    0);
    mkassign(           rcpss,    float,  floats,   0,   32,    0);
    mkassign(             rcr,      int,     bin,   0, VRSZ,    0);
    mkassign(           rdmsr,      int,   other,   0,    0,    0);
    mkassign(           rdpmc,  hwcount,   other,   0,    0,    0);
    mkassign(           rdtsc,  hwcount,   other,   0,    0,    0);
    mkassign(          rdtscp,  hwcount,   other,   0,    0,    0);
    mkassign(           repne,   string,  string,   0,    0,    0);
    mkassign(             rep,   string,  string,   0,    0,    0);
    mkassign(            retf,   return,  uncond,   0,    0,    BinFrame);
    mkassign(             ret,   return,  uncond,   0,    0,    BinFrame);
    mkassign(             rol,      int,     bin,   0, VRSZ,    0);
    mkassign(             ror,      int,     bin,   0, VRSZ,    0);
    mkassign(         roundpd,     simd,  floatv,   0,   64,    0);
    mkassign(         roundps,     simd,  floatv,   0,   32,    0);
    mkassign(         roundsd,     simd,  floats,   0,   64,    0);
    mkassign(         roundss,     simd,  floats,   0,   32,    0);
    mkassign(             rsm,  special,   other,   0,    0,    0);
    mkassign(         rsqrtps,    float,  floatv,   0,   32,    0);
    mkassign(         rsqrtss,    float,  floats,   0,   32,    0);
    mkassign(            sahf,      int,   other,   0,    0,    0);
    mkassign(            salc,      int,     bin,   0, VRSZ,    0);
    mkassign(             sal,      int,     bin,   0, VRSZ,    0);
    mkassign(             sar,      int,     bin,   0, VRSZ,    0);
    mkassign(             sbb,      int,     int,   0, VRSZ,    0);
    mkassign(           scasb,   string,  string,  si,    8,    0);
    mkassign(           scasd,   string,  string,  si,   32,    0);
    mkassign(           scasq,   string,  string,  si,   64,    0);
    mkassign(           scasw,   string,  string,  si,   16,    0);
    mkassign(            seta,      int,     bin,   0,    8,    0);
    mkassign(           setbe,      int,     bin,   0,    8,    0);
    mkassign(            setb,      int,     bin,   0,    8,    0);
    mkassign(           setge,      int,     bin,   0,    8,    0);
    mkassign(            setg,      int,     bin,   0,    8,    0);
    mkassign(           setle,      int,     bin,   0,    8,    0);
    mkassign(            setl,      int,     bin,   0,    8,    0);
    mkassign(           setnb,      int,     bin,   0,    8,    0);
    mkassign(           setno,      int,     bin,   0,    8,    0);
    mkassign(           setnp,      int,     bin,   0,    8,    0);
    mkassign(           setns,      int,     bin,   0,    8,    0);
    mkassign(           setnz,      int,     bin,   0,    8,    0);
    mkassign(            seto,      int,     bin,   0,    8,    0);
    mkassign(            setp,      int,     bin,   0,    8,    0);
    mkassign(            sets,      int,     bin,   0,    8,    0);
    mkassign(            setz,      int,     bin,   0,    8,    0);
    mkassign(          sfence,  special,   other,   0,    0,    0);
    mkassign(            sgdt,  special,   other,   0,    0,    0);
    mkassign(            shld,      int,     bin,   0, VRSZ,    0);
    mkassign(             shl,      int,     bin,   0, VRSZ,    0);
    mkassign(            shrd,      int,     bin,   0, VRSZ,    0);
    mkassign(             shr,      int,     bin,   0, VRSZ,    0);
    mkassign(          shufpd,    float,    binv,   0,   64,    0);
    mkassign(          shufps,    float,    binv,   0,   32,    0);
    mkassign(            sidt,  special,   other,   0,    0,    0);
    mkassign(          skinit,  special,   other,   0,    0,    0);
    mkassign(            sldt,  special,   other,   0,    0,    0);
    mkassign(            smsw,  special,   other,   0,    0,    0);
    mkassign(          sqrtpd,    float,  floatv,   0,   64,    0);
    mkassign(          sqrtps,    float,  floatv,   0,   32,    0);
    mkassign(          sqrtsd,    float,  floats,   0,   64,    0);
    mkassign(          sqrtss,    float,  floats,   0,   32,    0);
    mkassign(             stc,  special,   other,   0,    0,    0);
    mkassign(             std,  special,   other,   0,    0,    0);
    mkassign(            stgi,  special,   other,   0,    0,    0);
    mkassign(             sti,  special,   other,   0,    0,    0);
    mkassign(         stmxcsr,     move,   other,   0,    0,    0);
    mkassign(           stosb,   string,    move, dsi,    8,    0);
    mkassign(           stosd,   string,    move, dsi,   32,    0);
    mkassign(           stosq,   string,    move, dsi,   64,    0);
    mkassign(           stosw,   string,    move, dsi,   16,    0);
    mkassign(             str,     move,    move,   0, VRSZ,    0);
    mkassign(             sub,      int,     int,   0, VRSZ,    0);
    mkassign(           subpd,    float,  floatv,   0,   64,    0);
    mkassign(           subps,    float,  floatv,   0,   32,    0);
    mkassign(           subsd,    float,  floats,   0,   64,    0);
    mkassign(           subss,    float,  floats,   0,   32,    0);
    mkassign(          swapgs,  special,   other,   0,    0,    0);
    mkassign(         syscall,  syscall,  system,   0,    0,    BinFrame);
    mkassign(        sysenter,  syscall,  system,   0,    0,    BinFrame);
    mkassign(         sysexit,  syscall,  system,   0,    0,    BinFrame);
    mkassign(          sysret,  syscall,  system,   0,    0,    BinFrame);
    mkassign(            test,      int,     bin,   0, VRSZ,    0);
    mkassign(         ucomisd,    float,  floats,   0,   64,    0);
    mkassign(         ucomiss,    float,  floats,   0,   32,    0);
    mkassign(             ud2,  invalid, invalid,   0,    0,    0);
    mkassign(        unpckhpd,    float,    binv,   0,   64,    0);
    mkassign(        unpckhps,    float,    binv,   0,   32,    0);
    mkassign(        unpcklpd,    float,    binv,   0,   64,    0);
    mkassign(        unpcklps,    float,    binv,   0,   32,    0);
    mkassign(          vaddpd,      avx,       0,   0,    0,    0);
    mkassign(          vaddps,      avx,       0,   0,    0,    0);
    mkassign(          vaddsd,      avx,       0,   0,    0,    0);
    mkassign(          vaddss,      avx,       0,   0,    0,    0);
    mkassign(       vaddsubpd,      avx,       0,   0,    0,    0);
    mkassign(       vaddsubps,      avx,       0,   0,    0,    0);
    mkassign(     vaesdeclast,      aes,       0,   0,    0,    0);
    mkassign(         vaesdec,      aes,       0,   0,    0,    0);
    mkassign(     vaesenclast,      aes,       0,   0,    0,    0);
    mkassign(         vaesenc,      aes,       0,   0,    0,    0);
    mkassign(         vaesimc,      aes,       0,   0,    0,    0);
    mkassign(vaeskeygenassist,      aes,       0,   0,    0,    0);
    mkassign(         vandnpd,      avx,       0,   0,    0,    0);
    mkassign(         vandnps,      avx,       0,   0,    0,    0);
    mkassign(          vandpd,      avx,       0,   0,    0,    0);
    mkassign(          vandps,      avx,       0,   0,    0,    0);
    mkassign(        vblendpd,      avx,       0,   0,    0,    0);
    mkassign(        vblendps,      avx,       0,   0,    0,    0);
    mkassign(       vblendvpd,      avx,       0,   0,    0,    0);
    mkassign(       vblendvps,      avx,       0,   0,    0,    0);
    mkassign(      vbroadcast,     move,       0,   0,    0,    0);
    mkassign(          vcmppd,      avx,       0,   0,    0,    0);
    mkassign(          vcmpps,      avx,       0,   0,    0,    0);
    mkassign(          vcmpsd,      avx,       0,   0,    0,    0);
    mkassign(          vcmpss,      avx,       0,   0,    0,    0);
    mkassign(         vcomisd,      avx,       0,   0,    0,    0);
    mkassign(         vcomiss,      avx,       0,   0,    0,    0);
    mkassign(       vcvtdq2pd,      avx,       0,   0,    0,    0);
    mkassign(       vcvtdq2ps,      avx,       0,   0,    0,    0);
    mkassign(       vcvtpd2dq,      avx,       0,   0,    0,    0);
    mkassign(       vcvtpd2ps,      avx,       0,   0,    0,    0);
    mkassign(       vcvtph2ps,      avx,       0,   0,    0,    0);
    mkassign(       vcvtps2dq,      avx,       0,   0,    0,    0);
    mkassign(       vcvtps2pd,      avx,       0,   0,    0,    0);
    mkassign(       vcvtps2ph,      avx,       0,   0,    0,    0);
    mkassign(       vcvtsd2si,      avx,       0,   0,    0,    0);
    mkassign(       vcvtsd2ss,      avx,       0,   0,    0,    0);
    mkassign(       vcvtsi2sd,      avx,       0,   0,    0,    0);
    mkassign(       vcvtsi2ss,      avx,       0,   0,    0,    0);
    mkassign(       vcvtss2sd,      avx,       0,   0,    0,    0);
    mkassign(       vcvtss2si,      avx,       0,   0,    0,    0);
    mkassign(      vcvttpd2dq,      avx,       0,   0,    0,    0);
    mkassign(      vcvttps2dq,      avx,       0,   0,    0,    0);
    mkassign(      vcvttsd2si,      avx,       0,   0,    0,    0);
    mkassign(      vcvttss2si,      avx,       0,   0,    0,    0);
    mkassign(          vdivpd,      avx,       0,   0,    0,    0);
    mkassign(          vdivps,      avx,       0,   0,    0,    0);
    mkassign(          vdivsd,      avx,       0,   0,    0,    0);
    mkassign(          vdivss,      avx,       0,   0,    0,    0);
    mkassign(           vdppd,      avx,       0,   0,    0,    0);
    mkassign(           vdpps,      avx,       0,   0,    0,    0);
    mkassign(            verr,  special,   other,   0,    0,    0);
    mkassign(            verw,  special,   other,   0,    0,    0);
    mkassign(    vextractf128,      avx,       0,   0,    0,    0);
    mkassign(      vextractps,      avx,       0,   0,    0,    0);
    mkassign(         vhaddpd,      avx,       0,   0,    0,    0);
    mkassign(         vhaddps,      avx,       0,   0,    0,    0);
    mkassign(         vhsubpd,      avx,       0,   0,    0,    0);
    mkassign(         vhsubps,      avx,       0,   0,    0,    0);
    mkassign(     vinsertf128,      avx,       0,   0,    0,    0);
    mkassign(       vinsertps,      avx,       0,   0,    0,    0);
    mkassign(          vlddqu,     move,       0,   0, VRSZ,    0);
    mkassign(        vldmxcsr,     move,       0,   0,    0,    0);
    mkassign(     vmaskmovdqu,     move,       0,   0,    0,    0);
    mkassign(        vmaskmov,     move,       0,   0,    0,    0);
    mkassign(          vmaxpd,      avx,       0,   0,    0,    0);
    mkassign(          vmaxps,      avx,       0,   0,    0,    0);
    mkassign(          vmaxsd,      avx,       0,   0,    0,    0);
    mkassign(          vmaxss,      avx,       0,   0,    0,    0);
    mkassign(          vmcall,      vmx,   other,   0,    0,    0);
    mkassign(         vmclear,      vmx,   other,   0,    0,    0);
    mkassign(          vminpd,      avx,       0,   0,    0,    0);
    mkassign(          vminps,      avx,       0,   0,    0,    0);
    mkassign(          vminsd,      avx,       0,   0,    0,    0);
    mkassign(          vminss,      avx,       0,   0,    0,    0);
    mkassign(          vmload,      vmx,   other,   0,    0,    0);
    mkassign(         vmmcall,      vmx,   other,   0,    0,    0);
    mkassign(         vmovapd,     move,       0,   0,    0,    0);
    mkassign(         vmovaps,     move,       0,   0,    0,    0);
    mkassign(        vmovddup,     move,       0,   0,    0,    0);
    mkassign(           vmovd,     move,       0,   0,    0,    0);
    mkassign(         vmovdqa,     move,       0,   0,    0,    0);
    mkassign(         vmovdqu,     move,       0,   0,    0,    0);
    mkassign(         vmovhpd,     move,       0,   0,    0,    0);
    mkassign(         vmovhps,     move,       0,   0,    0,    0);
    mkassign(         vmovlpd,     move,       0,   0,    0,    0);
    mkassign(         vmovlps,     move,       0,   0,    0,    0);
    mkassign(       vmovmskpd,     move,       0,   0,    0,    0);
    mkassign(       vmovmskps,     move,       0,   0,    0,    0);
    mkassign(       vmovntdqa,     move,       0,   0,    0,    0);
    mkassign(        vmovntdq,     move,       0,   0,    0,    0);
    mkassign(        vmovntpd,     move,       0,   0,    0,    0);
    mkassign(        vmovntps,     move,       0,   0,    0,    0);
    mkassign(           vmovq,     move,       0,   0,    0,    0);
    mkassign(          vmovsd,     move,       0,   0,    0,    0);
    mkassign(       vmovshdup,     move,       0,   0,    0,    0);
    mkassign(       vmovsldup,     move,       0,   0,    0,    0);
    mkassign(          vmovss,     move,       0,   0,    0,    0);
    mkassign(         vmovupd,     move,       0,   0,    0,    0);
    mkassign(         vmovups,     move,       0,   0,    0,    0);
    mkassign(        vmpsadbw,      avx,       0,   0,    0,    0);
    mkassign(         vmptrld,      vmx,   other,   0,    0,    0);
    mkassign(         vmptrst,      vmx,   other,   0,    0,    0);
    mkassign(        vmresume,      vmx,   other,   0,    0,    0);
    mkassign(           vmrun,      vmx,   other,   0,    0,    0);
    mkassign(          vmsave,      vmx,   other,   0,    0,    0);
    mkassign(          vmulpd,      avx,       0,   0,    0,    0);
    mkassign(          vmulps,      avx,       0,   0,    0,    0);
    mkassign(          vmulsd,      avx,       0,   0,    0,    0);
    mkassign(          vmulss,      avx,       0,   0,    0,    0);
    mkassign(          vmxoff,      vmx,   other,   0,    0,    0);
    mkassign(           vmxon,      vmx,   other,   0,    0,    0);
    mkassign(           vorpd,      avx,       0,   0,    0,    0);
    mkassign(           vorps,      avx,       0,   0,    0,    0);
    mkassign(          vpabsb,      avx,       0,   0,    0,    0);
    mkassign(          vpabsd,      avx,       0,   0,    0,    0);
    mkassign(          vpabsw,      avx,       0,   0,    0,    0);
    mkassign(       vpackssdw,      avx,       0,   0,    0,    0);
    mkassign(       vpacksswb,      avx,       0,   0,    0,    0);
    mkassign(       vpackusdw,      avx,       0,   0,    0,    0);
    mkassign(       vpackuswb,      avx,       0,   0,    0,    0);
    mkassign(          vpaddb,      avx,       0,   0,    0,    0);
    mkassign(          vpaddd,      avx,       0,   0,    0,    0);
    mkassign(          vpaddq,      avx,       0,   0,    0,    0);
    mkassign(         vpaddsb,      avx,       0,   0,    0,    0);
    mkassign(         vpaddsw,      avx,       0,   0,    0,    0);
    mkassign(        vpaddusb,      avx,       0,   0,    0,    0);
    mkassign(        vpaddusw,      avx,       0,   0,    0,    0);
    mkassign(          vpaddw,      avx,       0,   0,    0,    0);
    mkassign(        vpalignr,      avx,       0,   0,    0,    0);
    mkassign(          vpandn,      avx,       0,   0,    0,    0);
    mkassign(           vpand,      avx,       0,   0,    0,    0);
    mkassign(          vpavgb,      avx,       0,   0,    0,    0);
    mkassign(          vpavgw,      avx,       0,   0,    0,    0);
    mkassign(       vpblendvb,      avx,       0,   0,    0,    0);
    mkassign(        vpblendw,      avx,       0,   0,    0,    0);
    mkassign(      vpclmulqdq,      avx,       0,   0,    0,    0);
    mkassign(        vpcmpeqb,      avx,       0,   0,    0,    0);
    mkassign(        vpcmpeqd,      avx,       0,   0,    0,    0);
    mkassign(        vpcmpeqq,      avx,       0,   0,    0,    0);
    mkassign(        vpcmpeqw,      avx,       0,   0,    0,    0);
    mkassign(      vpcmpestri,      avx,       0,   0,    0,    0);
    mkassign(      vpcmpestrm,      avx,       0,   0,    0,    0);
    mkassign(        vpcmpgtb,      avx,       0,   0,    0,    0);
    mkassign(        vpcmpgtd,      avx,       0,   0,    0,    0);
    mkassign(        vpcmpgtq,      avx,       0,   0,    0,    0);
    mkassign(        vpcmpgtw,      avx,       0,   0,    0,    0);
    mkassign(      vpcmpistri,      avx,       0,   0,    0,    0);
    mkassign(      vpcmpistrm,      avx,       0,   0,    0,    0);
    mkassign(       vpermf128,      avx,       0,   0,    0,    0);
    mkassign(       vpermilpd,      avx,       0,   0,    0,    0);
    mkassign(       vpermilps,      avx,       0,   0,    0,    0);
    mkassign(         vpextrb,      avx,       0,   0,    0,    0);
    mkassign(         vpextrd,      avx,       0,   0,    0,    0);
    mkassign(         vpextrq,      avx,       0,   0,    0,    0);
    mkassign(         vpextrw,      avx,       0,   0,    0,    0);
    mkassign(         vphaddd,      avx,       0,   0,    0,    0);
    mkassign(        vphaddsw,      avx,       0,   0,    0,    0);
    mkassign(         vphaddw,      avx,       0,   0,    0,    0);
    mkassign(     vphminposuw,      avx,       0,   0,    0,    0);
    mkassign(         vphsubd,      avx,       0,   0,    0,    0);
    mkassign(        vphsubsw,      avx,       0,   0,    0,    0);
    mkassign(         vphsubw,      avx,       0,   0,    0,    0);
    mkassign(         vpinsrb,      avx,       0,   0,    0,    0);
    mkassign(         vpinsrd,      avx,       0,   0,    0,    0);
    mkassign(         vpinsrq,      avx,       0,   0,    0,    0);
    mkassign(         vpinsrw,      avx,       0,   0,    0,    0);
    mkassign(      vpmaddusbw,      avx,       0,   0,    0,    0);
    mkassign(        vpmaddwd,      avx,       0,   0,    0,    0);
    mkassign(         vpmaxsb,      avx,       0,   0,    0,    0);
    mkassign(         vpmaxsd,      avx,       0,   0,    0,    0);
    mkassign(         vpmaxsw,      avx,       0,   0,    0,    0);
    mkassign(         vpmaxub,      avx,       0,   0,    0,    0);
    mkassign(         vpmaxud,      avx,       0,   0,    0,    0);
    mkassign(         vpmaxuw,      avx,       0,   0,    0,    0);
    mkassign(         vpminsb,      avx,       0,   0,    0,    0);
    mkassign(         vpminsd,      avx,       0,   0,    0,    0);
    mkassign(         vpminsw,      avx,       0,   0,    0,    0);
    mkassign(         vpminub,      avx,       0,   0,    0,    0);
    mkassign(         vpminud,      avx,       0,   0,    0,    0);
    mkassign(         vpminuw,      avx,       0,   0,    0,    0);
    mkassign(       vpmovmskb,     move,       0,   0,    0,    0);
    mkassign(       vpmovsxbd,     move,       0,   0,    0,    0);
    mkassign(       vpmovsxbq,     move,       0,   0,    0,    0);
    mkassign(       vpmovsxbw,     move,       0,   0,    0,    0);
    mkassign(       vpmovsxdq,     move,       0,   0,    0,    0);
    mkassign(       vpmovsxwd,     move,       0,   0,    0,    0);
    mkassign(       vpmovsxwq,     move,       0,   0,    0,    0);
    mkassign(       vpmovzxbd,     move,       0,   0,    0,    0);
    mkassign(       vpmovzxbq,     move,       0,   0,    0,    0);
    mkassign(       vpmovzxbw,     move,       0,   0,    0,    0);
    mkassign(       vpmovzxdq,     move,       0,   0,    0,    0);
    mkassign(       vpmovzxwd,     move,       0,   0,    0,    0);
    mkassign(       vpmovzxwq,     move,       0,   0,    0,    0);
    mkassign(         vpmuldq,      avx,       0,   0,    0,    0);
    mkassign(       vpmulhrsw,      avx,       0,   0,    0,    0);
    mkassign(        vpmulhuw,      avx,       0,   0,    0,    0);
    mkassign(         vpmulhw,      avx,       0,   0,    0,    0);
    mkassign(         vpmulld,      avx,       0,   0,    0,    0);
    mkassign(         vpmullw,      avx,       0,   0,    0,    0);
    mkassign(        vpmuludq,      avx,       0,   0,    0,    0);
    mkassign(            vpor,      avx,       0,   0,    0,    0);
    mkassign(         vpsadbw,      avx,       0,   0,    0,    0);
    mkassign(         vpshufb,      avx,       0,   0,    0,    0);
    mkassign(         vpshufd,      avx,       0,   0,    0,    0);
    mkassign(        vpshufhw,      avx,       0,   0,    0,    0);
    mkassign(        vpshuflw,      avx,       0,   0,    0,    0);
    mkassign(         vpsignb,      avx,       0,   0,    0,    0);
    mkassign(         vpsignd,      avx,       0,   0,    0,    0);
    mkassign(         vpsignw,      avx,       0,   0,    0,    0);
    mkassign(          vpslld,      avx,       0,   0,    0,    0);
    mkassign(         vpslldq,      avx,       0,   0,    0,    0);
    mkassign(          vpsllq,      avx,       0,   0,    0,    0);
    mkassign(          vpsllw,      avx,       0,   0,    0,    0);
    mkassign(          vpsrad,      avx,       0,   0,    0,    0);
    mkassign(          vpsraw,      avx,       0,   0,    0,    0);
    mkassign(          vpsrld,      avx,       0,   0,    0,    0);
    mkassign(         vpsrldq,      avx,       0,   0,    0,    0);
    mkassign(          vpsrlq,      avx,       0,   0,    0,    0);
    mkassign(          vpsrlw,      avx,       0,   0,    0,    0);
    mkassign(          vpsubb,      avx,       0,   0,    0,    0);
    mkassign(          vpsubd,      avx,       0,   0,    0,    0);
    mkassign(          vpsubq,      avx,       0,   0,    0,    0);
    mkassign(         vpsubsb,      avx,       0,   0,    0,    0);
    mkassign(         vpsubsw,      avx,       0,   0,    0,    0);
    mkassign(        vpsubusb,      avx,       0,   0,    0,    0);
    mkassign(        vpsubusw,      avx,       0,   0,    0,    0);
    mkassign(          vpsubw,      avx,       0,   0,    0,    0);
    mkassign(      vpunpckhbw,      avx,       0,   0,    0,    0);
    mkassign(      vpunpckhdq,      avx,       0,   0,    0,    0);
    mkassign(     vpunpckhqdq,      avx,       0,   0,    0,    0);
    mkassign(      vpunpckhwd,      avx,       0,   0,    0,    0);
    mkassign(      vpunpcklbw,      avx,       0,   0,    0,    0);
    mkassign(      vpunpckldq,      avx,       0,   0,    0,    0);
    mkassign(     vpunpcklqdq,      avx,       0,   0,    0,    0);
    mkassign(      vpunpcklwd,      avx,       0,   0,    0,    0);
    mkassign(           vpxor,      avx,       0,   0,    0,    0);
    mkassign(          vrcpps,      avx,       0,   0,    0,    0);
    mkassign(          vrcpss,      avx,       0,   0,    0,    0);
    mkassign(        vroundpd,      avx,       0,   0,    0,    0);
    mkassign(        vroundps,      avx,       0,   0,    0,    0);
    mkassign(        vroundsd,      avx,       0,   0,    0,    0);
    mkassign(        vroundss,      avx,       0,   0,    0,    0);
    mkassign(        vrsqrtps,      avx,       0,   0,    0,    0);
    mkassign(        vrsqrtss,      avx,       0,   0,    0,    0);
    mkassign(         vshufpd,      avx,       0,   0,    0,    0);
    mkassign(         vshufps,      avx,       0,   0,    0,    0);
    mkassign(         vsqrtpd,      avx,       0,   0,    0,    0);
    mkassign(         vsqrtps,      avx,       0,   0,    0,    0);
    mkassign(         vsqrtsd,      avx,       0,   0,    0,    0);
    mkassign(         vsqrtss,      avx,       0,   0,    0,    0);
    mkassign(        vstmxcsr,     move,       0,   0,    0,    0);
    mkassign(          vsubpd,      avx,       0,   0,    0,    0);
    mkassign(          vsubps,      avx,       0,   0,    0,    0);
    mkassign(          vsubsd,      avx,       0,   0,    0,    0);
    mkassign(          vsubss,      avx,       0,   0,    0,    0);
    mkassign(         vtestpd,      avx,       0,   0,    0,    0);
    mkassign(         vtestps,      avx,       0,   0,    0,    0);
    mkassign(        vucomisd,      avx,       0,   0,    0,    0);
    mkassign(        vucomiss,      avx,       0,   0,    0,    0);
    mkassign(       vunpckhpd,      avx,       0,   0,    0,    0);
    mkassign(       vunpckhps,      avx,       0,   0,    0,    0);
    mkassign(       vunpcklpd,      avx,       0,   0,    0,    0);
    mkassign(       vunpcklps,      avx,       0,   0,    0,    0);
    mkassign(          vxorpd,      avx,       0,   0,    0,    0);
    mkassign(          vxorps,      avx,       0,   0,    0,    0);
    mkassign(        vzeroall,      avx,       0,   0,    0,    0);
    mkassign(            wait,  special,   other,   0,    0,    0);
    mkassign(          wbinvd,  special,   other,   0,    0,    0);
    mkassign(           wrmsr,  special,   other,   0,    0,    0);
    mkassign(            xadd,      int,     int,   0, VRSZ,    0);
    mkassign(            xchg,      int,     int,   0, VRSZ,    0);
    mkassign(           xgetbv, special,   other,   0,    0,    0);
    mkassign(           xlatb,  special,   other,   0,    0,    0);
    mkassign(             xor,      int,     bin,   0, VRSZ,    0);
    mkassign(           xorpd,    float,    binv,   0,   64,    0);
    mkassign(           xorps,    float,    binv,   0,   32,    0);

    verify();
}
