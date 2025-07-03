/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: testApplet.c
 * @brief: Test HS applet for core switch
 */

//
// Includes
//
#include "dev_gsp_csb.h"
#include "dev_sec_csb.h"
#include "dev_fbhub_sec_bus.h"
#include "dev_fb.h"
#include "gsphs.h"
#include <lwmisc.h>
#include "flcntypes.h"
#include "hdcp22wired_selwreaction.h"

#define DISABLE_BIG_HAMMER_LOCKDOWN() csbWr32(LW_CGSP_SCP_CTL_CFG,     \
                DRF_DEF(_CGSP, _SCP_CTL_CFG, _LOCKDOWN_SCP, _DISABLE) |  \
                DRF_DEF(_CGSP, _SCP_CTL_CFG, _LOCKDOWN, _DISABLE))

extern FLCN_STATUS gspSelwreActionHsEntry(SELWRE_ACTION_ARG  *pArg);

LwBool selwreBusAccess
(
    LwBool                  bRead,
    LwU32                   addr,
    LwU32                   valueToWrite
)
{
    LwU32 status;
    LwU32 dioReadyVal;
    LwU32 ctrlAddr;
    LwU32 d0Addr;
    LwU32 d1Addr;

    // Setup defines based on bus target
    {
        dioReadyVal =   DRF_DEF(_CSEC, _FALCON_DIO_DOC_CTRL, _EMPTY,       _INIT) |
                        DRF_DEF(_CSEC, _FALCON_DIO_DOC_CTRL, _WR_FINISHED, _INIT) |
                        DRF_DEF(_CSEC, _FALCON_DIO_DOC_CTRL, _RD_FINISHED, _INIT);

        ctrlAddr = LW_CSEC_FALCON_DIO_DOC_CTRL(0);
        d0Addr   = LW_CSEC_FALCON_DIO_DOC_D0(0);
        d1Addr   = LW_CSEC_FALCON_DIO_DOC_D1(0);
    }

    //
    // Send out the read/write request onto the DIO
    // 1. Wait for channel to become empty/ready
    do
    {
        status = csbRd32(ctrlAddr) & dioReadyVal;
    }
    // TODO for jamesx for Bug 1732094: Timeout support
    while (status != dioReadyVal);

    // 2. If it is a write request the push value onto D1,
    //    otherwise set the read bit in the address
    if (bRead)
    {
        addr |= 0x10000;
    }
    else
    {
        csbWr32(d1Addr, valueToWrite);
    }

    // 3. Issue request
    csbWr32(d0Addr, addr);

    return LW_TRUE;
}


static int testHsFunction(void)
{
    LwU32 reg;

    csbWr32(LW_CGSP_MAILBOX(0), csbRd32(LW_CGSP_FALCON_SCTL)+1);

    reg = ~csbRd32(LW_CGSP_MAILBOX(1));
    csbWr32(LW_CGSP_MAILBOX(1), reg);

    reg = csbRd32(LW_CGSP_MAILBOX(2)) + 1;
    csbWr32(LW_CGSP_MAILBOX(2), reg);

    csbWr32(LW_CGSP_MAILBOX(3), 0);

    return 0;
}

/*!
 * @brief Wait for BAR0 to become idle
 */
static void bar0WaitIdle(void)
{
    LwU32 csr_data;
    LwU32 lwr_status = LW_CSEC_BAR0_CSR_STATUS_BUSY;

    //wait for the previous transaction to complete
    while(lwr_status == LW_CSEC_BAR0_CSR_STATUS_BUSY)
    {
        csr_data = csbRd32(LW_CSEC_BAR0_CSR);
        lwr_status = DRF_VAL(_CSEC, _BAR0_CSR, _STATUS, csr_data);
    }
}

/*!
 * @brief Writes BAR0
 */
static LwU32 writeBar0(LwU32 addr, LwU32 data)
{

    LwU32  val32;

    csbWr32(LW_CSEC_BAR0_ADDR, addr);
    csbWr32(LW_CSEC_BAR0_DATA, data);
    csbWr32(LW_CSEC_BAR0_TMOUT, 0x1000);
    bar0WaitIdle();

    csbWr32(LW_CSEC_BAR0_CSR,
            DRF_DEF(_CSEC, _BAR0_CSR, _CMD , _WRITE) |
            DRF_NUM(_CSEC, _BAR0_CSR, _BYTE_ENABLE, 0xF) |
            DRF_DEF(_CSEC, _BAR0_CSR, _TRIG, _TRUE));

    (void)csbRd32(LW_CSEC_BAR0_CSR);

    bar0WaitIdle();
    val32 = csbRd32(LW_CSEC_BAR0_DATA);
    return val32;
}


void enterHs(LwU32 cmd, LwU32 arg)
{
    switch(cmd)
    {
        case GSPHS_CMD_TIME_HS_SWITCH:
        // measure full HS-switch time
        {
            LwU32 lo, hi;
            do
            {
                hi = csbRd32(LW_CGSP_FALCON_PTIMER1);
                lo = csbRd32(LW_CGSP_FALCON_PTIMER0);
            } while (hi != csbRd32(LW_CGSP_FALCON_PTIMER1));

            csbWr32(LW_CGSP_MAILBOX(0), FLCN_OK);
            csbWr32(LW_CGSP_MAILBOX(1), lo);
            csbWr32(LW_CGSP_MAILBOX(2), hi);
            break;
        }
        case GSPHS_CMD_HS_SANITY:
        {
            testHsFunction();
            break;
        }
        case GSPHS_CMD_HS_IRQTEST:
        {
            selwreBusAccess(0, LW_SFBHUB_ENCRYPTION_ERROR_CODE_MASK_IRQ0, 0x3f);
            selwreBusAccess(0, LW_SFBHUB_ENCRYPTION_ERROR_CODE_MASK_IRQ1, 0x3f);
            writeBar0(0x00100B84, 0xFFFFFFFF);
            break;
        }
        case GSPHS_CMD_HS_IRQCLR:
        {
            selwreBusAccess(0, LW_SFBHUB_ENCRYPTION_ERROR_CLR_IRQ0, 0x3f);
            selwreBusAccess(0, LW_SFBHUB_ENCRYPTION_ERROR_CLR_IRQ1, 0x3f);
            break;
        }
        case GSPHS_CMD_HS_SWITCH:
        {
            PSELWRE_ACTION_ARG pSecActArg = (PSELWRE_ACTION_ARG)arg;
            // TODO: return error status back to the client
            gspSelwreActionHsEntry(pSecActArg);
            break;
        }
        default:
        {
            csbWr32(LW_CGSP_MAILBOX(0), FLCN_ERR_ILWALID_ARGUMENT);
            break;
        }
    }

    DISABLE_BIG_HAMMER_LOCKDOWN();
    return;
}
