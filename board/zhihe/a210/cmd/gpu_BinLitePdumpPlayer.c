#ifndef BINLITE_CPU
#include <malloc.h>
#include <stdio.h>
#include <command.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "../include/utils/io.h"

/*---------------------------------------------------------*/
#include <string.h>

#define BINLITE_SCRIPT_ADDR 0x80200000
#define BINLITE_SAB_ADDR    0x90000000

#define PDL_SUCCESS()                                                                                                  \
    do {                                                                                                               \
    } while (0)
#define PDL_INIT()                                                                                                     \
    do {                                                                                                               \
    } while (0)

#define VERBOSE_OUTPUT(_cmd, _command_script)                                                                          \
    do {                                                                                                               \
        printf("%s:%d \033[031m%s\033[0m: 0x%x\n", __FUNCTION__, __LINE__, #_cmd, (uint32_t)(uintptr_t)(_command_script));  \
    } while (0)

#define ERROR_OUTPUT(err_string)                                                                                       \
    do {                                                                                                               \
        printf("%s", err_string);                                                                                      \
    } while (0)

#define PDL_ASSERT(expression)                                                                                         \
    do {                                                                                                               \
        if (!(expression)) {                                                                                           \
            printf("\033[031m%s():%d\033[0m\n", __func__, __LINE__);                                                   \
        }                                                                                                              \
    } while (0)

#define WriteReg(addr, value)                                                                                          \
    do {                                                                                                               \
        printf("%s:%d\033[033m WriteReg\033[0m: : addr:0x%x value:0x%x\n",__FUNCTION__,__LINE__, (uint32_t)(uintptr_t)(addr), value);           \
        /*wr(addr, value);*/                                                                                               \
         *(volatile uint32_t*)(uintptr_t)(addr) = value; \
    } while (0)
#define WriteReg64(addr, value)                                                                                        \
    do {                                                                                                               \
        printf("%s:%d\033[033m WriteReg64\033[0m: : addr:0x%x value:0x%llx\n",__FUNCTION__,__LINE__, (uint32_t)(uintptr_t)(addr), value);           \
        /*wr(addr, value);*/ \
        *(volatile uint32_t*)(uintptr_t)(addr) = (uint32_t)value; \
        *(volatile uint32_t*)((uintptr_t)(addr)+4) = (value >> 32); \
    } while (0)
#define ReadReg(addr, pValue)                                                                                          \
    do {                                                                                                               \
        /* *pValue = rd(addr);  */                                                                                           \
        *(uint32_t*)pValue = (*(volatile uint32_t*)(uintptr_t)(addr));    \
        printf("%s:%d\033[033m ReadReg\033[0m: : addr:0x%x value:0x%x\n",__FUNCTION__,__LINE__, (uint32_t)(uintptr_t)(addr), (uint32_t)*pValue);\
    } while (0)
#define ReadReg64(addr, pValue)                                                                                        \
    do {                                                                                                               \
        /* *pValue = rd(addr); */                                                                                           \
        *(uint32_t*)pValue = (*(volatile uint32_t*)(uintptr_t)(addr)); \
        *(((uint32_t*)pValue)+1) = (*((volatile uint32_t*)(uintptr_t)(addr)+1)); \
        printf("%s:%d\033[033m ReadReg64\033[0m: : addr:0x%x value:0x%x\n",__FUNCTION__,__LINE__, (uint32_t)(uintptr_t)(addr), (uint32_t)*pValue);\
    } while (0)
#define WriteMem(addr, value)                                                                                          \
    do {                                                                                                               \
        printf("%s:%d\033[033m WriteMem\033[0m: : addr:0x%x value:0x%x\n",__FUNCTION__,__LINE__, (uint32_t)(uintptr_t)(addr), value);           \
        /*wr(addr, value);   */                                                                                            \
        *(volatile uint32_t*)(uintptr_t)(addr) = value;\
        flush_and_invalid_cache(GPU_CACHE_OP_CLEAN, addr, addr);\
    } while (0)
#define WriteMem64(addr, value)                                                                                      \
    do {                                                                                                               \
        printf("%s:%d\033[033m WriteMem64\033[0m: : addr:0x%x value:0x%llx\n",__FUNCTION__,__LINE__, (uint32_t)(uintptr_t)(addr), value);         \
        /* wr(addr, value);  */                                                                                            \
        *(volatile uint64_t*)(uintptr_t)(addr) = value;         \
        flush_and_invalid_cache(GPU_CACHE_OP_CLEAN, addr, addr);\
    } while (0)
#define ReadMem(addr, pValue)                                                                                          \
    do {                                                                                                               \
        flush_and_invalid_cache(GPU_CACHE_OP_CLEAN, addr, addr);\
        *pValue = rd(addr);                                                                                             \
        /* *(volatile uint32_t*)(uintptr_t)(pValue) = *(volatile uint32_t*)(uintptr_t)(addr); */ \
        printf("%s:%d\033[033m ReadMem\033[0m: : addr:0x%x value:0x%x\n",__FUNCTION__,__LINE__, (uint32_t)(uintptr_t)(addr), *(uint32_t *)pValue);\
    } while (0)
#define ReadMem64(addr, pValue)                                                                                      \
    do {                                                                                                               \
        flush_and_invalid_cache(GPU_CACHE_OP_CLEAN, addr, addr);\
        *pValue = rd(addr);                                                                                           \
        /* *(volatile uint64_t*)(uintptr_t)(pValue) = *(volatile uint64_t*)(uintptr_t)(addr);  */ \
        printf("%s:%d\033[033m ReadMem64\033[0m: : addr:0x%x value:0x%x\n",__FUNCTION__,__LINE__, (uint32_t)(uintptr_t)(addr), (uint32_t)*pValue);\
    } while (0)

#define CopyMem(_to, _from, size) \
    do{ \
            memcpy((uint8_t *)_to, (uint8_t *)_from, size);\
            printf("%s:%d\033[033m CopyMem\033[0m: : addr:0x%x <-:0x%x size:%d\n",__FUNCTION__,__LINE__, (uint32_t)(uintptr_t)(_to), (uint32_t)(uintptr_t)(_from), size); \
            flush_and_invalid_cache(GPU_CACHE_OP_CLEAN, _to, (unsigned long)((uint8_t *)(_to)+size));\
    }while(0)

#define SaveMem(_to, _from, _count)                                                                                  \
    do {                                                                                                               \
            printf("%s:%d\033[033m SaveMem\033[0m: : addr:0x%x ->:0x%x\n",__FUNCTION__,__LINE__, (uint32_t)(uintptr_t)(_from), (uint32_t)(uintptr_t)(_to)); \
            memcpy((uint8_t *)_to, (uint8_t *)_from, _count);\
    } while (0)
#define FillMem(addr, byte_data, size) \
    do { \
            memset((uint8_t*)(addr), byte_data, size);     \
            printf("%s:%d\033[033m FillMem\033[0m: : addr:0x%x <-:%c size:%d\n",__FUNCTION__,__LINE__, (uint32_t)(uintptr_t)(addr), (byte_data), size); \
    }while(0)
#define Delay(ui32Delay) mdelay(ui32Delay)

typedef uint32_t GPU_CACHE_OP;             /*!< Type represents cache maintenance operation */
#define GPU_CACHE_OP_CLEAN               0x1U    /*!< Flush w/o invalidate */
#define GPU_CACHE_OP_INVALIDATE          0x2U    /*!< Invalidate w/o flush */
#define GPU_CACHE_OP_FLUSH               0x3U    /*!< Flush w/ invalidate */

#define CONFIG_SYS_CACHELINE_SIZE   64
#define sync_is()   asm volatile (".long 0x01b0000b")

static void clean_dcache_range(unsigned long start, unsigned long end)
{
    printf("\033[031m%s():%d cache -> ddr\033[0m\n", __func__, __LINE__);

    register unsigned long i asm("a0") = start & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
    for (; i < end; i += CONFIG_SYS_CACHELINE_SIZE){
        asm volatile(".long 0x02b5000b");  /* dcache.cipa a0 */
    }

    sync_is();
}

static void invalidate_dcache_range(unsigned long start, unsigned long end)
{
    int count = 0;
    unsigned long ii = 0;
    printf("%s():%d ddr -> cache; start:0x%lx end:0x%lx\n", __FUNCTION__,__LINE__, start, end);
    
    register unsigned long i asm("a0") = start & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
    asm volatile(".long 0x02a5000b"); 
    ii = i;
    count++;

    i = i+ CONFIG_SYS_CACHELINE_SIZE;

    for (; i <= end; i += CONFIG_SYS_CACHELINE_SIZE){
        asm volatile(".long 0x02a5000b");  /* dcache.ipa a0 */
        count++;
        ii = i;
    }

    sync_is();    

    printf("\033[031m%s():%d count:%d ii:0x%lx\033[0m\n", __func__, __LINE__, count, ii);

}

static void flush_and_invalid_cache(GPU_CACHE_OP eRequestType,
                                    unsigned long start,
                                    unsigned long end)
{
    switch (eRequestType) {
        case GPU_CACHE_OP_CLEAN:
            clean_dcache_range(start, end);
            break;

        case GPU_CACHE_OP_INVALIDATE:
            invalidate_dcache_range(start, end);
            break;

        case GPU_CACHE_OP_FLUSH:
            clean_dcache_range(start, end);            
            break;
    }
}

/*---------------------------------------------------------*/
// This header contains the implementation of key transactions.

#define IMG_TRUE (1)
#define IMG_FALSE (0)
#define IMG_BOOL int

#define PDL_SIGNATURE1 (0x504D4450)
#define PDL_SIGNATURE2 (0x504F5453)
#define PDL_SIGNATURE_MASK (0xF0FFFFFF)
#define PDL_SIGNATURE_COMPSCRIPT_HSTART (0x41545348)
#define PDL_SIGNATURE_COMPSCRIPT_HEND (0x444E4548)

#define PDL_CHECK_FUNC_EQ (0)
#define PDL_CHECK_FUNC_LT (1)
#define PDL_CHECK_FUNC_LE (2)
#define PDL_CHECK_FUNC_GT (3)
#define PDL_CHECK_FUNC_GE (4)
#define PDL_CHECK_FUNC_NE (5)
#define PDL_POLLCOUNT_INFINITE (0xFFFFFFFF)

#define NUMBER_OF_INTERNAL_REGS (64)

#define PDL_SCRIPT_NO psScriptCB->pui32Script[1]

#if !defined(TAL_PDUMPL_NO_OPTIMISE)
// Flag which tells the optimiser not to optimise this command
#define TAL_PDUMPL_NO_OPTIMISE (0x80000000)
#endif /* TAL_PDUMPL_NO_OPTIMISE */

#if !defined(ADDR64)
#define ADDR64(_addr32) (uint64_t)(_addr32 + (((uint64_t)(*(((uint32_t*)(&_addr32)) + 1))) << 32))
#endif /* ADDR64 */

#if !defined(DATA64)
#define DATA64(_data32) (uint64_t)(_data32 + (((uint64_t)(*(((uint32_t*)(&_data32)) + 1))) << 32))
#endif /* DATA64 */

#define MAX_PDUMPLTESTS (8)
#define MAX_PDUMPLSCRIPTS (32)
#define MAX_SEMAPHORES (64)
#define MAX_SYNC_IDS (32) //(64)
#define NO_SYSSYNC_IDS (64)

typedef struct PDL_tag_sTestCB {
    uint32_t ui32NumScripts;
    int32_t ai32Semaphores[MAX_SEMAPHORES];
    uint32_t aui32SystemSyncId[NO_SYSSYNC_IDS];
} PDL_sTestCB, *PDL_psTestCB;
/* Typedef etc */

typedef struct PDL_tag_sScriptCB {
    uint32_t ui32ScriptId; /*<! Script Identifier */
    uint32_t ui32TestId; /*<! Test Identifier */

    uint32_t* pui32ScriptBase; /*<! The base of the script */
    uint32_t* pui32Script; /*<! Points to the current position in the script */
    IMG_BOOL bEOF; /*<! Indicates whether the end of script has been reached */

    uint32_t ui32FileID; /*<! File ID in the last command */
    uint32_t ui32LineNum; /*<! Line number in the last command */

    /* Synchronisation information */
    uint32_t ui32DisabledSyncs; /*<! Sync Enable info */
    IMG_BOOL bWaiting; /*<! Is the script waiting for sync?        */
    uint32_t ui32SyncWaiting; /*<! The number of the sync the script is waiting for */
    uint32_t ui32SemaWaiting; /*<! The number of the Semaphore the script is
                               waiting for */

    /* POLL Information */
    IMG_BOOL bPollPending; /*<! Is POLL command pending for this CB?   */
    uint32_t ui32LoopCount; /*<! The number of times the POL was executed */
    uint32_t ui32Delay;
    uint32_t ui32TimeCount; /*<! Counting the time between polls        */

    uint32_t ui32SabFileID; /*<! File ID in the last SAB command */
    uint32_t ui32SabLineNum; /*<! Line number in the last SAB command */
    uint32_t ui32SabSize; /*<! Size of current SAB command (cummulates with SABs
                             from same line in pdump2) */
    uint32_t* pui32SabSize; /*<! Pointer to SAB size for current command */

    /* Extra Information */
    void* pvInfo;
} PDL_sScriptCB, *PDL_psScriptCB;

// Script function callback
typedef void (*PDL_pfnScriptFunction)(PDL_psScriptCB psScriptCB);

/******************************************************************************/
/*                                                                            */
/*      Prototypes and look-up table for script processing functions          */
/*                                                                            */
/******************************************************************************/
void PDL_WriteRegister(PDL_psScriptCB psScriptCB);
void PDL_WriteMemory(PDL_psScriptCB psScriptCB);
void PDL_WriteSlavePort(PDL_psScriptCB psScriptCB);

void PDL_ReadRegister(PDL_psScriptCB psScriptCB);
void PDL_ReadMemory(PDL_psScriptCB psScriptCB);

void PDL_PollRegister(PDL_psScriptCB psScriptCB);
void PDL_PollMemory(PDL_psScriptCB psScriptCB);

void PDL_LoadWordsToRegister(PDL_psScriptCB psScriptCB);
void PDL_LoadBytesToMemory(PDL_psScriptCB psScriptCB);
void PDL_LoadWordsToSlavePort(PDL_psScriptCB psScriptCB);
void PDL_FillMem(PDL_psScriptCB psScriptCB);

void PDL_Idle(PDL_psScriptCB psScriptCB);

void PDL_MemCircularBufferPoll(PDL_psScriptCB psScriptCB);

void PDL_WriteIntRegToRegister(PDL_psScriptCB psScriptCB);
void PDL_WriteIntRegToMemory(PDL_psScriptCB psScriptCB);
void PDL_ReadRegisterToIntReg(PDL_psScriptCB psScriptCB);
void PDL_ReadMemoryToIntReg(PDL_psScriptCB psScriptCB);

void PDL_IntRegMOV(PDL_psScriptCB psScriptCB);
void PDL_IntRegAND(PDL_psScriptCB psScriptCB);
void PDL_IntRegOR(PDL_psScriptCB psScriptCB);
void PDL_IntRegXOR(PDL_psScriptCB psScriptCB);
void PDL_IntRegNOT(PDL_psScriptCB psScriptCB);
void PDL_IntRegSHR(PDL_psScriptCB psScriptCB);
void PDL_IntRegSHL(PDL_psScriptCB psScriptCB);
void PDL_IntRegADD(PDL_psScriptCB psScriptCB);
void PDL_IntRegSUB(PDL_psScriptCB psScriptCB);
void PDL_IntRegMUL(PDL_psScriptCB psScriptCB);
void PDL_IntRegIMUL(PDL_psScriptCB psScriptCB);
void PDL_IntRegDIV(PDL_psScriptCB psScriptCB);
void PDL_IntRegIDIV(PDL_psScriptCB psScriptCB);
void PDL_IntRegMOD(PDL_psScriptCB psScriptCB);
void PDL_IntRegEQU(PDL_psScriptCB psScriptCB);
void PDL_IntRegLT(PDL_psScriptCB psScriptCB);
void PDL_IntRegLTE(PDL_psScriptCB psScriptCB);
void PDL_IntRegGT(PDL_psScriptCB psScriptCB);
void PDL_IntRegGTE(PDL_psScriptCB psScriptCB);
void PDL_IntRegNEQ(PDL_psScriptCB psScriptCB);

void PDL_IntRegMOV2(PDL_psScriptCB psScriptCB);
void PDL_IntRegAND2(PDL_psScriptCB psScriptCB);
void PDL_IntRegOR2(PDL_psScriptCB psScriptCB);
void PDL_IntRegXOR2(PDL_psScriptCB psScriptCB);
void PDL_IntRegSHR2(PDL_psScriptCB psScriptCB);
void PDL_IntRegSHL2(PDL_psScriptCB psScriptCB);
void PDL_IntRegADD2(PDL_psScriptCB psScriptCB);
void PDL_IntRegSUB2(PDL_psScriptCB psScriptCB);
void PDL_IntRegMUL2(PDL_psScriptCB psScriptCB);
void PDL_IntRegIMUL2(PDL_psScriptCB psScriptCB);
void PDL_IntRegDIV2(PDL_psScriptCB psScriptCB);
void PDL_IntRegIDIV2(PDL_psScriptCB psScriptCB);
void PDL_IntRegMOD2(PDL_psScriptCB psScriptCB);
void PDL_IntRegEQU2(PDL_psScriptCB psScriptCB);
void PDL_IntRegLT2(PDL_psScriptCB psScriptCB);
void PDL_IntRegLTE2(PDL_psScriptCB psScriptCB);
void PDL_IntRegGT2(PDL_psScriptCB psScriptCB);
void PDL_IntRegGTE2(PDL_psScriptCB psScriptCB);
void PDL_IntRegNEQ2(PDL_psScriptCB psScriptCB);

void PDL_IntRegMOV_IntReg(PDL_psScriptCB psScriptCB);
void PDL_IntRegAND_IntReg(PDL_psScriptCB psScriptCB);
void PDL_IntRegOR_IntReg(PDL_psScriptCB psScriptCB);
void PDL_IntRegXOR_IntReg(PDL_psScriptCB psScriptCB);
void PDL_IntRegSHR_IntReg(PDL_psScriptCB psScriptCB);
void PDL_IntRegSHL_IntReg(PDL_psScriptCB psScriptCB);
void PDL_IntRegADD_IntReg(PDL_psScriptCB psScriptCB);
void PDL_IntRegSUB_IntReg(PDL_psScriptCB psScriptCB);
void PDL_IntRegMUL_IntReg(PDL_psScriptCB psScriptCB);
void PDL_IntRegIMUL_IntReg(PDL_psScriptCB psScriptCB);
void PDL_IntRegDIV_IntReg(PDL_psScriptCB psScriptCB);
void PDL_IntRegIDIV_IntReg(PDL_psScriptCB psScriptCB);
void PDL_IntRegMOD_IntReg(PDL_psScriptCB psScriptCB);
void PDL_IntRegEQU_IntReg(PDL_psScriptCB psScriptCB);
void PDL_IntRegLT_IntReg(PDL_psScriptCB psScriptCB);
void PDL_IntRegLTE_IntReg(PDL_psScriptCB psScriptCB);
void PDL_IntRegGT_IntReg(PDL_psScriptCB psScriptCB);
void PDL_IntRegGTE_IntReg(PDL_psScriptCB psScriptCB);
void PDL_IntRegNEQ_IntReg(PDL_psScriptCB psScriptCB);

void PDL_RegCircularBufferPoll(PDL_psScriptCB psScriptCB);
void PDL_Sync(PDL_psScriptCB psScriptCB);
void PDL_Lock(PDL_psScriptCB psScriptCB);
void PDL_Unlock(PDL_psScriptCB psScriptCB);

void PDL_WriteMemory64(PDL_psScriptCB psScriptCB);
void PDL_ReadMemory64(PDL_psScriptCB psScriptCB);
void PDL_PollMemory64(PDL_psScriptCB psScriptCB);
void PDL_LoadBytesToMemory64(PDL_psScriptCB psScriptCB);
void PDL_FillMem64(PDL_psScriptCB psScriptCB);
void PDL_MemCircularBufferPoll64(PDL_psScriptCB psScriptCB);
void PDL_WriteIntRegToMemory64(PDL_psScriptCB psScriptCB);
void PDL_ReadMemoryToIntReg64(PDL_psScriptCB psScriptCB);
void PDL_IntRegMOV64(PDL_psScriptCB psScriptCB);
void PDL_IntRegMOV_IntReg(PDL_psScriptCB psScriptCB);

void PDL_Padding(PDL_psScriptCB psScriptCB);
void PDL_SaveBytesFromMemory(PDL_psScriptCB psScriptCB);
void PDL_SaveBytesFromMemory64(PDL_psScriptCB psScriptCB);

void PDL_WriteRegister64(PDL_psScriptCB psScriptCB);
void PDL_ReadRegister64(PDL_psScriptCB psScriptCB);
void PDL_WriteIntRegToRegister64(PDL_psScriptCB psScriptCB);
void PDL_ReadRegister64ToIntReg(PDL_psScriptCB psScriptCB);
void PDL_PollRegister64(PDL_psScriptCB psScriptCB);

void PDL_Write64Register64(PDL_psScriptCB psScriptCB);
void PDL_Write64Memory64(PDL_psScriptCB psScriptCB);
void PDL_Read64Register64(PDL_psScriptCB psSccharriptCB);
void PDL_Read64Memory64(PDL_psScriptCB psScriptCB);
void PDL_Poll64Register64(PDL_psScriptCB psScriptCB);
void PDL_Poll64Memory64(PDL_psScriptCB psScriptCB);
void PDL_Write64IntRegToRegister64(PDL_psScriptCB psScriptCB);
void PDL_Write64IntRegToMemory64(PDL_psScriptCB psScriptCB);
void PDL_Read64Register64ToIntReg(PDL_psScriptCB psScriptCB);
void PDL_Read64Memory64ToIntReg(PDL_psScriptCB psScriptCB);

void PDL_NULL(PDL_psScriptCB psScriptCB);

PDL_pfnScriptFunction pfnScriptFunction[] = {
    &PDL_WriteRegister, // 0
    &PDL_WriteMemory, // 1
    &PDL_WriteSlavePort, // 2

    &PDL_ReadRegister, // 3
    &PDL_ReadMemory, // 4

    &PDL_PollRegister, // 5
    &PDL_PollMemory, // 6

    &PDL_LoadBytesToMemory, // 7
    &PDL_LoadWordsToRegister, // 8
    &PDL_LoadWordsToSlavePort, // 9

    &PDL_Idle, // A

    &PDL_MemCircularBufferPoll, // B

    &PDL_WriteIntRegToRegister, // C
    &PDL_WriteIntRegToMemory, // D
    &PDL_ReadRegisterToIntReg, // E
    &PDL_ReadMemoryToIntReg, // F

    // DEPRECATED!
    &PDL_IntRegMOV, // 10
    &PDL_IntRegAND, // 11
    &PDL_IntRegOR, // 12
    &PDL_IntRegXOR, // 13
    &PDL_IntRegNOT, // 14
    &PDL_IntRegSHR, // 15
    &PDL_IntRegSHL, // 16
    &PDL_IntRegADD, // 17
    &PDL_IntRegSUB, // 18
    &PDL_IntRegMUL, // 19
    &PDL_IntRegIMUL, // 1A
    &PDL_IntRegDIV, // 1B
    &PDL_IntRegIDIV, // 1C
    &PDL_IntRegMOD, // 1D
    &PDL_IntRegEQU, // 1E
    &PDL_IntRegLT, // 1F
    &PDL_IntRegLTE, // 20
    &PDL_IntRegGT, // 21
    &PDL_IntRegGTE, // 22
    &PDL_IntRegNEQ, // 23
    // END DEPRECATED!

    &PDL_RegCircularBufferPoll, // 24

    &PDL_Sync, // 25
    &PDL_Lock, // 26
    &PDL_Unlock, // 27

    &PDL_WriteMemory64, // 28
    &PDL_ReadMemory64, // 29
    &PDL_PollMemory64, // 2A
    &PDL_LoadBytesToMemory64, // 2B
    &PDL_MemCircularBufferPoll64, // 2C
    &PDL_WriteIntRegToMemory64, // 2D
    &PDL_ReadMemoryToIntReg64, // 2E
    &PDL_IntRegMOV64, // DEPRECATED //2F

    &PDL_Padding, // 30
    &PDL_SaveBytesFromMemory, // 31
    &PDL_SaveBytesFromMemory64, // 32

    &PDL_WriteRegister64, // 33
    &PDL_ReadRegister64, // 34
    &PDL_WriteIntRegToRegister64, // 35
    &PDL_ReadRegister64ToIntReg, // 36
    &PDL_PollRegister64, // 37

    &PDL_Write64Register64, // 38
    &PDL_Write64Memory64, // 39
    &PDL_Read64Register64, // 3A
    &PDL_Read64Memory64, // 3B
    &PDL_Poll64Register64, // 3C
    &PDL_Poll64Memory64, // 3D
    &PDL_Write64IntRegToRegister64, // 3E
    &PDL_Write64IntRegToMemory64, // 3F
    &PDL_Read64Register64ToIntReg, // 40
    &PDL_Read64Memory64ToIntReg, // 41

    &PDL_FillMem, // 42
    &PDL_FillMem64, // 43

    &PDL_IntRegMOV_IntReg, // 44
    &PDL_IntRegAND_IntReg, // 45
    &PDL_IntRegOR_IntReg, // 46
    &PDL_IntRegXOR_IntReg, // 47
    &PDL_NULL, // 48
    &PDL_IntRegSHR_IntReg, // 49
    &PDL_IntRegSHL_IntReg, // 4A
    &PDL_IntRegADD_IntReg, // 4B
    &PDL_IntRegSUB_IntReg, // 4C
    &PDL_IntRegMUL_IntReg, // 4D
    &PDL_IntRegIMUL_IntReg, // 4E
    &PDL_IntRegDIV_IntReg, // 4F
    &PDL_IntRegIDIV_IntReg, // 50
    &PDL_IntRegMOD_IntReg, // 51
    &PDL_IntRegEQU_IntReg, // 52
    &PDL_IntRegLT_IntReg, // 53
    &PDL_IntRegLTE_IntReg, // 54
    &PDL_IntRegGT_IntReg, // 55
    &PDL_IntRegGTE_IntReg, // 56
    &PDL_IntRegNEQ_IntReg, // 57

    &PDL_IntRegMOV2, // 58
    &PDL_IntRegAND2, // 59
    &PDL_IntRegOR2, // 5A
    &PDL_IntRegXOR2, // 5B
    &PDL_NULL, // 5C
    &PDL_IntRegSHR2, // 5D
    &PDL_IntRegSHL2, // 5E
    &PDL_IntRegADD2, // 5F
    &PDL_IntRegSUB2, // 60
    &PDL_IntRegMUL2, // 61
    &PDL_IntRegIMUL2, // 62
    &PDL_IntRegDIV2, // 63
    &PDL_IntRegIDIV2, // 64
    &PDL_IntRegMOD2, // 65
    &PDL_IntRegEQU2, // 66
    &PDL_IntRegLT2, // 67
    &PDL_IntRegLTE2, // 68
    &PDL_IntRegGT2, // 69
    &PDL_IntRegGTE2, // 6A
    &PDL_IntRegNEQ2, // 6B

    /* etc */
};

/* Global data */

/*==========================================================================================*/
/*                                                                                          */
/*      pvScriptPointer MUST be set to either 0x00000000 to just run the system
 * checks      */
/*      or the address at which the Pdump Player Lite binary script has been
 * loaded.        */
/*                                                                                          */
/*==========================================================================================*/

volatile void* pvScriptPointer = (volatile void*)(uintptr_t)(BINLITE_SCRIPT_ADDR);

// pui8SABOutput is the address at which to dump the SAB output
volatile uint8_t* pui8SABOutput = (uint8_t*)(uintptr_t)(BINLITE_SAB_ADDR);

//#define IGNORE_CHECKS

uint32_t ui32CheckLittleEndian = 0x01020304;

int gbl_argc;
char** gbl_argv;

uint32_t gui32BinLitePlayerFlags = 0;

uint64_t aui64InternalRegisters[MAX_PDUMPLSCRIPTS][NUMBER_OF_INTERNAL_REGS];

PDL_sScriptCB asScriptCB[MAX_PDUMPLSCRIPTS];
uint32_t gui32NumScripts = 0;

PDL_sTestCB asTestCB[MAX_PDUMPLTESTS];
uint32_t gui32NumTests = 0;

uint32_t _ui32SysSyncId = 32;
volatile uint32_t* pui32SABOrigin;
uint8_t* sab_addr;

/******************************************************************************/
/*                                                                            */
/*      Function prototypes                                                   */
/*                                                                            */
/******************************************************************************/
uint32_t GetSysSyncId(PDL_psScriptCB psScriptCB, uint32_t ui32SyncId);

uint32_t ui32MaxScriptFunction = sizeof(pfnScriptFunction) / sizeof(PDL_pfnScriptFunction) - 1;

/******************************************************************************/
/*                                                                            */
/*      Operation functions for internal register commands with 3 operands    */
/*                                                                            */
/******************************************************************************/

/* Function prototype */
typedef uint64_t (*PDL_pfnIntRegOpFunction)(uint64_t ui64Op1, uint64_t ui64Op2);

/* Common function used to process internal register commands with 3 operands */
void PDL_IntRegThreeOperandCmd(
    PDL_psScriptCB psScriptCB, PDL_pfnIntRegOpFunction pfnIntRegOpFunction, IMG_BOOL intRegOperand);

uint64_t PDL_IntRegOpAND(uint64_t ui64Op1, uint64_t ui64Op2) { return (ui64Op1 & ui64Op2); }

uint64_t PDL_IntRegOpOR(uint64_t ui64Op1, uint64_t ui64Op2) { return (ui64Op1 | ui64Op2); }

uint64_t PDL_IntRegOpXOR(uint64_t ui64Op1, uint64_t ui64Op2) { return (ui64Op1 ^ ui64Op2); }

uint64_t PDL_IntRegOpSHR(uint64_t ui64Op1, uint64_t ui64Op2) { return (ui64Op1 >> ui64Op2); }

uint64_t PDL_IntRegOpSHL(uint64_t ui64Op1, uint64_t ui64Op2) { return (ui64Op1 << ui64Op2); }

uint64_t PDL_IntRegOpADD(uint64_t ui64Op1, uint64_t ui64Op2) { return (ui64Op1 + ui64Op2); }

uint64_t PDL_IntRegOpSUB(uint64_t ui64Op1, uint64_t ui64Op2) { return (ui64Op1 - ui64Op2); }

uint64_t PDL_IntRegOpMUL(uint64_t ui64Op1, uint64_t ui64Op2) { return (ui64Op1 * ui64Op2); }

uint64_t PDL_IntRegOpIMUL(uint64_t ui64Op1, uint64_t ui64Op2)
{
    return (uint64_t)((int64_t)ui64Op1 * (int64_t)ui64Op2);
}

uint64_t PDL_IntRegOpDIV(uint64_t ui64Op1, uint64_t ui64Op2) { return (ui64Op1 / ui64Op2); }

uint64_t PDL_IntRegOpIDIV(uint64_t ui64Op1, uint64_t ui64Op2)
{
    return (uint64_t)((int64_t)ui64Op1 / (int64_t)ui64Op2);
}

uint64_t PDL_IntRegOpMOD(uint64_t ui64Op1, uint64_t ui64Op2) { return (ui64Op1 % ui64Op2); }

uint64_t PDL_IntRegOpEQU(uint64_t ui64Op1, uint64_t ui64Op2) { return (ui64Op1 == ui64Op2); }

uint64_t PDL_IntRegOpLT(uint64_t ui64Op1, uint64_t ui64Op2) { return (ui64Op1 < ui64Op2); }

uint64_t PDL_IntRegOpLTE(uint64_t ui64Op1, uint64_t ui64Op2) { return (ui64Op1 <= ui64Op2); }

uint64_t PDL_IntRegOpGT(uint64_t ui64Op1, uint64_t ui64Op2) { return (ui64Op1 > ui64Op2); }

uint64_t PDL_IntRegOpGTE(uint64_t ui64Op1, uint64_t ui64Op2) { return (ui64Op1 >= ui64Op2); }

uint64_t PDL_IntRegOpNEQ(uint64_t ui64Op1, uint64_t ui64Op2) { return (ui64Op1 != ui64Op2); }

/******************************************************************************/
/*                                                                            */
/*      Script processing functions                                           */
/*                                                                            */
/******************************************************************************/

void PDL_NULL(PDL_psScriptCB psScriptCB)
{
    // Should never be run

    PDL_ASSERT(psScriptCB->pui32Script);
}

/********************/
/*      "WRW"       */
/********************/
void PDL_WriteRegister(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "write register" (0x00000000)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address
     * psScriptCB->pui32Script[4]  =   value
     */

    VERBOSE_OUTPUT(WRW_REG, psScriptCB->pui32Script);

    WriteReg(psScriptCB->pui32Script[3], psScriptCB->pui32Script[4]);

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 5;
}

void PDL_WriteMemory(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "write memory" (0x00000001)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address
     * psScriptCB->pui32Script[4]  =   value
     */

    VERBOSE_OUTPUT(WRW_MEM, psScriptCB->pui32Script);

    WriteMem(psScriptCB->pui32Script[3], psScriptCB->pui32Script[4]);
    // flush_and_invalid_cache(GPU_CACHE_OP_CLEAN, psScriptCB->pui32Script[3], psScriptCB->pui32Script[3]);

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 5;
}

void PDL_WriteSlavePort(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "write slave port" (0x00000002)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address
     * psScriptCB->pui32Script[4]  =   value
     */

    VERBOSE_OUTPUT(WRW_SLV, psScriptCB->pui32Script);

    PDL_ASSERT(psScriptCB->pui32Script); // Slave ports have been deprecated
    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 5;
}

/********************/
/*      "RDW"       */
/********************/
void PDL_ReadRegister(PDL_psScriptCB psScriptCB)
{
    uint32_t ui32Temp;

    /*
     * psScriptCB->pui32Script[0]  =   "read register" (0x00000003)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address
     */

    VERBOSE_OUTPUT(RDW_REG, psScriptCB->pui32Script);

    ReadReg(psScriptCB->pui32Script[3], &ui32Temp);
    printf("PDL_ReadRegister: get rdata:%x from addr:%x", ui32Temp, psScriptCB->pui32Script[3]);

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 4;
}

void PDL_ReadMemory(PDL_psScriptCB psScriptCB)
{
    uint32_t ui32Temp;

    /*
     * psScriptCB->pui32Script[0]  =   "read memory" (0x00000004)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address
     */

    VERBOSE_OUTPUT(RDW_MEM, psScriptCB->pui32Script);

    ReadMem(psScriptCB->pui32Script[3], &ui32Temp);

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 4;
}

/********************/
/*      "POL"       */
/********************/
typedef enum {
    PDL_POLL_REG,
    PDL_POLL_REG64,
    PDL_POLL64_REG64,
    PDL_POLL_MEM,
    PDL_POLL_MEM64,
    PDL_POLL64_MEM64
} PDL_ePollTarget;

void PDL_Poll(PDL_psScriptCB psScriptCB, PDL_ePollTarget ePollTarget)
{
    uint64_t ui64Temp;
    uint32_t ui32Temp;
    IMG_BOOL bContinuePolling = IMG_TRUE;
    IMG_BOOL bPollCountExceeded = IMG_FALSE;
    IMG_BOOL bPollFailed = IMG_TRUE;
    uint64_t ui64Address = 0;
    uint64_t ui64RequiredValue = 0;
    uint64_t ui64EnableMask = 0;
    uint32_t ui32CheckFuncId = 0;
    uint32_t ui32PollCount = 0;
    uint32_t ui32Delay = 0;

    /*
     * psScriptCB->pui32Script[0]  =   "poll register" (0x00000005)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address
     * psScriptCB->pui32Script[4]  =   required value
     * psScriptCB->pui32Script[5]  =   enable mask
     * psScriptCB->pui32Script[6]  =   check function id
     * psScriptCB->pui32Script[7]  =   poll count
     * psScriptCB->pui32Script[8]  =   delay
     */
    switch (ePollTarget) {
        case PDL_POLL64_MEM64:
        case PDL_POLL64_REG64:
            ui64Address = ADDR64(psScriptCB->pui32Script[3]);
            ui64RequiredValue = DATA64(psScriptCB->pui32Script[5]);
            ui64EnableMask = DATA64(psScriptCB->pui32Script[7]);
            ui32CheckFuncId = psScriptCB->pui32Script[9];
            ui32PollCount = psScriptCB->pui32Script[10];
            ui32Delay = psScriptCB->pui32Script[11];
            break;

        case PDL_POLL_MEM64:
        case PDL_POLL_REG64:
            ui64Address = ADDR64(psScriptCB->pui32Script[3]);
            ui64RequiredValue = psScriptCB->pui32Script[5];
            ui64EnableMask = psScriptCB->pui32Script[6];
            ui32CheckFuncId = psScriptCB->pui32Script[7];
            ui32PollCount = psScriptCB->pui32Script[8];
            ui32Delay = psScriptCB->pui32Script[9];
            break;

        case PDL_POLL_MEM:
        case PDL_POLL_REG:
            ui64Address = psScriptCB->pui32Script[3];
            ui64RequiredValue = psScriptCB->pui32Script[4];
            ui64EnableMask = psScriptCB->pui32Script[5];
            ui32CheckFuncId = psScriptCB->pui32Script[6];
            ui32PollCount = psScriptCB->pui32Script[7];
            ui32Delay = psScriptCB->pui32Script[8];
            break;

        default:
            PDL_ASSERT(psScriptCB->pui32Script);
            break;
    }
    printf(" pollReg ui64RequiredValue 0x%8.8llx \n", ui64RequiredValue);
    printf(" pollReg ui64Address 0x%8.8llx \n", ui64Address);
    printf(" pollReg ui64EnableMask 0x%8.8llx \n", ui64EnableMask);
    printf(" pollReg ui32PollCount 0x%8.8x \n", ui32PollCount);
    /* Is this the first time we execute this POLL command? */
    if (!psScriptCB->bPollPending) {
        psScriptCB->bPollPending = IMG_TRUE;
        psScriptCB->ui32LoopCount = 0;
        psScriptCB->ui32Delay = ui32Delay;
    }

    ui64RequiredValue &= ui64EnableMask;
    printf(" pollReg ui64RequiredValue after mask 0x%8.8llx \n", ui64RequiredValue);
    ui64Temp = ~ui64RequiredValue;

    while (bContinuePolling) {
        switch (ePollTarget) {
            case PDL_POLL_REG:
            case PDL_POLL_REG64:
                ReadReg(ui64Address, &ui32Temp);
                printf(" pollReg32 FuncID:%d Mask:0x%llx%8.8llx expect "
                       "value:0x%llx%8.8llx ",
                    ui32CheckFuncId, (ui64EnableMask >> 32) & 0xffffffff, ui64EnableMask & 0xffffffff,
                    (ui64RequiredValue >> 32) & 0xffffffff, ui64RequiredValue & 0xffffffff);
                printf("poll addr:0x%llx value to addr 0x%p \n", ui64Address, &ui32Temp);
                ui64Temp = ui32Temp;
                printf("get ui64Temp:0x%llx \n", ui64Temp);
                break;

            case PDL_POLL64_REG64:
                printf(" pollReg64 %d 0x%llx%8.8llx 0x%llx%8.8llx ", ui32CheckFuncId,
                    (ui64EnableMask >> 32) & 0xffffffff, ui64EnableMask & 0xffffffff,
                    (ui64RequiredValue >> 32) & 0xffffffff, ui64RequiredValue & 0xffffffff);
                printf("poll addr:0x%llx value to addr 0x%p \n", ui64Address, &ui64Temp);
                ReadReg64(ui64Address, &ui64Temp);
                printf("get ui64Temp:0x%llx \n", ui64Temp);
                break;

            case PDL_POLL_MEM:
            case PDL_POLL_MEM64:
                ReadMem(ui64Address, &ui32Temp);
                ui64Temp = ui32Temp;
                printf("%s():%d poll mem or poll_mem64:  addr:0x%llx, get ui64Temp:0x%llx, ui32Temp:%d\n", __FUNCTION__,__LINE__, ui64Address, ui64Temp, ui32Temp);
                break;

            case PDL_POLL64_MEM64:
                ReadMem64(ui64Address, &ui64Temp);
                break;
        }

        ui64Temp &= ui64EnableMask;
        // bContinuePolling = IMG_FALSE; bPollFailed = IMG_FALSE; break;// [cunrong]
        // jump out of poll

        switch (ui32CheckFuncId) {
            case PDL_CHECK_FUNC_EQ: {
                printf(" pollReg if EQU 0x%8.8x = 0x%8.8x \n", (unsigned int)ui64Temp & 0xffffffff, (unsigned int)ui64RequiredValue & 0xffffffff);

                if (ui64Temp == ui64RequiredValue) {
                    bContinuePolling = IMG_FALSE;
                    bPollFailed = IMG_FALSE;
                }
                break;
            }

            case PDL_CHECK_FUNC_LT: {
                if (ui64Temp < ui64RequiredValue) {
                    bContinuePolling = IMG_FALSE;
                    bPollFailed = IMG_FALSE;
                }
                break;
            }

            case PDL_CHECK_FUNC_LE: {
                if (ui64Temp <= ui64RequiredValue) {
                    bContinuePolling = IMG_FALSE;
                    bPollFailed = IMG_FALSE;
                }
                break;
            }

            case PDL_CHECK_FUNC_GT: {
                printf(" pollReg if GT 0x%8.8x > 0x%8.8x \n", (unsigned int)ui64Temp, (unsigned int)ui64RequiredValue);
                if (ui64Temp > ui64RequiredValue) {
                    bContinuePolling = IMG_FALSE;
                    bPollFailed = IMG_FALSE;
                }
                break;
            }

            case PDL_CHECK_FUNC_GE: {
                if (ui64Temp >= ui64RequiredValue) {
                    bContinuePolling = IMG_FALSE;
                    bPollFailed = IMG_FALSE;
                }
                break;
            }

            case PDL_CHECK_FUNC_NE: {
                printf(" pollReg if NEQU 0x%8.8llx != 0x%8.8llx \n", ui64Temp, ui64RequiredValue);
                if (ui64Temp != ui64RequiredValue) {
                    bContinuePolling = IMG_FALSE;
                    bPollFailed = IMG_FALSE;
                }
                break;
            }

            default: {
                ERROR_OUTPUT("Unrecognised Function Id\n");
                PDL_ASSERT(psScriptCB->pui32Script);
                break;
            }
        }

        /* Still going? */
        if (bContinuePolling) {
            psScriptCB->ui32LoopCount++;

            /* have we exceeded the POL Count */
            if ((psScriptCB->ui32LoopCount >= ui32PollCount) && (ui32PollCount != PDL_POLLCOUNT_INFINITE)) {
                bPollCountExceeded = IMG_TRUE;
                bContinuePolling = IMG_FALSE;
            }
            /* If the number of scripts is more than 1 then we do not continue polling
             */
            else if (gui32NumScripts > 1) {
                bPollCountExceeded = IMG_FALSE;
                bContinuePolling = IMG_FALSE;
            } else if (ui32Delay) {
                Delay(ui32Delay);
            }
        }
    }

    if (bPollFailed && !bPollCountExceeded) {
        /* Reset the time count to 0 */
        psScriptCB->ui32TimeCount = 0;
        /* We do not advance the script - it will be parsed again on next call to
         * this function */
    } else {
        if (bPollFailed) {
            ERROR_OUTPUT("Poll Failed\n");
            PDL_ASSERT(psScriptCB->pui32Script);
        }

        /* Ready to move on */
        psScriptCB->bPollPending = IMG_FALSE;

        /* Advance script pointer and exit */
        switch (ePollTarget) {
            case PDL_POLL64_MEM64:
            case PDL_POLL64_REG64:
                psScriptCB->pui32Script += 12;
                break;

            case PDL_POLL_MEM64:
            case PDL_POLL_REG64:
                psScriptCB->pui32Script += 10;
                break;

            case PDL_POLL_MEM:
            case PDL_POLL_REG:
                psScriptCB->pui32Script += 9;
                break;
        }
    }
}

void PDL_PollRegister(PDL_psScriptCB psScriptCB)
{
    if (!psScriptCB->bPollPending) {
        VERBOSE_OUTPUT(POL_REG, psScriptCB->pui32Script);
    }

    PDL_Poll(psScriptCB, PDL_POLL_REG);
}

void PDL_PollMemory(PDL_psScriptCB psScriptCB)
{
    if (!psScriptCB->bPollPending) {
        VERBOSE_OUTPUT(POL_MEM, psScriptCB->pui32Script);
    }

    // if(0xc02e6004 == psScriptCB->pui32Script[3]){
    //     flush_and_invalid_cache(GPU_CACHE_OP_INVALIDATE, ADDR64(psScriptCB->pui32Script[3]), ADDR64(psScriptCB->pui32Script[3])) ;
    // }
    PDL_Poll(psScriptCB, PDL_POLL_MEM);
}

/********************/
/*      "LDW/B"     */
/********************/
void PDL_LoadBytesToMemory(PDL_psScriptCB psScriptCB)
{
    uint32_t ui32DataSize = psScriptCB->pui32Script[4];

    /*
     * psScriptCB->pui32Script[0]  =   "load bytes to memory" (0x00000007)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address
     * psScriptCB->pui32Script[4]  =   size (in bytes)
     * psScriptCB->pui32Script[5]  =   data[1], data[2], data[3], data[4],
     * psScriptCB->pui32Script[6]  =   data[5], data[6], data[7], data[8],
     * ...
     * ...
     */

    VERBOSE_OUTPUT(LDB_MEM, psScriptCB->pui32Script);
    // flush_and_invalid_cache(GPU_CACHE_OP_INVALIDATE, &psScriptCB->pui32Script[5], ((uint8_t *)&psScriptCB->pui32Script[5])+ui32DataSize);
    CopyMem((uintptr_t)psScriptCB->pui32Script[3], &psScriptCB->pui32Script[5], ui32DataSize);

    // uintptr_t end_addr = ((uint8_t *)psScriptCB->pui32Script[3])+ui32DataSize;
    // flush_and_invalid_cache(GPU_CACHE_OP_CLEAN, ADDR64(psScriptCB->pui32Script[3]), ADDR64(end_addr));
    /* Advance script pointer and exit (round byte count up and convert to word
     * count)  */
    ui32DataSize += 3;
    ui32DataSize >>= 2;
    psScriptCB->pui32Script += (5 + ui32DataSize);
}

void PDL_LoadWordsToRegister(PDL_psScriptCB psScriptCB)
{
    uint32_t ui32Address = psScriptCB->pui32Script[3];
    uint32_t ui32DataSize = psScriptCB->pui32Script[4];
    uint32_t i, j;

    /*
     * psScriptCB->pui32Script[0]  =   "load words to register" (0x00000008)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address
     * psScriptCB->pui32Script[4]  =   size
     * psScriptCB->pui32Script[5]  =   data[1]
     * psScriptCB->pui32Script[6]  =   data[2]
     * ...
     * ...
     * psScriptCB->pui32Script[size + 2 ]  =   data[size]
     *
     */

    VERBOSE_OUTPUT(LDW_REG, psScriptCB->pui32Script);

    for (i = 0, j = 5; i < ui32DataSize; i++, j++) {
        WriteReg((ui32Address + i * sizeof(uint32_t)), psScriptCB->pui32Script[j]);
    }

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += (5 + ui32DataSize);
}

void PDL_LoadWordsToSlavePort(PDL_psScriptCB psScriptCB)
{
    uint32_t ui32Address = psScriptCB->pui32Script[3];
    uint32_t ui32DataSize = psScriptCB->pui32Script[4];
    uint32_t i, j;

    /*
     * psScriptCB->pui32Script[0]  =   "load words to slave port" (0x00000009)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address
     * psScriptCB->pui32Script[4]  =   size
     * psScriptCB->pui32Script[5]  =   data[1]
     * psScriptCB->pui32Script[6]  =   data[2]
     * ...
     * ...
     * psScriptCB->pui32Script[size + 2 ]  =   data[size]
     *
     */

    VERBOSE_OUTPUT(LDW_SLV, psScriptCB->pui32Script);

    for (i = 0, j = 5; i < ui32DataSize; i++, j++) {
        WriteReg((ui32Address + i * sizeof(uint32_t)), psScriptCB->pui32Script[j]);
    }

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += (5 + ui32DataSize);
}

void PDL_FillMem(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "fill memory" (0x00000042)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address
     * psScriptCB->pui32Script[4]  =   byte
     * psScriptCB->pui32Script[5]  =   size (in bytes)
     */

    VERBOSE_OUTPUT(FILL_MEM, psScriptCB->pui32Script);

    FillMem((uintptr_t)psScriptCB->pui32Script[3], (uint8_t)psScriptCB->pui32Script[4], psScriptCB->pui32Script[5]);

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 6;
}

/********************/
/*      "IDL"       */
/********************/
void PDL_Idle(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "idle" (0x0000000A)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   idle cycle count
     */

    VERBOSE_OUTPUT(IDL, psScriptCB->pui32Script);

    Delay(psScriptCB->pui32Script[3]);

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 4;
}

/********************/
/*      "CBP"       */
/********************/
void PDL_MemCircularBufferPoll(PDL_psScriptCB psScriptCB)
{
    uint32_t ui32RoffAddr = psScriptCB->pui32Script[3];
    uint32_t ui32WoffValue = psScriptCB->pui32Script[4];
    uint32_t ui32RequiredPacketSize = psScriptCB->pui32Script[5];
    uint32_t ui32CircularBufferSize = psScriptCB->pui32Script[6];
    uint32_t ui32RoffValue;
    uint32_t ui32FreeSpace;
    IMG_BOOL bContinue = IMG_TRUE;
    IMG_BOOL bCBPCompleted = IMG_FALSE;

    /*
     * psScriptCB->pui32Script[0]  =   "circular buffer poll" (0x0000000B)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   read offset address
     * psScriptCB->pui32Script[4]  =   write offset value
     * psScriptCB->pui32Script[5]  =   required packet size
     * psScriptCB->pui32Script[6]  =   circular buffer size
     */

    /* Is this the first time we execute this CBP command? */
    if (!psScriptCB->bPollPending) {
        psScriptCB->bPollPending = IMG_TRUE;
        psScriptCB->ui32LoopCount = 0;
        psScriptCB->ui32Delay = 10;

        VERBOSE_OUTPUT(CBP_MEM, psScriptCB->pui32Script);
    }

    while (bContinue) {
        ReadMem(ui32RoffAddr, &ui32RoffValue);
        // bCBPCompleted = IMG_TRUE;bContinue = IMG_FALSE;break; //[cunrong] jump
        // out of polling

        if (ui32RoffValue > ui32WoffValue) {
            ui32FreeSpace = ui32RoffValue - ui32WoffValue - 1;
        } else {
            ui32FreeSpace = (ui32RoffValue - ui32WoffValue) + (ui32CircularBufferSize - 1);
        }

        if (ui32FreeSpace >= ui32RequiredPacketSize) {
            bCBPCompleted = IMG_TRUE;
            bContinue = IMG_FALSE;
        } else if (gui32NumScripts > 1) {
            bCBPCompleted = IMG_FALSE;
            bContinue = IMG_FALSE;
        } else if (psScriptCB->ui32Delay) {
            Delay(psScriptCB->ui32Delay);
        }
    }

    if (!bCBPCompleted) {
        /* Reset the time count to 0 */
        psScriptCB->ui32TimeCount = 0;
    } else {
        /* Ready to move on */
        psScriptCB->bPollPending = IMG_FALSE;

        /* Advance script pointer and exit */
        psScriptCB->pui32Script += 7;
    }
}

void PDL_RegCircularBufferPoll(PDL_psScriptCB psScriptCB)
{
    uint32_t ui32RoffAddr = psScriptCB->pui32Script[3];
    uint32_t ui32WoffValue = psScriptCB->pui32Script[4];
    uint32_t ui32RequiredPacketSize = psScriptCB->pui32Script[5];
    uint32_t ui32CircularBufferSize = psScriptCB->pui32Script[6];
    uint32_t ui32RoffValue;
    uint32_t ui32FreeSpace;
    IMG_BOOL bContinue = IMG_TRUE;
    IMG_BOOL bCBPCompleted = IMG_FALSE;

    /*
     * psScriptCB->pui32Script[0]  =   "circular buffer poll" (0x0000000B)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   read offset address
     * psScriptCB->pui32Script[4]  =   write offset value
     * psScriptCB->pui32Script[5]  =   required packet size
     * psScriptCB->pui32Script[6]  =   circular buffer size
     */

    /* Is this the first time we execute this CBP command? */
    if (!psScriptCB->bPollPending) {
        psScriptCB->bPollPending = IMG_TRUE;
        psScriptCB->ui32LoopCount = 0;
        psScriptCB->ui32Delay = 10;

        VERBOSE_OUTPUT(CBP_REG, psScriptCB->pui32Script);
    }

    while (bContinue) {
        ReadReg(ui32RoffAddr, &ui32RoffValue);
        // bCBPCompleted = IMG_TRUE;bContinue = IMG_FALSE;break;//[cunrong] jump out
        // of poll

        if (ui32RoffValue > ui32WoffValue) {
            ui32FreeSpace = ui32RoffValue - ui32WoffValue - 1;
        } else {
            ui32FreeSpace = (ui32RoffValue - ui32WoffValue) + (ui32CircularBufferSize - 1);
        }

        if (ui32FreeSpace >= ui32RequiredPacketSize) {
            bCBPCompleted = IMG_TRUE;
            bContinue = IMG_FALSE;
        } else if (gui32NumScripts > 1) {
            bCBPCompleted = IMG_FALSE;
            bContinue = IMG_FALSE;
        }
    }

    if (!bCBPCompleted) {
        /* Reset the time count to 0 */
        psScriptCB->ui32TimeCount = 0;
    } else {
        /* Ready to move on */
        psScriptCB->bPollPending = IMG_FALSE;

        /* Advance script pointer and exit */
        psScriptCB->pui32Script += 7;
    }
}

/******************************************************************************/
/*                                                                            */
/*      Synchronisation Commands                                              */
/*                                                                            */
/******************************************************************************/
void PDL_Sync(PDL_psScriptCB psScriptCB)
{
    uint32_t ui32SyncId = psScriptCB->pui32Script[3];
    PDL_psTestCB psTestCB = &asTestCB[psScriptCB->ui32TestId - 1];
    PDL_psScriptCB psScriptItrCB;
    uint32_t ui32SysSyncId;
    IMG_BOOL bEnableSyncs = IMG_TRUE;

    /*
     * psScriptCB->pui32Script[0]  =   "sync" (0x00000025)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   sync id
     */

    VERBOSE_OUTPUT(SYNC, psScriptCB->pui32Script);

    /*Check for a valid Sync Id */
    if (ui32SyncId >= NO_SYSSYNC_IDS) {
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    if (ui32SyncId > 31) {
        ui32SysSyncId = psTestCB->aui32SystemSyncId[ui32SyncId];

        /* If the inter sync does not already have a system sync Id it is not active
         */
        if (ui32SysSyncId >= MAX_SYNC_IDS) {
            uint32_t ui32TestIdIndex;

            /* Test to see if all inter-syncs are waiting */
            for (ui32TestIdIndex = 0; ui32TestIdIndex < gui32NumTests; ui32TestIdIndex++) {
                PDL_psTestCB psTestItrCB = &asTestCB[ui32TestIdIndex];

                /* Ignore the current test */
                if (psTestItrCB != psTestCB) {
                    uint32_t i;

                    for (i = 32; i < NO_SYSSYNC_IDS; i++) {
                        /* Look for a matching System Sync Id */
                        if (psTestItrCB->aui32SystemSyncId[i] == ui32SysSyncId) {
                            uint32_t ui32ScriptIdIndex;

                            for (ui32ScriptIdIndex = 0; ui32ScriptIdIndex < gui32NumScripts; ui32ScriptIdIndex++) {
                                psScriptItrCB = &asScriptCB[ui32ScriptIdIndex];

                                /* If one of the files from this test is waiting on this sync */
                                if (psScriptItrCB->bWaiting && (psScriptItrCB->ui32TestId == (ui32TestIdIndex + 1))
                                    && (psScriptItrCB->ui32SyncWaiting == ui32SysSyncId)) {
                                    /* It should not be possible for these to be the same file */
                                    if (psScriptCB == psScriptItrCB) {
                                        PDL_ASSERT(psScriptCB->pui32Script);
                                    }
                                    break;
                                }
                            }
                            /* If the test above did not find an active sync then we must
                             * still wait for that sync */
                            if (ui32ScriptIdIndex >= gui32NumScripts) {
                                bEnableSyncs = IMG_FALSE;
                                break;
                            }
                        }
                    }
                }
                /* If we can't enable the syncs we don't need to keep checking */
                if (!bEnableSyncs) {
                    break;
                }
            }
        }
    } else /* Check for intra-syncs */
    {
        uint32_t ui32ScriptIdIndex;

        ui32SysSyncId = GetSysSyncId(psScriptCB, ui32SyncId);

        for (ui32ScriptIdIndex = 0; ui32ScriptIdIndex < gui32NumScripts; ui32ScriptIdIndex++) {
            psScriptItrCB = &asScriptCB[ui32ScriptIdIndex];

            /* If it is one of the files from this test and this sync is enabled */
            if ((psScriptItrCB->ui32TestId == psScriptCB->ui32TestId)
                && ((psScriptItrCB->ui32DisabledSyncs & (1 << ui32SyncId)) == 0) && (psScriptItrCB != psScriptCB)) {
                /* If this script is not waiting on the same sync then we can't enable
                 * the syncs */
                if ((psScriptItrCB->bWaiting == IMG_FALSE) || (psScriptItrCB->ui32SyncWaiting != ui32SysSyncId)) {
                    bEnableSyncs = IMG_FALSE;
                    break;
                }
            }
        }
    }

    /* If all the syncs have called they can all be enabled */
    if (bEnableSyncs) {
        uint32_t ui32ScriptIdIndex = 0;

        for (ui32ScriptIdIndex = 0; (ui32ScriptIdIndex < gui32NumScripts) && (ui32SysSyncId >= MAX_SYNC_IDS);
             ui32ScriptIdIndex++) {
            psScriptItrCB = &asScriptCB[ui32ScriptIdIndex];

            if (psScriptItrCB->ui32SyncWaiting == ui32SysSyncId) {
                /* This sync must be waiting */
                if (psScriptItrCB->bWaiting != IMG_TRUE) {
                    PDL_ASSERT(psScriptCB->pui32Script);
                }

                /* Enable this file */
                psScriptItrCB->bWaiting = IMG_FALSE;
                psScriptItrCB->ui32SyncWaiting = 0;
            }
        }
    } else {
        psScriptCB->ui32SyncWaiting = ui32SysSyncId;
        psScriptCB->bWaiting = IMG_TRUE;
    }

    psScriptCB->pui32Script += 4;
}

void PDL_Lock(PDL_psScriptCB psScriptCB)
{
    PDL_psTestCB psTestCB = &asTestCB[psScriptCB->ui32TestId - 1];
    uint32_t ui32SemaId = psScriptCB->pui32Script[3];

    /*
     * psScriptCB->pui32Script[0]  =   "lock" (0x00000026)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   semaphore id
     */

    VERBOSE_OUTPUT(LOCK, psScriptCB->pui32Script);

    if (ui32SemaId >= MAX_SEMAPHORES) {
        PDL_ASSERT(psScriptCB->pui32Script);
        return;
    }

    if (psTestCB->ai32Semaphores[ui32SemaId] <= 0) {
        /* Wait for Unlock */
        psScriptCB->bWaiting = IMG_TRUE;
        psScriptCB->ui32SemaWaiting = ui32SemaId;
    }

    psTestCB->ai32Semaphores[ui32SemaId]--;

    psScriptCB->pui32Script += 4;
}

void PDL_Unlock(PDL_psScriptCB psScriptCB)
{
    PDL_psTestCB psTestCB = &asTestCB[psScriptCB->ui32TestId - 1];
    uint32_t ui32SemaId = psScriptCB->pui32Script[3];

    /*
     * psScriptCB->pui32Script[0]  =   "unlock" (0x00000027)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   semaphore id
     */

    VERBOSE_OUTPUT(UNLOCK, psScriptCB->pui32Script);

    if (psTestCB->ai32Semaphores[ui32SemaId] < 0) {
        uint32_t ui32Index;

        /* unlock one file */
        for (ui32Index = 0; ui32Index < gui32NumScripts; ui32Index++) {
            PDL_psScriptCB psScriptItrCB = &asScriptCB[ui32Index];

            if ((psScriptItrCB->ui32TestId == psScriptCB->ui32TestId) && psScriptItrCB->bWaiting
                && (psScriptItrCB->ui32SemaWaiting == ui32SemaId)) {
                psScriptItrCB->ui32SemaWaiting = 0;
                psScriptItrCB->bWaiting = IMG_FALSE;
                break;
            }
        }
    }

    psTestCB->ai32Semaphores[ui32SemaId]++;

    psScriptCB->pui32Script += 4;
}

// Padding command, this command does nothing
void PDL_Padding(PDL_psScriptCB psScriptCB) { psScriptCB->pui32Script++; }

// SAB command, by default this command does nothing
void PDL_SaveBytesFromMemory(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "Save Bytes" (0x00000031)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address
     * psScriptCB->pui32Script[4]  =   size
     */
    VERBOSE_OUTPUT(SAB_MEM, psScriptCB->pui32Script);

    // This block of code generates an area of memory which contains all the SAB
    // and SII dumps This area of memory can be dumped to a binary file which the
    // PdumpExtractImage tool can interpret
    if (pui8SABOutput) {
        // if this is a new SAB line in the original pdump,
        // add a header of the 0x31 SAB id, file id, line number and size to the
        // save mem area
        if (psScriptCB->ui32SabFileID != psScriptCB->pui32Script[1]
            || psScriptCB->ui32SabLineNum != psScriptCB->pui32Script[2]) {
            if ((uintptr_t)pui8SABOutput & 0x3) {
                uint32_t skipbytes = 4 - ((uintptr_t)pui8SABOutput & 0x3);
                *pui32SABOrigin += skipbytes;
                pui8SABOutput += skipbytes; // skip to 4 byte alignment
            }

#ifdef EMU_TARGET
            EmuSaveMemFlush();
#endif
            *(uint32_t*)pui8SABOutput = psScriptCB->pui32Script[0];
            pui8SABOutput += sizeof(uint32_t);
            *(uint32_t*)pui8SABOutput = psScriptCB->pui32Script[1];
            pui8SABOutput += sizeof(uint32_t);
            *(uint32_t*)pui8SABOutput = psScriptCB->pui32Script[2];
            pui8SABOutput += sizeof(uint32_t);
            psScriptCB->ui32SabFileID = psScriptCB->pui32Script[1];
            psScriptCB->ui32SabLineNum = psScriptCB->pui32Script[2];

            // Create a size variable as the fourth header number
            psScriptCB->pui32SabSize = (uint32_t*)pui8SABOutput;
            psScriptCB->ui32SabSize = 0;
            *psScriptCB->pui32SabSize = psScriptCB->ui32SabSize;
            pui8SABOutput += sizeof(uint32_t);
            *pui32SABOrigin += 4 * sizeof(uint32_t);
        }

        // Copy the SAB data into the save mem area
        SaveMem(pui8SABOutput, (uintptr_t)psScriptCB->pui32Script[3], psScriptCB->pui32Script[4]);
        pui8SABOutput += psScriptCB->pui32Script[4];
        *pui32SABOrigin += psScriptCB->pui32Script[4];
        psScriptCB->ui32SabSize += psScriptCB->pui32Script[4];
        *psScriptCB->pui32SabSize = psScriptCB->ui32SabSize;
        wr((uintptr_t)sab_addr, (unsigned int)(pui8SABOutput - sab_addr));
        printf("\033[031m%s():%d\033[0m\n", __func__, __LINE__);
    }

    psScriptCB->pui32Script += 5;
}

/******************************************************************************/
/*                                                                            */
/*      64bit Address Commands                                                */
/*                                                                            */
/******************************************************************************/
static inline IMG_BOOL pdl_CanRun64BitCommands(uint64_t ui64Address)
{
// Ignore Visual Studio warning C4127: conditional expression is constant
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127)
#endif

#ifndef IGNORE_CHECKS
    if (sizeof(void*) == 8) {
        return IMG_TRUE;
    } else {
        return (ui64Address < (0x1ULL << 32));
    }
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif
}

void PDL_WriteRegister64(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "write register" (0x00000000)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address(lower32)
     * psScriptCB->pui32Script[4]  =   address(upper32)
     * psScriptCB->pui32Script[5]  =   value
     */

    // Check we have 64bit compatibility
    if (!pdl_CanRun64BitCommands(ADDR64(psScriptCB->pui32Script[3]))) {
        ERROR_OUTPUT("64bit commands cannot run on a 32bit system\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    VERBOSE_OUTPUT(WRW_REG64, psScriptCB->pui32Script);

    WriteReg(ADDR64(psScriptCB->pui32Script[3]), psScriptCB->pui32Script[5]);

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 6;
}

void PDL_WriteMemory64(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "write memory" (0x00000001)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address(lower32)
     * psScriptCB->pui32Script[4]  =   address(upper32)
     * psScriptCB->pui32Script[5]  =   value
     */

    // Check we have 64bit compatibility
    if (!pdl_CanRun64BitCommands(ADDR64(psScriptCB->pui32Script[3]))) {
        ERROR_OUTPUT("64bit commands cannot run on a 32bit system\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    VERBOSE_OUTPUT(WRW_MEM64, psScriptCB->pui32Script);

    WriteMem(ADDR64(psScriptCB->pui32Script[3]), psScriptCB->pui32Script[5]);
    // flush_and_invalid_cache(GPU_CACHE_OP_CLEAN, ADDR64(psScriptCB->pui32Script[3]), ADDR64(psScriptCB->pui32Script[3]));
    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 6;
}

void PDL_ReadRegister64(PDL_psScriptCB psScriptCB)
{
    uint32_t ui32Temp;

    /*
     * psScriptCB->pui32Script[0]  =   "read register" (0x00000003)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address(lower32)
     * psScriptCB->pui32Script[4]  =   address(upper32)
     */

    // Check we have 64bit compatibility
    if (!pdl_CanRun64BitCommands(ADDR64(psScriptCB->pui32Script[3]))) {
        ERROR_OUTPUT("64bit commands cannot run on a 32bit system\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    VERBOSE_OUTPUT(RDW_REG64, psScriptCB->pui32Script);

    ReadReg(ADDR64(psScriptCB->pui32Script[3]), &ui32Temp);

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 5;
}

void PDL_ReadMemory64(PDL_psScriptCB psScriptCB)
{
    uint32_t ui32Temp;

    /*
     * psScriptCB->pui32Script[0]  =   "read memory" (0x00000004)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address(lower32)
     * psScriptCB->pui32Script[4]  =   address(upper32)
     */

    // Check we have 64bit compatibility
    if (!pdl_CanRun64BitCommands(ADDR64(psScriptCB->pui32Script[3]))) {
        ERROR_OUTPUT("64bit commands cannot run on a 32bit system\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    VERBOSE_OUTPUT(RDW_MEM64, psScriptCB->pui32Script);

    ReadMem(ADDR64(psScriptCB->pui32Script[3]), &ui32Temp);

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 5;
}

void PDL_PollRegister64(PDL_psScriptCB psScriptCB)
{
    // Check we have 64bit compatibility
    if (!pdl_CanRun64BitCommands(ADDR64(psScriptCB->pui32Script[3]))) {
        ERROR_OUTPUT("64bit commands cannot run on a 32bit system\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    if (!psScriptCB->bPollPending) {
        VERBOSE_OUTPUT(POL_REG64, psScriptCB->pui32Script);
    }

    PDL_Poll(psScriptCB, PDL_POLL_REG64);
}

void PDL_PollMemory64(PDL_psScriptCB psScriptCB)
{
    // Check we have 64bit compatibility
    if (!pdl_CanRun64BitCommands(ADDR64(psScriptCB->pui32Script[3]))) {
        ERROR_OUTPUT("64bit commands cannot run on a 32bit system\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    if (!psScriptCB->bPollPending) {
        VERBOSE_OUTPUT(POL_MEM64, psScriptCB->pui32Script);
    }

    // flush_and_invalid_cache(GPU_CACHE_OP_INVALIDATE, ADDR64(psScriptCB->pui32Script[3]), ADDR64(psScriptCB->pui32Script[3]));
    PDL_Poll(psScriptCB, PDL_POLL_MEM64);
}

void PDL_LoadBytesToMemory64(PDL_psScriptCB psScriptCB)
{
    uint32_t ui32DataSize = psScriptCB->pui32Script[5];

    /*
     * psScriptCB->pui32Script[0]  =   "load bytes to memory" (0x00000007)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address(lower32)
     * psScriptCB->pui32Script[4]  =   address(upper32)
     * psScriptCB->pui32Script[5]  =   size (in bytes)
     * psScriptCB->pui32Script[6]  =   data[1], data[2], data[3], data[4],
     * psScriptCB->pui32Script[7]  =   data[5], data[6], data[7], data[8],
     * ...
     * ...
     */

    // Check we have 64bit compatibility
    if (!pdl_CanRun64BitCommands(ADDR64(psScriptCB->pui32Script[3]))) {
        ERROR_OUTPUT("64bit commands cannot run on a 32bit system\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    VERBOSE_OUTPUT(LDB_MEM64, psScriptCB->pui32Script);

    CopyMem(ADDR64(psScriptCB->pui32Script[3]), &psScriptCB->pui32Script[6], ui32DataSize);

    /* Advance script pointer and exit (round byte count up and convert to word
     * count)  */
    ui32DataSize += 3;
    ui32DataSize >>= 2;
    psScriptCB->pui32Script += (6 + ui32DataSize);
}

void PDL_FillMem64(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "fill memory" (0x00000043)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address(lower32)
     * psScriptCB->pui32Script[4]  =   address(upper32)
     * psScriptCB->pui32Script[5]  =   byte
     * psScriptCB->pui32Script[6]  =   size (in bytes)
     */

    // Check we have 64bit compatibility
    if (!pdl_CanRun64BitCommands(ADDR64(psScriptCB->pui32Script[3]))) {
        ERROR_OUTPUT("64bit commands cannot run on a 32bit system\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    VERBOSE_OUTPUT(FILL_MEM64, psScriptCB->pui32Script);

    FillMem(ADDR64(psScriptCB->pui32Script[3]), (uint8_t)psScriptCB->pui32Script[5], psScriptCB->pui32Script[6]);

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 7;
}

void PDL_MemCircularBufferPoll64(PDL_psScriptCB psScriptCB)
{
    uint64_t ui64RoffAddr = ADDR64(psScriptCB->pui32Script[3]);
    uint32_t ui32WoffValue = psScriptCB->pui32Script[5];
    uint32_t ui32RequiredPacketSize = psScriptCB->pui32Script[6];
    uint32_t ui32CircularBufferSize = psScriptCB->pui32Script[7];
    uint32_t ui32RoffValue;
    uint32_t ui32FreeSpace;
    IMG_BOOL bContinue = IMG_TRUE;
    IMG_BOOL bCBPCompleted = IMG_FALSE;

    /*
     * psScriptCB->pui32Script[0]  =   "circular buffer poll" (0x0000000B)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address(lower32)
     * psScriptCB->pui32Script[4]  =   address(upper32)
     * psScriptCB->pui32Script[6]  =   write offset value
     * psScriptCB->pui32Script[7]  =   required packet size
     * psScriptCB->pui32Script[8]  =   circular buffer size
     */

    // Check we have 64bit compatibility
    if (!pdl_CanRun64BitCommands(ADDR64(psScriptCB->pui32Script[3]))) {
        ERROR_OUTPUT("64bit commands cannot run on a 32bit system\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    /* Is this the first time we execute this CBP command? */
    if (!psScriptCB->bPollPending) {
        psScriptCB->bPollPending = IMG_TRUE;
        psScriptCB->ui32LoopCount = 0;
        psScriptCB->ui32Delay = 10;

        VERBOSE_OUTPUT(CBP_MEM64, psScriptCB->pui32Script);
    }

    while (bContinue) {
        ReadMem(ui64RoffAddr, &ui32RoffValue);
        // bCBPCompleted = IMG_TRUE;bContinue = IMG_FALSE;break; // [cunrong]

        if (ui32RoffValue > ui32WoffValue) {
            ui32FreeSpace = ui32RoffValue - ui32WoffValue - 1;
        } else {
            ui32FreeSpace = (ui32RoffValue - ui32WoffValue) + (ui32CircularBufferSize - 1);
        }

        if (ui32FreeSpace >= ui32RequiredPacketSize) {
            bCBPCompleted = IMG_TRUE;
            bContinue = IMG_FALSE;
        } else if (gui32NumScripts > 1) {
            bCBPCompleted = IMG_FALSE;
            bContinue = IMG_FALSE;
        }
    }

    if (!bCBPCompleted) {
        /* Reset the time count to 0 */
        psScriptCB->ui32TimeCount = 0;
    } else {
        /* Ready to move on */
        psScriptCB->bPollPending = IMG_FALSE;

        /* Advance script pointer and exit */
        psScriptCB->pui32Script += 8;
    }
}

void PDL_WriteIntRegToRegister64(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "write intreg to register64" (0x00000035)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address(lower32)
     * psScriptCB->pui32Script[4]  =   address(upper32)
     * psScriptCB->pui32Script[5]  =   internal register ID
     */

    VERBOSE_OUTPUT(INTREG_WRW_REG64, psScriptCB->pui32Script);

    if (psScriptCB->pui32Script[5] < NUMBER_OF_INTERNAL_REGS) {
        WriteReg(ADDR64(psScriptCB->pui32Script[3]),
            (uint32_t)aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[5]]);
    } else {
        ERROR_OUTPUT("Register Number out of scope\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 6;
}

void PDL_WriteIntRegToMemory64(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "write intreg to memory 64" (0x0000002D)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address(lower32)
     * psScriptCB->pui32Script[4]  =   address(upper32)
     * psScriptCB->pui32Script[5]  =   internal register ID
     */

    VERBOSE_OUTPUT(INTREG_WRW_MEM64, psScriptCB->pui32Script);

    if (psScriptCB->pui32Script[5] < NUMBER_OF_INTERNAL_REGS) {
        WriteMem(ADDR64(psScriptCB->pui32Script[3]),
            (uint32_t)aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[5]]);
    } else {
        ERROR_OUTPUT("Register Number out of scope\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 6;
}

void PDL_ReadMemoryToIntReg64(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "read memory to intreg" (0x0000000F)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address(lower32)
     * psScriptCB->pui32Script[4]  =   address(upper32)
     * psScriptCB->pui32Script[5]  =   internal register ID
     */

    VERBOSE_OUTPUT(INTREG_RDW_MEM64, psScriptCB->pui32Script);

    if (psScriptCB->pui32Script[5] < NUMBER_OF_INTERNAL_REGS) {
        aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[5]] = 0;
        uint32_t read;
        ReadMem(ADDR64(psScriptCB->pui32Script[3]), &read);
        aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[5]] = read;
    } else {
        ERROR_OUTPUT("Register Number out of scope\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 6;
}

void PDL_ReadRegister64ToIntReg(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "read register64 to intreg" (0x0000000F)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address(lower32)
     * psScriptCB->pui32Script[4]  =   address(upper32)
     * psScriptCB->pui32Script[5]  =   internal register ID
     */

    VERBOSE_OUTPUT(INTREG_RDW_REG64, psScriptCB->pui32Script);

    if (psScriptCB->pui32Script[5] < NUMBER_OF_INTERNAL_REGS) {
        aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[5]] = 0;
        uint32_t read;
        ReadReg(ADDR64(psScriptCB->pui32Script[3]), &read);
        aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[5]] = read;
    } else {
        ERROR_OUTPUT("Internal Variable Number out of scope\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 6;
}

// SAB command, by default this command does nothing
void PDL_SaveBytesFromMemory64(PDL_psScriptCB psScriptCB)
{
    // uint64_t addr_64bit;
    /*
     * psScriptCB->pui32Script[0]  =   "Save Bytes 64" (0x00000032)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address(lower32)
     * psScriptCB->pui32Script[4]  =   address(upper32)
     * psScriptCB->pui32Script[5]  =   size
     */
    VERBOSE_OUTPUT(SAB_MEM64, psScriptCB->pui32Script);

    // This block of code generates an area of memory which contains all the SAB
    // and SII dumps This area of memory can be dumped to a binary file which the
    // PdumpExtractImage tool can interpret
    if (pui8SABOutput) {
        // if this is a new SAB line in the original pdump,
        // add a header of the 0x31 SAB id, file id, line number and size to the
        // save mem area
        if (psScriptCB->ui32SabFileID != psScriptCB->pui32Script[1]
            || psScriptCB->ui32SabLineNum != psScriptCB->pui32Script[2]) {
            if ((uintptr_t)pui8SABOutput & 0x3) {
                uint32_t skipbytes = 4 - ((uintptr_t)pui8SABOutput & 0x3);
                *pui32SABOrigin += skipbytes;
                pui8SABOutput += skipbytes; // skip to 4 byte alignment
            }

            //*(uint32_t*)pui8SABOutput = psScriptCB->pui32Script[0];
            *(uint32_t*)pui8SABOutput = 0x00000031;
            pui8SABOutput += sizeof(uint32_t);
            *(uint32_t*)pui8SABOutput = psScriptCB->pui32Script[1];
            pui8SABOutput += sizeof(uint32_t);
            *(uint32_t*)pui8SABOutput = psScriptCB->pui32Script[2];
            pui8SABOutput += sizeof(uint32_t);
            psScriptCB->ui32SabFileID = psScriptCB->pui32Script[1];
            psScriptCB->ui32SabLineNum = psScriptCB->pui32Script[2];

            // Create a size variable as the fourth header number
            psScriptCB->pui32SabSize = (uint32_t*)pui8SABOutput;
            psScriptCB->ui32SabSize = 0;
            *psScriptCB->pui32SabSize = psScriptCB->ui32SabSize;
            pui8SABOutput += sizeof(uint32_t);
            *pui32SABOrigin += 4 * sizeof(uint32_t);
        }

        // Copy the SAB data into the save mem area
        printf("SaveMem64 ...\n");
        SaveMem(pui8SABOutput, ADDR64(psScriptCB->pui32Script[3]), psScriptCB->pui32Script[5]);
        pui8SABOutput += psScriptCB->pui32Script[5];
        *pui32SABOrigin += psScriptCB->pui32Script[5];
        psScriptCB->ui32SabSize += psScriptCB->pui32Script[5];
        *psScriptCB->pui32SabSize = psScriptCB->ui32SabSize;
    }

    psScriptCB->pui32Script += 6;
}

/******************************************************************************/
/*                                                                            */
/*      64bit Address and 64bit Data Commands                                 */
/*                                                                            */
/******************************************************************************/
void PDL_Write64Register64(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "write register" (0x00000000)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address(lower32)
     * psScriptCB->pui32Script[4]  =   address(upper32)
     * psScriptCB->pui32Script[5]  =   value(lower32)
     * psScriptCB->pui32Script[6]  =   value(upper32)
     */

    // Check we have 64bit compatibility
    if (!pdl_CanRun64BitCommands(ADDR64(psScriptCB->pui32Script[3]))) {
        ERROR_OUTPUT("64bit commands cannot run on a 32bit system\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    VERBOSE_OUTPUT(WRW64_REG64, psScriptCB->pui32Script);

    WriteReg64(ADDR64(psScriptCB->pui32Script[3]), DATA64(psScriptCB->pui32Script[5]));

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 7;
}

void PDL_Write64Memory64(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "write memory" (0x00000001)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address(lower32)
     * psScriptCB->pui32Script[4]  =   address(upper32)
     * psScriptCB->pui32Script[5]  =   value(lower32)
     * psScriptCB->pui32Script[6]  =   value(upper32)
     */

    // Check we have 64bit compatibility
    if (!pdl_CanRun64BitCommands(ADDR64(psScriptCB->pui32Script[3]))) {
        ERROR_OUTPUT("64bit commands cannot run on a 32bit system\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    VERBOSE_OUTPUT(WRW64_MEM64, psScriptCB->pui32Script);

    WriteMem64(ADDR64(psScriptCB->pui32Script[3]), DATA64(psScriptCB->pui32Script[5]));

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 7;
}

void PDL_Read64Register64(PDL_psScriptCB psScriptCB)
{
    uint64_t ui64Temp;

    /*
     * psScriptCB->pui32Script[0]  =   "read register" (0x00000003)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address(lower32)
     * psScriptCB->pui32Script[4]  =   address(upper32)
     */

    // Check we have 64bit compatibility
    if (!pdl_CanRun64BitCommands(ADDR64(psScriptCB->pui32Script[3]))) {
        ERROR_OUTPUT("64bit commands cannot run on a 32bit system\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    VERBOSE_OUTPUT(RDW64_REG64, psScriptCB->pui32Script);

    ReadReg64(ADDR64(psScriptCB->pui32Script[3]), &ui64Temp);
    printf("ReadReg64 get ui64Temp :0x%llx from 0x%llx\n", ui64Temp, ADDR64(psScriptCB->pui32Script[3]));

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 5;
}

void PDL_Read64Memory64(PDL_psScriptCB psScriptCB)
{
    uint64_t ui64Temp;

    /*
     * psScriptCB->pui32Script[0]  =   "read memory" (0x00000004)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address(lower32)
     * psScriptCB->pui32Script[4]  =   address(upper32)
     */

    // Check we have 64bit compatibility
    if (!pdl_CanRun64BitCommands(ADDR64(psScriptCB->pui32Script[3]))) {
        ERROR_OUTPUT("64bit commands cannot run on a 32bit system\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    VERBOSE_OUTPUT(RDW64_MEM64, psScriptCB->pui32Script);

    ReadMem64(ADDR64(psScriptCB->pui32Script[3]), &ui64Temp);

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 5;
}

void PDL_Poll64Register64(PDL_psScriptCB psScriptCB)
{
    // Check we have 64bit compatibility
    if (!pdl_CanRun64BitCommands(ADDR64(psScriptCB->pui32Script[3]))) {
        ERROR_OUTPUT("64bit commands cannot run on a 32bit system\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    if (!psScriptCB->bPollPending) {
        VERBOSE_OUTPUT(POL64_REG64, psScriptCB->pui32Script);
    }

    PDL_Poll(psScriptCB, PDL_POLL64_REG64);
}

void PDL_Poll64Memory64(PDL_psScriptCB psScriptCB)
{
    // Check we have 64bit compatibility
    if (!pdl_CanRun64BitCommands(ADDR64(psScriptCB->pui32Script[3]))) {
        ERROR_OUTPUT("64bit commands cannot run on a 32bit system\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    if (!psScriptCB->bPollPending) {
        VERBOSE_OUTPUT(POL64_MEM64, psScriptCB->pui32Script);
    }

    PDL_Poll(psScriptCB, PDL_POLL64_MEM64);
}

void PDL_Write64IntRegToRegister64(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "write intreg to register64" (0x00000035)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address(lower32)
     * psScriptCB->pui32Script[4]  =   address(upper32)
     * psScriptCB->pui32Script[5]  =   internal register ID
     */

    VERBOSE_OUTPUT(INTREG_WRW64_REG64, psScriptCB->pui32Script);

    if (psScriptCB->pui32Script[5] < NUMBER_OF_INTERNAL_REGS) {
        WriteReg64(
            ADDR64(psScriptCB->pui32Script[3]), aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[5]]);
    } else {
        ERROR_OUTPUT("Register Number out of scope\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 6;
}

void PDL_Write64IntRegToMemory64(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "write intreg to memory 64" (0x0000002D)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address(lower32)
     * psScriptCB->pui32Script[4]  =   address(upper32)
     * psScriptCB->pui32Script[5]  =   internal register ID
     */

    VERBOSE_OUTPUT(INTREG_WRW64_MEM64, psScriptCB->pui32Script);

    if (psScriptCB->pui32Script[5] < NUMBER_OF_INTERNAL_REGS) {
        WriteMem64(
            ADDR64(psScriptCB->pui32Script[3]), aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[5]]);
    } else {
        ERROR_OUTPUT("Register Number out of scope\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 6;
}

void PDL_Read64Memory64ToIntReg(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "read memory to intreg" (0x0000000F)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address(lower32)
     * psScriptCB->pui32Script[4]  =   address(upper32)
     * psScriptCB->pui32Script[5]  =   internal register ID
     */

    VERBOSE_OUTPUT(INTREG_RDW64_MEM64, psScriptCB->pui32Script);

    if (psScriptCB->pui32Script[5] < NUMBER_OF_INTERNAL_REGS) {
        aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[5]] = 0;
        ReadMem64(
            ADDR64(psScriptCB->pui32Script[3]), &aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[5]]);
    } else {
        ERROR_OUTPUT("Register Number out of scope\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 6;
}

void PDL_Read64Register64ToIntReg(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "read register64 to intreg" (0x0000000F)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address(lower32)
     * psScriptCB->pui32Script[4]  =   address(upper32)
     * psScriptCB->pui32Script[5]  =   internal register ID
     */

    VERBOSE_OUTPUT(INTREG_RDW64_REG64, psScriptCB->pui32Script);

    if (psScriptCB->pui32Script[5] < NUMBER_OF_INTERNAL_REGS) {
        aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[5]] = 0;
        ReadReg64(
            ADDR64(psScriptCB->pui32Script[3]), &aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[5]]);
    } else {
        ERROR_OUTPUT("Internal Variable Number out of scope\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 6;
}

/******************************************************************************/
/*                                                                            */
/*      Internal Register commands                                            */
/*                                                                            */
/******************************************************************************/

void PDL_IntRegThreeOperandCmd_generic(PDL_psScriptCB psScriptCB, PDL_pfnIntRegOpFunction pfnIntRegOpFunction,
    uint32_t fileID, uint32_t destIntRegID, uint32_t opIntRegID, uint64_t lastOperand, IMG_BOOL isRegLastOperand)
{
    /*
     * psScriptCB->pui32Script[0]  =   "intreg command" (0x00000010 - 0x0000001D)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   bIsRegLastOperand
     * psScriptCB->pui32Script[4]  =   dest intreg ID
     * psScriptCB->pui32Script[5]  =   op intreg ID
     * psScriptCB->pui32Script[6]  =   last operand
     */

    if (isRegLastOperand) /* If the last operand is a register */
    {
        uint32_t lastoperand32 = (uint32_t)lastOperand;
        if ((destIntRegID < NUMBER_OF_INTERNAL_REGS) && (opIntRegID < NUMBER_OF_INTERNAL_REGS)
            && (lastoperand32 < NUMBER_OF_INTERNAL_REGS)) {
            aui64InternalRegisters[fileID][destIntRegID] = pfnIntRegOpFunction(
                aui64InternalRegisters[fileID][opIntRegID], aui64InternalRegisters[fileID][lastoperand32]);
        } else {
            ERROR_OUTPUT("Incorrect Command\n");
            PDL_ASSERT(psScriptCB->pui32Script);
        }
    } else /* Last operand is an immediate data value */
    {
        if ((destIntRegID < NUMBER_OF_INTERNAL_REGS) && (opIntRegID < NUMBER_OF_INTERNAL_REGS)) {
            aui64InternalRegisters[fileID][destIntRegID]
                = pfnIntRegOpFunction(aui64InternalRegisters[fileID][opIntRegID], DATA64(lastOperand));
        } else {
            ERROR_OUTPUT("Incorrect Command\n");
            PDL_ASSERT(psScriptCB->pui32Script);
        }
    }
}

void PDL_IntRegThreeOperandCmd_DEPRECATED(
    PDL_psScriptCB psScriptCB, PDL_pfnIntRegOpFunction pfnIntRegOpFunction, IMG_BOOL intRegOperand)
{
    /*
     * psScriptCB->pui32Script[0]  =   "intreg command" (0x00000010 - 0x0000001D)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   bIsRegLastOperand
     * psScriptCB->pui32Script[4]  =   dest intreg ID
     * psScriptCB->pui32Script[5]  =   op intreg ID
     * psScriptCB->pui32Script[6]  =   last operand
     */

    PDL_IntRegThreeOperandCmd_generic(psScriptCB, pfnIntRegOpFunction, psScriptCB->pui32Script[1],
        psScriptCB->pui32Script[4], psScriptCB->pui32Script[5], psScriptCB->pui32Script[6], intRegOperand);
    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 7;
}

void PDL_IntRegThreeOperandCmd(
    PDL_psScriptCB psScriptCB, PDL_pfnIntRegOpFunction pfnIntRegOpFunction, IMG_BOOL intRegOperand)
{
    /*
     * psScriptCB->pui32Script[0]  =   "intreg command" (0x00000010 - 0x0000001D)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   dest intreg ID
     * psScriptCB->pui32Script[4]  =   op intreg ID
     * psScriptCB->pui32Script[5]  =   last operand (lower32) / op2 intreg id
     * psScriptCB->pui32Script[6]  =   last operand (upper32) / Does not exist! so
     * script is now 6 long instead of 7
     */

    PDL_IntRegThreeOperandCmd_generic(psScriptCB, pfnIntRegOpFunction, psScriptCB->pui32Script[1],
        psScriptCB->pui32Script[3], psScriptCB->pui32Script[4], DATA64(psScriptCB->pui32Script[5]), intRegOperand);

    if (!intRegOperand) {
        psScriptCB->pui32Script += 7;
    } else {
        /* Advance script pointer and exit */
        psScriptCB->pui32Script += 6;
    }
}

void PDL_WriteIntRegToRegister(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "write intreg to register" (0x0000000C)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address
     * psScriptCB->pui32Script[4]  =   internal register ID
     */

    VERBOSE_OUTPUT(INTREG_WRW_REG, psScriptCB->pui32Script);

    if (psScriptCB->pui32Script[4] < NUMBER_OF_INTERNAL_REGS) {
        WriteReg(
            psScriptCB->pui32Script[3], (uint32_t)aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[4]]);
    } else {
        ERROR_OUTPUT("Register Number out of scope\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 5;
}

void PDL_WriteIntRegToMemory(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "write intreg to register" (0x0000000D)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address
     * psScriptCB->pui32Script[4]  =   internal register ID
     */

    VERBOSE_OUTPUT(INTREG_WRW_MEM, psScriptCB->pui32Script);

    if (psScriptCB->pui32Script[4] < NUMBER_OF_INTERNAL_REGS) {
        WriteMem(
            psScriptCB->pui32Script[3], (uint32_t)aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[4]]);
    } else {
        ERROR_OUTPUT("Register Number out of scope\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 5;
}

void PDL_ReadRegisterToIntReg(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "read register to intreg" (0x0000000E)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address
     * psScriptCB->pui32Script[4]  =   internal register ID
     */

    VERBOSE_OUTPUT(INTREG_RDW_REG, psScriptCB->pui32Script);

    if (psScriptCB->pui32Script[4] < NUMBER_OF_INTERNAL_REGS) {
        aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[4]] = 0;
        ReadReg(psScriptCB->pui32Script[3], &aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[4]]);
    } else {
        ERROR_OUTPUT("Register Number out of scope\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 5;
}

void PDL_ReadMemoryToIntReg(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "read memory to intreg" (0x0000000F)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   address
     * psScriptCB->pui32Script[4]  =   internal register ID
     */

    VERBOSE_OUTPUT(INTREG_RDW_MEM, psScriptCB->pui32Script);
    if (psScriptCB->pui32Script[4] < NUMBER_OF_INTERNAL_REGS) {
        aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[4]] = 0;
        ReadMem(psScriptCB->pui32Script[3], &aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[4]]);
    } else {
        ERROR_OUTPUT("Register Number out of scope\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 5;
}

void PDL_IntRegMOV(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "intreg move" (0x00000010)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   bIsRegLastOperand
     * psScriptCB->pui32Script[4]  =   dest intreg ID
     * psScriptCB->pui32Script[5]  =   last operand
     */

    VERBOSE_OUTPUT(INTREG_MOV, psScriptCB->pui32Script);

    if (psScriptCB->pui32Script[3]) /* If the last operand is a register */
    {
        if ((psScriptCB->pui32Script[4] < NUMBER_OF_INTERNAL_REGS)
            && (psScriptCB->pui32Script[5] < NUMBER_OF_INTERNAL_REGS)) {
            aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[4]]
                = aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[5]];
        } else {
            ERROR_OUTPUT("Register Number out of scope\n");
            PDL_ASSERT(psScriptCB->pui32Script);
        }
    } else /* Last operand is an immediate data value */
    {
        if (psScriptCB->pui32Script[4] < NUMBER_OF_INTERNAL_REGS) {
            aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[4]] = psScriptCB->pui32Script[5];
        } else {
            ERROR_OUTPUT("Register Number out of scope\n");
            PDL_ASSERT(psScriptCB->pui32Script);
        }
    }

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 6;
}

void PDL_IntRegMOV64(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "intreg move" (0x00000010)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   dest intreg ID
     * psScriptCB->pui32Script[4]  =   upper 32bits of word
     * psScriptCB->pui32Script[5]  =   lower 32bits of word
     */

    // Check we have 64bit compatibility
    if (!pdl_CanRun64BitCommands(ADDR64(psScriptCB->pui32Script[4]))) {
        ERROR_OUTPUT("64bit commands cannot run on a 32bit system\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    VERBOSE_OUTPUT(INTREG_MOV64, psScriptCB->pui32Script);

    if (psScriptCB->pui32Script[3] < NUMBER_OF_INTERNAL_REGS) {
        aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[3]] = ADDR64(psScriptCB->pui32Script[4]);
    } else {
        ERROR_OUTPUT("Register Number out of scope\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 6;
}

void PDL_IntRegMOV_IntReg(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "intreg move" (0x00000010)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   dest intreg ID
     * psScriptCB->pui32Script[4]  =   src intreg ID
     */

    VERBOSE_OUTPUT(INTREG_MOV_INTREG, psScriptCB->pui32Script);

    if ((psScriptCB->pui32Script[3] < NUMBER_OF_INTERNAL_REGS)
        && (psScriptCB->pui32Script[4] < NUMBER_OF_INTERNAL_REGS)) {
        aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[3]]
            = aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[4]];
    } else {
        ERROR_OUTPUT("Register Number out of scope\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 5;
}

void PDL_IntRegAND(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_AND, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd_DEPRECATED(psScriptCB, &PDL_IntRegOpAND, IMG_FALSE);
}

void PDL_IntRegOR(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_OR, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd_DEPRECATED(psScriptCB, &PDL_IntRegOpOR, IMG_FALSE);
}

void PDL_IntRegXOR(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_XOR, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd_DEPRECATED(psScriptCB, &PDL_IntRegOpXOR, IMG_FALSE);
}

void PDL_IntRegNOT(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "intreg NOT" (0x00000014)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   dest intreg ID
     * psScriptCB->pui32Script[4]  =   op intreg ID
     */

    VERBOSE_OUTPUT(INTREG_NOT, psScriptCB->pui32Script);

    if ((psScriptCB->pui32Script[3] < NUMBER_OF_INTERNAL_REGS)
        && (psScriptCB->pui32Script[4] < NUMBER_OF_INTERNAL_REGS)) {
        aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[3]]
            = ~aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[4]];
    } else {
        ERROR_OUTPUT("Register Number out of scope\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 5;
}

void PDL_IntRegSHR(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_SHR, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd_DEPRECATED(psScriptCB, &PDL_IntRegOpSHR, IMG_FALSE);
}

void PDL_IntRegSHL(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_SHL, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd_DEPRECATED(psScriptCB, &PDL_IntRegOpSHL, IMG_FALSE);
}

void PDL_IntRegADD(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_ADD, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd_DEPRECATED(psScriptCB, &PDL_IntRegOpADD, IMG_FALSE);
}

void PDL_IntRegSUB(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_SUB, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd_DEPRECATED(psScriptCB, &PDL_IntRegOpSUB, IMG_FALSE);
}

void PDL_IntRegMUL(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_MUL, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd_DEPRECATED(psScriptCB, &PDL_IntRegOpMUL, IMG_FALSE);
}

void PDL_IntRegIMUL(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_IMUL, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd_DEPRECATED(psScriptCB, &PDL_IntRegOpIMUL, IMG_FALSE);
}

void PDL_IntRegDIV(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_DIV, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd_DEPRECATED(psScriptCB, &PDL_IntRegOpDIV, IMG_FALSE);
}

void PDL_IntRegIDIV(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_IDIV, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd_DEPRECATED(psScriptCB, &PDL_IntRegOpIDIV, IMG_FALSE);
}

void PDL_IntRegMOD(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_MOD, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd_DEPRECATED(psScriptCB, &PDL_IntRegOpMOD, IMG_FALSE);
}

void PDL_IntRegEQU(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_EQU, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd_DEPRECATED(psScriptCB, &PDL_IntRegOpEQU, IMG_FALSE);
}

void PDL_IntRegGT(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_GT, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd_DEPRECATED(psScriptCB, &PDL_IntRegOpGT, IMG_FALSE);
}

void PDL_IntRegGTE(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_GTE, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd_DEPRECATED(psScriptCB, &PDL_IntRegOpGTE, IMG_FALSE);
}

void PDL_IntRegLT(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_LT, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd_DEPRECATED(psScriptCB, &PDL_IntRegOpLT, IMG_FALSE);
}

void PDL_IntRegLTE(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_LTE, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd_DEPRECATED(psScriptCB, &PDL_IntRegOpLTE, IMG_FALSE);
}

void PDL_IntRegNEQ(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_NEQ, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd_DEPRECATED(psScriptCB, &PDL_IntRegOpNEQ, IMG_FALSE);
}

// new version 2 commands

void PDL_IntRegMOV2(PDL_psScriptCB psScriptCB)
{
    /*
     * psScriptCB->pui32Script[0]  =   "intreg move" (0x00000010)
     * psScriptCB->pui32Script[1]  =   file id
     * psScriptCB->pui32Script[2]  =   line number
     * psScriptCB->pui32Script[3]  =   dest intreg ID
     * psScriptCB->pui32Script[4]  =   data (lower32)
     * psScriptCB->pui32Script[5]  =   data (upper32)
     */

    VERBOSE_OUTPUT(INTREG_MOV, psScriptCB->pui32Script);

    if (psScriptCB->pui32Script[3] < NUMBER_OF_INTERNAL_REGS) {
        aui64InternalRegisters[PDL_SCRIPT_NO][psScriptCB->pui32Script[3]] = DATA64(psScriptCB->pui32Script[4]);
    } else {
        ERROR_OUTPUT("Register Number out of scope\n");
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    /* Advance script pointer and exit */
    psScriptCB->pui32Script += 6;
}

void PDL_IntRegAND2(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_AND2, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpAND, IMG_FALSE);
}

void PDL_IntRegOR2(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_OR2, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpOR, IMG_FALSE);
}

void PDL_IntRegXOR2(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_XOR2, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpXOR, IMG_FALSE);
}

void PDL_IntRegSHR2(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_SHR2, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpSHR, IMG_FALSE);
}

void PDL_IntRegSHL2(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_SHL2, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpSHL, IMG_FALSE);
}

void PDL_IntRegADD2(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_ADD2, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpADD, IMG_FALSE);
}

void PDL_IntRegSUB2(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_SUB2, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpSUB, IMG_FALSE);
}

void PDL_IntRegMUL2(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_MUL2, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpMUL, IMG_FALSE);
}

void PDL_IntRegIMUL2(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_IMUL2, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpIMUL, IMG_FALSE);
}

void PDL_IntRegDIV2(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_DIV2, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpDIV, IMG_FALSE);
}

void PDL_IntRegIDIV2(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_IDIV2, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpIDIV, IMG_FALSE);
}

void PDL_IntRegMOD2(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_MOD2, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpMOD, IMG_FALSE);
}

void PDL_IntRegEQU2(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_EQU2, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpEQU, IMG_FALSE);
}

void PDL_IntRegGT2(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_GT2, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpGT, IMG_FALSE);
}

void PDL_IntRegGTE2(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_GTE2, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpGTE, IMG_FALSE);
}

void PDL_IntRegLT2(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_LT2, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpLT, IMG_FALSE);
}

void PDL_IntRegLTE2(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_LTE2, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpLTE, IMG_FALSE);
}

void PDL_IntRegNEQ2(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_NEQ2, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpNEQ, IMG_FALSE);
}

// Int Var operators with int var last parameter

void PDL_IntRegAND_IntReg(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_AND_INTREG, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpAND, IMG_TRUE);
}

void PDL_IntRegOR_IntReg(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_OR_INTREG, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpOR, IMG_TRUE);
}

void PDL_IntRegXOR_IntReg(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_XOR_INTREG, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpXOR, IMG_TRUE);
}

void PDL_IntRegSHR_IntReg(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_SHR_INTREG, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpSHR, IMG_TRUE);
}

void PDL_IntRegSHL_IntReg(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_SHL_INTREG, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpSHL, IMG_TRUE);
}

void PDL_IntRegADD_IntReg(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_ADD_INTREG, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpADD, IMG_TRUE);
}

void PDL_IntRegSUB_IntReg(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_SUB_INTREG, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpSUB, IMG_TRUE);
}

void PDL_IntRegMUL_IntReg(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_MUL_INTREG, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpMUL, IMG_TRUE);
}

void PDL_IntRegIMUL_IntReg(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_IMUL_INTREG, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpIMUL, IMG_TRUE);
}

void PDL_IntRegDIV_IntReg(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_DIV_INTREG, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpDIV, IMG_TRUE);
}

void PDL_IntRegIDIV_IntReg(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_IDIV_INTREG, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpIDIV, IMG_TRUE);
}

void PDL_IntRegMOD_IntReg(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_MOD_INTREG, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpMOD, IMG_TRUE);
}

void PDL_IntRegEQU_IntReg(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_EQU_INTREG, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpEQU, IMG_TRUE);
}

void PDL_IntRegGT_IntReg(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_GT_INTREG, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpGT, IMG_TRUE);
}

void PDL_IntRegGTE_IntReg(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_GTE_INTREG, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpGTE, IMG_TRUE);
}

void PDL_IntRegLT_IntReg(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_LT_INTREG, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpLT, IMG_TRUE);
}

