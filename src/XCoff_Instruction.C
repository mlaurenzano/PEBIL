#include <Instruction.h>
#include <SectHeader.h>
#include <SymbolTable.h>
#include <DemangleWrapper.h>
#include <LoaderSection.h>
#include <RawSection.h>
#include <Iterator.h>
#include <Function.h>
#include <XCoffFile.h>
#include <BinaryFile.h>

/** Later we can make it pointer depending on what the sub classes will do **/

Operand Operand::IntegerOperand0 = IntegerOperand(0);

Operand* Operand::GPRegisterOperands = initGPRegisterOperands();

Operand* Operand::initGPRegisterOperands(){
    Operand* ret = new Operand[NUM_OF_GPR_REGS];
    for(uint32_t i=0;i<NUM_OF_GPR_REGS;i++)
        ret[i] = GPRegisterOperand(i);
    return ret;
}

BitSet<>* Instruction::memoryOperationXops = Instruction::initMemoryOperationXops();

BitSet<>* Instruction::initMemoryOperationXops(){
    BitSet<>* ret = new BitSet<>(INVALID_XOP);
    uint32_t xops[] = { xcode_Lwarx,xcode_Ldx,xcode_Lwzx,xcode_Ldux,xcode_Lwzux,xcode_Ldarx,
                        xcode_Lbzx,xcode_Lbzux,xcode_Stdx,xcode_Stwcxc,xcode_Stwx,xcode_Stdux,
                        xcode_Stwux,xcode_Stdcxc,xcode_Stbx,xcode_Stbux,xcode_Lhzx,xcode_Lhzux,
                        xcode_Lwax,xcode_Lhax,xcode_Lwaux,xcode_Lhaux,xcode_Sthx,xcode_Sthux,
                        xcode_Lswx,xcode_Lwbrx,xcode_Lfsx,xcode_Lfsux,xcode_Lswi,xcode_Lfdx,
                        xcode_Lfdux,xcode_Stswx,xcode_Stwbrx,xcode_Stfsx,xcode_Stfsux,xcode_Stswi,
                        xcode_Stfdx,xcode_Stfdux,xcode_Lhbrx,xcode_Sthbrx,xcode_Stfiwx,
                        xcode_LscbxC,
                        INVALID_XOP };

    for(uint32_t i=0;xops[i] != INVALID_XOP;i++)
        ret->insert(xops[i]);

    return ret;
}

bool Instruction::isCondBranch() { 
    if(!ppci.lk.lk &&
       IS_BC(ppci) && 
       !BRANCH_ALWAYS(ppci.b.bo))
       return true;
    return false; 
}

bool Instruction::isJump() { 
    if(!ppci.lk.lk){
        if(IS_B(ppci))
            return true;
        if(IS_BC(ppci) && 
           BRANCH_ALWAYS(ppci.b.bo))
            return true;
    }
    return false; 
}

bool Instruction::isReturn() { 
    if(!ppci.lk.lk){
        if(IS_BCLR(ppci) && 
           BRANCH_ALWAYS(ppci.xl.bt_bo) &&
           !GET_BH(ppci))
            return true;
    }
    return isCondReturn();
}

bool Instruction::isCondReturn(){
    if(!ppci.lk.lk){
        if(IS_BCLR(ppci) &&
           !BRANCH_ALWAYS(ppci.xl.bt_bo))
            return true;
    }
    return false;
}

bool Instruction::isCall() { 
    if(ppci.lk.lk){
        if(IS_B(ppci))
            return true;
        if(IS_BC(ppci) && BRANCH_ALWAYS(ppci.b.bo))
            return true;
        if(IS_BC(ppci) && !BRANCH_ALWAYS(ppci.b.bo)) /** Conditional Call **/
            return true; 
        if((IS_BCLR(ppci) || IS_BCCTR(ppci)) && BRANCH_ALWAYS(ppci.xl.bt_bo))
            return true;
    }
    return false; 
}

