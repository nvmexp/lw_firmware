/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>

// include manuals
#include "dev_fbfalcon_csb.h"
#include "dev_gc6_island.h"

#include "osptimer.h"

#include "priv.h"
#include "fbflcn_helpers.h"


typedef struct
{
    LwU32 startTime;
    LwU32 adr;
    LwU32 data;
    LwU16 count;
    struct AccessFlags {
        LwU16 inx:    10;
        LwU8 rsvd:     4;
        LwU8 blocking: 1;
        LwU8 read:     1;
    } flags;
} PRIV_PROFILING_LOG_STRUCT;

/*
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_HW_BAR0_MASTER))
static LwU32 gMaxPrivFlightTime = 0;
static LwU32 gMaxAddr = 0;
#endif
*/

/*
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_HW_BAR0_MASTER))
void
flightTimeReport
(
        LwU32 startTime,
        LwU32 addr
)
{
    LwU32 stopTime = falc_ldx((LwU32*)LW_CFBFALCON_FALCON_PTIMER0,0);
    LwU32 flighttime = stopTime - startTime;
    if (flighttime > gMaxPrivFlightTime)
    {
        gMaxPrivFlightTime = flighttime;
        gMaxAddr = addr;
    }
    FW_MBOX_WR32(10,gMaxPrivFlightTime);
    FW_MBOX_WR32(11,flighttime);
    FW_MBOX_WR32(12,addr);
    FW_MBOX_WR32(13,gMaxAddr);
}
#endif
*/

/*
 *-----------------------------------------------------------------------------
 * PRIV Profiling functions
 * functions to support priv access profiling to dmem. Logging addr, data,
 * start and end time as well as r/w information.
 *-----------------------------------------------------------------------------
 */

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_PRIV_PROFILING))

// index into the priv loggin buffer
LwU16 gPPIndex = 1;
// priv logging enable flag
LwU8 gPPEnabled = 0;
// priv logging current access
PRIV_PROFILING_LOG_STRUCT gPPActiveRecord;

PRIV_PROFILING_LOG_STRUCT gPP[PRIV_BUFFER_SIZE]
    GCC_ATTRIB_USED()
    GCC_ATTRIB_SECTION("ppArea", "gPP");

// reset the priv profiling buffer pointer to start
// and clean the buffer to all zero.
void
privprofilingReset
(
        void
)
{
    privprofilingClean();
    gPPIndex = 1;
    //memsetLwU8(&PP[0], 0x0, PRIV_BUFFER_SIZE * sizeof(PRIV_PROFILING_LOG_STRUCT));
    // advertise location and size in Mailbox2 and Mailbox3 register
    FW_MBOX_WR32_STALL(PRIV_PROFILING_MAILBOX_PP_START_ADR, (LwU32)&gPP[0]);
    FW_MBOX_WR32_STALL(PRIV_PROFILING_MAILBOX_PP_SIZE, (LwU32)(PRIV_BUFFER_SIZE));
    FW_MBOX_WR32_STALL(PRIV_PROFILING_MAILBOX_PP_ENTRIES, 0);
}


void
privprofilingClean
(
        void
)
{
    LwU8 val = 0;
    gPPIndex = 1;
    memsetLwU8(&gPP[0], val, PRIV_BUFFER_SIZE * sizeof(PRIV_PROFILING_LOG_STRUCT));
    // put structure location and size into 1st entry
    gPP[0].startTime = (LwU32)&gPP[0];
    gPP[0].data = gPPIndex;
}


void
privprofilingEnable
(
        void
)
{
    gPPEnabled = 1;
}

void
privprofilingDisable
(
        void
)
{
    gPPEnabled = 0;
}

