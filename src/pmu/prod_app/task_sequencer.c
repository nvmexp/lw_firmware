/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file task_sequencer.c
 *
 * @section _SeqScriptAllocation Sequencer Script Allocation
 *
 * Sequencer scripts are variable length.  In practice, the length may vary
 * from anywhere from less than 16 bytes to over 1KB. For this reason, the
 * content of script (opcodes, parameters, etc...) is not written into the
 * PMU's command queues when the sequencer commands are issued. Instead, the
 * RM allocates a portion of DMEM from the section of DMEM it owns, copies
 * the script to that location, and specifies the DMEM offset of the command
 * within the command structure that is queued (see diagram below).
 *
 * <pre>
 * GENERIC PMU SEQUENCER COMMAND:
 *          +----------------+
 *          |                |<-- COMMAND STRUCTURE IN DMEM
 *          | HEADER         |             (COMMAND QUEUES)
 *          |                |
 *          +================+
 *          | ERRCODE        |
 *          +----------------+
 *          | ERRPC          |
 *          +----------------+
 *          | TOUT_STAT      |
 *          +------+----+----+            PREALLOCATED SCRATCH SPACE IN DMEM
 *  D=IN    | SIZE | A  | D  |                        (RM-MANAGED DMEM HEAP)
 *  A=DMEM  +------+----+----+            +--------------+
 *          | OFFSET       o------------->|              |
 *          +------+----+----+            | SEQUENCER    |
 *  D=OUT   | SIZE | A  | D  |            | SCRIPT       |
 *  A=DMEM  +------+----+----+            |              |
 *          | OFFSET         |            |              |
 *          +----------------+            +--------------+
 * </pre>
 *
 * @endsection
 * @section _SeqVirtualRegister Virtual Sequencer Registers
 *
 * Virtual registers are supported by the PMU Sequencer to allow scripts to
 * produce data that may later be retrieved by the RM. The basic idea is that
 * in addition to having an aclwmulator (ACC) we also have up to 'n' general-
 * purpose virtual registers that may be used as scratch space. Most opcodes
 * continue to operate on the real ACC, but special opcodes exist to allow
 * scripts to move data back and forth between the aclwmulator and the virtual
 * registers.
 *
 * Virtual registers are supported using the same indirection technique that is
 * used the script (see above). When a script utilizes a virtual registers or a
 * set of virtual registers, the RM must allocate space in DMEM for the
 * registers and specify the location of the virtual register area/buffer
 * in the sequencer command structure.  See the following diagram for more
 * information.
 *
 * <pre>
 * PMU SEQUENCER COMMAND w/GP REGISTER SUPPORT:
 *
 *          +----------------+
 *          |                |
 *          |                |
 *          | HEADER         |
 *          |                |
 *          +================+            PREALLOCATED SCRATCH SPACE IN DMEM
 *          | ERRCODE        |                        (RM-MANAGED DMEM HEAP)
 *          +----------------+            +--------------+
 *          | ERRPC          |     +----->|              |
 *          +----------------+     |      | SEQUENCER    |
 *          | TOUT_STAT      |     |      | SCRIPT       |
 *          +------+----+----+     |      |              |
 *  D=IN    | SIZE | A  | D  |     |      |              |
 *  A=DMEM  +------+----+----+     |      |              |
 *          | OFFSET       o-------+      |              |
 *          +------+----+----+            |              |
 *  D=OUT   | SIZE | A  | D  |            +--------------+
 *  A=DMEM  +------+----+----+
 *          | OFFSET       o-------+
 *          +----------------+     |      PREALLOCATED SCRATCH SPACE IN DMEM
 *                                 |                  (RM-MANAGED DMEM HEAP)
 *                                 |      +--------------+
 *                                 +----->| VREG0        |
 *                                        +--------------+
 *                                        | VREG1        |
 *                                        +--------------+
 *                                        | VREG2        |
 *                                        +--------------+
 *                                        | ...          |
 *                                        +--------------+
 *                                        | VREGn        |
 *                                        +--------------+
 * </pre>
 *
 * @endsection
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "pmusw.h"
#include "pmu/ssurface.h"

#include "os/pmu_lwos_task.h"
#include "pmu_oslayer.h"

#include "dev_pwr_csb.h"
#include "dev_bus.h"
#include "dev_master.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objseq.h"
#include "pmu_objpmu.h"
#include "pmu_objfb.h"
#include "pmu_objpmu.h"
#include "pmu_objtimer.h"
#include "therm/objtherm.h"
#include "unit_api.h"
#include "cmdmgmt/cmdmgmt.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"
#include "main.h"
#include "main_init_api.h"
#include "dbgprintf.h"

#include "g_pmurpc.h"

/* ------------------------- Reserve Registers ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/*!
 * SEQ_OP enum type defines internal sequencer operations.
 */
enum SEQ_OP
{
    SEQ_OP_OK,
    SEQ_OP_EXIT,
    SEQ_OP_ERROR,
    SEQ_OP_BRANCH,
    SEQ_OP_DMA,
};
typedef enum SEQ_OP SEQ_OP;

typedef void (*SEQ_REG_WR32)     (LwU32  , LwU32);
typedef void (*SEQ_REG_WR32_MULT)(LwU32 *, LwU32);

