/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
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

sysTASK_DATA OBJLWLINK Lwlink;

/*!
 * Pre-STATE_INIT for LWLINK
 */
sysSYSLIB_CODE void
lwlinkPreInit_LS10(void)
{
    Lwlink.linksEnableMask = 0xFFFFFFFFFULL;
}

/*!
 * @brief   Service LWLINK interrupts
 */
sysSYSLIB_CODE void
lwlinkService_LS10(void)
{
    // TODO: Expected interrupts are likely MINION
    // TODO: Add this call to the hanlder in SAW once routing is established
}

sysSYSLIB_CODE LwBool
lwlinkIsLinkInReset_LS10(LwU32 baseAddress)
{
    LwU32 val;

    val = REG_RD32(BAR0, baseAddress + LW_LWLIPT_LNK_RESET_RSTSEQ_LINK_RESET);

    return FLD_TEST_DRF(_LWLIPT_LNK, _RESET_RSTSEQ_LINK_RESET, _LINK_RESET_STATUS, _ASSERTED,  val);
}