void PDL_IntRegLTE_IntReg(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_LTE_INTREG, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpLTE, IMG_TRUE);
}

void PDL_IntRegNEQ_IntReg(PDL_psScriptCB psScriptCB)
{
    VERBOSE_OUTPUT(INTREG_NEQ_INTREG, psScriptCB->pui32Script);

    PDL_IntRegThreeOperandCmd(psScriptCB, &PDL_IntRegOpNEQ, IMG_TRUE);
}

/******************************************************************************/
/*                                                                            */
/*      Startup and script processing loop functions                          */
/*                                                                            */
/******************************************************************************/
void CheckSignature(uint32_t* pui32SigAddr, uint32_t ui32Sig)
{
    if ((((uintptr_t)pui32SigAddr) & 0x00000003) != 0x00000000) {
        /* Not 32 bit aligned */
        printf("\033[031m%s():%d\033[0m\n", __func__, __LINE__);
        ERROR_OUTPUT("Not 32-bit Aligned\n");
        PDL_ASSERT(IMG_FALSE);
    } else if ((*pui32SigAddr & PDL_SIGNATURE_MASK) != ui32Sig) {
        /* Not the expected signature */
        printf("\033[031m%s():%d\033[0m\n", __func__, __LINE__);
        ERROR_OUTPUT("Not the expected Signature\n");
        PDL_ASSERT(IMG_FALSE);
    }
}