#if PMUCFG_FEATURE_ENABLED(PMU_SEQ_SCRIPT_RPC_SUPER_SURFACE)
LwU8 progBufferLocal[RM_PMU_SEQ_CMD_PROG_BUFFER_ALLOC_MAX] GCC_ATTRIB_SECTION("dmem_seqRpcBuffer", "progBufferLocal");
LwU8 vRegsLocal[RM_PMU_SEQ_CMD_VREGS_ALLOC_MAX] GCC_ATTRIB_SECTION("dmem_seqRpcBuffer", "vRegsLocal");
#endif
/*!
 * Struct for all kinds of information that will be used during the sequencer
 * operation. They are bundled in a struct t avoid linker generating holes on
 * DMEM.
 */
typedef struct
{
    LwU32  *pSeqVRegFile;
    LwU32   SeqRegs[3];                 // ACC=0, ADDR=1, TOUT=2
    LwBool  SeqCflag;
    LwBool  SeqZflag;
    LwU8    SeqVRegCount;
} SEQ_INFO;

/* ------------------------- Macros and Defines ----------------------------- */
#define ACC        (SeqInfo.SeqRegs[0])
#define ADDR       (SeqInfo.SeqRegs[1])
#define TOUT       (SeqInfo.SeqRegs[2])
#define VREG(idx)  (SeqInfo.pSeqVRegFile[(idx)])

/*!
 * Defines the size of each virtual-register. Expressed as a power-of-2.
 */
#define SEQ_VREG_SIZE      (2)

/*!
 * Validate the instruction index.
 *
 * @param[in]   _index
 *     The Nth word of an instruction
 * @param[in]   _size
 *     The size of the instruction, in words. Should be 'instrSize'
 * @param[in]   _label
 *     The exit label on error.  Should be 'seq_op_error'
 *
 * The pInstr pointer is a pointer to the current instruction in an array of
 * 32-bit variable-length Sequencer instructions.  instrSize is a parameter
 * passed to s_seqExelwteInstr() that specifies the size (in words) of that
 * instruction.
 *
 * _index is the index into the array of bytes for the instruction.  For
 * example, if the instruction is 3 words long (_size==3), _index==1
 * refers to the second word.  It is an error for the index to point to a
 * word past the instruction.
 *
 * This check is enabled only if the PMU_SELWRITY_HARDENING_SEQ feature is
 * enabled (see Features.pm).  If it's not enabled, the assertion is always
 * true, so the code is optimized-out.
 *
 * For use only in the s_seqExelwteInstr() function, below.
 */
#define SEQ_IS_VALID_IDX(_index, _size, _label)                                \
    do                                                                         \
    {                                                                          \
        if (!(!PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING_SEQ) || (_index) < (_size))) \
        {                                                                      \
            PMU_BREAKPOINT();                                                  \
            goto _label;                                                       \
        }                                                                      \
    } while (LW_FALSE)

/* ------------------------- Prototypes ------------------------------------- */
static SEQ_OP       s_seqExelwteInstr       (LwU32 **, LwU32, LwU32 *);

// Bar0 Optimization
static void         seqBar0RegWr32          (LwU32,   LwU32);
       void         seqBar0RegWr32Opt       (LwU32,   LwU32);
static void         seqBar0RegWr32Mult      (LwU32 *, LwU32);
static void         seqBar0RegWr32MultOpt   (LwU32 *, LwU32);

lwrtosTASK_FUNCTION(task_sequencer, pvParameters);

/* ------------------------- Static Variables ------------------------------- */
/*!
 * We need to align this structure only if it contains the DMA buffer.
 */
static SEQ_INFO SeqInfo;

static SEQ_REG_WR32      pSeqRegWr32     = seqBar0RegWr32;
static SEQ_REG_WR32_MULT pSeqRegWr32Mult = seqBar0RegWr32Mult;

#if PMUCFG_FEATURE_ENABLED(PMU_SEQ_UPDATE_TRP)
static LwBool SeqIsFBStopped;
#endif

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Exelwtes the instruction located at the specified program-count value (as
 * pointed to by ppInstr).
 *
 * @param[in,out]  ppInstr
 *     Pointer to the sequencer instruction to execute.  Will be updated with
 *     location of the next instruction (or just after the end of the current
 *     instruction) if the current instruction is not a branch-instruction or
 *     and EXIT instruction. In cases, the program-count pointer will not
 *     change.
 *
 * @param[in]      instrSize
 *     The size of the sequencer instruction to execute (in words)
 *
 * @param[out]     pRetData;
 *     Used under two cirlwmstances.  First when exelwtion of the current
 *     instruction resulted in a branch.  In that case this value is written
 *     with the branch-offset (relative to the start of the script) and the
 *     return value will be set to 'SEQ_OP_BRANCH'. The second case is for EXIT
 *     handling and error-conditions.  In those cases, this value will will be
 *     set to either the EXIT opcode or the offending opcode (on
 *     error-conditions). In this case the return value will be to
 *     'SEQ_OP_EXIT' or 'SEQ_OP_ERROR'.
 *
 * @return 'SEQ_OP_OK'     Instruction was exelwted successfully and the next
 *                         instuction to execute is reflected in 'ppInstr'.
 *
 * @return 'SEQ_OP_EXIT'   Returned upon exelwtion of the the EXIT opcode.
 *                         'pRetData' is written with the exit code.
 *                         (ppInstr is not updated)
 *
 * @return 'SEQ_OP_ERROR'  Returned upon error while exelwting a script.
 *                         'pRetData' is written with the offending opcode.
 *                         (ppInstr is not updated)
 *
 * @return 'SEQ_OP_BRANCH' Instruction was exelwted successfully and resulted
 *                         in a branch; the offset of the branch is reflected
 *                         in 'pRetData' (ppInstr is not updated).
 *
 * @return 'SEQ_OP_DMA'    Instruction was exelwted successfully and resulted
 *                         in a DMA request; the size of the requested block
 *                         size in words is saved in 'pRetData' (ppInstr is not
 *                         updated).
 *
 */
