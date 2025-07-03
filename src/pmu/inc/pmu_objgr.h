/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJGR_H
#define PMU_OBJGR_H

/*!
 * @file pmu_objgr.h
 */

/* ------------------------ System includes -------------------------------- */
#include "flcntypes.h"
/* ------------------------ Application includes --------------------------- */
#include "gr/grtpc.h"
#include "config/g_gr_hal.h"
/* ------------------------ Macros ----------------------------------------- */
/*!
 * @brief Timeout for acquiring GR mutex. We will abort power gating if PMU
 *        fails to acquire this mutex.
 */
#define GR_MUTEX_ACQUIRE_TIMEOUT_NS       (10000) // 10us

#define grMutexAcquire(tk)            mutexAcquire(LW_MUTEX_ID_GR,             \
                                                   GR_MUTEX_ACQUIRE_TIMEOUT_NS, tk)

#define grMutexRelease(tk)            mutexRelease(LW_MUTEX_ID_GR, tk)

// Get GR holdoff mask
#define grHoldoffMaskGet()            (GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG)->holdoffMask)

// Gets status of ignore holdoff interrupt WAR for bug 1793923
#define grIsHoldoffIntrWarActivated() Gr.bIgnoreHoldoffIntr

// Get GR sequence cache data
#define GR_SEQ_GET_CACHE()            Gr.pSeqCache

/*!
 * Size of Perfmon WAR cache
 *
 * This size depends on the no. of Perfmon CG control registers
 * we need to save/restore 
 * (Reg list is defined in grPerfmonWarStateInit_TU10X function)
 */
#define GR_PERMON_WAR_CACHE_SIZE                  (4)

/*!
 * Timeout in usec while submitting a method to FECS
 */
#define GR_FECS_SUBMIT_METHOD_TIMEOUT_US          (4000)

/*!
 * Timeout in usec while submitting a method to FECS to save
 */
#define GR_FECS_SUBMIT_METHOD_SAVE_REGLIST_TIMEOUT_US           (4000)

/*!
 * Timeout in usec while submitting a method to FECS to Restore
 */
#define GR_FECS_SUBMIT_METHOD_RESTORE_REGLIST_TIMEOUT_US        (10000)

/*!
 * GR Priv Blocker timeout
 * Here 10uS is blocker engage time and 20uS is priv access
 * latency which might be there.
 */
#define GR_PRIV_BLOCKER_ENGAGE_TIMEOUT_US (30)

/*!
 * GR SRAM ECC Scrubbing timeout
 * 1 ms timeout
 */
#define GR_SRAM_ECC_SCRUB_TIMEOUT_NS      (1000000)

/*!
 * Timeout in usec for RAM repair to complete
 */
#define GR_RAM_REPAIR_TIMEOUT_US          (100)

/*!
 * PMU HW takes extra 1024 GPC Clock cycles before
 * reporting the GPC_PRIV signal as idle even though actual
 * GPC PRIV is idle (Bug: 200483795 has all the details). So we have
 * to wait for extra time to turely make sure that GPC_PRIV
 * is idle otherwise  we will falsely think that signal is
 * busy and we will  abort the GR Entry.
 *
 * So the slowest GPC Clock can be 27MHz (i.e Crystal clock).
 * So time for 1024 GPC Cycles will be 1024 * 37 nS = 37888 nS
 * or apx 38 uS.
 *
 * In normal cases, when GPC is apx 300 MHz, this time will be
 * 1024 * 3 nS = 3072 nS or 3 uS.  But we have to consider the
 * worst case time here.
 *
 * Therefor, we are adding extra 10 uS marging and keeping the
 * delay as 50 uS.
 */
#define GR_IDLE_STATUS_CHECK_TIMEOUT_NS   (50000)

/*!
 * @brief Offset of GR abort stage in gr abort reason mask
 *
 * Final abortReason mask will have first n bits corresponding to 
 * Abort checkpoint and last 32-n bits corresponding to abort reason,
 * where n is defined by GR_ABORT_REASON_OFFSET
 */
