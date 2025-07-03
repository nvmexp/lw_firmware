/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/* ------------------------ System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------ Application Includes --------------------------- */
#include "dev_soe_csb.h"
#include "dev_soe_ip_addendum.h"
#include "dev_lwlsaw_ip.h"
#include "dev_nport_ip.h"

#include "soe_bar0.h"
#include "soe_cmdmgmt.h"
#include "soe_objspi.h"
#include "soe_objsaw.h"
#include "spidev.h"
#include "soe_timer.h"
#include "soe_objcore.h"
#include "soe_objdiscovery.h"
#include "config/g_timer_hal.h"


/* ------------------------ Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define ISR_REG_RD32(addr) _soeBar0RegRd32_LR10(addr)

/* ------------------------ Function Prototypes ---------------------------- */
static OsTimerCallback(_coreSoeResetPollingCallback_LR10)
    GCC_ATTRIB_USED();

/*!
 * Pre-STATE_INIT for DMA engine
 */
void
coreDmaInit_LR10(void)
{
    // Disable CTX requirements for Falcon DMA
    LwU32 val = REG_RD32_STALL(CSB, LW_CSOE_FBIF_CTL);
    val = FLD_SET_DRF(_CSOE, _FBIF, _CTL_ALLOW_PHYS_NO_CTX, _ALLOW, val);
    REG_WR32_STALL(CSB, LW_CSOE_FBIF_CTL, val);

    val = REG_RD32_STALL(CSB, LW_CSOE_FALCON_DMACTL);
    val = FLD_SET_DRF(_CSOE, _FALCON_DMACTL, _REQUIRE_CTX, _FALSE, val);
    REG_WR32_STALL(CSB, LW_CSOE_FALCON_DMACTL, val);

    // Enable DMA read/writes
    val = REG_RD32(SAW, LW_LWLSAW_SOE_DMA_ENABLE);
    val = FLD_SET_DRF_NUM(_LWLSAW, _SOE_DMA_ENABLE, _RD_ENB, 1, val);
    val = FLD_SET_DRF_NUM(_LWLSAW, _SOE_DMA_ENABLE, _WR_ENB, 1, val);
    REG_WR32(SAW, LW_LWLSAW_SOE_DMA_ENABLE, val);

}

/*!
 * @brief Report ROM Size to Driver.
 *
 * @return FLCN_OK
 */
FLCN_STATUS
coreReadBiosSize_LR10
(
    RM_FLCN_U64 dmaHandle
)
{
    PROM_REGION pRegion = spiRom.pBiosRegion;
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    {
        // Write size back.
        LwU32 size = pRegion->endAddress - pRegion->startAddress;

        if (FLCN_OK != soeDmaWriteToSysmem_HAL(&Soe,
                                               (void*)&size,
                                               dmaHandle,
                                               0,
                                               sizeof(size)))
        {
            SOE_HALT();
            goto exit;
        }
    }

    status = FLCN_OK;
exit:
    return status;
}

/*!
 * @brief Allows Driver to read BIOS portion of the ROM.
 *
 * @return FLCN_OK
 */
FLCN_STATUS
coreReadBios_LR10
(
    LwU32 offset,
    LwU32 size,
    RM_FLCN_U64 dmaHandle
)
{
    if (spiRomIsInitialized())
        return spiRomReadFromRegion(&spiRom, spiRom.pBiosRegion, offset, size, dmaHandle);
    else
        return FLCN_ERROR;
}

void
coreSoeResetPolling_LR10(void)
{
    LwU32 intervalUs = SOE_UNLOAD_CALLBACK_TIMEOUT_US; // 10 ms

    REG_WR32(BAR0, LW_SOE_RESET_SEQUENCE,
        DRF_DEF(_SOE, _RESET_SEQUENCE, _REQUESTED, _NO));

    // Schedule callback for SOE reset polling
    osTimerScheduleCallback(
        &CoreOsTimer,                                             // pOsTimer
        CORE_OS_TIMER_ENTRY_SOE_RESET,                            // index
        lwrtosTaskGetTickCount32(),                               // ticksPrev
        intervalUs,                                               // usDelay
        DRF_DEF(_OS_TIMER, _FLAGS, _RELWRRING, _YES_MISSED_EXEC), // flags
        _coreSoeResetPollingCallback_LR10,                        // pCallback
        NULL,                                                     // pParam
        OVL_INDEX_ILWALID);                                       // ovlIdx
}

static void
_coreSoeResetPollingCallback_LR10
(
    void  *pParam,
    LwU32 ticksExpired,
    LwU32 skipped
)
{
    LwU32 data;
    LwU32 data32;
    LwU32 idx_nport;
    LwU32 ucastBaseAddress;

    data = ISR_REG_RD32(LW_SOE_RESET_SEQUENCE);
    if (FLD_TEST_DRF(_SOE, _RESET_SEQUENCE, _REQUESTED, _YES, data))
    {
        soeLowerPrivLevelMasks_HAL();

        for (idx_nport = 0; idx_nport < NUM_NPORT_ENGINE; idx_nport++)
        {
            ucastBaseAddress = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_NPORT, idx_nport, ADDRESS_UNICAST, 0);
            data32 = REG_RD32(BAR0, ucastBaseAddress + LW_NPORT_SCRUBCOMPLETE);

            // NOTE: HW bug #2635500 prevents LINKTABLE scrubbing
            if (FLD_TEST_DRF(_NPORT, _SCRUBCOMPLETE, _REMAPTAB, _CLEAR, data32) &&
                FLD_TEST_DRF(_NPORT, _SCRUBCOMPLETE, _RIDTAB, _CLEAR, data32) &&
                FLD_TEST_DRF(_NPORT, _SCRUBCOMPLETE, _RLANTAB, _CLEAR, data32))
            {
                // If scrubbing is idle on this NPORT, then start a scrub
                data32 = 0;
                data32 = FLD_SET_DRF(_NPORT, _SCRUBCOMPLETE, _REMAPTAB, _CLEAR, data32);
                data32 = FLD_SET_DRF(_NPORT, _SCRUBCOMPLETE, _RIDTAB, _CLEAR, data32);
                data32 = FLD_SET_DRF(_NPORT, _SCRUBCOMPLETE, _RLANTAB, _CLEAR, data32);

                REG_WR32(BAR0, ucastBaseAddress + LW_NPORT_SCRUBCOMPLETE, data32);
            }
            else
            {
                 // Scrubbing is already active.  Seems a little odd, but just skip this NPORT
            }
        }

        // DeSchedule callback for SOE reset polling
        osTimerDeScheduleCallback(&CoreOsTimer,
            CORE_OS_TIMER_ENTRY_SOE_RESET);
    }
}

void
corePreInit_LR10(void)
{
    coreSoeResetPolling_LR10();
    coreDmaInit_LR10();
}