static SEQ_OP
s_seqExelwteInstr
(
    LwU32 **ppInstr,
    LwU32   instrSize,
    LwU32  *pRetData
)
{
    FLCN_STATUS     status;
    LwU32          *pInstr;
    LwU32           data32;
    LwU32           vregIdx;
    LwU32           branchOffset;
    LwU8            opcode;
    LwU8            idx;
    LwS8            shift;
    FLCN_TIMESTAMP  current;

    pInstr  = *ppInstr;
    opcode  = (pInstr[0] >> LW_PMU_SEQ_INSTR_OPC_SHIFT)
                  & LW_PMU_SEQ_INSTR_OPC_MASK;

    switch (opcode)
    {
        case LW_PMU_SEQ_LOAD_ACC_OPC:
        case LW_PMU_SEQ_LOAD_ADDR_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_LOAD_ADDR_PARAM_VAL, instrSize, seq_op_error);

            idx = opcode & 1;
            SeqInfo.SeqRegs[idx] = pInstr[LW_PMU_SEQ_LOAD_ADDR_PARAM_VAL];
            break;
        }
        case LW_PMU_SEQ_OR_ADDR_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_OR_ADDR_PARAM_VAL, instrSize, seq_op_error);

            idx = opcode & 1;
            SeqInfo.SeqRegs[idx] |= pInstr[LW_PMU_SEQ_OR_ADDR_PARAM_VAL];
            break;
        }
        case LW_PMU_SEQ_AND_ADDR_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_AND_ADDR_PARAM_VAL, instrSize, seq_op_error);

            idx = opcode & 1;
            SeqInfo.SeqRegs[idx] &= pInstr[LW_PMU_SEQ_AND_ADDR_PARAM_VAL];
            break;
        }
        case LW_PMU_SEQ_ADD_ACC_OPC:
        case LW_PMU_SEQ_ADD_ADDR_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_ADD_ADDR_PARAM_VAL, instrSize, seq_op_error);

            idx = opcode & 1;
            SeqInfo.SeqRegs[idx] += pInstr[LW_PMU_SEQ_ADD_ADDR_PARAM_VAL];
            break;
        }
        case LW_PMU_SEQ_LSHIFT_ADDR_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_LSHIFT_ADDR_PARAM_VAL, instrSize, seq_op_error);

            idx   = opcode & 1;
            shift = (LwS8)pInstr[LW_PMU_SEQ_LSHIFT_ADDR_PARAM_VAL];
            if (shift >= 0)
            {
                SeqInfo.SeqRegs[idx] <<= shift;
            }
            else
            {
                SeqInfo.SeqRegs[idx] >>= -shift;
            }
            break;
        }
        case LW_PMU_SEQ_READ_ADDR_OPC:
        {
            ACC = REG_RD32(BAR0, ADDR);
            break;
        }
        case LW_PMU_SEQ_READ_IMM_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_READ_IMM_PARAM_VAL, instrSize, seq_op_error);

            ACC = REG_RD32(BAR0, pInstr[LW_PMU_SEQ_READ_IMM_PARAM_VAL]);
            break;
        }
        case LW_PMU_SEQ_READ_INDEX_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_READ_INDEX_PARAM_VAL, instrSize, seq_op_error);

            ACC = REG_RD32(BAR0, ADDR + pInstr[LW_PMU_SEQ_READ_INDEX_PARAM_VAL]);
            break;
        }
        case LW_PMU_SEQ_WRITE_IMM_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_WRITE_IMM_PARAM_VAL, instrSize, seq_op_error);

            pSeqRegWr32(pInstr[LW_PMU_SEQ_WRITE_IMM_PARAM_VAL], ACC);
            break;
        }
        case LW_PMU_SEQ_WRITE_INDEX_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_WRITE_INDEX_PARAM_VAL, instrSize, seq_op_error);

            pSeqRegWr32(ADDR + pInstr[LW_PMU_SEQ_WRITE_INDEX_PARAM_VAL], ACC);
            break;
        }
        case LW_PMU_SEQ_WRITE_REG_OPC:
        {
            if (instrSize > LW_PMU_SEQ_OPC_SIZE_WORDS(WRITE_REG))
            {
                SEQ_IS_VALID_IDX(LW_PMU_SEQ_WRITE_REG_PARAM_ADDR, instrSize, seq_op_error);

                // pass a pointer to the first addr/data pair
                pSeqRegWr32Mult(&pInstr[LW_PMU_SEQ_WRITE_REG_PARAM_ADDR],
                                    instrSize-1);
            }
            else
            {
                SEQ_IS_VALID_IDX(LW_PMU_SEQ_WRITE_REG_PARAM_DATA, instrSize, seq_op_error);

                pSeqRegWr32(pInstr[LW_PMU_SEQ_WRITE_REG_PARAM_ADDR],
                            pInstr[LW_PMU_SEQ_WRITE_REG_PARAM_DATA]);
            }
            if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
            {
                if (instrSize < 3)
                {
                    // Make sure instrSize is not too small, either
                    goto seq_op_error;
                }
            }
            ADDR = pInstr[instrSize-2];
            ACC  = pInstr[instrSize-1];
            break;
        }
        case LW_PMU_SEQ_WAIT_FLUSH_NS_OPC:
        {
            (void)REG_RD32(BAR0, LW_PMC_BOOT_0);
            // intentional fall-through
        }
        case LW_PMU_SEQ_WAIT_NS_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_WAIT_NS_PARAM_VAL, instrSize, seq_op_error);

            OS_PTIMER_SPIN_WAIT_NS(pInstr[LW_PMU_SEQ_WAIT_NS_PARAM_VAL]);
            break;
        }
        case LW_PMU_SEQ_WAIT_SIGNAL_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_WAIT_SIGNAL_PARAM_VAL, instrSize, seq_op_error);

            TOUT <<= 1;
            status = seqWaitSignal_HAL(&Seq,
                               pInstr[LW_PMU_SEQ_WAIT_SIGNAL_PARAM_SIG],
                               pInstr[LW_PMU_SEQ_WAIT_SIGNAL_PARAM_VAL]);
            if (status == FLCN_ERR_TIMEOUT)
            {
                // On timeout, set Z flag
                SeqInfo.SeqZflag = LW_TRUE;
                TOUT            |= 0x1;
            }
            else if (status == FLCN_ERR_NOT_SUPPORTED)
            {
            }
            break;
        }
        case LW_PMU_SEQ_POLL_CONTINUE_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_POLL_CONTINUE_PARAM_TIMEOUT, instrSize, seq_op_error);

            TOUT <<= 1;
            if (!PMU_REG32_POLL_NS(
                    ADDR, pInstr[LW_PMU_SEQ_POLL_CONTINUE_PARAM_MASK],
                    ACC,  pInstr[LW_PMU_SEQ_POLL_CONTINUE_PARAM_TIMEOUT],
                    PMU_REG_POLL_MODE_BAR0_EQ))
            {
                SeqInfo.SeqZflag = LW_TRUE;
                TOUT            |= 0x1;
            }
            break;
        }
        case LW_PMU_SEQ_EXIT_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_EXIT_PARAM_VAL, instrSize, seq_op_error);

            *pRetData = pInstr[LW_PMU_SEQ_EXIT_PARAM_VAL];
            // PC still points to the instruction
            return SEQ_OP_EXIT;
        }
        case LW_PMU_SEQ_DMA_READ_NEXT_BLOCK_OPC:
        {
            *pRetData = LW_PMU_SEQ_DMA_BLOCK_SIZE_WORDS;
            return SEQ_OP_DMA;
        }
        case LW_PMU_SEQ_CMP_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_CMP_PARAM_VAL, instrSize, seq_op_error);

            SeqInfo.SeqCflag = LW_FALSE;
            SeqInfo.SeqZflag = LW_FALSE;

            if (ACC < pInstr[LW_PMU_SEQ_CMP_PARAM_VAL])
            {
                SeqInfo.SeqCflag = LW_TRUE;
            }
            if (ACC == pInstr[LW_PMU_SEQ_CMP_PARAM_VAL])
            {
                SeqInfo.SeqZflag = LW_TRUE;
            }
            break;
        }
        case LW_PMU_SEQ_BRA_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_BRA_PARAM_OFFSET, instrSize, seq_op_error);

            branchOffset = pInstr[LW_PMU_SEQ_BRA_PARAM_OFFSET];
            goto seq_op_branch;
        }
        case LW_PMU_SEQ_BREQ_OPC:
        {
            if (SeqInfo.SeqZflag)
            {
                SEQ_IS_VALID_IDX(LW_PMU_SEQ_BREQ_PARAM_OFFSET, instrSize, seq_op_error);

                branchOffset = pInstr[LW_PMU_SEQ_BREQ_PARAM_OFFSET];
                goto seq_op_branch;
            }
            break;
        }
        case LW_PMU_SEQ_BRNEQ_OPC:
        {
            if (!SeqInfo.SeqZflag)
            {
                SEQ_IS_VALID_IDX(LW_PMU_SEQ_BRNEQ_PARAM_OFFSET, instrSize, seq_op_error);

                branchOffset = pInstr[LW_PMU_SEQ_BRNEQ_PARAM_OFFSET];
                goto seq_op_branch;
            }
            break;
        }
        case LW_PMU_SEQ_BRLT_OPC:
        {
            if (SeqInfo.SeqCflag)
            {
                SEQ_IS_VALID_IDX(LW_PMU_SEQ_BRLT_PARAM_OFFSET, instrSize, seq_op_error);

                branchOffset = pInstr[LW_PMU_SEQ_BRLT_PARAM_OFFSET];
                goto seq_op_branch;
            }
            break;
        }
        case LW_PMU_SEQ_BRGT_OPC:
        {
            if (!SeqInfo.SeqCflag & !SeqInfo.SeqZflag)
            {
                SEQ_IS_VALID_IDX(LW_PMU_SEQ_BRGT_PARAM_OFFSET, instrSize, seq_op_error);

                branchOffset = pInstr[LW_PMU_SEQ_BRGT_PARAM_OFFSET];
                goto seq_op_branch;
            }
            break;
        }
