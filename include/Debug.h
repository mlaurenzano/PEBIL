#ifndef _Debug_h_
#define _Debug_h_

#include <iostream>

// debugging macros -- these can produce copious amounts of output
#define WARNING_SEVERITY 7

//#define DEVELOPMENT
//#define DEBUG_MEMTRACK
//#define DEBUG_OPERAND
//#define DEBUG_OPTARGET
//#define DEBUG_OPCODE
//#define DEBUG_HASH
//#define DEBUG_NOTE
//#define DEBUG_LINEINFO
//#define DEBUG_BASICBLOCK
//#define DEBUG_HASHCODE
//#define DEBUG_CFG
//#define DEBUG_LOOP
//#define DEBUG_INST
//#define DEBUG_ANCHOR
//#define DEBUG_FUNC_RELOC
//#define DEBUG_JUMP_TABLE
//#define DEBUG_POINT_CHAIN
//#define DEBUG_LEAF_OPT
//#define DEBUG_DATA_PLACEMENT
//#define DEBUG_ADDR_ALIGN
//#define DEBUG_BLOAT_FILTER
//#define DEBUG_LOADADDR
//#define DEBUG_LIVE_REGS

// some common macros to help debug instrumentation
//#define RELOC_MOD_OFF 0
//#define RELOC_MOD 2
//#define TURNOFF_FUNCTION_RELOCATION
//#define BLOAT_MOD_OFF 3
//#define BLOAT_MOD     2
//#define TURNOFF_FUNCTION_BLOAT
//#define SWAP_MOD_OFF 0
//#define SWAP_MOD     2
//#define SWAP_FUNCTION_ONLY "raise"
//#define TURNOFF_INSTRUCTION_SWAP
#define ANCHOR_SEARCH_BINARY
//#define PRINT_INSTRUCTION_DETAIL 
#define OPTIMIZE_NONLEAF
//#define VALIDATE_ANCHOR_SEARCH
//#define FILL_RELOCATED_WITH_INTERRUPTS
//#define JUMPTABLE_USE_REGISTER_OPS
//#define THREAD_SAFE
//#define NO_REG_ANALYSIS

#ifdef WARNING_SEVERITY
#define WARN_FILE stdout
#define PRINT_WARN(__severity,...)  if (__severity >= WARNING_SEVERITY){ \
    fprintf(WARN_FILE,"*** WARNING : ");                            \
    fprintf(WARN_FILE,## __VA_ARGS__);                              \
    fprintf(WARN_FILE,"\n");                                        \
    fflush(WARN_FILE); }
#else
#define PRINT_WARN(...)
#endif

