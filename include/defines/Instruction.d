/*
##################################################################
udis86 ud_operand
  Type	                Name
  enum ud_type          type;
  uint8_t               size;
  union {
        int8_t          sbyte;
        uint8_t         ubyte;
        int16_t         sword;
        uint16_t        uword;
        int32_t         sdword;
        uint32_t        udword;
        int64_t         sqword;
        uint64_t        uqword;

        struct {
                uint16_t seg;
                uint32_t off;
        } ptr;
  } lval;
  enum ud_type          base;
  enum ud_type          index;
  uint8_t               offset;
  uint8_t               scale;

##################################################################
*/

#define OPERAND_MACROS_CLASS(__str) /** __str **/ \
    GET_FIELD_CLASS(ud_type,type); \
    GET_FIELD_CLASS(uint8_t,size); \
    GET_FIELD_CLASS(uint8_t,position); \
    GET_FIELD_CLASS_A(int8_t,sbyte,lval); \
    GET_FIELD_CLASS_A(uint8_t,ubyte,lval); \
    GET_FIELD_CLASS_A(int16_t,sword,lval); \
    GET_FIELD_CLASS_A(uint16_t,uword,lval); \
    GET_FIELD_CLASS_A(int32_t,sdword,lval); \
    GET_FIELD_CLASS_A(uint32_t,udword,lval); \
    GET_FIELD_CLASS_A(int64_t,sqword,lval); \
    GET_FIELD_CLASS_A(uint64_t,uqword,lval); \
    GET_FIELD_CLASS(ud_type,base); \
    GET_FIELD_CLASS(ud_type,index); \
    GET_FIELD_CLASS(uint8_t,offset); \
    GET_FIELD_CLASS(uint8_t,scale);


/* we don't give all fields macro access
##################################################################
udis86 ud
  Type	                Name
  int                   (*inp_hook) (struct ud*);
  uint8_t               inp_curr;
  uint8_t               inp_fill;
  FILE*                 inp_file;
  uint8_t               inp_ctr;
  uint8_t*              inp_buff;
  uint8_t*              inp_buff_end;
  uint8_t               inp_end;
  void                  (*translator)(struct ud*);
  uint64_t              insn_offset;
  char                  insn_hexcode[32];
  char                  insn_buffer[64];
  unsigned int          insn_fill;
  uint8_t               dis_mode;
  uint64_t              pc;
  uint8_t               vendor;
  struct map_entry*     mapen;
  enum ud_mnemonic_code mnemonic;
  struct ud_operand     operand[3];
  uint8_t               error;
  uint8_t               pfx_rex;
  uint8_t               pfx_seg;
  uint8_t               pfx_opr;
  uint8_t               pfx_adr;
  uint8_t               pfx_lock;
  uint8_t               pfx_rep;
  uint8_t               pfx_repe;
  uint8_t               pfx_repne;
  uint8_t               pfx_insn;
  uint8_t               default64;
  uint8_t               opr_mode;
  uint8_t               adr_mode;
  uint8_t               br_far;
  uint8_t               br_near;
  uint8_t               implicit_addr;
  uint8_t               c1;
  uint8_t               c2;
  uint8_t               c3;
  uint8_t               inp_cache[256];
  uint8_t               inp_sess[64];
  struct ud_itab_entry * itab_entry;
##################################################################
*/

#define INSTRUCTION_MACROS_CLASS(__str) /** __str **/ \
    GET_FIELD_CLASS(uint64_t,insn_offset); \
    GET_FIELD_CLASS(char*,insn_hexcode); \
    GET_FIELD_CLASS(char*,insn_buffer); \
    GET_FIELD_CLASS(unsigned int,insn_fill); \
    GET_FIELD_CLASS(uint8_t,dis_mode); \
    GET_FIELD_CLASS(uint64_t,pc); \
    GET_FIELD_CLASS(uint8_t,vendor); \
    GET_FIELD_CLASS(enum ud_mnemonic_code,mnemonic); \
    GET_FIELD_CLASS(struct ud_operand*,operand); \
    GET_FIELD_CLASS(uint8_t,error); \
    GET_FIELD_CLASS(uint8_t,pfx_rex); \
    GET_FIELD_CLASS(uint8_t,pfx_seg); \
    GET_FIELD_CLASS(uint8_t,pfx_opr); \
    GET_FIELD_CLASS(uint8_t,pfx_adr); \
    GET_FIELD_CLASS(uint8_t,pfx_lock); \
    GET_FIELD_CLASS(uint8_t,pfx_rep); \
    GET_FIELD_CLASS(uint8_t,pfx_repe); \
    GET_FIELD_CLASS(uint8_t,pfx_repne); \
    GET_FIELD_CLASS(uint8_t,pfx_insn); \
    GET_FIELD_CLASS(uint8_t,default64); \
    GET_FIELD_CLASS(uint8_t,opr_mode); \
    GET_FIELD_CLASS(uint8_t,adr_mode); \
    GET_FIELD_CLASS(uint8_t,br_far); \
    GET_FIELD_CLASS(uint8_t,br_near); \
    GET_FIELD_CLASS(uint8_t,implicit_addr); \
    GET_FIELD_CLASS(uint8_t,c1); \
    GET_FIELD_CLASS(uint8_t,c2); \
    GET_FIELD_CLASS(uint8_t,c3); \
    GET_FIELD_CLASS(struct ud_itab_entry *,itab_entry); \
        \
    SET_FIELD_CLASS(uint64_t,insn_offset); 