void
privprofilingPreRd
(
    LwU32 adr,
    LwU8 blocking
)
{
    if (gPPEnabled == 0)
    {
        return;
    }
    // if we are polling a register, meaning the we do repeated reads to the same location
    // the we only log it once (otherwise we would overflow the buffer really quickly.
    if ((gPPActiveRecord.adr == adr) && (gPPActiveRecord.flags.read == 1) && (gPPIndex>1))
    {
        // increment the count, no other update
        gPPActiveRecord.count++;
        gPPIndex--;
    }
    else
    {
        gPPActiveRecord.adr = adr;
        gPPActiveRecord.flags.read = 1;
        gPPActiveRecord.flags.blocking = blocking;
        gPPActiveRecord.count = 1;
        gPPActiveRecord.data = 0xdeadbeef;
        // don't use any Istx/ldx macros, that would create an infinte loop in case we log from there

        gPPActiveRecord.startTime = falc_ldx((LwU32*)LW_CFBFALCON_FALCON_PTIMER0,0);
    }
}

void
privprofilingPreWr
(
    LwU32 adr,
    LwU8 blocking
)
{
    if (gPPEnabled == 0)
    {
        return;
    }
        gPPActiveRecord.adr = adr;
        gPPActiveRecord.flags.read = 0;
        gPPActiveRecord.flags.blocking = blocking;
        gPPActiveRecord.count = 1;
        gPPActiveRecord.data = 0xdeadbeef;
        // don't use any Istx/ldx macros, that would create an infinte loop in case we log from there

        gPPActiveRecord.startTime = falc_ldx((LwU32*)LW_CFBFALCON_FALCON_PTIMER0,0);
}

void
privprofilingPost
(
        LwU32 data
)
{
    if (gPPEnabled == 0)
    {
        return;
    }

    // don't use any Istx/ldx macros, that would create an infinte loop in case we log from there
    gPPActiveRecord.data = data;

    gPP[gPPIndex].startTime  = gPPActiveRecord.startTime;
    gPP[gPPIndex].adr        = gPPActiveRecord.adr;
    gPP[gPPIndex].data       = gPPActiveRecord.data;
    gPP[gPPIndex].count      = gPPActiveRecord.count;
    gPP[gPPIndex].flags      = gPPActiveRecord.flags;
    gPP[gPPIndex].flags.inx  = (LwU16)(gPPIndex & 0x3ff);
    gPP[0].data = gPPIndex;
    gPPIndex++;
    // silent stop on buffer overflow
    // the log should be reviewed and buffer sized adjusted to
    // match a regular mclk switch.
    if (gPPIndex >= PRIV_BUFFER_SIZE)
    {
        privprofilingDisable();
    }
}


void
privreadcheck
(
LwU32  addr,
LwU32 data,
LwU32 idx
)
{

  LwU32 readbackdata;
  readbackdata = falc_ldx((LwU32*)addr,0);
  if (data != readbackdata)
  {
    FW_MBOX_WR32(10, idx);
    FW_MBOX_WR32(11, addr);
    FW_MBOX_WR32(12, data);
    FW_MBOX_WR32(13, readbackdata);
    FBFLCN_HALT(FBFLCN_ERROR_CODE_PRIV_READCHECK_ERROR);
  }
    return;
}

#endif


/*
 * Priv access and error checking
 * The fbfalcon unifies the CSB space with the BAR0 space in terms of address
 * ranges and functional connection to bar0 world (fecs).  Error reporting is
 * identical for local registers and registers in the bar0 world.
 *
 * For error handling all blocking accesses are routed through a priv error
 * check in fbflcn2PrivErrorChkRegRd32 for reads and fbflcn2PrivErrorChkRegWr32NonPosted
 * for writes.
 *
 * negative verification:
 * non blocking store to inexisting pri register check
 *  does not result in pri error  (checked: stefans)
 *   REG_WR32(BAR0, 0x9A3E00,0x100);
 * blocking store to inexisting pri register check
 *  results in pri erorr  (checked: stefans)
 *   REG_WR32_STALL(BAR0, 0x9A3E00,0x100);
 * read of non existend pri regster
 *  results in pri error (checked: stefans)
 *   LwU32 unknown = REG_RD32(BAR0, 0x9A3E00);
 *   FW_MBOX_WR32_STALL(8, unknown);
 *
 */