uint64_t Instruction::getTargetAddress(uint64_t insnAddr){

    ASSERT((isCondBranch() || isJump()) && 
           "FATAL : getTargetAddress can oly be called for cond branch and jump");;

    int32_t offset = 0x0;

    if(IS_B(ppci)){
        offset = ppci.i.li;
    } else if(IS_BC(ppci)){
        offset = ppci.b.bd;
    }
    offset <<= 2;

    if(ppci.lk.aa){
        return (uint64_t)offset;
    }

    return insnAddr+offset;
}

/**************************************************************************************
xlc c                                            xlc++
32                        64                        32                            64
l       r31,104(r2)        ld         r31,192(r2)        l       r31,152(r2)            ld        r31,296(r2)

cmpli     cr0,0x0,r4,0xb     cmpli     cr0,0x0,r4,0xb     cmpli     cr0,0x0,r3,0xb         cmpli     cr0,0x0,r3,0xb
bgt     0x10000794         bgt     0x1000251c         bgt     0x10000085c         bgt     0x100002800 
sli     r3,r3,0x2         sli     r3,r3,0x0        sli     r4,r3,0x2             sli     r3,r3,0x0
                        rldicr     r3,r3,0x3,0x39                                rldicr     r4,r3,0x3,0x39
                                                addi     r3,0x84(r31)        addi     r3,0xe0(r31)
lwzx     r3,r31,r3        ldx     r3,r31,r3        lwzx    r3,r3,r4            ldx     r3,r3,r4


gnu c                                            gnu c++
32                        64                        32                            64
cmpli   7,r0,11            cmplwi  cr7,r0,11        cmpli   7,r0,11                cmplwi  cr7,r0,11
bgt     7,0x10000b28     bgt-    cr7,0x100009e4    bgt     7,0x100013f4        bgt-    cr7,0x1000143c
l       r0,104(r31)        lwz     r0,192(r31)        l       r0,104(r31)            lwz     r0,192(r31)
                        clrldi  r0,r0,32                                    clrldi  r0,r0,32
rlinm   r9,r0,2,0,29    rldicr  r9,r0,2,61        rlinm   r9,r0,2,0,29        rldicr  r9,r0,2,61    
l       r0,108(r2)        ld      r0,200(r2)        l       r0,192(r2)            ld      r0,368(r2)
cax     r9,r9,r0        add     r9,r9,r0        cax     r9,r9,r0            add     r9,r9,r0
l       r9,0(r9)        lwz     r0,0(r9)        l       r9,0(r9)            lwz     r0,0(r9)
l       r0,108(r2)        ld      r9,200(r2)        l       r0,192(r2)            ld      r9,368(r2)
                        extsw   r0,r0                                        extsw   r0,r0
cax     r9,r9,r0        add     r0,r0,r9        cax     r9,r9,r0            add     r0,r0,r9
**************************************************************************************/

bool Instruction::isIndirectJump() { 
    return (isIndirectJumpCtr() || isIndirectJumpLnk());
}

bool Instruction::isIndirectJumpCtr(){
    if(!ppci.lk.lk &&
       IS_BCCTR(ppci) &&
       BRANCH_ALWAYS(ppci.xl.bt_bo))
        return true;
    return false;
}
bool Instruction::isIndirectJumpLnk(){
    if(!ppci.lk.lk &&
       IS_BCLR(ppci) &&
       BRANCH_ALWAYS(ppci.xl.bt_bo) &&
       GET_BH(ppci))
        return true;
    return false;
}

bool Instruction::isOtherBranch(){
    if(ppci.lk.lk){
        if(IS_BCLR(ppci) && !BRANCH_ALWAYS(ppci.xl.bt_bo)){ /** Conditional Return and Call **/
            return true;
        }
        if(IS_BCCTR(ppci) && !BRANCH_ALWAYS(ppci.xl.bt_bo)){ /** Conditional Indirect Call **/
            return true;
        }
    } else {
        if(IS_BCCTR(ppci) && !BRANCH_ALWAYS(ppci.xl.bt_bo)){ /** Conditional Indirect Branch **/
            return true; 
        }
    }
    return false;
}

