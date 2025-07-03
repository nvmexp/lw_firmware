/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: hkdgv10xf.c
 * @brief: HDCP key decryption routine running on SEC2 for Volta and above.
 */

//
// Includes
//
#include "dev_sec_csb.h"
#include "dev_disp_seb.h"
#include "hkd.h"

//
// Extern Variables
//
extern LwU8 g_lc128e[];
extern LwU8 g_lc128e_debug[];
extern LwU8 g_lc128[];

// This field is not exposed in HW ref manuals. It is depth of secure bus FIFO.
#define LW_CSEC_FALCON_DOC_FIFO_DEPTH 4

#define CSB_REG(name)    (LW_CSEC_FALCON##name)
#define CSB_FIELD(name)  LW_CSEC_FALCON##name

#define DO_PANIC(x)        hkdPanic((x), pLc128)
#define SET_ERROR_CODE(X)  HKD_REG_WR32_STALL(CSB, LW_CSEC_FALCON_MAILBOX0, (X))

static   void hkdPanic(LwU32 code, LwU8 *pLc128)         ATTR_OVLY(OVL_NAME);
static LwBool hkdSelwreBusRead(LwU32 addr, LwU32 *pData) ATTR_OVLY(OVL_NAME);
static LwBool hkdSelwreBusWrite(LwU32 addr, LwU32 val)   ATTR_OVLY(OVL_NAME);

/*!
 * Wrapper function for blocking falcon read instruction to halt in case of Priv Error.
 */
LwU32
falc_ldxb_i_with_halt
(
    LwU32 addr
)
{
    LwU32   val           = 0;
    LwU32   csbErrStatVal = 0;

    val = falc_ldxb_i ((LwU32*)(addr), 0);

    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CSEC_FALCON_CSBERRSTAT), 0);

    if (FLD_TEST_DRF(_CSEC, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        // If CSBERRSTAT is true, halt no matter what register was read.
        falc_stxb_i ((LwU32*)LW_CSEC_FALCON_MAILBOX0, 0, HKD_ERROR_CSB_RD_FAILED);
        falc_halt();
    }
    else if ((val & CSB_INTERFACE_MASK_VALUE) == CSB_INTERFACE_BAD_VALUE)
    {
        //
        // If we get here, we thought we read a bad value.
        // We are not reading RNG or timer values here.
        //
        falc_stxb_i ((LwU32*)LW_CSEC_FALCON_MAILBOX0, 0, HKD_ERROR_CSB_RD_FAILED);
        falc_halt();
    }

    return val;
}

/*!
 * Wrapper function for blocking falcon write instruction to halt in case of Priv Error.
 */
void
falc_stxb_i_with_halt
(
    LwU32 addr,
    LwU32 data
)
{
    LwU32 csbErrStatVal = 0;

    falc_stxb_i ((LwU32*)(addr), 0, data);
    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CSEC_FALCON_CSBERRSTAT), 0);
    if (FLD_TEST_DRF(_CSEC, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        // If CSBERRSTAT is true, halt no matter what register was read.
        falc_stxb_i ((LwU32*)LW_CSEC_FALCON_MAILBOX0, 0, HKD_ERROR_CSB_RD_FAILED);
        falc_halt();
    }
}

/*!
 * @brief Sends a read request over secure bus.
 *
 * Copied from se_segp10x.c (we don't link with anything)
 *
 * @param[in]  addr  Bus Address
 * @param[out] pData Data read from bus
 *
 * @return LW_TRUE on success, LW_FALSE on failure
 */
static LwBool hkdSelwreBusRead(LwU32 addr, LwU32 *pData)
{
    LwU32 error, data, count, ctrl;

    // Wait for empty FIFO so all further access errors belong to us
    do
    {
        count = HKD_REG_RD32(CSB, CSB_REG(_DOC_CTRL)) & 0xFF;
    } while (count < LW_CSEC_FALCON_DOC_FIFO_DEPTH);

    // Clear DOC_CTRL.rd_error
    ctrl = HKD_REG_RD32(CSB, CSB_REG(_DOC_CTRL));
    HKD_REG_WR32(CSB, CSB_REG(_DOC_CTRL),
                 ctrl & ~DRF_SHIFTMASK(CSB_FIELD(_DOC_CTRL_RD_ERROR)));

    //Output data: address + operation type (read request)
    HKD_REG_WR32(CSB, CSB_REG(_DOC_D0), addr | 0x10000);

    // Poll for operation finished
    do
    {
        ctrl = HKD_REG_RD32(CSB, CSB_REG(_DOC_CTRL));
    } while ( (ctrl & REF_NUM(CSB_FIELD(_DOC_CTRL_RD_FINISHED), 1)) == 0);

    // Wait until operation is _really_ finished, that is - there is nothing in output FIFO
    do
    {
        count = HKD_REG_RD32(CSB, CSB_REG(_DOC_CTRL)) & 0xFF;
    } while (count < LW_CSEC_FALCON_DOC_FIFO_DEPTH);

    // Wait for at least one entry in input FIFO
    do
    {
        count = HKD_REG_RD32(CSB, CSB_REG(_DIC_CTRL)) & 0xFF;
    } while (count == 0);

    // Pop data from FIFO, do it even if there was read error
    HKD_REG_WR32(CSB, CSB_REG(_DIC_CTRL), REF_NUM(CSB_FIELD(_DIC_CTRL_POP), 1));

    //Check for VALID bit
    error = HKD_REG_RD32(CSB, CSB_REG(_DIC_CTRL)) & REF_NUM(CSB_FIELD(_DIC_CTRL_VALID), 1);
    if (error == 0)
    {
        return LW_FALSE;
    }

    // Read data from FIFO
    data = HKD_REG_RD32(CSB, CSB_REG(_DIC_D0));

    // Check for read errors clear them if needed
    ctrl = HKD_REG_RD32(CSB, CSB_REG(_DOC_CTRL));
    if (ctrl & REF_NUM(CSB_FIELD(_DOC_CTRL_RD_ERROR), 1)) {
        HKD_REG_WR32(CSB, CSB_REG(_DOC_CTRL),
                     ctrl & ~DRF_SHIFTMASK(CSB_FIELD(_DOC_CTRL_RD_ERROR)));
        return LW_FALSE;
    }

    if (pData)
    {
        *pData = data;
    }
    return LW_TRUE;
}