/*!
 * Reads the given CSB address. The read transaction is nonposted.
 *
 * Checks the return read value for priv errors and returns a status.
 * It is up the the calling apps to decide how to handle a failing status.
 *
 * @param[in]  addr   The CSB address to read
 * @param[out] *pData The 32-bit value of the requested address.
 *                     This function allows this value to be NULL
 *                     as well.
 *
 * @return The status of the read operation.
 *
 * The function has bee colwerted to return read data and halt on error to
 * better align with the direct priv read commands that this replaces
 *
 */
#if (FBFALCONCFG_FEATURE_ENABLED(REGISTER_ACCESS_RETRY))
static const LwU8 errorAndRetryOnRead = PRIV_MAX_RETRY_ON_READ;
static const LwU8 errorAndRetryOnWrite = PRIV_MAX_RETRY_ON_WRITE;
#else
static const LwU8 errorAndRetryOnRead = 1;
static const LwU8 errorAndRetryOnWrite = 1;
#endif

// reset value for errstat regstier, clearing all contetn to zero  and set the enable bit
// (this is the reset value of the register)
static const LwU32 errstatRstValue = FLD_SET_DRF(_CFBFALCON_FALCON, _CSBERRSTAT, _ENABLE, _TRUE, 0);

LwU32
fbflcn2PrivErrorChkRegRd32
(
        LwU32  addr
)
{
    LwU32 val;
    LwU32 csbErrStatVal = 0;
    LwU8 errorCnt = 0;

    LW_STATUS flcnStatus = LW_ERR_ILWALID_READ;
    LwU32 requestTime;
    LwU32 responseTime;

    requestTime = falc_ldxb_i((LwU32*)LW_CFBFALCON_FALCON_PTIMER0,0);
    while (flcnStatus == LW_ERR_ILWALID_READ)
    {
        // Read and save the requested register and error register
        // do not replace w/ or use priv macro as this code is already run inside that macro
        val = falc_ldxb_i((LwU32*)(addr), 0);
        responseTime = falc_ldxb_i((LwU32*)LW_CFBFALCON_FALCON_PTIMER0,0);
        csbErrStatVal = falc_ldxb_i((LwU32*)(LW_CFBFALCON_FALCON_CSBERRSTAT), 0);

        // If FALCON_CSBERRSTAT_VALID == _FALSE, CSB read succeeded
        if (FLD_TEST_DRF(_CFBFALCON, _FALCON_CSBERRSTAT, _VALID, _FALSE, csbErrStatVal))
        {
            //
            // It is possible that sometimes we get bad data on CSB despite hw
            // thinking that read was a success. Apply a heuristic to trap such
            // cases.
            //
            if ((val & FLCN_CSB_PRIV_PRI_ERROR_MASK) == FLCN_CSB_PRIV_PRI_ERROR_CODE)
            {
                if (addr == LW_CFBFALCON_FALCON_PTIMER1)
                {
                    //
                    // PTIMER1 can report a constant 0xbadfxxxx for a long time, so
                    // just make an exception for it.
                    //
                    flcnStatus = LW_OK;
                }
                else if (addr == LW_CFBFALCON_FALCON_PTIMER0)
                {
                    //
                    // PTIMER0 can report a 0xbadfxxxx value for up to 2^16 ns, but it
                    // should keep changing its value every nanosecond. We just check
                    // to make sure we can detect its value changing by reading it a
                    // few times.
                    //
                    LwU32   val1 = 0;
                    LwU32   i;

                    for (i = 0; i < 3; i++)
                    {
                        val1 = falc_ldx_i((LwU32 *)(addr), 0);
                        if (val1 != val)
                        {
                            val = val1;
                            flcnStatus = LW_OK;
                            break;
                        }
                    }
                }
            }
            else
            {
                // succesful priv access
                flcnStatus = LW_OK;
            }
        }
        if (flcnStatus != LW_OK)
        {
            // if we detect an error on the priv access the fbfalcon will halt, dumping information
            // for error signature to the mailboxes,
            // NOTES:
            //   1.) the priv accesses for the halt command need to be straight priv write opcodes
            //       so that the don't go through error check and loop
            //   2.) ERROR message will have to be cleaned if we decide to continue instead of
            //       halt. (it will pollute the next check otherwise)
            errorCnt++;
            falc_stxb_i((LwU32*)(LW_CFBFALCON_FIRMWARE_MAILBOX(6)), 0, addr);
            falc_stxb_i((LwU32*)(LW_CFBFALCON_FIRMWARE_MAILBOX(7)), 0, requestTime);
            falc_stxb_i((LwU32*)(LW_CFBFALCON_FIRMWARE_MAILBOX(8)), 0, responseTime);
            falc_stxb_i((LwU32*)(LW_CFBFALCON_FIRMWARE_MAILBOX(9)), 0, errorCnt);

            falc_stxb_i((LwU32*)(LW_CFBFALCON_FIRMWARE_MAILBOX(10)), 0,
                    falc_ldx_i((LwU32*)(LW_CFBFALCON_FALCON_CSBERRSTAT),0));
            falc_stxb_i((LwU32*)(LW_CFBFALCON_FIRMWARE_MAILBOX(11)), 0,
                    falc_ldx_i((LwU32*)(LW_CFBFALCON_FALCON_CSBERR_INFO),0));
            falc_stxb_i((LwU32*)(LW_CFBFALCON_FIRMWARE_MAILBOX(12)), 0,
                    falc_ldx_i((LwU32*)(LW_CFBFALCON_FALCON_CSBERR_ADDR),0));
            falc_stxb_i((LwU32*)(LW_CFBFALCON_FIRMWARE_MAILBOX(13)), 0, csbErrStatVal);

            // halt only if we exceed number of retries
            if (errorCnt >= errorAndRetryOnRead)
            {
                FBFLCN_HALT(FBFLCN_ERROR_CODE_CODE_PRIV_READ_STATUS_ERROR);
            }
            else
            {
                // doing another retry so we have to clean lwrrrent error information
                falc_stxb_i((LwU32*)(LW_CFBFALCON_FALCON_CSBERRSTAT),0,errstatRstValue);
                falc_stxb_i((LwU32*)(LW_CFBFALCON_FALCON_CSBERR_ADDR),0,0x0);
                // slow down a minimal delay before next retry
                OS_PTIMER_SPIN_WAIT_US(1);
            }
        }
    }
    return val;
}