bool Instruction::definesJTBaseAddress(){
    if(IS_LWZ(ppci) && (ppci.d.ra == REG_TOC))
        return true;
    if(IS_LD(ppci) && (ppci.ds.ra == REG_TOC))
        return true;
    return false;
}
int32_t Instruction::getJTBaseOffsetTOC(){
    ASSERT(definesJTBaseAddress());
    if(IS_LWZ(ppci))
        return ppci.d.d_ui_si;
    if(IS_LD(ppci))
        return (ppci.ds.ds << 2);
    return 0;
}
uint32_t Instruction::getJTBaseAddrTarget(){
    return ppci.d.rt_s_bf;
}

bool Instruction::definesJTEntryCount(){
    return IS_CMPLI(ppci);
}
uint32_t Instruction::getJTEntryCount(){
    return (uint32_t)(ppci.d.d_ui_si & 0x0000ffff);
}

bool Instruction::isAddBeforeJump(){
    return IS_ADD(ppci);
}

bool Instruction::isLoadBeforeJump(){
    return (IS_LDX(ppci) || IS_LWZX(ppci));
}

uint32_t Instruction::getLoadBeforeJumpSrc1(){
    return ppci.x.ra_sr;
}

bool Instruction::definesJTBaseAddrIndir(){
    return IS_ADDI(ppci);
}

int32_t Instruction::getJTBaseAddrIndirOffset(){
    ASSERT(definesJTBaseAddrIndir());
    return ppci.d.d_ui_si; 
}

uint32_t Instruction::getJTBaseAddrIndirSrc(){
    return ppci.d.ra;
}


#ifdef USE_DISASSEMBLER
extern "C" int32_t print_insn_powerpc_for_meminst(uint64_t memaddr, uint32_t insn, int32_t dialect,char* buffer);
#else
int32_t print_insn_powerpc_for_meminst(uint64_t memaddr, uint32_t insn, int32_t dialect,char* buffer){
    if(buffer) strcpy(buffer,"no_insn");
    return 0;
}
#endif

void Instruction::print(uint64_t insnAddr,bool is64Bit){

#ifdef USE_DISASSEMBLER
    char buffer[1024];
    fprintf(stdout,"\t%#18llx : ",insnAddr);
    if(is64Bit){
        print_insn_powerpc_for_meminst(insnAddr,ppci.bits,67169,buffer);
    } else {
        print_insn_powerpc_for_meminst(insnAddr,ppci.bits,2,buffer);
    }
    fprintf(stdout,buffer);

    if(isCondBranch()){
        PRINT_DEBUG("\tisCondBranch");
    } else if(isJump()){
        PRINT_DEBUG("\tisJump");
    } else if(isCondReturn()){
        PRINT_DEBUG("\tisCondReturn");
    } else if(isReturn()){
        PRINT_DEBUG("\tisReturn");
    } else if(isCall()){
        PRINT_DEBUG("\tisCall");
    } else if(isIndirectJump()){
        PRINT_DEBUG("\tisIndirectJump");
    } else if(isMemoryDForm()){
        PRINT_DEBUG("\tisMemoryDForm");
    } else if(isMemoryDsForm()){
        PRINT_DEBUG("\tisMemoryDsForm");
    } else if(isMemoryXForm()){
        PRINT_DEBUG("\tisMemoryXForm");
    } else if(isUnhandledMemoryOp()){
        PRINT_DEBUG("\tisUnhandledMemoryOp");
    } else if(isFloatAForm()){
        PRINT_DEBUG("\tisFloatAForm");
    } else if(isFloatXForm()){
        PRINT_DEBUG("\tisFloatXForm");
    }
#endif
}