#if PMUCFG_FEATURE_ENABLED(PMU_SEQ_FB_STOP_SUPPORTED)
        case LW_PMU_SEQ_FB_STOP_OPC:
        {
            LwBool bStop;
            LwU32  args;

            SEQ_IS_VALID_IDX(LW_PMU_SEQ_FB_STOP_PARAM_ARGS, instrSize, seq_op_error);

            bStop = (LwBool)pInstr[LW_PMU_SEQ_FB_STOP_PARAM_STOP];
            args  = (LwU32)pInstr[LW_PMU_SEQ_FB_STOP_PARAM_ARGS];

            fbStop_HAL(&Fb, bStop, args);
// Log whether FB stop is asserted if we support updating tRP value
#if PMUCFG_FEATURE_ENABLED(PMU_SEQ_UPDATE_TRP)
            SeqIsFBStopped = bStop;
#endif
            break;
        }
#endif
        case LW_PMU_SEQ_ENTER_CRITICAL_OPC:
        {
            appTaskCriticalEnter();
            break;
        }
        case LW_PMU_SEQ_EXIT_CRITICAL_OPC:
        {
            appTaskCriticalExit();
            break;
        }
        case LW_PMU_SEQ_LOAD_VREG_OPC:
        case LW_PMU_SEQ_LOAD_VREG_IND_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_LOAD_VREG_PARAM_IDX, instrSize, seq_op_error);

            vregIdx = pInstr[LW_PMU_SEQ_LOAD_VREG_PARAM_IDX];
            if ((SeqInfo.pSeqVRegFile == NULL) || (vregIdx >= SeqInfo.SeqVRegCount))
            {
                goto seq_op_error;
            }
            if (opcode & 1)
            {
                vregIdx = VREG(vregIdx);
                if (vregIdx >= SeqInfo.SeqVRegCount)
                {
                    goto seq_op_error;
                }
            }
            VREG(vregIdx) = ACC;
            break;
        }
        case LW_PMU_SEQ_LOAD_VREG_VAL_OPC:
        case LW_PMU_SEQ_LOAD_VREG_VAL_IND_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_LOAD_VREG_VAL_PARAM_VAL, instrSize, seq_op_error);

            vregIdx = pInstr[LW_PMU_SEQ_LOAD_VREG_VAL_PARAM_IDX];
            if ((SeqInfo.pSeqVRegFile == NULL) || (vregIdx >= SeqInfo.SeqVRegCount))
            {
                goto seq_op_error;
            }
            if (opcode & 1)
            {
                vregIdx = VREG(vregIdx);
                if (vregIdx >= SeqInfo.SeqVRegCount)
                {
                    goto seq_op_error;
                }
            }
            VREG(vregIdx) = pInstr[LW_PMU_SEQ_LOAD_VREG_VAL_PARAM_VAL];
            break;
        }
        case LW_PMU_SEQ_LOAD_VREG_TS_NS_OPC:
        case LW_PMU_SEQ_LOAD_VREG_TS_NS_IND_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_LOAD_VREG_PARAM_IDX, instrSize, seq_op_error);

            vregIdx = pInstr[LW_PMU_SEQ_LOAD_VREG_PARAM_IDX];
            if ((SeqInfo.pSeqVRegFile == NULL) || (vregIdx >= SeqInfo.SeqVRegCount))
            {
                goto seq_op_error;
            }
            if (opcode & 1)
            {
                vregIdx = VREG(vregIdx);
                if (vregIdx >= SeqInfo.SeqVRegCount)
                {
                    goto seq_op_error;
                }
            }
            osPTimerTimeNsLwrrentGet(&current);
            VREG(vregIdx) = current.parts.lo;
            break;
        }
        case LW_PMU_SEQ_LOAD_ADDR_VREG_OPC:
        case LW_PMU_SEQ_LOAD_ADDR_VREG_IND_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_LOAD_ADDR_VREG_PARAM_IDX, instrSize, seq_op_error);

            vregIdx = pInstr[LW_PMU_SEQ_LOAD_ADDR_VREG_PARAM_IDX];
            if ((SeqInfo.pSeqVRegFile == NULL) || (vregIdx >= SeqInfo.SeqVRegCount))
            {
                goto seq_op_error;
            }
            if (opcode & 1)
            {
                vregIdx = VREG(vregIdx);
                if (vregIdx >= SeqInfo.SeqVRegCount)
                {
                    goto seq_op_error;
                }
            }
            ADDR = VREG(vregIdx);
            break;
        }
        case LW_PMU_SEQ_ADD_VREG_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_ADD_VREG_PARAM_ADDEND, instrSize, seq_op_error);

            vregIdx = pInstr[LW_PMU_SEQ_ADD_VREG_PARAM_IDX];
            if ((SeqInfo.pSeqVRegFile == NULL) || (vregIdx >= SeqInfo.SeqVRegCount))
            {
                goto seq_op_error;
            }
            VREG(vregIdx) += pInstr[LW_PMU_SEQ_ADD_VREG_PARAM_ADDEND];
            break;
        }
        case LW_PMU_SEQ_CMP_VREG_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_CMP_VREG_PARAM_VAL, instrSize, seq_op_error);

            vregIdx = pInstr[LW_PMU_SEQ_CMP_VREG_PARAM_IDX];
            data32  = pInstr[LW_PMU_SEQ_CMP_VREG_PARAM_VAL];
            if ((SeqInfo.pSeqVRegFile == NULL) || (vregIdx >= SeqInfo.SeqVRegCount))
            {
                goto seq_op_error;
            }
            SeqInfo.SeqCflag = LW_FALSE;
            SeqInfo.SeqZflag = LW_FALSE;
            if (VREG(vregIdx) < data32)
            {
                SeqInfo.SeqCflag = LW_TRUE;
            }
            if (VREG(vregIdx) == data32)
            {
                SeqInfo.SeqZflag = LW_TRUE;
            }
            break;
        }