#ifdef DEBUG_MEMTRACK
#include <MemTrack.h>
#define PRINT_DEBUG_MEMTRACK(...) fprintf(stdout,"MEMTRACK : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#define PRINT_MEMTRACK_STATS(...) \
    PRINT_DEBUG_MEMTRACK("-----------------------------------------------------------"); \
    PRINT_DEBUG_MEMTRACK("Memory Stats @ line %d in file %s in function %s", ## __VA_ARGS__); \
    MemTrack::TrackListMemoryUsage((double)1.00); \
    PRINT_DEBUG_MEMTRACK("-----------------------------------------------------------");
#define DEBUG_MEMTRACK(...) __VA_ARGS__
#else
#define PRINT_DEBUG_MEMTRACK(...)
#define PRINT_MEMTRACK_STATS(...)
#define DEBUG_MEMTRACK(...)
#endif

#ifdef DEBUG_OPCODE
#define PRINT_DEBUG_OPCODE(...) fprintf(stdout,"OPCODE : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_OPCODE(...)
#endif

#ifdef DEBUG_OPERAND
#define PRINT_DEBUG_OPERAND(...) fprintf(stdout,"OPERAND : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_OPERAND(...)
#endif

#ifdef DEBUG_OPTARGET
#define PRINT_DEBUG_OPTARGET(...) fprintf(stdout,"OPTARGET : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_OPTARGET(...)
#endif

#ifdef DEBUG_HASH
#define PRINT_DEBUG_HASH(...) fprintf(stdout,"HASH : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#define DEBUG_HASH(...) __VA_ARGS__
#else
#define PRINT_DEBUG_HASH(...)
#define DEBUG_HASH(...)
#endif

#ifdef DEBUG_NOTE
#define PRINT_DEBUG_NOTE(...) fprintf(stdout,"NOTE : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#define DEBUG_NOTE(...) __VA_ARGS__
#else
#define PRINT_DEBUG_NOTE(...)
#define DEBUG_NOTE(...)
#endif

#ifdef DEBUG_LINEINFO
#define PRINT_DEBUG_LINEINFO(...) fprintf(stdout,"LINEINFO : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#define DEBUG_LINEINFO(...) __VA_ARGS__
#else
#define PRINT_DEBUG_LINEINFO(...)
#define DEBUG_LINEINFO(...)
#endif

#ifdef DEBUG_BASICBLOCK
#define PRINT_DEBUG_BASICBLOCK(...) fprintf(stdout,"BASICBLOCK : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_BASICBLOCK(...)
#endif

#ifdef DEBUG_HASHCODE
#define PRINT_DEBUG_HASHCODE(...) fprintf(stdout,"HASHCODE : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#define DEBUG_HASHCODE(...) __VA_ARGS__
#else
#define PRINT_DEBUG_HASHCODE(...)
#define DEBUG_HASHCODE(...)
#endif

#ifdef DEBUG_CFG
#define PRINT_DEBUG_CFG(...) fprintf(stdout,"CFG : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#define DEBUG_CFG(...) __VA_ARGS__
#else
#define PRINT_DEBUG_CFG(...)
#define DEBUG_CFG(...)
#endif

#ifdef DEBUG_LOOP
#define PRINT_DEBUG_LOOP(...) fprintf(stdout,"LOOP : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#define DEBUG_LOOP(...) __VA_ARGS__
#else
#define PRINT_DEBUG_LOOP(...)
#define DEBUG_LOOP(...)
#endif

#ifdef DEBUG_INST
#define PRINT_DEBUG_INST(...) fprintf(stdout,"INST : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#define DEBUG_INST(...) __VA_ARGS__
#else
#define PRINT_DEBUG_INST(...)
#define DEBUG_INST(...)
#endif

#ifdef DEBUG_ANCHOR
#define PRINT_DEBUG_ANCHOR(...) fprintf(stdout,"ANCHOR : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#define DEBUG_ANCHOR(...) __VA_ARGS__
#else
#define PRINT_DEBUG_ANCHOR(...)
#define DEBUG_ANCHOR(...)
#endif

#ifdef DEBUG_FUNC_RELOC
#define PRINT_DEBUG_FUNC_RELOC(...) fprintf(stdout,"FUNC_RELOC : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#define DEBUG_FUNC_RELOC(...) __VA_ARGS__
#else
#define PRINT_DEBUG_FUNC_RELOC(...)
#define DEBUG_FUNC_RELOC(...)
#endif

#ifdef DEBUG_JUMP_TABLE
#define PRINT_DEBUG_JUMP_TABLE(...) fprintf(stdout,"JUMP_TABLE : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_JUMP_TABLE(...)
#endif

#ifdef DEBUG_POINT_CHAIN
#define PRINT_DEBUG_POINT_CHAIN(...) fprintf(stdout,"POINT_CHAIN : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_POINT_CHAIN(...)
#endif

#ifdef DEBUG_LEAF_OPT
#define PRINT_DEBUG_LEAF_OPT(...) fprintf(stdout,"LEAF_OPT : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_LEAF_OPT(...)
#endif

#ifdef DEBUG_DATA_PLACEMENT
#define PRINT_DEBUG_DATA_PLACEMENT(...) fprintf(stdout,"DATA_PLACEMENT : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_DATA_PLACEMENT(...)
#endif

#ifdef DEBUG_ADDR_ALIGN
#define PRINT_DEBUG_ADDR_ALIGN(...) fprintf(stdout,"ADDR_ALIGN : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_ADDR_ALIGN(...)
#endif

#ifdef DEBUG_BLOAT_FILTER
#define PRINT_DEBUG_BLOAT_FILTER(...) fprintf(stdout,"BLOAT_FILTER : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#define DEBUG_BLOAT_FILTER(...) __VA_ARGS__
#else
#define PRINT_DEBUG_BLOAT_FILTER(...)
#define DEBUG_BLOAT_FILTER(...)
#endif


#ifdef DEBUG_LOADADDR
#define PRINT_DEBUG_LOADADDR(...) fprintf(stdout,"LOADADDR : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#define DEBUG_LOADADDR(...) __VA_ARGS__
#else
#define PRINT_DEBUG_LOADADDR(...)
#define DEBUG_LOADADDR(...)
#endif

#define PRINT_REG_LIST_BASIS(__list, __elts, __i)         \
    PRINT_INFO(); \
    PRINT_OUT("instruction %d %s list: ", __i, #__list);      \
    for (uint32_t __j = 0; __j < __elts; __j++){\
    if (__list[__i]->contains(__j)){\
    PRINT_OUT("reg:%d ", __j);\
    }\
    }\
    PRINT_OUT("\n");

#ifdef DEBUG_LIVE_REGS
#define PRINT_DEBUG_LIVE_REGS(...) fprintf(stdout,"LIVE_REGS : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#define PRINT_REG_LIST_R PRINT_REG_LIST_BASIS
#define PRINT_REG_LIST PRINT_REG_LIST_BASIS
#define DEBUG_LIVE_REGS(...) __VA_ARGS__
#else
#define PRINT_DEBUG_LIVE_REGS(...)
#define DEBUG_LIVE_REGS(...)
#define PRINT_REG_LIST(...)
#define PRINT_REG_LIST_R PRINT_REG_LIST_BASIS
#endif


#ifdef  DEVELOPMENT

#define PRINT_DEBUG(...) fprintf(stdout,"----------- DEBUG : "); \
                         fprintf(stdout,## __VA_ARGS__); \
                         fprintf(stdout,"\n"); \
                         fflush(stdout);


#define ASSERT(__str) assert(__str);
#define DEBUG(...) __VA_ARGS__
#define DEBUG_MORE(...)
#define TIMER(...) __VA_ARGS__
#define INNER_TIMER(...) __VA_ARGS__

#else

#define PRINT_DEBUG(...)
#define DEBUG(...)
#define DEBUG_MORE(...)
#define ASSERT(__str) assert(__str);
#define TIMER(...) __VA_ARGS__
#define INNER_TIMER(...) 

#endif // DEVELOPMENT

#endif // _Debug_h_