void Instruction::print(char* buffer,uint64_t baseAddress,uint32_t sizeInBytes,bool is64Bit){
#ifdef USE_DISASSEMBLER
    for(uint32_t i=0;i<sizeInBytes;i+=sizeof(uint32_t)){
        uint64_t insnAddr = baseAddress + i;
        uint32_t insnBits = 0;
        memcpy(&insnBits,buffer+i,sizeof(uint32_t));
        Instruction insn(insnBits);
        insn.print(insnAddr,is64Bit);
    }
#endif
}

bool Instruction::isMemoryDForm(){
    return ((ppci.opc.opc >= opcode_Lwz) && 
            (ppci.opc.opc <= opcode_Stfdu));
}
bool Instruction::isMemoryDsForm(){
    return ((ppci.opc.opc == opcode_Ld) || 
            (ppci.opc.opc == opcode_Std));
}
bool Instruction::isMemoryXForm(){
    return ((ppci.opc.opc == opcode_Lwarx) && 
            memoryOperationXops->contains(ppci.x.xo));
}
bool Instruction::isUnhandledMemoryOp(){
    return ((ppci.opc.opc == opcode_Lwarx) && 
            !memoryOperationXops->contains(ppci.x.xo) &&
            ((ppci.x.xo & MEMORY_MASK) == MEMORY_MATCH));
}

bool Instruction::isFloatAForm(){
    return ((ppci.opc.opc == opcode_FdivsC) ||
            ((ppci.opc.opc == opcode_Fcmpu) && 
            ((ppci.x.xo & FOP_AFORM_MASK) == FOP_AFORM_MATCH)));
}
bool Instruction::isFloatXForm(){
    return ((ppci.opc.opc == opcode_Fcmpu) && 
            ((ppci.x.xo & FOP_AFORM_MASK) != FOP_AFORM_MATCH) &&
            ((ppci.x.xo & FOP_CONDREG_MASK) != FOP_CONDREG_MATCH) &&
            (ppci.x.xo != xcode_Mcrfs));
}

uint32_t Instruction::getDFormSrc1() 
{ 
    return ppci.d.ra; 
}
uint32_t Instruction::getDFormTgt() 
{ 
    return ppci.d.rt_s_bf; 
}
int32_t Instruction::getDFormImmediate() 
{ 
    return ppci.d.d_ui_si; 
}
int32_t Instruction::getDsFormImmediate() 
{ 
    return (ppci.ds.ds << 2); 
}
uint32_t Instruction::getXFormSrc1() 
{ 
    return ppci.x.ra_sr; 
}
uint32_t Instruction::getXFormSrc2() 
{ 
    return ppci.x.rb_sh; 
}
bool Instruction::isMemoryXFormButNoSrc2() 
{ 
    return (IS_LSWI(ppci) || IS_STSWI(ppci)); 
}

bool Instruction::isInLoadOffsetInsnRange(int32_t value){
    int32_t max = 0x7fff;
    int32_t min = -(max+1);
    if((min <= value) && (value <= max))
        return true;
    return false;
}

bool Instruction::isInJumpInsnRange(uint64_t from,uint64_t to){
    int32_t distance = to - from;
    ASSERT(!(distance & 0x3));
    distance >>= 2;
    int32_t max = 0x7fffff;
    int32_t min = -(max+1);
    if((min <= distance) && (distance <= max))
        return true;
    return false;
}
Instruction Instruction::generateJumpInsn(uint64_t from,uint64_t to){
    Instruction ret;
    ASSERT(isInJumpInsnRange(from,to));
    int32_t distance = to - from;
    distance >>= 2;
    ret.ppci.i.opc = opcode_BLA;
    ret.ppci.i.li = distance;
    ret.ppci.i.aa = 0;
    ret.ppci.i.lk = 0;
    return ret;
}
Instruction Instruction::generateAdd(uint32_t tgt,uint32_t src1,uint32_t src2){
    Instruction ret;
    ret.ppci.xo.opc = opcode_AddOC;
    ret.ppci.xo.xo = xcode_AddOC;
    ret.ppci.xo.oe = 0;
    ret.ppci.xo.rc = 0;
    ret.ppci.xo.rt = tgt;
    ret.ppci.xo.ra = src1;
    ret.ppci.xo.rb = src2;
    return ret;
}