#define GR_ABORT_REASON_OFFSET                      (16)

// Abort checkpoints in GR
#define GR_ABORT_CHKPT_AFTER_PG_ON                              BIT(0)
#define GR_ABORT_CHKPT_AFTER_DISABLE_CLK_ACCESS                 BIT(1)
#define GR_ABORT_CHKPT_AFTER_OVERLAY_LOAD                       BIT(2)
#define GR_ABORT_CHKPT_AFTER_FIFO_MUTEX_ACQUIRE                 BIT(3)
#define GR_ABORT_CHKPT_AFTER_GR_MUTEX_ACQUIRE                   BIT(4)
#define GR_ABORT_CHKPT_AFTER_SCHEDULER_DISABLE                  BIT(5)
#define GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_0                  BIT(6)
#define GR_ABORT_CHKPT_AFTER_PREEMPT_SEQUENCE                   BIT(7)
#define GR_ABORT_CHKPT_AFTER_HOLDOFF_ENABLED_1                  BIT(8)
#define GR_ABORT_CHKPT_AFTER_PRIV_BLOCKER                       BIT(9)
#define GR_ABORT_CHKPT_AFTER_MMU_UNBIND                         BIT(10)
#define GR_ABORT_CHKPT_AFTER_SAVE_REGLIST_REQUEST               BIT(11)

// Abort reasons for GR
#define GR_ABORT_REASON_MUTEX_ACQUIRE_ERROR         BIT(GR_ABORT_REASON_OFFSET+0)
#define GR_ABORT_REASON_GR_INTR_PENDING             BIT(GR_ABORT_REASON_OFFSET+1)
#define GR_ABORT_REASON_HOLDOFF_ENABLE_TIMEOUT      BIT(GR_ABORT_REASON_OFFSET+2)
#define GR_ABORT_REASON_ENGINE_BUSY                 BIT(GR_ABORT_REASON_OFFSET+3)
#define GR_ABORT_REASON_FIFO_PREEMPT_ERROR          BIT(GR_ABORT_REASON_OFFSET+4)
#define GR_ABORT_REASON_IDLE_FLIPPED                BIT(GR_ABORT_REASON_OFFSET+5)
#define GR_ABORT_REASON_PRIV_BLKR_ALL_TIMEOUT       BIT(GR_ABORT_REASON_OFFSET+6)
#define GR_ABORT_REASON_WAKEUP                      BIT(GR_ABORT_REASON_OFFSET+7)
#define GR_ABORT_REASON_MMU_PRI_FIFO_BUSY           BIT(GR_ABORT_REASON_OFFSET+8)
#define GR_ABORT_REASON_UNBIND                      BIT(GR_ABORT_REASON_OFFSET+9)
#define GR_ABORT_REASON_SUBUNIT_BUSY                BIT(GR_ABORT_REASON_OFFSET+10)
#define GR_ABORT_REASON_PRIV_BLKR_EQU_TIMEOUT       BIT(GR_ABORT_REASON_OFFSET+11)
#define GR_ABORT_REASON_PRIV_PATH_FLUSH_TIMEOUT     BIT(GR_ABORT_REASON_OFFSET+12)
#define GR_ABORT_REASON_DISABLE_CLK_ACCESS_ERROR    BIT(GR_ABORT_REASON_OFFSET+13)
#define GR_ABORT_REASON_FG_RPPG_EXCLUSION_FAILED    BIT(GR_ABORT_REASON_OFFSET+14)
#define GR_ABORT_REASON_REGLIST_SAVE_TIMEOUT        BIT(GR_ABORT_REASON_OFFSET+15)