#if PMUCFG_FEATURE_ENABLED(PMU_SEQ_CONFIGURE_LWLINK_TRAFFIC)
        case LW_PMU_SEQ_LTCS_OPC:
        case LW_PMU_SEQ_LTCS2_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_LTCS_PARAM_VAL, instrSize, seq_op_error);

            //
            // LTCS registers will be priv protected and can be updated
            // only from priv level 2. Hence, the priv level for the sequencer
            // task needs to be raised temporarily and switched back to the default
            // once the update is complete.
            //
            OS_TASK_SET_PRIV_LEVEL_BEGIN(RM_FALC_PRIV_LEVEL_LS,
                                         RM_FALC_PRIV_LEVEL_LS);
            {
                seqUpdateLtcsRegs_HAL(&Seq, opcode,
                    pInstr[LW_PMU_SEQ_LTCS_PARAM_VAL]);
            }
            OS_TASK_SET_PRIV_LEVEL_END();
            break;
        }
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_SEQ_UPDATE_TRP)
        case LW_PMU_SEQ_FBPA_CONFIG0_TRP_OPC:
        {
            SEQ_IS_VALID_IDX(LW_PMU_SEQ_FBPA_CONFIG0_TRP_PARAM_VAL, instrSize, seq_op_error);

            //
            // Update tRP only if FB stop has been asserted.
            // Lack of this check would enable RM to send just the tRP update
            // instruction (without the FB STOP being asserted prior to it), which
            // might lead to unforeseen issues.
            //
            if (SeqIsFBStopped)
            {
                seqUpdateFbpaConfig0Trp_HAL(&Seq,
                        pInstr[LW_PMU_SEQ_FBPA_CONFIG0_TRP_PARAM_VAL]);
            }
            break;
        }