Instruction Instruction::generateAddImm(uint32_t tgt,uint32_t src,int32_t value){
    Instruction ret;
    ret.ppci.d.opc = opcode_Addi;
    ret.ppci.d.rt_s_bf = tgt;
    ret.ppci.d.ra = src;
    ASSERT(isInLoadOffsetInsnRange(value));
    ret.ppci.d.d_ui_si = value;
    return ret;
}

Instruction Instruction::generateIncrement(uint32_t reg,int32_t value){
    return generateAddImm(reg,reg,value);
}
Instruction Instruction::generateLoadImmediate(uint32_t reg,int32_t value){
    return generateAddImm(reg,0,value);
}

Instruction Instruction::generateAddImmShifted(uint32_t tgt,uint32_t src,int32_t imm){
    Instruction ret;
    ret.ppci.d.opc = opcode_Addis;
    ret.ppci.d.rt_s_bf = tgt;
    ret.ppci.d.ra = src;
    ASSERT(isInLoadOffsetInsnRange(imm));
    ret.ppci.d.d_ui_si = imm;
    return ret;
}
Instruction Instruction::generateLoad32BitHigh(uint32_t reg,int32_t value){
    return generateAddImmShifted(reg,0,(value >> 16));
}
Instruction Instruction::generateOrImm(uint32_t tgt,uint32_t src,int32_t imm){
    Instruction ret;
    ret.ppci.d.opc = opcode_Ori;
    ret.ppci.d.rt_s_bf = src;
    ret.ppci.d.ra = tgt;
    ret.ppci.d.d_ui_si = imm;
    return ret;
}
Instruction Instruction::generateLoad32BitLow(uint32_t reg,int32_t value){
    return generateOrImm(reg,reg,(value & 0xffff));
}

Instruction Instruction::generateAnd(uint32_t tgt,uint32_t src1,uint32_t src2){
    Instruction ret;
    ret.ppci.x.opc = opcode_AndC;
    ret.ppci.x.xo = xcode_AndC;
    ret.ppci.x.rt_s_bf = src1;
    ret.ppci.x.ra_sr = tgt;
    ret.ppci.x.rb_sh = src2;
    ret.ppci.x.rc = 0;
    return ret;
}

Instruction Instruction::generateXorImm(uint32_t tgt,uint32_t src,int32_t imm){
    Instruction ret;
    ret.ppci.d.opc = opcode_Xori;
    ret.ppci.d.rt_s_bf = src;
    ret.ppci.d.ra = tgt;
    ret.ppci.d.d_ui_si = imm;
    return ret;
}
Instruction Instruction::generateXorImmShifted(uint32_t tgt,uint32_t src,int32_t imm){
    Instruction ret;
    ret.ppci.d.opc = opcode_Xoris;
    ret.ppci.d.rt_s_bf = src;
    ret.ppci.d.ra = tgt;
    ret.ppci.d.d_ui_si = imm;
    return ret;
}
Instruction Instruction::multiplyImmediate(uint32_t tgt,uint32_t src,int32_t imm){
    Instruction ret;
    ret.ppci.d.opc = opcode_Mulli;
    ret.ppci.d.rt_s_bf = tgt;
    ret.ppci.d.ra = src;
    ASSERT(isInLoadOffsetInsnRange(imm));
    ret.ppci.d.d_ui_si = imm;
    return ret;
}