/*!
 * Macros for GR RG semaphore.
 *
 * @copydoc GrRgSemaphore
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_RG))
    //
    // GPC-RG Entry will wait for at max 10 PMU ticks to grab semaphore.
    // If it cann't grab it, it will abort GPC-RG entry.
    //
    #define grRgSemaphoreTake()                                         \
        (lwrtosSemaphoreTake(GrRgSemaphore, 10) == FLCN_OK)

    //
    // VF switch will trigger GPC-RG exit and wait forever to grab the
    // semaphore.
    //
    #define grRgSemaphoreTakeWaitForever()                              \
        lwrtosSemaphoreTakeWaitForever(GrRgSemaphore)

    //
    // Release the semaphore.
    //
    #define grRgSemaphoreGive()                                         \
        lwrtosSemaphoreGive(GrRgSemaphore)
#else
    #define grRgSemaphoreTake()                       LW_TRUE
    #define grRgSemaphoreTakeWaitForever()            lwosNOP()
    #define grRgSemaphoreGive()                       lwosNOP()
#endif

/* ------------------------ Types definitions ------------------------------ */
typedef struct GR_PRIV_BLOCK
{
    LwU32    sysLo; // low  mask of PRIV buses in sys cluster
    LwU32    gpcLo; // low  mask of PRIV buses in gpc cluster
    LwU32    gpcHi; // high mask of PRIV buses in gpc cluster
    LwU32    fbpLo; // low  mask of PRIV buses in fbp cluster
} GR_PRIV_BLOCK;

/*!
 * @brief Structure for GR index register info
 */
typedef struct GR_REG_INDEX
{
    LwU32   baseAddr;
    LwU32   offset;
    LwU32   count;
} GR_REG_INDEX;

/*!
 * @brief Structure for GR auto increment register info
 */
typedef struct GR_REG_AUTO_INCR
{
    LwU32   ctrlAddr;
    LwU32   dataAddr;
    LwU32   count;
} GR_REG_AUTO_INCR;

/*!
 * @brief Structure to cache global Graphics state
 */
typedef struct GR_REG
{
    LwU32   addr;
    LwU32   data;
} GR_REG;

/*!
 * @brief GR sequence cache
 *
 * It used to cache data for GR entry, exit and abort sequence
 */
typedef struct GR_SEQ_CACHE
{
    //
    // Info regarding the buffer which will save and restore
    // the global state of GR engine
    //
    GR_REG   *pGlobalStateBuffer;
    LwU32     globalStateRegCount;

    //
    // Info regarding the buffer which contains the array of
    // index control for auto increment registers. These need
    // to be reset before saving/restoring the global state
    //
    LwU32    *pAutoIncrBuffer;
    LwU32     autoIncrBufferRegCount;

    // Cache for variables used by GPC-RG sequence
    LwU32     smcarbTimestamp;
    LwU32     utilsClockCG;

    // Cache for variables used by all Graphics features
    LwBool    bSkipSchedEnable;
    LwU8      fifoMutexToken;
    LwU8      grMutexToken;
} GR_SEQ_CACHE;

/*!
 * @brief Structure to cache PERFMON CG registers
 *
 * It is used to cache PERFMON CG registers. 
 * We need to restore them during TPC PG Exit sequence.
 */
typedef struct GR_PERMON_WAR_REG
{
    LwU32   addr;
    LwU32   data;
} GR_PERMON_WAR_REG;

/*!
 * @brief Structure to cache PERFMON CG state.
 */
typedef struct GR_PERMON_WAR_CACHE
{
    // Pointer for GR_BA_REG
    GR_PERMON_WAR_REG  perfmonReg[GR_PERMON_WAR_CACHE_SIZE];
} GR_PERMON_WAR_CACHE;

/*!
 * @brief Structure to cache BA registers
 *
 * It used to cache BA Weight registers. We need to
 * restore them during TPC PG Exit sequence.
 */
typedef struct GR_BA_REG
{
    LwU32   addr;
    LwU32   data;
} GR_BA_REG;

/*!
 * @brief Structure to cache BA Index registers
 *
 * It used to cache BA DATA registers. We need to
 * restore them during TPC PG Exit sequence.
 */