/******************************************************************************/

void CheckValidScript(uint32_t* pui32ScriptPointer)
{
    uint32_t* pui32;

    pui32 = pui32ScriptPointer;

    /* Possible to check for valid pointer? */
    // if ( 0 /* Check pui32 for validity ? */ )
    //{
    //  /* Placeholder for address validity check failure */
    //  ERROR_OUTPUT("Placeholder for address validity check failure\n");
    //    PDL_ASSERT(IMG_FALSE);
    //}
    // else
    {
        /* Check 1st signature valid */
        CheckSignature(pui32, PDL_SIGNATURE1);

        /* 1st signature valid. Move to where the 2nd signature should be and check.
         */
        pui32 += pui32[1];
        pui32 += 2;
        CheckSignature(pui32, PDL_SIGNATURE2);
    }
}

/******************************************************************************/

/*!
 ********************************************************************************
 *
 * @Function              ProcessScripts
 *
 *******************************************************************************/
void ProcessScripts(void)
{
    printf("entered IMG ProcessScripts \n");
    uint32_t i, ui32ScriptFnIndex, ui32MinWait = PDL_POLLCOUNT_INFINITE;
    IMG_BOOL bAllWaiting, bAllAtEOF = IMG_FALSE, bAllWaitOrPoll = IMG_FALSE;

    while (!bAllAtEOF) {
        if (bAllWaitOrPoll) {
            ui32MinWait = PDL_POLLCOUNT_INFINITE;
        }

        /* Process next command from PDUMP files...*/
        for (uint32_t i = 0; i < gui32NumScripts; i++) {
            // printf("%s():%d i:%d NumScripts:%d\n", __FUNCTION__,__LINE__, i, gui32NumScripts);

            if (!asScriptCB[i].bEOF && !asScriptCB[i].bWaiting) {
                if (asScriptCB[i].pui32Script == asScriptCB[i].pui32ScriptBase) {
                    CheckValidScript(asScriptCB[i].pui32Script);
                    asScriptCB[i].pui32Script += 2;
                }

                /* If all are waiting on polls or syncs find the shortest waiting time
                 */
                if (bAllWaitOrPoll) {
                    uint32_t ui32WaitTime = 0;

                    if (asScriptCB[i].ui32TimeCount < asScriptCB[i].ui32Delay) {
                        ui32WaitTime = asScriptCB[i].ui32Delay - asScriptCB[i].ui32TimeCount;
                    }

                    if (ui32WaitTime < ui32MinWait) {
                        ui32MinWait = ui32WaitTime;
                    }
                }

                if (!bAllWaitOrPoll || (ui32MinWait == 0)) {
                    if ((*asScriptCB[i].pui32Script & PDL_SIGNATURE_MASK) != PDL_SIGNATURE2) {
                        ui32ScriptFnIndex = (*asScriptCB[i].pui32Script) & ~TAL_PDUMPL_NO_OPTIMISE;
                        if (ui32ScriptFnIndex <= ui32MaxScriptFunction) {
                            if (asScriptCB[i].bPollPending) {
                                asScriptCB[i].ui32TimeCount++;

                                if (asScriptCB[i].ui32TimeCount >= asScriptCB[i].ui32Delay) {
                                    printf("%s():%d  --> pfnScriptFunction[%d]( asScriptCB[%d] )\n", __FUNCTION__,__LINE__, ui32ScriptFnIndex, i);
                                    (pfnScriptFunction[ui32ScriptFnIndex])(&asScriptCB[i]);
                                }
                            } else {
                                /* Save file id and line number in case of failure */
                                asScriptCB[i].ui32FileID = asScriptCB[i].pui32Script[1];
                                asScriptCB[i].ui32LineNum = asScriptCB[i].pui32Script[2];
                                // printf("=============asScriptCB[%d].ui32LineNum:%d \n ", i, asScriptCB[i].ui32LineNum);
                                printf("=============\n ");
                                printf("LineNum= %d start \n ", asScriptCB[i].ui32LineNum);
                                // printf("%s():%d  --> pfnScriptFunction[%d]( asScriptCB[%d] )\n", __FUNCTION__,__LINE__, ui32ScriptFnIndex, i);
                                (pfnScriptFunction[ui32ScriptFnIndex])(&asScriptCB[i]);
                                printf("LineNum= %d end \n ", asScriptCB[i].ui32LineNum);
                                printf("\n");
                            }
                        } else {
                            PDL_ASSERT(asScriptCB[i].pui32Script);
                        }
                    } else {
                        asScriptCB[i].bEOF = IMG_TRUE;
                    }
                }
            }
        }

        /* If they are all waiting then wait till next crucial time point */
        if ((bAllWaitOrPoll == IMG_TRUE) && (ui32MinWait != 0)) {
            for (i = 0; i < gui32NumScripts; i++) {
                /* If this script is not at EOF and not waiting for sync...*/
                if (!asScriptCB[i].bEOF && !asScriptCB[i].bWaiting) {
                    if (!asScriptCB[i].bPollPending) {
                        PDL_ASSERT(asScriptCB[i].pui32Script);
                    }

                    asScriptCB[i].ui32TimeCount += ui32MinWait;
                }
            }

            /* Wait appropriate time */
            Delay(ui32MinWait);
        }

        /* Check if all files are at EOF or are waiting for a SYNC ...*/
        bAllAtEOF = IMG_TRUE;
        bAllWaiting = IMG_TRUE;
        bAllWaitOrPoll = IMG_TRUE;

        for (i = 0; i < gui32NumScripts; i++) {
            // printf("%s():%d i:%d NumScripts:%d\n", __FUNCTION__,__LINE__, i, gui32NumScripts);

            bAllAtEOF = bAllAtEOF && asScriptCB[i].bEOF;
            bAllWaiting = bAllWaiting && (asScriptCB[i].bWaiting);
            bAllWaitOrPoll
                = bAllWaitOrPoll && (asScriptCB[i].bEOF || asScriptCB[i].bWaiting || asScriptCB[i].bPollPending);
        }

        /* If we have found a condition of all the scripts being stuck */
        if (bAllWaiting) {
            PDL_ASSERT(asScriptCB[i].pui32Script);
        }
    }

    PDL_SUCCESS();    
    
}