Instruction Instruction::generateStoreDoubleFloat(uint32_t src,uint32_t base,int32_t offset){
    Instruction ret;
    ASSERT(!(offset & 0x3));
    ASSERT(isInLoadOffsetInsnRange(offset));
    ret.ppci.d.opc = opcode_Stfd;
    ret.ppci.d.rt_s_bf = src;
    ret.ppci.d.ra = base;
    ret.ppci.d.d_ui_si = offset;
    return ret;
}
Instruction Instruction::generateLoadDoubleFloat(uint32_t tgt,uint32_t base,int32_t offset){
    Instruction ret;
    ASSERT(!(offset & 0x3));
    ASSERT(isInLoadOffsetInsnRange(offset));
    ret.ppci.d.opc = opcode_Lfd;
    ret.ppci.d.rt_s_bf = tgt;
    ret.ppci.d.ra = base;
    ret.ppci.d.d_ui_si = offset;
    return ret;
}
Instruction Instruction::generateStoreWordFloat(uint32_t src,uint32_t base,int32_t offset){
    Instruction ret;
    ASSERT(!(offset & 0x3));
    ASSERT(isInLoadOffsetInsnRange(offset));
    ret.ppci.d.opc = opcode_Stfs;
    ret.ppci.d.rt_s_bf = src;
    ret.ppci.d.ra = base;
    ret.ppci.d.d_ui_si = offset;
    return ret;
}
Instruction Instruction::generateLoadWordFloat(uint32_t tgt,uint32_t base,int32_t offset){
    Instruction ret;
    ASSERT(!(offset & 0x3));
    ASSERT(isInLoadOffsetInsnRange(offset));
    ret.ppci.d.opc = opcode_Lfs;
    ret.ppci.d.rt_s_bf = tgt;
    ret.ppci.d.ra = base;
    ret.ppci.d.d_ui_si = offset;
    return ret;
}

Instruction Instruction::generateStoreDouble(uint32_t src,uint32_t base,int32_t offset){
    Instruction ret;
    ret.ppci.ds.opc = opcode_Std;
    ret.ppci.ds.xo = xcode_Std;
    ret.ppci.ds.rt_s = src;
    ret.ppci.ds.ra = base;
    ASSERT(!(offset & 0x3));
    ASSERT(isInLoadOffsetInsnRange(offset));
    offset >>= 2;
    ret.ppci.ds.ds = offset;
    return ret;
}
Instruction Instruction::generateLoadDouble(uint32_t tgt,uint32_t base,int32_t offset){
    Instruction ret;
    ret.ppci.ds.opc = opcode_Ld;
    ret.ppci.ds.xo = xcode_Ld;
    ret.ppci.ds.rt_s = tgt;
    ret.ppci.ds.ra = base;
    ASSERT(!(offset & 0x3));
    ASSERT(isInLoadOffsetInsnRange(offset));
    offset >>= 2;
    ret.ppci.ds.ds = offset;
    return ret;
}

Instruction Instruction::generateStoreWord(uint32_t src,uint32_t base,int32_t offset){
    Instruction ret;
    ret.ppci.d.opc = opcode_Stw;
    ret.ppci.d.rt_s_bf = src;
    ret.ppci.d.ra = base;
    ASSERT(!(offset & 0x3));
    ASSERT(isInLoadOffsetInsnRange(offset));
    ret.ppci.d.d_ui_si = offset;
    return ret;
}
Instruction Instruction::generateLoadWordIndx(uint32_t tgt,uint32_t base1,uint32_t base2){
    Instruction ret;
    ret.ppci.x.opc = opcode_Lwzx;
    ret.ppci.x.xo = xcode_Lwzx;
    ret.ppci.x.rt_s_bf = tgt;
    ret.ppci.x.ra_sr = base1;
    ret.ppci.x.rb_sh = base2;
    ret.ppci.x.rc = 0;
    return ret;
}
Instruction Instruction::generateStoreWordIndx(uint32_t src,uint32_t base1,uint32_t base2){
    Instruction ret;
    ret.ppci.x.opc = opcode_Stwx;
    ret.ppci.x.xo = xcode_Stwx;
    ret.ppci.x.rt_s_bf = src;
    ret.ppci.x.ra_sr = base1;
    ret.ppci.x.rb_sh = base2;
    ret.ppci.x.rc = 0;
    return ret;
}