/*!
 * @brief Sends a write request over secure bus.
 *
 * @param[in]  addr  Bus Address
 * @param[out] val   Data to be written
 *
 * @return LW_TRUE on success, LW_FALSE on failure
 */
static LwBool hkdSelwreBusWrite(LwU32 addr, LwU32 val)
{
    LwU32 count, ctrl;

    // Wait for empty FIFO so all further access errors belong to us
    do
    {
        count = HKD_REG_RD32(CSB, CSB_REG(_DOC_CTRL)) & 0xFF;
    } while (count < LW_CSEC_FALCON_DOC_FIFO_DEPTH);

    // Clear past write errors
    ctrl = HKD_REG_RD32(CSB, CSB_REG(_DOC_CTRL));
    HKD_REG_WR32(CSB, CSB_REG(_DOC_CTRL),
                 ctrl & ~DRF_SHIFTMASK(CSB_FIELD(_DOC_CTRL_WR_ERROR)));

    // Write value and register address (address triggers operation)
    HKD_REG_WR32(CSB, CSB_REG(_DOC_D1), val);
    HKD_REG_WR32(CSB, CSB_REG(_DOC_D0), addr & 0xFFFF);

    // Poll for operation finished
    do
    {
        ctrl = HKD_REG_RD32(CSB, CSB_REG(_DOC_CTRL));
    } while ( (ctrl & REF_NUM(CSB_FIELD(_DOC_CTRL_WR_FINISHED), 1)) == 0);

    // Wait until operation is _really_ finished, that is - there is nothing in output FIFO
    do
    {
        count = HKD_REG_RD32(CSB, CSB_REG(_DOC_CTRL)) & 0xFF;
    } while (count < LW_CSEC_FALCON_DOC_FIFO_DEPTH);

    // Check if write was success, clear wr_error in case of failure.
    if (ctrl & REF_NUM(CSB_FIELD(_DOC_CTRL_WR_ERROR), 1)) {
        HKD_REG_WR32(CSB, CSB_REG(_DOC_CTRL),
                     ctrl & ~DRF_SHIFTMASK(CSB_FIELD(_DOC_CTRL_WR_ERROR)));
        return LW_FALSE;
    }

    return LW_TRUE;
}

static void hkdPanic(LwU32 code, LwU8 *pLc128)
{
    int i;

    // Clear SCP registers
    falc_scp_xor(SCP_R0, SCP_R0);
    falc_scp_xor(SCP_R1, SCP_R1);
    falc_scp_xor(SCP_R2, SCP_R2);
    falc_scp_xor(SCP_R3, SCP_R3);
    falc_scp_xor(SCP_R4, SCP_R4);
    falc_scp_xor(SCP_R5, SCP_R5);
    falc_scp_xor(SCP_R6, SCP_R6);
    falc_scp_xor(SCP_R7, SCP_R7);

    // Scrub LC128
    if (pLc128) {
        for (i=0; i<16; ++i)
        {
            pLc128[i] = 0;
        }
    }

    // Set error code and halt falcon
    SET_ERROR_CODE(code);
    falc_halt();
}

/*!
 * @brief Start HDCP key decryption
 */