/*!
 ********************************************************************************
 *
 * @Function              GetSystemSyncId
 *
 *******************************************************************************/
static uint32_t GetSystemSyncId(void) { return _ui32SysSyncId++; }

/*!
 ********************************************************************************
 *
 * @Function              GetScript
 *
 *******************************************************************************/
static PDL_psScriptCB GetScript(uint32_t ui32ScriptId, uint32_t ui32TestId)
{
    PDL_psScriptCB psScriptCB = NULL;
    uint32_t i = 1;

    /* Count through to the correct script Number */
    for (i = 0; i < gui32NumScripts; i++) {
        psScriptCB = &asScriptCB[i];

        if ((ui32ScriptId == psScriptCB->ui32ScriptId) && (ui32TestId == psScriptCB->ui32TestId)) {
            break;
        }
    }

    if (i >= gui32NumScripts) {
        PDL_ASSERT(psScriptCB->pui32Script);
        return NULL;
    }
    return psScriptCB;
}

/*!
 ********************************************************************************
 *
 * @Function              DisableSync
 *
 *******************************************************************************/
static void DisableSync(uint32_t ui32SyncId, uint32_t ui32TestId)
{
    uint32_t i;

    for (i = 0; i < gui32NumScripts; i++) {
        PDL_psScriptCB psScriptCB = &asScriptCB[i];

        if (psScriptCB->ui32TestId == ui32TestId) {
            psScriptCB->ui32DisabledSyncs |= (1 << ui32SyncId);
        }
    }
}

