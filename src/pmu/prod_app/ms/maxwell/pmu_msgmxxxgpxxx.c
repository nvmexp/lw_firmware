/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_msgmxxxgpxxx.c
 * @brief  HAL-interface for the GMXXX Memory System Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_master.h"
#include "dev_top.h"
#include "dev_fuse.h"
#include "dev_fb.h"
#include "dev_fbpa.h"
#include "dev_fbpa_addendum.h"
#include "dev_lw_xve.h"
#include "dev_bus.h"
#include "dev_trim.h"
#include "dev_flush.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objms.h"
#include "pmu_objfifo.h"
#include "pmu_bar0.h"

#include "dbgprintf.h"
#include "config/g_ms_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* --------------------------- Prototypes ------------------------------------*/
/* ------------------------ Public Functions  ------------------------------- */

/*!
 * Check if host memory operation are pending or not
 *
 * On Maxwell/PASCAL, this API checks for pending and outstanding
 * flush and bind requests in Host
 *
 * @return   LW_TRUE  if a host mem op are pending
 * @return   LW_FALSE otherwise
 */
LwBool
msIsHostMemOpPending_GM10X(void)
{
    LwU32 val;

    // check if Host flush is pending
    val = REG_RD32(BAR0, LW_UFLUSH_FB_FLUSH);
    if (FLD_TEST_DRF(_UFLUSH, _FB_FLUSH, _PENDING,      _BUSY, val) ||
        FLD_TEST_DRF(_UFLUSH, _FB_FLUSH, _OUTSTANDING,  _TRUE, val))
    {
        return LW_TRUE;
    }

    // Check if BAR1/BAR2/IFB Bind status is pending or outstanding return true
    val = REG_RD32(BAR0, LW_PBUS_BIND_STATUS);
    if ((FLD_TEST_DRF(_PBUS, _BIND_STATUS, _BAR1_PENDING, _BUSY, val)     ||
        FLD_TEST_DRF(_PBUS, _BIND_STATUS, _BAR1_OUTSTANDING, _TRUE, val)) ||
        (FLD_TEST_DRF(_PBUS, _BIND_STATUS, _BAR2_PENDING, _BUSY, val)     ||
        FLD_TEST_DRF(_PBUS, _BIND_STATUS, _BAR2_OUTSTANDING, _TRUE, val)) ||
        (FLD_TEST_DRF(_PBUS, _BIND_STATUS, _IFB_PENDING, _BUSY, val)      ||
        FLD_TEST_DRF(_PBUS, _BIND_STATUS, _IFB_OUTSTANDING, _TRUE, val)))
    {
        return LW_TRUE;
    }
    return LW_FALSE;
}
