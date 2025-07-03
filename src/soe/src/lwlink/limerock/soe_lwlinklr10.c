/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/* ------------------------ System Includes -------------------------------- */
#include "soesw.h"
#include "soe_bar0.h"
#include "soe_objlwlink.h"
#include "lwlinkip_discovery.h"
#include "soe_objdiscovery.h"
#include "soe_objsoe.h"
#include "dev_lwlipt_lnk_ip.h"
#include "dev_soe_ip_addendum.h"
/* ------------------------ Application Includes --------------------------- */
#include "config/g_lwlink_hal.h"


OBJLWLINK Lwlink;

/*!
 * Pre-STATE_INIT for LWLINK
 */
void
lwlinkPreInit_LR10(void)
{
    Lwlink.linksEnableMask = 0xFFFFFFFFFULL;
}

/*!
 * @brief   Service LWLINK interrupts
 */
void
lwlinkService_LR10(void)
{
    // TODO: Expected interrupts are likely MINION
    // TODO: Add this call to the hanlder in SAW once routing is established
}

LwBool
lwlinkIsLinkInReset_LR10(LwU32 baseAddress)
{
    LwU32 val;

    val = ISR_REG_RD32(baseAddress + LW_LWLIPT_LNK_RESET_RSTSEQ_LINK_RESET);

    return FLD_TEST_DRF(_LWLIPT_LNK, _RESET_RSTSEQ_LINK_RESET, _LINK_RESET_STATUS, _ASSERTED,  val);
}

/*!
 * @brief   Force SLM on or off on all links
 */
#if (SOECFG_FEATURE_ENABLED(SOE_LWLINK_SLM))
FLCN_STATUS
lwlinkForceSlmAll_LR10(LwBool bForceSlmOn)
{
    LwU64 i;
    LwU32 baseAddress;

    ct_assert(NUM_TLC_ENGINE < (sizeof(i) * 8)); // making sure we do not iterate past 64bits in i.

    if (bForceSlmOn)
    {    	
        for (i = 0; i < NUM_TLC_ENGINE; i++)
        {
            if (Lwlink.linksEnableMask & BIT64(i))
            {
                LwU32 val;
                baseAddress = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_LWLIPT_LINK, i, ADDRESS_UNICAST,0);

                if (!lwlinkIsLinkInReset_HAL(&Lwlink, baseAddress))
                {
                    val = ISR_REG_RD32(baseAddress + LW_LWLIPT_LNK_PWRM_CTRL);
                    val = FLD_SET_DRF(_LWLIPT, _LNK, _PWRM_CTRL_FORCE_SLM, _FORCE_SLM, val);
                    ISR_REG_WR32((baseAddress + LW_LWLIPT_LNK_PWRM_CTRL), val);
                }
            }
        }
    }
    else
    {
  	    for (i = 0; i < NUM_TLC_ENGINE; i++)
        {
            if (Lwlink.linksEnableMask & BIT64(i))
            {
                LwU32 val;
                baseAddress = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_LWLIPT_LINK, i, ADDRESS_UNICAST,0);

                if (!lwlinkIsLinkInReset_HAL(&Lwlink, baseAddress))
                {
                    val = ISR_REG_RD32(baseAddress + LW_LWLIPT_LNK_PWRM_CTRL);
                    val = FLD_SET_DRF(_LWLIPT, _LNK, _PWRM_CTRL_FORCE_SLM, _DO_NOT_FORCE_SLM , val);
                    ISR_REG_WR32((baseAddress + LW_LWLIPT_LNK_PWRM_CTRL), val);
                }
            }
        }
    }

    return FLCN_OK;
}
#endif

//
// TODO: Other functions that are not P0:
//       MINION reset
//       MINION bootstrap
//