/*!
 ********************************************************************************
 *
 * @Function              GetSysSyncId
 *
 *******************************************************************************/
uint32_t GetSysSyncId(PDL_psScriptCB psScriptCB, uint32_t ui32SyncId)
{
    PDL_psTestCB psTestCB = &asTestCB[psScriptCB->ui32TestId - 1];
    uint32_t ui32SysSyncId;

    /*Check for a valid Sync Id  */
    if (ui32SyncId >= MAX_SYNC_IDS) {
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    /* Check to see if this sync is disabled */
    if (psScriptCB->ui32DisabledSyncs & (1 << ui32SyncId)) {
        return 0;
    }

    /* Find the system sync Id or get a new one */
    ui32SysSyncId = psTestCB->aui32SystemSyncId[ui32SyncId];
    if (ui32SysSyncId == 0) {
        ui32SysSyncId = GetSystemSyncId();
        psTestCB->aui32SystemSyncId[ui32SyncId] = ui32SysSyncId;
    }
    return ui32SysSyncId;
}

/*!
 ******************************************************************************
 *
 * @Function              IntraSync
 *
 ******************************************************************************/
void IntraSync(uint32_t ui32TestId, uint32_t ui32SyncId, uint32_t ui32ScriptId)
{
    PDL_psScriptCB psScriptCB = NULL;
    PDL_psTestCB psTestCB;

    /* Check the sync id is valid */
    if (ui32SyncId >= 32) {
        PDL_ASSERT(psScriptCB->pui32Script);
    }

    /* Locate the test */
    psTestCB = &asTestCB[ui32TestId - 1];

    /* Check that this sync Id has not already been used */
    if (psTestCB->aui32SystemSyncId[ui32SyncId] == 0) {
        /* Set a system sync Id */
        psTestCB->aui32SystemSyncId[ui32SyncId] = GetSystemSyncId();

        /* Disable Syncs */
        DisableSync(ui32SyncId, ui32TestId);
    }

    /* Find Defined Script */
    psScriptCB = GetScript(ui32ScriptId, ui32TestId);

    /* Enable this script */
    psScriptCB->ui32DisabledSyncs = psScriptCB->ui32DisabledSyncs & (~(1 << ui32SyncId));
}

/*!
 ******************************************************************************
 *
 * @Function              InterSync
 *
 ******************************************************************************/
void InterSync(uint32_t ui32TestId1, uint32_t ui32Sync1, uint32_t ui32TestId2, uint32_t ui32Sync2)
{
    PDL_psTestCB psTestCB1, psTestCB2;
    uint32_t ui32SysSync;

    /* Check the sync ids are valid */
    if ((ui32Sync1 >= NO_SYSSYNC_IDS) || (ui32Sync1 < MAX_SYNC_IDS) || (ui32Sync2 >= NO_SYSSYNC_IDS)
        || (ui32Sync2 < MAX_SYNC_IDS)) {
        PDL_ASSERT(IMG_FALSE);
    }

    /* Locate the tests */
    psTestCB1 = &asTestCB[ui32TestId1 - 1];
    psTestCB2 = &asTestCB[ui32TestId2 - 1];

    /* Check to see if one of the tests already has a Sync Id defined */
    if (psTestCB1->aui32SystemSyncId[ui32Sync1] != 0) {
        /* If one of the Ids is already set the other shouldn't be */
        if (psTestCB2->aui32SystemSyncId[ui32Sync2] != 0) {
            PDL_ASSERT(IMG_FALSE);
        }

        ui32SysSync = psTestCB1->aui32SystemSyncId[ui32Sync1];
    } else if (psTestCB2->aui32SystemSyncId[ui32Sync2] != 0) {
        /* If one of the Ids is already set the other shouldn't be */
        if (psTestCB1->aui32SystemSyncId[ui32Sync1] != 0) {
            ERROR_OUTPUT("Script Function No out of range\n");
            PDL_ASSERT(IMG_FALSE);
        }

        ui32SysSync = psTestCB2->aui32SystemSyncId[ui32Sync2];
    } else {
        /* Neither is defined so get a new one */
        ui32SysSync = GetSystemSyncId();
    }

    /* Set the system Sync Ids to the same value */
    psTestCB1->aui32SystemSyncId[ui32Sync1] = ui32SysSync;
    psTestCB2->aui32SystemSyncId[ui32Sync2] = ui32SysSync;
}

/*!
 ******************************************************************************
 *
 * @Function              InitialiseScript
 *
 ******************************************************************************/
void InitialiseScript(PDL_psScriptCB psScriptCB, uint32_t ui32TestId, uint32_t ui32ScriptId, uint32_t* pui32StartLocation)
{
    psScriptCB->ui32ScriptId = ui32ScriptId;
    psScriptCB->ui32TestId = ui32TestId;

    psScriptCB->pui32ScriptBase = pui32StartLocation;
    psScriptCB->pui32Script = pui32StartLocation;
    psScriptCB->bEOF = IMG_FALSE;

    psScriptCB->ui32FileID = 0xFFFFFFFF;
    psScriptCB->ui32LineNum = 0;

    psScriptCB->ui32DisabledSyncs = 0;
    psScriptCB->bWaiting = IMG_FALSE;
    psScriptCB->ui32SyncWaiting = 0;
    psScriptCB->ui32SemaWaiting = 0;

    psScriptCB->bPollPending = IMG_FALSE;
    psScriptCB->ui32LoopCount = 0;
    psScriptCB->ui32TimeCount = 0;

    psScriptCB->ui32SabFileID = 0xFFFFFFFF;
    psScriptCB->ui32SabLineNum = 0;

    psScriptCB->pvInfo = NULL;
}

/*!
 ******************************************************************************
 *
 * @Function              PreprocessScripts
 *
 ******************************************************************************/
IMG_BOOL
PreprocessScripts(void)
{
    printf("entered IMG_BOOL PreprocessScripts. 0x%x vs 0x%x, 0x%x \n", 
        *(uint32_t*)pvScriptPointer, PDL_SIGNATURE_MASK, PDL_SIGNATURE_COMPSCRIPT_HSTART);

    uint32_t* pui32ScriptBase = (uint32_t*)pvScriptPointer;
    uint32_t* pui32ScriptPointer = pui32ScriptBase;

    /* Single script */
    if ((*pui32ScriptPointer & PDL_SIGNATURE_MASK) == PDL_SIGNATURE1) {
        printf("%s():%d run single script\n", __FUNCTION__,__LINE__);
        InitialiseScript(&asScriptCB[0], 1, 1, pui32ScriptBase);
        gui32NumScripts = 1;
    }
    /* PdumpL header present */
    else if (*pui32ScriptPointer == PDL_SIGNATURE_COMPSCRIPT_HSTART) {
        printf("%s():%d %d vs %d\n", __FUNCTION__,__LINE__, *pui32ScriptPointer, PDL_SIGNATURE_COMPSCRIPT_HSTART);
        uint32_t ui32TestId, ui32HSize, ui32NumSyncs, ui32SyncIndex;

        ui32HSize = pui32ScriptPointer[1];
        gui32NumTests = pui32ScriptPointer[2];

        printf("%s():%d ui32HSize:%d, gui32NumTests:%d\n", __FUNCTION__,__LINE__, ui32HSize, gui32NumTests);

        if (gui32NumTests > MAX_PDUMPLTESTS) {
            PDL_ASSERT(IMG_FALSE);
            return IMG_FALSE;
        }

        /* Firstly check that the HEND signature is located where it is expected */
        if (pui32ScriptBase[ui32HSize - 1] != PDL_SIGNATURE_COMPSCRIPT_HEND) {
            PDL_ASSERT(IMG_FALSE);
            return IMG_FALSE;
        }

        pui32ScriptPointer += 3;

        /* For each one of the tests */
        for (ui32TestId = 1; ui32TestId <= gui32NumTests; ui32TestId++) {
            uint32_t ui32Index;
            uint32_t ui32ScriptId;
            uint32_t ui32NumScripts = pui32ScriptPointer[0];
            pui32ScriptPointer++;

            /* Initialise the test CB for this test */
            asTestCB[ui32TestId - 1].ui32NumScripts = ui32NumScripts;
            for (ui32Index = 0; ui32Index < MAX_SEMAPHORES; ui32Index++) {
                asTestCB[ui32TestId - 1].ai32Semaphores[ui32Index] = 0;
            }

            for (ui32Index = 0; ui32Index < NO_SYSSYNC_IDS; ui32Index++) {
                asTestCB[ui32TestId - 1].aui32SystemSyncId[ui32Index] = 0;
            }

            printf("%s():%d TestId:%d NumScripts:%d\n", __FUNCTION__,__LINE__, ui32TestId, ui32NumScripts);

            /* Parse information for each one of the scripts in the test */
            for (ui32ScriptId = 1; ui32ScriptId <= ui32NumScripts; ui32ScriptId++) {
                uint32_t* pui32ScriptStart = pui32ScriptBase + (ui32HSize + pui32ScriptPointer[0]);
                pui32ScriptPointer++;

                printf("%s():%d ScriptId:%d\n", __FUNCTION__,__LINE__, ui32ScriptId);

                InitialiseScript(&asScriptCB[gui32NumScripts], ui32TestId, ui32ScriptId, pui32ScriptStart);
                gui32NumScripts++;

                if (gui32NumScripts > MAX_PDUMPLSCRIPTS) {
                    /* We may want to increase the number of available scripts */
                    printf("\033[031m%s():%d\033[0m\n", __func__, __LINE__);
                    PDL_ASSERT(IMG_FALSE);
                    return IMG_FALSE;
                }
            }
        }

        /* Next we read in the sync information */
        ui32NumSyncs = pui32ScriptPointer[0];
        pui32ScriptPointer++;

        printf("%s():%d NumSyncs:%d\n", __FUNCTION__,__LINE__, ui32NumSyncs);

        for (ui32SyncIndex = 0; ui32SyncIndex < ui32NumSyncs; ui32SyncIndex++) {
            uint32_t ui32TestNum = pui32ScriptPointer[0];
            uint32_t ui32SyncId = pui32ScriptPointer[1];
            uint32_t ui32ElemCount = pui32ScriptPointer[2];
            uint32_t ui32ElemIndex;

            pui32ScriptPointer += 3;

            if (ui32ElemCount == 0) {
                /* This is a disable Sync Command */
                DisableSync(ui32SyncId, ui32TestNum);
            } else {
                for (ui32ElemIndex = 0; ui32ElemIndex < ui32ElemCount; ui32ElemIndex++) {
                    if (ui32SyncId <= 31) {
                        uint32_t ui32ScriptId = pui32ScriptPointer[0];

                        pui32ScriptPointer++;

                        IntraSync(ui32TestNum, ui32SyncId, ui32ScriptId);
                    } else {
                        uint32_t ui32TestNum2 = pui32ScriptPointer[0];
                        uint32_t ui32SyncId2 = pui32ScriptPointer[1];

                        pui32ScriptPointer += 2;

                        InterSync(ui32TestNum, ui32SyncId, ui32TestNum2, ui32SyncId2);
                    }
                }
            }
        }

        /* End of header */
        if (pui32ScriptPointer[0] != PDL_SIGNATURE_COMPSCRIPT_HEND) {
            printf("\033[031m%s():%d\033[0m\n", __func__, __LINE__);
            PDL_ASSERT(IMG_FALSE);
            return IMG_FALSE;
        }

        pui32ScriptPointer++;
    } else {
        /* This case cannot be handled */
        printf(" This case cannot be handled ... \n");
        PDL_ASSERT(IMG_FALSE);
        return IMG_FALSE;
    }
    
    printf("%s():%d End!!!\n", __FUNCTION__,__LINE__);

    return IMG_TRUE;
}

/******************************************************************************/
/******************************************************************************/
void DoSystemValidationChecks(void)
{
    uint8_t* pui8;

    /* Check for little-endian, etc */
    pui8 = (uint8_t*)&ui32CheckLittleEndian;
    if (*pui8 != 0x04) {
        /* Not little endian */
        ERROR_OUTPUT("Not little endian\n");
        PDL_ASSERT(IMG_FALSE);
    }

    /* Check for ** what else ** ? */
}

/******************************************************************************/

int gpu_binlite_main(int argc, char* argv[])
{
    gbl_argc = argc;
    gbl_argv = argv;

    /* Target-specific initialisation */
    PDL_INIT();

    if (pui8SABOutput) {
        if ((uintptr_t)pui8SABOutput & 0x1F) {
            uint32_t skipbytes = 0x20 - ((uintptr_t)pui8SABOutput & 0x1F);
            pui8SABOutput += skipbytes;
            // wr(0x107FFFF20,pui8SABOutput);//write sab addr to sv
        }

        if (!((uintptr_t)pui8SABOutput & 0x70000000)) {
            pui8SABOutput = (volatile uint8_t *)((uintptr_t)pui8SABOutput | 0x70000000);
        }

        wr(0x107FFFF20, (uintptr_t)pui8SABOutput); // write sab addr to sv
        pui32SABOrigin = (uint32_t*)pui8SABOutput;
        sab_addr = (uint8_t *)pui8SABOutput;
        pui8SABOutput += 4;
        *pui32SABOrigin = 4; // Allow 4 bytes for this variable
    }

    /* Verify that the user has set the base pointer and proceed accordingly */
    switch ((uintptr_t)(pvScriptPointer)) {
        case -1:
            /* Not set! */
            printf("\033[031m%s():%d\033[0m\n", __func__, __LINE__);
            ERROR_OUTPUT("Base pointer not set\n");
            PDL_ASSERT(IMG_FALSE);
            break; /* Shouldn't actually get here */

        case 0:
            /* Set, but just for system validation checks */
            printf("\033[031m%s():%d\033[0m\n", __func__, __LINE__);
            DoSystemValidationChecks();
            break;

        default:
            /* Gather information about the script(s) */
            if (PreprocessScripts()) {
                // Process the script(s)
                ProcessScripts();
                printf("\033[031m%s():%d----------------------Done------------------------\033[0m\n", __func__, __LINE__);

            }
            break;
    }
    if(pui8SABOutput)
    {
       printf("sab addr is 0x%p\n",sab_addr);
       printf("pui8SABOutput is 0x%p\n",pui8SABOutput);
       printf("size:0x%x\n", (uint32_t)(pui8SABOutput - sab_addr));
    }
    return 0;
}

#endif
