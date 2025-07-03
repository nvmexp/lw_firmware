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

/* ------------------------ Application Includes --------------------------- */
#include "dev_soe_csb.h"
#include "dev_soe_ip_addendum.h"
#include "dev_lwlsaw_ip.h"
#include "dev_nport_ip.h"

#include "soe_bar0.h"
#include "cmdmgmt.h"
#include "soe_objspi.h"
#include "soe_objsaw.h"
#include "spidev.h"
#include "soe_objcore.h"
#include "soe_objdiscovery.h"
#include "soe_objsoe.h"


/* ------------------------ Static Variables ------------------------------- */
static sysTASK_DATA OS_TMR_CALLBACK coreTaskOsTmrCb;
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
static OsTmrCallbackFunc (_coreSoeResetPollingCallback_LS10);

sysTASK_DATA LwU32 sawBase;
/*!
 * Pre-STATE_INIT for DMA engine
 */
sysSYSLIB_CODE void
coreDmaInit_LS10(void)
{
    // Disable CTX requirements for Falcon DMA
    LwU32 val = csbRead(LW_CSOE_FBIF_CTL);
    val = FLD_SET_DRF(_CSOE, _FBIF, _CTL_ALLOW_PHYS_NO_CTX, _ALLOW, val);
    csbWrite(LW_CSOE_FBIF_CTL, val);

    val = csbRead(LW_CSOE_FALCON_DMACTL);
    val = FLD_SET_DRF(_CSOE, _FALCON_DMACTL, _REQUIRE_CTX, _FALSE, val);
    csbWrite(LW_CSOE_FALCON_DMACTL, val);

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
sysSYSLIB_CODE FLCN_STATUS
coreReadBiosSize_LS10
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
sysSYSLIB_CODE FLCN_STATUS
coreReadBios_LS10
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

sysSYSLIB_CODE void
coreSoeResetPolling_LS10(void)
{
    LwU32 intervalUs = SOE_UNLOAD_CALLBACK_TIMEOUT_US; // 10 ms

    REG_WR32(BAR0, LW_SOE_RESET_SEQUENCE,
        DRF_DEF(_SOE, _RESET_SEQUENCE, _REQUESTED, _NO));

    // Initialize core task callback
    memset(&coreTaskOsTmrCb, 0, sizeof(OS_TMR_CALLBACK));

    osTmrCallbackCreate(&coreTaskOsTmrCb,                               // pCallback
                OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED, // type
                0,                                                      // ovlImem
                _coreSoeResetPollingCallback_LS10,                      // pTmrCallbackFunc
                Disp2QCoreThd,                                          // queueHandle
                intervalUs,                                             // periodNormalus
                intervalUs,                                             // periodSleepus
                OS_TIMER_RELAXED_MODE_USE_NORMAL,                       // bRelaxedUseSleep
                RM_SOE_UNIT_CORE);                                      // taskId
 
    // Schedule the callback
    osTmrCallbackSchedule(&coreTaskOsTmrCb);
}

static FLCN_STATUS
_coreSoeResetPollingCallback_LS10
(
    OS_TMR_CALLBACK *pCallback
)
{
    LwU32 data;
    LwU32 data32;
    LwU32 idx_nport;
    LwU32 ucastBaseAddress;

    data = REG_RD32(BAR0, LW_SOE_RESET_SEQUENCE);
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
        osTmrCallbackCancel(&coreTaskOsTmrCb);
    }

    return FLCN_OK;
}

sysSYSLIB_CODE void
corePreInit_LS10(void)
{
    // Update saw base
    sawBase = getEngineBaseAddress(LW_DISCOVERY_ENGINE_NAME_SAW, INSTANCE0, ADDRESS_UNICAST, 0x0);

    // This function is failing at osTmrCallbackCreate.
    // TODO : Debug this function. Not needed this yet.
    // coreSoeResetPolling_LS10();
    coreDmaInit_LS10();
}