/*!
 * Writes a 32-bit value to the given priv address.
 *
 * After writing the registers, check for issues that may have oclwrred with
 * the write and return status.  It is up the the calling apps to decide how
 * to handle a failing status.
 *
 * "stxb" is the non-posted version for CSB writes.
 *
 * @param[in]  addr  The priv address to write
 * @param[in]  data  The data to write to the address
 *
 * @return The status of the write operation.
 *
 * The function has been covlwerted for the fbfalcon to halt on error
 */

LW_STATUS
fbflcn2PrivErrorChkRegWr32NonPosted
(
        LwU32  addr,
        LwU32  data
)
{
    LwU32 fbflcnErrStatVal = 0;
    LW_STATUS flcnStatus = LW_ERR_ILWALID_WRITE;
    LwU8 errorCnt = 0;

    LwU32 requestTime;
    LwU32 responseTime;

    requestTime = falc_ldxb_i((LwU32*)LW_CFBFALCON_FALCON_PTIMER0,0);
    while (flcnStatus == LW_ERR_ILWALID_WRITE)
    {
        // Write requested register value and read error register
        // do not replace w/ priv macro as this code is already run inside that macro
        falc_stxb_i((LwU32*)(addr), 0, data);
        fbflcnErrStatVal = falc_ldxb_i((LwU32*)(LW_CFBFALCON_FALCON_CSBERRSTAT), 0);
        responseTime = falc_ldxb_i((LwU32*)LW_CFBFALCON_FALCON_PTIMER0,0);

        if (FLD_TEST_DRF(_CFBFALCON, _FALCON_CSBERRSTAT, _VALID, _FALSE, fbflcnErrStatVal))
        {
            flcnStatus = LW_OK;
        }
        else
        {
            // if we detect an error on the priv access the fbfalcon will halt, dumping information
            // for error signature to the mailboxes,
            // NOTES:
            //   1.) the priv accesses for the halt command need to be straight priv write opcodes
            //       so that the don't go through error check and loop
            //   2.) ERROR message will have to be cleaned if we decide to continue instead of
            //       halt. (it will pollute the next check otherwise)
            //

            // do not replace w/ or use priv macro as this code is already run inside that macro
            errorCnt++;
            falc_stxb_i((LwU32*)(LW_CFBFALCON_FIRMWARE_MAILBOX(1)), 0, errorCnt);
            falc_stxb_i((LwU32*)(LW_CFBFALCON_FIRMWARE_MAILBOX(6)), 0, addr);
            falc_stxb_i((LwU32*)(LW_CFBFALCON_FIRMWARE_MAILBOX(7)), 0, data);
            falc_stxb_i((LwU32*)(LW_CFBFALCON_FIRMWARE_MAILBOX(8)), 0, requestTime);
            falc_stxb_i((LwU32*)(LW_CFBFALCON_FIRMWARE_MAILBOX(9)), 0, responseTime);

            falc_stxb_i((LwU32*)(LW_CFBFALCON_FIRMWARE_MAILBOX(10)), 0,
                    falc_ldx_i((LwU32*)(LW_CFBFALCON_FALCON_CSBERRSTAT),0));
            falc_stxb_i((LwU32*)(LW_CFBFALCON_FIRMWARE_MAILBOX(11)), 0,
                    falc_ldx_i((LwU32*)(LW_CFBFALCON_FALCON_CSBERR_INFO),0));
            falc_stxb_i((LwU32*)(LW_CFBFALCON_FIRMWARE_MAILBOX(12)), 0,
                    falc_ldx_i((LwU32*)(LW_CFBFALCON_FALCON_CSBERR_ADDR),0));
            falc_stxb_i((LwU32*)(LW_CFBFALCON_FIRMWARE_MAILBOX(13)), 0, fbflcnErrStatVal);

            if (errorCnt >= errorAndRetryOnWrite)
            {
                FBFLCN_HALT(FBFLCN_ERROR_CODE_CODE_PRIV_WRITE_STATUS_ERROR);
            }
            else
            {
                // doing another retry so we have to clean lwrrrent error information
                falc_stxb_i((LwU32*)(LW_CFBFALCON_FALCON_CSBERRSTAT),0,errstatRstValue);
                falc_stxb_i((LwU32*)(LW_CFBFALCON_FALCON_CSBERR_ADDR),0,0x0);
                OS_PTIMER_SPIN_WAIT_US(1);
            }
        }

    }
    return flcnStatus;
}

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_HW_BAR0_MASTER))