typedef struct GR_BA_INDEX_REG
{
    // Address of index control register
    LwU32   ctrlAddr;

    // Address of Data registers
    LwU32   dataAddr;

    // Number of registers referenced through ctrlAddr/dataAddr.
    LwU32   count;

    // Cache of size "count"
    LwU32  *pData;
} GR_BA_INDEX_REG;

/*!
 * @brief Structure to cache BA State.
 */
typedef struct GR_BA_STATE
{
    // Pointer for GR_BA_REG
    GR_BA_REG       *pReg;

    // Pointer for GR_BA_INDEX_REG
    GR_BA_INDEX_REG *pIndexReg;

    // Number of instance of structure GR_BA_REG
    LwU32            regCount;

    // Number of instance of structure GR_BA_INDEX_REG
    LwU32            indexRegCount;
} GR_BA_STATE;

/* ------------------------ External definitions --------------------------- */
/*!
 * GR object Definition
 */
typedef struct
{
    // Cache to store data required during entry, exit and abort sequence.
    GR_SEQ_CACHE        *pSeqCache;

    // BA State Cache
    GR_BA_STATE         *pGrBaState;

    // PERFMON WAR Cache
    GR_PERMON_WAR_CACHE *pPerfmonWarCache;

    GR_PRIV_BLOCK        privBlock;

    // Boolean to activate/deactivate WAR for bug 1793923
    LwBool               bIgnoreHoldoffIntr;

    // SMC mask corresponding to sysPipeID in use
    LwU32                smcSysPipeMask;

    // Total processing time for save/restore methods
    LwU32                stateMethodTimeUs;
} OBJGR;

extern OBJGR Gr;

/*!
 * Semaphore to synchronize the shared GPC-RG sequence exelwtion between
 * Perf Daemon and LPWR task.
 *
 * @ref https://confluence.lwpu.com/display/RMPER/Change+Sequencer+Architecture+and+Design#ChangeSequencerArchitectureandDesign-VFSwitchwhileinGPC-RG
 *
 * If the GPU is in GPC-RG state when the new VF switch arrives, Perf
 * Daemon task will trigger GPC-RG exit and sleep on this semaphore
 * for Low Power Task to finish the exection of critical GPC-RG sequence.
 * Once the Low Power Task is done, it will release the semaphore to
 * wake up Perf Daemon Task to execute the post processing of GPC-RG
 * followed by the pending VF switch. After successful VF switch
 * exelwtion, Perf Daemon Task will release the semaphore.
 *
 * @note This semaphore does NOT protect the entire GPC-RG sequence so
 * client except Perf Daemon Task shall use the GPC-RG client RW semaphore
 * or use @ref pgCtrlIsPoweredOn_HAL.
 */
extern LwrtosSemaphoreHandle GrRgSemaphore;

/* ------------------------ Private Variables ------------------------------ */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

//
// TODO-aranjan and TODO-pankumar
//
// Think about unifying the GR initialization rather than being scattered.
// Also Rename grPostInit to a suitable name as it happens before grInit,
// and rename grLpwrCtrlPostInit to grPostInit or lpwrGrPostInit
//

void        grInit(RM_PMU_RPC_STRUCT_LPWR_LOADIN_PG_CTRL_INIT *pParams)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "grInit");

void        grRgInit(RM_PMU_RPC_STRUCT_LPWR_LOADIN_PG_CTRL_INIT *pParams)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "grRgInit");

void        grPassiveInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "grPassiveInit");

void        grLpwrCtrlPostInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "grLpwrCtrlPostInit");

FLCN_STATUS grPostInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "grPostInit");

LwBool      grWakeupPending(void);

void        grPgAssertEngineReset(LwBool);

void        grSfmHandler(LwU8 ctrlId)
            GCC_ATTRIB_SECTION("imem_lpwr", "grSfmHandler");

void        grRgPerfSeqDisallowAll(LwBool)
            GCC_ATTRIB_SECTION("imem_lpwr", "grRgPerfSeqDisallowAll");
#endif // PMU_OBJGR_H