Instruction Instruction::generateLoadDoubleIndx(uint32_t tgt,uint32_t base1,uint32_t base2){
    Instruction ret;
    ret.ppci.x.opc = opcode_Ldx;
    ret.ppci.x.xo = xcode_Ldx;
    ret.ppci.x.rt_s_bf = tgt;
    ret.ppci.x.ra_sr = base1;
    ret.ppci.x.rb_sh = base2;
    ret.ppci.x.rc = 0;
    return ret;
}
Instruction Instruction::generateStoreDoubleIndx(uint32_t src,uint32_t base1,uint32_t base2){
    Instruction ret;
    ret.ppci.x.opc = opcode_Stdx;
    ret.ppci.x.xo = xcode_Stdx;
    ret.ppci.x.rt_s_bf = src;
    ret.ppci.x.ra_sr = base1;
    ret.ppci.x.rb_sh = base2;
    ret.ppci.x.rc = 0;
    return ret;
}

Instruction Instruction::generateLoadWord(uint32_t tgt,uint32_t base,int32_t offset){
    Instruction ret;
    ret.ppci.d.opc = opcode_Lwz;
    ret.ppci.d.rt_s_bf = tgt;
    ret.ppci.d.ra = base;
    ASSERT(!(offset & 0x3));
    ASSERT(isInLoadOffsetInsnRange(offset));
    ret.ppci.d.d_ui_si = offset;
    return ret;
}
Instruction Instruction::generateMoveToSPR(uint32_t tgt,uint32_t regcode){
    Instruction ret;
    ret.ppci.xfx.opc = opcode_Mtspr;
    ret.ppci.xfx.xo = xcode_Mtspr;
    ret.ppci.xfx.rt_rs = tgt;
    ret.ppci.xfx.spr_tbr = regcode;
    return ret;
}

Instruction Instruction::generateMoveFromSPR(uint32_t src,uint32_t regcode){
    Instruction ret;
    ret.ppci.xfx.opc = opcode_Mfspr;
    ret.ppci.xfx.xo = xcode_Mfspr;
    ret.ppci.xfx.rt_rs = src;
    ret.ppci.xfx.spr_tbr = regcode;
    return ret;
}
Instruction Instruction::generateMoveFromCR(uint32_t tgt){
    Instruction ret;
    ret.ppci.xfx.opc = opcode_Mfcr;
    ret.ppci.xfx.xo = xcode_Mfcr;
    ret.ppci.xfx.rt_rs = tgt;
    ret.ppci.xfx.spr_tbr = 0x0;
    return ret;
}
Instruction Instruction::generateMoveToCR(uint32_t src){
    Instruction ret;
    ret.ppci.xfx.opc = opcode_Mtcrf;
    ret.ppci.xfx.xo = xcode_Mtcrf;
    ret.ppci.xfx.rt_rs = src;
    ret.ppci.xfx.spr_tbr = 0x1fe;
    return ret;
}
Instruction Instruction::generateMoveFromFPSCR(uint32_t tgt){
    Instruction ret;
    ret.ppci.x.opc = opcode_MffsC;
    ret.ppci.x.xo = xcode_MffsC;
    ret.ppci.x.rt_s_bf = tgt;
    ret.ppci.x.rc = 0;
    return ret;
}
Instruction Instruction::generateMoveToFPSCR(uint32_t src){
    Instruction ret;
    ret.ppci.xfl.opc = opcode_MtfsfC;
    ret.ppci.xfl.xo = xcode_MtfsfC;
    ret.ppci.xfl.frb = src;
    ret.ppci.xfl.flm = 0xff;
    ret.ppci.xfl.rc = 0;
    return ret;
}