void hkdStart()
{
    LwU32 ctrl     = 0;
    LwU32 index    = 0;
    LwU8  *pLc128e = g_lc128e_debug;
    LwU8  *pLc128  = g_lc128;

    SET_ERROR_CODE(HKD_START);

    // Find if running debug mode or prod mode, switch key if needed
    ctrl = HKD_REG_RD32_STALL(CSB, LW_CSEC_SCP_CTL_STAT);
    if (FLD_TEST_DRF(_CSEC_SCP, _CTL_STAT, _DEBUG_MODE, _DISABLED, ctrl)) {
        pLc128e = g_lc128e;
    }

    // Decrypt lc128
    hkdDecryptLc128Hs(pLc128e, pLc128);

    // Check keystore status, LW_PDISP_HDCPRIF_CTRL_1X_KEY_READY and LW_PDISP_HDCPRIF_CTRL_FULL have to be
    // set prior to loading HDCP2 keys (i.e. HDCP1 keys must be present)
    if (hkdSelwreBusRead(LW_SSE_SE_SWITCH_DISP_HDCPRIF_CTRL, &ctrl) == LW_FALSE) {
        DO_PANIC(HKD_ERROR_SEB_CTRL_RD_FAILED);
    }

    // Check 2X_KEY_READY
    if (FLD_TEST_DRF(_SSE_SE_SWITCH_DISP, _HDCPRIF_CTRL, _2X_KEY_READY, _TRUE, ctrl)) {
        DO_PANIC(HKD_ERROR_2X_ALREADY_LOADED);
    }

    // Check only 1X_KEY_READY
    if (FLD_TEST_DRF(_SSE_SE_SWITCH_DISP, _HDCPRIF_CTRL, _1X_KEY_READY, _FALSE, ctrl)) {
        DO_PANIC(HKD_ERROR_1X_KEY_NOT_READY);
    }

    // Program key, preserve LOCAL bit
    ctrl &= DRF_SHIFTMASK(LW_SSE_SE_SWITCH_DISP_HDCPRIF_CTRL_LOCAL);
    ctrl |= DRF_DEF(_SSE_SE_SWITCH_DISP,_HDCPRIF_CTRL,_AUTOINC,_ENABLED) |
            DRF_DEF(_SSE_SE_SWITCH_DISP,_HDCPRIF_CTRL,_WRITE4,_TRIGGER);

    while (index < 4)
    {
        if (hkdSelwreBusWrite(LW_SSE_SE_SWITCH_DISP_HDCPRIF_KEY(1), 0x0) == LW_FALSE) {
            DO_PANIC(HKD_ERROR_SEB_KEY1_WR_FAILED);
        }

        if (hkdSelwreBusWrite(LW_SSE_SE_SWITCH_DISP_HDCPRIF_KEY(0),
                            *((LwU32 *)&(pLc128[(index * 4) ]))) < 0) {
            DO_PANIC(HKD_ERROR_SEB_KEY0_WR_FAILED);
        }

        if (hkdSelwreBusWrite(LW_SSE_SE_SWITCH_DISP_HDCPRIF_CTRL, ctrl) == LW_FALSE) {
            DO_PANIC(HKD_ERROR_SEB_CTRL_WR_FAILED);
        }

        // Wait for write completion
        while (LW_TRUE)
        {
            LwU32 reg;
            if (hkdSelwreBusRead(LW_SSE_SE_SWITCH_DISP_HDCPRIF_CTRL, &reg) == LW_FALSE) {
                DO_PANIC(HKD_ERROR_SEB_CTRL_RD_FAILED);
            }

            // write failed
            if (FLD_TEST_DRF(_SSE_SE_SWITCH_DISP, _HDCPRIF_CTRL, _WRITE_ERROR, _TRUE, reg)) {
                DO_PANIC(HKD_ERROR_CTRL_WRITE_ERROR);
            }

            if (FLD_TEST_DRF(_SSE_SE_SWITCH_DISP, _HDCPRIF_CTRL, _WRITE4, _DONE, reg)) {
                break;
            }
        }

        // Wipe pLc128 location
        *((LwU32 *)&(pLc128[(index * 4) ])) = (0xDEAD0000 + index);

        ++index;
    }

    // Wait for key to be stored

    while (LW_TRUE)
    {
        LwU32 reg;
        if (hkdSelwreBusRead(LW_SSE_SE_SWITCH_DISP_HDCPRIF_CTRL, &reg) == LW_FALSE) {
            DO_PANIC(HKD_ERROR_SEB_CTRL_RD_FAILED);
        }

        if (FLD_TEST_DRF(_SSE_SE_SWITCH_DISP,_HDCPRIF_CTRL,_2X_KEY_READY,_TRUE, reg) &&
            FLD_TEST_DRF(_SSE_SE_SWITCH_DISP,_HDCPRIF_CTRL, _FULL, _TRUE, reg)) {
            break;
        }
    }

    //
    // Cleanup
    // Reset SCP registers
    //
    falc_scp_xor(SCP_R0, SCP_R0);
    falc_scp_xor(SCP_R1, SCP_R1);
    falc_scp_xor(SCP_R2, SCP_R2);
    falc_scp_xor(SCP_R3, SCP_R3);
    falc_scp_xor(SCP_R4, SCP_R4);
    falc_scp_xor(SCP_R5, SCP_R5);
    falc_scp_xor(SCP_R6, SCP_R6);
    falc_scp_xor(SCP_R7, SCP_R7);

    SET_ERROR_CODE(HKD_PASS);

    falc_halt();
    return;
}

void hkdInitErrorCodeNs(void)
{
    SET_ERROR_CODE(HKD_ERROR_UNKNOWN);
}
