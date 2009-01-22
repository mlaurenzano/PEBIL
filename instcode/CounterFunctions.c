#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <errno.h>
#include <limits.h>
#include <ucontext.h>

#define NOINST_VALUE 0xffffffff

#define USERDEF_TRAP SIGTRAP
#define HASHMAP_SIZE_RATIO 16
//#define DEBUG_TRAP

struct block_map {
    int64_t source_addr;
    int64_t tramp_addr;
    int32_t seq_idx;
    int32_t reserved;
};
struct block_map* inst_mapping_table;
int32_t hash_entries;

int32_t instbp_hash(int64_t addr){
    return (int32_t)addr;
}

void x86inst_trap_handler(int signum, siginfo_t* info, void *uctx)
{
    ucontext_t *uc = uctx;
    int32_t *eip = &uc->uc_mcontext.gregs[REG_EIP];
    (*eip)--;
#ifdef DEBUG_TRAP
    int32_t *esp = &uc->uc_mcontext.gregs[REG_ESP];
    fprintf(stderr, "Calling signal handler x86inst_trap_handler from breakpoint at 0x%08x\n", *eip);
    fprintf(stderr, "\tsigno\t %d\n", info->si_signo);
    fprintf(stderr, "\tcode\t %d\n", info->si_code);
#endif
    int32_t hash_id = instbp_hash(*eip) % hash_entries;
    while (inst_mapping_table[hash_id].source_addr != *eip){
        hash_id = (hash_id + 1) % hash_entries;
#ifdef DEBUG_TRAP
        if (inst_mapping_table[hash_id].source_addr == 0){
            fprintf(stderr, "serious error in instrumentation lib: a trap was found whose source address is not found in the instrumentations mapping table, exiting!\n");
            exit(-1);
        }
#endif
    }
#ifdef DEBUG_TRAP
    fprintf(stderr, "hash_id for point %08x is %d\n", *eip, hash_id);
    fprintf(stderr, "stack ptr is %08x\n", *esp);
#endif

    (*eip) = inst_mapping_table[hash_id].tramp_addr;
#ifdef DEBUG_TRAP
    fprintf(stderr, "setting pc to %x\n", *eip);
#endif
}

void register_trap_handler(void* arg1, void* arg2, void* arg3, void* arg4){
    int32_t numBlocks = *(int32_t*)arg1;
    int64_t* blockAddrs = (int64_t*)arg2;
    int32_t numTramps = *(int32_t*)arg3;
    int64_t* trampMap = (int64_t*)arg4;
    int32_t i;

    struct sigaction new_act, old_act;
    sigset_t sset;
    sigemptyset(&sset);
    new_act.sa_handler = x86inst_trap_handler;
    new_act.sa_flags = 0 | SA_SIGINFO;
    new_act.sa_mask = sset;
    fprintf(stdout, "Registering signal handler: %x %x %x %x\n", arg1, arg2, arg3, arg4);
    if (sigaction(USERDEF_TRAP, &new_act, &old_act)){
        fprintf(stderr, "Cannot set up signal handler for instrumentation, killing program\n");
        exit(-1);
    }
    fprintf(stdout, "Registered handler for signal %d\n", USERDEF_TRAP);

    // setting up hashtable that maps block address to trampoline address
    fprintf(stdout, "nblocks: %d\tblockarray: %x\tntramps: %d\ttramparray: %x\n", numBlocks, blockAddrs, numTramps, trampMap);
    hash_entries = numBlocks*HASHMAP_SIZE_RATIO;
    int32_t collisions = 0;
    int64_t blockAddr;
    int64_t trampAddr;
    inst_mapping_table = malloc(sizeof(struct block_map)*hash_entries);
    bzero(inst_mapping_table, sizeof(struct block_map)*numBlocks*HASHMAP_SIZE_RATIO);
    for (i = 0; i < numTramps; i++){
        blockAddr = trampMap[(i*2)];
        trampAddr = trampMap[(i*2)+1];
        int32_t hash_id = instbp_hash(blockAddr) % hash_entries;
        while (inst_mapping_table[hash_id].source_addr != 0){
            hash_id = (hash_id + 1) % hash_entries;
            collisions++;
        }
        inst_mapping_table[hash_id].source_addr = blockAddr;
        inst_mapping_table[hash_id].tramp_addr = trampAddr;
        inst_mapping_table[hash_id].seq_idx = i;
        //        fprintf(stdout, "setting hash table for tramp %d/%d at address %#llx -> %#llx -- hash idx %d\n", i, numTramps, blockAddr, trampAddr, hash_id);
    }
    fprintf(stdout, "collision rate of hash table: %d collisions for %d blocks\n", collisions, numBlocks);
#ifdef DEBUG_TRAP
    for (i = 0; i < hash_entries; i++){
        fprintf(stdout, "%d:\t%llx %llx %d %d\n", i, inst_mapping_table[i].source_addr, inst_mapping_table[i].tramp_addr, inst_mapping_table[i].seq_idx, inst_mapping_table[i].reserved);
    }
    fflush(stdout);
#endif
}



int32_t functioncounter(void* arg1, void* arg2, void* arg3){
    int32_t i;
    int32_t numFunctions = *(int32_t*)arg1;
    int32_t* functionCounts = (int32_t*)(arg2);
    char** functionNames = *(char**)arg3;

    fprintf(stdout, "\n*** Instrumentation Summary ****\n");
    fprintf(stdout, "Raw instrumentation function arguments: %x %x %x\n", arg1, arg2, arg3);
    fprintf(stdout, "There are %d functions in the code:\n", numFunctions);

    for (i = 0; i < numFunctions; i++){
        fprintf(stdout, "\tFunction(%d) -- %x -- %s -- executed %d times\n", i, functionNames[i], functionNames[i], functionCounts[i]);
    }

    fflush(stdout);
    return 0;
}

int32_t blockcounter(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5){
    int32_t i;
    int32_t excluded = 0;

    // interpret the arguments
    int32_t numBlocks = *(int32_t*)arg1;
    int32_t* functionCounts = (int32_t*)(arg2);
    int64_t* blockAddrs = (int64_t*)(arg3);
    int32_t* lineNumbers = (int32_t*)(arg4);
    char** fileNames = (char**)arg5;
    
    fprintf(stdout, "\n*** Instrumentation Summary ****\n");
    fprintf(stdout, "Raw instrumentation function arguments: %x %x %x %x %x\n", arg1, arg2, arg3, arg4, arg5);
    fprintf(stdout, "There are %d basic blocks in the code:\n", numBlocks);

    fprintf(stdout, "Index\t\tAddress\t\tLine\t\tCounter\n");
    for (i = 0; i < numBlocks; i++){
        fprintf(stdout, "%d\t0x%016llx\t%s:%d\t%d\n", i, blockAddrs[i], fileNames[i], lineNumbers[i], functionCounts[i]);
        if (functionCounts[i] == NOINST_VALUE){
            excluded++;
        }
    }
    fprintf(stdout, "NOTE -- %d blocks weren't instrumented, these are denoted by a counter value of %d (0x%x)\n\n", excluded, NOINST_VALUE, NOINST_VALUE);

    fflush(stdout);
    return 0;
}