Instruction Instruction::generateCallToCTR(){
    Instruction ret;
    ret.ppci.xl.opc = opcode_BcctrL;
    ret.ppci.xl.xo = xcode_BcctrL;
    ret.ppci.xl.lk = 1;
    ret.ppci.xl.bt_bo = 0x14; 
    ret.ppci.xl.ba_bi = 0x0;
    ret.ppci.xl.bb = 0x0;
    return ret;
}

Instruction Instruction::generateReturnToLnk(){
    Instruction ret;
    ret.ppci.xl.opc = opcode_BclrL;
    ret.ppci.xl.xo = xcode_BclrL;
    ret.ppci.xl.lk = 0;
    ret.ppci.xl.bt_bo = 0x14; 
    ret.ppci.xl.ba_bi = 0x0;
    ret.ppci.xl.bb = 0x0;
    return ret;
}

Instruction Instruction::generateCallToImmediate(uint64_t from,uint64_t to){
    Instruction ret = generateJumpInsn(from,to);
    ret.ppci.i.lk = 1;
    return ret;
}

Instruction Instruction::generateMoveReg(uint32_t from,uint32_t to){
    Instruction ret;
    ret.ppci.x.opc = opcode_OrC;
    ret.ppci.x.xo = xcode_OrC;
    ret.ppci.x.rt_s_bf = from;
    ret.ppci.x.ra_sr = to;
    ret.ppci.x.rb_sh = from;
    ret.ppci.x.rc = 0;
    return ret;
}

Instruction Instruction::generateSPIncrementWord(int32_t offset){
    Instruction ret;
    ret.ppci.d.opc = opcode_Stwu;
    ret.ppci.d.rt_s_bf = REG_SP;
    ret.ppci.d.ra = REG_SP;
    ASSERT(!(offset & 0x3));
    ASSERT(isInLoadOffsetInsnRange(offset));
    ret.ppci.d.d_ui_si = offset;
    return ret;
}
Instruction Instruction::generateSPIncrementDouble(int32_t offset){
    Instruction ret;
    ret.ppci.ds.opc = opcode_Stdu;
    ret.ppci.ds.xo = xcode_Stdu;
    ret.ppci.ds.rt_s = REG_SP;
    ret.ppci.ds.ra = REG_SP;
    ASSERT(!(offset & 0x3));
    ASSERT(isInLoadOffsetInsnRange(offset));
    offset >>= 2;
    ret.ppci.ds.ds = offset;
    return ret;
}

Instruction Instruction::generateCompare(uint32_t reg1,uint32_t reg2,uint32_t field){
    Instruction ret;
    ret.ppci.x.opc = opcode_Cmpl;
    ret.ppci.x.xo = xcode_Cmpl;
    ret.ppci.x.rt_s_bf = field << 2;
    ret.ppci.x.ra_sr = reg1;
    ret.ppci.x.rb_sh = reg2;
    return ret;
}

Instruction Instruction::generateCondBranch(uint32_t field,uint32_t op,uint32_t tf,int32_t offset){
    Instruction ret;
    ret.ppci.b.opc = opcode_BcLA;
    ret.ppci.b.bo = (0x7 | (tf << 3));
    ret.ppci.b.bi = (field << 2) + op;
    ASSERT(!(offset & 0x3));
    ASSERT(isInLoadOffsetInsnRange(offset));
    offset >>= 2;
    ret.ppci.b.bd = offset;
    ret.ppci.b.aa = 0x0;
    ret.ppci.b.lk = 0x0;
    return ret;
}

bool Instruction::isMemoryDFormFloat(){
    return ((ppci.opc.opc >= opcode_Lfs) && 
            (ppci.opc.opc <= opcode_Stfdu));
}