#endif
        default:
        {
            *pRetData = LW_PMU_SEQ_ILWALID_OPC;

            // PC still points to the instruction
            return SEQ_OP_ERROR;
            break;
        }
    }

    // opcode exelwted successfully
    *ppInstr += instrSize;
    return SEQ_OP_OK;

seq_op_branch:
     // PC still points to the instruction
    *pRetData = branchOffset;
    return SEQ_OP_BRANCH;

seq_op_error:
     // PC still points to the instruction
    *pRetData = opcode;
    return SEQ_OP_ERROR;
}


/*!
 * @brief Execute a block of sequencer instructions
 *
 * A block contains multiple sequencer instructions. A single block could
 * contain an entire sequencer script or a big script can be broken into
 * multiple blocks of instructions. This function returns either when the
 * script is finished running or terminated upon error(SEQ_OP_EXIT,
 * SEQ_OP_ERROR) or the next instruction to be exelwted does not exist in the
 * current block. (SEQ_OP_BRANCH, SEQ_OP_DMA)
 *
 * @param[in,out]  pPc
 *     A relative program counter from the base address of 0. pPc is a pointer
 *     to the current script offset. pPc is updated upon exit to point the
 *     last exelwted instruction.
 *
 * @param[in]      blockBase
 *     A relative base address of the current block in word addressing.
 *
 * @param[in]      blockSize
 *     The size of the current block in words.
 *
 * @param[in]      pBlock
 *     Pointer to the sequencer instruction
 *
 * @param[out]     pRetData
 *     Contains the various return codes from s_seqExelwteInstr. Each SEQ_OP
 *     writes pRetData with different meanings.
 *
 * @return SEQ_OP. @see SEQ_OP and @see s_seqExelwteInstr for more information.
 *
 */
