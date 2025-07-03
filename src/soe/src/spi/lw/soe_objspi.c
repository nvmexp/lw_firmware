/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/*!
 * @file   soe_objspi.c
 * @brief  Container-object for SOE SPI routines. Contains generic non-HAL
 *         interrupt-routines plus logic required to hook-up chip-specific
 *         interrupt HAL-routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------- Application Includes --------------------------- */
#include "soe_objhal.h"
#include "soe_objsoe.h"
#include "soe_objspi.h"

#include "config/g_spi_hal.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
SPI_HAL_IFACES SpiHal;
SPI_DEVICE_ROM spiRom;
INFOROM_REGION inforomRegion;
ERASE_LEDGER_REGION ledgerRegion;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */

void
constructSpi(void)
{
    IfaceSetup->spiHalIfacesSetupFn(&SpiHal);

    spiRom.super.busId          = 0;
    spiRom.super.chipSelectId   = 0;
    spiRom.super.clockPolarity  = 0;
    spiRom.super.clockPhase     = 0;
    spiRom.super.clockPeriod    = 50;
    spiRom.pInforom = &inforomRegion;
    spiRom.pLedgerRegion = &ledgerRegion;
}