LW_STATUS
_fbflcn2Bar0WaitIdle
(
        LwBool bEnablePrivErrChk
)
{
    LwBool bDone = LW_FALSE;
    LW_STATUS flcnStatus = FLCN_OK;

    while (!bDone)
    {
        LwU32 privCsr = REG_RD32(CSB, LW_CFBFALCON_BAR0_PRIV_CSR);
        switch (DRF_VAL(_CFBFALCON, _BAR0_PRIV_CSR, _STATUS, privCsr))
        {
            case LW_CFBFALCON_BAR0_PRIV_CSR_STATUS_IDLE:
                bDone = LW_TRUE;
                break;

            case LW_CFBFALCON_BAR0_PRIV_CSR_STATUS_PENDING:
            case LW_CFBFALCON_BAR0_PRIV_CSR_STATUS_BUSY:
                break;

            // unexpected error conditions
            case LW_CFBFALCON_BAR0_PRIV_CSR_STATUS_TMOUT:
            case LW_CFBFALCON_BAR0_PRIV_CSR_STATUS_ERR:
            default:
            {
                FBFLCN_HALT(FBFLCN_ERROR_CODE_PRIV_BAR0_ERROR);
            }
        }
    }
    return flcnStatus;
}

void
_fbflcn2BarRegWr32NonPosted
(
        LwU32 addr,
        LwU32 data
)
{
    // The bar-0 master will not properly work with a CSB address
    // but in some cases we have dynamic addresses (e.g queues) which could go to either target.
    // The code below colwerts csb accesses to the csb interface

    if (addr < CSB_LWTOFF) {
        REG_WR32(CSB, addr, data);
        return;
    }

    (void)_fbflcn2Bar0WaitIdle(LW_FALSE);

    //LwU32 start = falc_ldx((LwU32*)LW_CFBFALCON_FALCON_PTIMER0,0);

    REG_WR32(CSB, LW_CFBFALCON_BAR0_PRIV_ADDR, addr);
    REG_WR32(CSB, LW_CFBFALCON_BAR0_PRIV_WRDATA, data);

    LwU32 priv_csr_write = DRF_DEF(_CFBFALCON, _BAR0_PRIV_CSR, _CMD, _WRITE) |
        DRF_DEF(_CFBFALCON, _BAR0_PRIV_CSR, _TRIG, _TRUE);

    REG_WR32(CSB, LW_CFBFALCON_BAR0_PRIV_CSR, priv_csr_write);

    //flightTimeReport(start,addr);

    (void)_fbflcn2Bar0WaitIdle(LW_FALSE);


}