static SEQ_OP
s_seqExelwteInstrBlock
(
    LwU32  *pPc,
    LwU32   blockBase,
    LwU32   blockSize,
    LwU32  *pBlock,
    LwU32  *pRetData
)
{
    SEQ_OP  seqOp = SEQ_OP_OK;
    LwU32   *pInstr;
    LwU32   *pBlockEnd = &pBlock[blockSize];
    LwU32   instrSize;

    PMU_HALT_COND(*pPc >= blockBase);

    pInstr = &pBlock[*pPc - blockBase];
    while (pInstr < pBlockEnd)
    {
        instrSize = (pInstr[0] >> LW_PMU_SEQ_INSTR_SIZE_SHIFT) &
            LW_PMU_SEQ_INSTR_SIZE_MASK;

        if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING_SEQ))
        {
            // Make sure the instruction isn't truncated in the block
            if (((pInstr + instrSize) < pInstr) || // overflow check
                ((pInstr + instrSize) > pBlockEnd))
            {
                seqOp = SEQ_OP_ERROR;
                break;
            }
        }

        seqOp = s_seqExelwteInstr(&pInstr, instrSize, pRetData);

        if (seqOp == SEQ_OP_BRANCH)
        {
            // if branch offset in pRetData is in the range
            if ((*pRetData >= blockBase) &&
                (*pRetData <  blockBase + blockSize))
            {
                pInstr = &pBlock[*pRetData - blockBase];
                continue;
            }
        }
        if (seqOp != SEQ_OP_OK)
        {
            break;
        }
    }
    // update the relative pc
    *pPc = blockBase + (pInstr - pBlock);
    return seqOp;
}

/*!
 * Execute the entire script contained in the specific PMU command.
 *
 * @param[in/out]   pParams     A pointer to the RPC structure containing script to
 *                              execute/run and return status.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_SEQ_SCRIPT_RPC_SUPER_SURFACE))
FLCN_STATUS
pmuRpcSeqRunScript
(
    RM_PMU_RPC_STRUCT_SEQ_RUN_SCRIPT *pParams
)
{
    LwU32 *pBuffer;
    LwU32  scriptSize;
    LwU32  pc        = 0;
    LwU32  retData   = 0;
    SEQ_OP seqOp;
    FLCN_STATUS status = LW_OK;

    ACC              = 0;
    ADDR             = 0;
    TOUT             = 0;
    SeqInfo.SeqCflag = LW_FALSE;
    SeqInfo.SeqZflag = LW_FALSE;

    status = ssurfaceRd(&(progBufferLocal),
           LW_OFFSETOF(RM_PMU_SUPER_SURFACE, pascalAndLater.seq.seqScriptData.progBuffer),
           pParams->progBufferSize);

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        return status;
    }

    // initialize the general-purpose register file if the command supports it
    if (pParams->vRegSize == 0)
    {
        SeqInfo.SeqVRegCount = 0;
        SeqInfo.pSeqVRegFile = NULL;
    }
    else
    {
        //
        // Use the size of the register allocation payload to determine the
        // number of virtual registers that the script may access.
        //
        SeqInfo.SeqVRegCount = pParams->vRegSize >> SEQ_VREG_SIZE;
        SeqInfo.pSeqVRegFile = (LwU32 *)vRegsLocal;
        memset(SeqInfo.pSeqVRegFile, 0x00, pParams->vRegSize);
    }

    //
    // Extract the starting offset of the script and the size.  A size
    // colwersion is required as the size is passed down in bytes and all logic
    // here is defined in words.
    //
    pBuffer    = (LwU32 *)progBufferLocal;
    scriptSize = pParams->progBufferSize >> 2;

    seqOp = s_seqExelwteInstrBlock(&pc, 0, scriptSize, pBuffer, &retData);

    // no other sequencer operation is expected lwrrently.
    PMU_HALT_COND(seqOp == SEQ_OP_EXIT);

    // error code is in retData
    pParams->errorCode      = (LwU8)retData;
    pParams->errorPc        = (LwU16)pc;
    pParams->timeoutStatus  = TOUT;

    status = ssurfaceWr(&(vRegsLocal),
        LW_OFFSETOF(RM_PMU_SUPER_SURFACE, pascalAndLater.seq.seqScriptData.vRegs),
        pParams->vRegSize);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}
#endif

/*!
 * Execute the entire script contained in the specific PMU command.
 *
 * @param[in]   pRunCmd     A pointer to the command containing the script to
 *                          execute/run.
 *
 * @param[out]  pErrorCode  Written with the value of the offending opcode
 *                          upon error or with the script size upon success.
 *
 * @param[out]  pErrorPc    Set to offset of the last opcode exelwted (offset
 *                          relative to the start of the script).
 *
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_SEQ_SCRIPT_RPC_PRE_GP102))
FLCN_STATUS
pmuRpcSeqRunScriptPreGp102
(
    RM_PMU_RPC_STRUCT_SEQ_RUN_SCRIPT_PRE_GP102 *pParams
)
{
    LwU32 *pBuffer;
    LwU32  scriptSize;
    LwU32  pc        = 0;
    LwU32  retData   = 0;
    SEQ_OP seqOp;

    ACC              = 0;
    ADDR             = 0;
    TOUT             = 0;
    SeqInfo.SeqCflag = LW_FALSE;
    SeqInfo.SeqZflag = LW_FALSE;

    // initialize the general-purpose register file if the command supports it
    if (pParams->vRegSize == 0)
    {
        SeqInfo.SeqVRegCount = 0;
        SeqInfo.pSeqVRegFile = NULL;
    }
    else
    {
        //
        // Use the size of the register allocation payload to determine the
        // number of virtual registers that the script may access.
        //
        SeqInfo.SeqVRegCount = pParams->vRegSize >> SEQ_VREG_SIZE;
        SeqInfo.pSeqVRegFile = (LwU32 *)(pParams->progBuffer + LW_ALIGN_UP(pParams->progBufferSize, sizeof(LwU32)));
        memset(SeqInfo.pSeqVRegFile, 0x00, pParams->vRegSize);
    }

    //
    // Extract the starting offset of the script and the size.  A size
    // colwersion is required as the size is passed down in bytes and all logic
    // here is defined in words.
    //
    pBuffer    = (LwU32 *)(LwUPtr)(pParams->progBuffer);
    scriptSize = pParams->progBufferSize >> 2;

    seqOp = s_seqExelwteInstrBlock(&pc, 0, scriptSize, pBuffer, &retData);

    // no other sequencer operation is expected lwrrently.
    PMU_HALT_COND(seqOp == SEQ_OP_EXIT);

    // error code is in retData
    pParams->errorCode      = (LwU8)retData;
    pParams->errorPc        = (LwU16)pc;
    pParams->timeoutStatus  = TOUT;

    return LW_OK;
}
#endif

/*!
 * @brief      Initialize the Sequencer Task.
 *
 * @return     FLCN_OK on success,
 * @return     descriptive error code otherwise.
 */
FLCN_STATUS
seqPreInitTask(void)
{
    FLCN_STATUS status = FLCN_OK;

    //
    // 16 bins of 2^15[ns] == ~32.8[us] ending at ~524[us]
    // 16 bins of 2^18[ns] == ~262[us] last bin starting at ~4.2[ms]
    //
    OSTASK_CREATE(status, PMU, SEQ);
    if (status != FLCN_OK)
    {
        goto seqPreInitTask_exit;
    }

    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, SEQ), 2, sizeof(DISPATCH_SEQ));
    if (status != FLCN_OK)
    {
        goto seqPreInitTask_exit;
    }

seqPreInitTask_exit:
    return status;
}

static FLCN_STATUS
s_seqRmCommandHandle
(
    DISPATCH_SEQ *pDispSeq
)
{
    FLCN_STATUS status;

    OSTASK_ATTACH_AND_LOAD_DMEM_OVERLAY(OVL_INDEX_DMEM(seqRpcBuffer)); // NJ??
    status = pmuRpcProcessUnitSeq(&(pDispSeq->rpc));
    OSTASK_DETACH_DMEM_OVERLAY(OVL_INDEX_DMEM(seqRpcBuffer));

    return status;
}

/*!
 * Task entry-point and event-loop for the Sequencer Task. Will run in an
 * infinite-loop waiting for sequencer commands to arrive in its command-
 * queue (at which point it will execute the command and go back to sleep).
 */
lwrtosTASK_FUNCTION(task_sequencer, pvParameters)
{
    DISPATCH_SEQ dispSeq;

    LWOS_TASK_LOOP_NO_CALLBACKS(LWOS_QUEUE(PMU, SEQ), &dispSeq, status,
                                LWOS_TASK_CMD_BUFFER_ILWALID)
    {
        status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;

        if (dispSeq.hdr.eventType == DISP2UNIT_EVT_RM_RPC)
        {
            status = s_seqRmCommandHandle(&dispSeq);
        }
    }
    LWOS_TASK_LOOP_END(status);
}

/*!
 * A wrapper to the regular bar0 write function.
 *
 * @param[in]  addr  BAR0/PRIV address to write to
 * @param[in]  data  data to write to the PRIV address
 */
static void
seqBar0RegWr32
(
    LwU32  addr,
    LwU32  data
)
{
    PMU_BAR0_WR32_SAFE(addr, data);
}

/*!
 * The following is an optimized version (2nd version) of our standard regWrite
 * function that we use only when interrupts are disabled. The idea is to
 * perform the minimal steps required to write a register to maximize sequencer
 * performance while remaining safe.
 *
 * @param[in]  addr  BAR0/PRIV address to write to
 * @param[in]  data  data to write to the PRIV address
 */
void
seqBar0RegWr32Opt
(
    LwU32  addr,
    LwU32  data
)
{
    PMU_BAR0_WR32_SAFE(addr, data);
}

/*!
 * A register-write function that is optimized for writing multiple registers
 * out at the same time.  The function receives an array of address/data pairs
 * and writes them out in loop in the most efficient way possible.
 *
 * @param[in]  pWrData  Array of address/data pairs where each pair represents
 *                      a single register-write operation to perform
 *
 * @param[in]  size     The size (in bytes) of the array (not the number of
 *                      address/data pairs)
 */
static void
seqBar0RegWr32Mult
(
    LwU32 *pWrData,
    LwU32  size
)
{
    seqBar0RegWr32MultOpt(pWrData, size);
}

/*!
 * A register-write function that is optimized for writing multiple registers
 * out at the same time.  The function receives an array of address/data pairs
 * and writes them out in loop in the most efficient way possible.
 *
 * WARNING: This function assumes we're in critical section and should not be
 * called directly unless interrupts are disabled.
 *
 * @param[in]  pWrData  Array of address/data pairs where each pair represents
 *                      a single register-write operation to perform
 *
 * @param[in]  size     The size (in bytes) of the array (not the number of
 *                      address/data pairs)
 */
static void
seqBar0RegWr32MultOpt
(
    LwU32 *pWrData,
    LwU32  size
)
{
    LwU32 i;
    for (i = 0; i < size; i+=2)
    {
        PMU_BAR0_WR32_SAFE(pWrData[i+0], pWrData[i+1]);
    }
}