void
_fbflcn2BarRegWr32Posted
(
        LwU32 addr,
        LwU32 data
)
{
    // The bar-0 master will not properly work with a CSB address
    // but in some cases we have dynamic addresses (e.g queues) which could go to either target.
    // The code below colwerts csb accesses to the csb interface

    if (addr < CSB_LWTOFF) {
        REG_WR32(CSB, addr, data);
        return;
    }

    (void)_fbflcn2Bar0WaitIdle(LW_FALSE);

    //LwU32 start = falc_ldx((LwU32*)LW_CFBFALCON_FALCON_PTIMER0,0);

    REG_WR32(CSB, LW_CFBFALCON_BAR0_PRIV_ADDR, addr);
    REG_WR32(CSB, LW_CFBFALCON_BAR0_PRIV_WRDATA, data);

    LwU32 priv_csr_write_posted =
        DRF_DEF(_CFBFALCON, _BAR0_PRIV_CSR, _CMD , _WRITE_POSTED) |
        DRF_DEF(_CFBFALCON, _BAR0_PRIV_CSR, _TRIG, _TRUE);

    REG_WR32(CSB, LW_CFBFALCON_BAR0_PRIV_CSR, priv_csr_write_posted);

    //flightTimeReport(start,addr);
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_AD10X))
#else
    (void)_fbflcn2Bar0WaitIdle(LW_FALSE);
#endif
}

LwU32
_fbflcn2BarRegRd32
(
        LwU32 addr
)
{
    // The bar-0 master will not properly work with a CSB address
    // but in some cases we have dynamic addresses (e.g queues) which could go to either target.
    // The code below colwerts csb accesses to the csb interface

    if (addr < CSB_LWTOFF) {
        return REG_RD32(CSB, addr);
    }

    (void)_fbflcn2Bar0WaitIdle(LW_FALSE);

    //LwU32 start = falc_ldx((LwU32*)LW_CFBFALCON_FALCON_PTIMER0,0);

    REG_WR32(CSB, LW_CFBFALCON_BAR0_PRIV_ADDR, addr);

    LwU32 priv_csr_read =
        DRF_DEF(_CFBFALCON, _BAR0_PRIV_CSR, _CMD , _READ) |
        DRF_DEF(_CFBFALCON, _BAR0_PRIV_CSR, _TRIG, _TRUE);

    REG_WR32(CSB, LW_CFBFALCON_BAR0_PRIV_CSR, priv_csr_read);

    //flightTimeReport(start,addr);

    (void)_fbflcn2Bar0WaitIdle(LW_FALSE);

    LwU32 val = REG_RD32(CSB, LW_CFBFALCON_BAR0_PRIV_RDDATA);
    return val;
}
#endif


