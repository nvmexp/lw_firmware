/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author Chandrabhanu Mahapatra
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */

#include "pmu_bar0.h"
#include "dev_top.h"
#include "dev_pwr_csb.h"
#include "pmu/pmumailboxid.h"

#include "clk3/clk3.h"
#include "clk3/fs/clkbif.h"
#include "config/g_bif_private.h"
#include "pmu_objbif.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

CLK_DEFINE_VTABLE__FREQSRC(Bif);

static RM_PMU_BIF_LINK_SPEED s_validateTargetGenspeed(RM_PMU_BIF_LINK_SPEED target)
    GCC_ATTRIB_SECTION("imem_libClkConfig", "s_validateTargetGenspeed");
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Virtual Implemenations ------------------------- */

/*!
 * @see         clkRead_FreqSrc
 * @brief       Get the link speed from the previous phase else from hardware
 *
 * @memberof    ClkBif
 *
 * @param[in]   this the instance of ClkBif to read the frequency of.
 *
 * @param[out]  pOutput a pointer to the struct to which the freuncy will be
 *              written.
 *
 * @return      return FLCN_OK under all cirlwmstances.
 */
FLCN_STATUS
clkRead_Bif
(
    ClkBif     *this,
    ClkSignal  *pOutput,
    LwBool      bActive
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 platformData;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBif)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, bif)
    };

    // Only the freqKHz field is used; discard others.
    *pOutput = clkEmpty_Signal;

    //
    // Skip these registers on FMODEL since they are not modelled.
    // Assume a reasonable default instead.
    //
    platformData = REG_RD32(BAR0, LW_PTOP_PLATFORM);
    if (FLD_TEST_DRF(_PTOP, _PLATFORM, _TYPE, _FMODEL, platformData))
    {
        this->lwrrentGenSpeed = RM_PMU_BIF_LINK_SPEED_GEN1PCIE;
        status = FLCN_OK;
    }
    else
    {
        // Attach overlays -- Must exit through after this point.
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            // Get the gen speed.
            status = bifGetBusSpeed_HAL(&Bif, &this->lwrrentGenSpeed);

            // Make sure the gen speed is valid.
            if (status != FLCN_OK ||
                this->lwrrentGenSpeed <= RM_PMU_BIF_LINK_SPEED_ILWALID ||
                this->lwrrentGenSpeed > this->maxGenSpeed)
            {
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_ERROR_INFO),
                    DRF_NUM(_MAILBOX, _CLK_ERROR_INFO, _FIELD0, status)                |
                    DRF_NUM(_MAILBOX, _CLK_ERROR_INFO, _FIELD1, this->lwrrentGenSpeed) |
                    DRF_NUM(_MAILBOX, _CLK_ERROR_INFO, _FIELD2, this->maxGenSpeed));
                this->lwrrentGenSpeed = RM_PMU_BIF_LINK_SPEED_ILWALID;
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
            }

            //
            // Since there is no actual frequency with gen speed, we use the link
            // speed as the frequency.  This is kinda hacky, but colwenient since
            // we can reuse the interface used by OBJPERF, various MODS tests, etc.
            //
            pOutput->freqKHz = this->lwrrentGenSpeed;
            this->targetGenSpeed = this->lwrrentGenSpeed;
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

    // Done.
    return status;
}


/*!
 * @see         clkConfig_FreqSrc
 * @brief       Configure this object for the target frequency and set the
 *              clkProgram implementation appropriately.
 *
 *              Specifically, this means that, if we're in phase two or later
 *              and the previous phase has the same frequency, then clkProgram
 *              is set to clkProgram_Nop_FreqSrc (i.e. "Done") since the
 *              programming happened in a previous phase.
 *
 *              Otherwise, it is set to clkProgram_Pending_BifWrapper ("Pending"),
 *              so that is will get batched up in this phase.
 *
 * @memberof    ClkBif
 *
 * @param[in]   this the instance of ClkBif to configure.
 *
 * @param[out]  pOutput a struct to write the genspeed to be configured
 *
 * @param[in]   the phase of the phase array to configure.
 *
 * @param[in]   pTarget a pointer to a struct containing the target genspeed.
 *
 * @param[in]   bHotSwitch not used
 *
 * @return      return FLCN_OK, unless the path is not empty, in which case
 *              return FLCN_ERR_ILWALID_PATH.
 */
FLCN_STATUS
clkConfig_Bif
(
    ClkBif                 *this,
    ClkSignal              *pOutput,
    ClkPhaseIndex           phase,
    const ClkTargetSignal  *pTarget,
    LwBool                  bHotSwitch
)
{
    RM_PMU_BIF_LINK_SPEED target;
    FLCN_STATUS status;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, bif)
    };

    // Most output signal fields are meaningless.  Only frequency is used.
    *pOutput = clkEmpty_Signal;

    //
    // NOTE: 'pTarget->super.freqKHz' and 'pFreqSrc->output.freqKHz' do not
    // contain actual frequencies per se, but the gen speed.
    //
    target = pTarget->super.freqKHz;

    //
    // Do sanity checks only if we're actually changing the Gen speed.
    // Otherwise, there is nothing to do.
    //
    if (this->lwrrentGenSpeed == target)
    {
        pOutput->freqKHz = target;
        return FLCN_OK;
    }

    // Make sure the target is compile-time valid.
    if (target == RM_PMU_BIF_LINK_SPEED_ILWALID)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Apply the maximum support speed.
    if (target > this->maxGenSpeed)
    {
        target = this->maxGenSpeed;
    }

    // Attach overlays -- Must exit through clkConfig_Bif_exit after this point.
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // We can't do anything until OBJBIF is there for us.
        if (!Bif.bInitialized || !Bif.bifCaps)
        {
            status = FLCN_ERR_ILWALID_STATE;
            goto clkConfig_Bif_exit;
        }

        //
        // Choose the highest genspeed that is less than or equal to the target
        // based on the capability mask.
        //
        target = s_validateTargetGenspeed(target);

        // Make sure there is a supported genspeed that is at or below the target.
        if (target == RM_PMU_BIF_LINK_SPEED_ILWALID)
        {
            status = FLCN_ERR_FREQ_NOT_SUPPORTED;
            goto clkConfig_Bif_exit;
        }

        // Remember the target configuration.
        this->targetGenSpeed = pOutput->freqKHz = target;

        // We're good.
        status = FLCN_OK;

        // Exit point after overlays are entered.
clkConfig_Bif_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    // Done
    return status;
}


/*!
 * @see         clkProgram_FreqSrc
 * @brief       Calls into BIF module to program the required pciegenclk
 *
 * @memberof    ClkBif
 *
 * @param[in]   this the instance of ClkBif to program.
 *
 * @param[in]   the phase of the phase array to set the bif to.
 *
 * @return      return FLCN_OK under all cirlwmstances.
 */
FLCN_STATUS
clkProgram_Bif
(
    ClkBif       *this,
    ClkPhaseIndex phase
)
{
    RM_PMU_BIF_LINK_SPEED bifLinkSpeed;
    LwU32 platformData;
    FLCN_STATUS status;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBif)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, bif)
    };

    // Nothing to do if we're already at the correct gen speed.
    if (this->lwrrentGenSpeed == this->targetGenSpeed)
    {
        return FLCN_OK;
    }

    // No error (so far)
    this->debug.bifErrorId = 0;

    // Gen speed not yet reported.
    this->debug.bifGenSpeed = 255;

    // Attach overlays -- Must exit through clkProgram_Bif_exit after this point.
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        //
        // clkConfig_Bif would have returned an error if OBJBIF is not
        // initialized. So if we hit this assertion, it means that the caller
        // (clkProgramDomainList_PerfDaemon) called clkConfig, got an error,
        // but called clkProgram anyway, which it should not do.
        //
        PMU_HALT_COND(Bif.bInitialized);

        //
        // Skip programming on FMODEL since BIF is not modelled.
        // Update the status as though everything worked okay.
        //
        platformData = REG_RD32(BAR0, LW_PTOP_PLATFORM);
        if (FLD_TEST_DRF(_PTOP, _PLATFORM, _TYPE, _FMODEL, platformData))
        {
            this->lwrrentGenSpeed =
            this->debug.bifGenSpeed = this->targetGenSpeed;
            status = FLCN_OK;
            goto clkProgram_Bif_exit;
        }

        // Get the current link speed.
        status = bifGetBusSpeed_HAL(&Bif, &bifLinkSpeed);
        if (status != FLCN_OK)
        {
            this->debug.bifErrorId = 255;
            goto clkProgram_Bif_exit;
        }

        // Set the link speed to match the target.
        if (this->debug.bifGenSpeed != this->targetGenSpeed)
        {
            status = bifChangeBusSpeed_HAL(&Bif, this->targetGenSpeed,
                        &bifLinkSpeed, &this->debug.bifErrorId);

            //
            // Update the target just in case the requested target differs from the
            // actual result.  This differs from the usual Clocks 3.x protocol which
            // normally determines the maximum in clkConfig.  However, it is necessary
            // in this wraper approach because OBJBIF doesn't give us a way to
            // determine the actual maximum (which may depend on the motherboard)
            // until we program it here.
            //
            if (status == FLCN_OK)
            {
                this->targetGenSpeed = bifLinkSpeed;
            }
            else
            {
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_ERROR_INFO),
                    DRF_NUM(_MAILBOX, _CLK_ERROR_INFO, _FIELD0, status)                |
                    DRF_NUM(_MAILBOX, _CLK_ERROR_INFO, _FIELD1, this->targetGenSpeed)  |
                    DRF_NUM(_MAILBOX, _CLK_ERROR_INFO, _FIELD2, this->lwrrentGenSpeed) |
                    DRF_NUM(_MAILBOX, _CLK_ERROR_INFO, _FIELD3, this->debug.bifErrorId));
                PMU_BREAKPOINT();
            }
        }

        // Returned link speed for debugging purposes only
        this->debug.bifGenSpeed = bifLinkSpeed;

clkProgram_Bif_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    // Done
    return status;
}


/*!
 * @see         clkCleanup_FreqSrc
 * @brief       Do nothing, there is no cleanup work to be done
 *
 * @memberof    ClkBif
 *
 * @param[in]   this the instance of ClkBif to clean up.
 *
 * @param[in]   bActive whether this object is part of the path that produces
 *              the final gen speed.
 *
 * @return      return FLCN_OK under all cirlwmstances.
 */
FLCN_STATUS
clkCleanup_Bif
(
    ClkBif  *this,
    LwBool   bActive
)
{
    this->lwrrentGenSpeed = this->targetGenSpeed;
    return FLCN_OK;
}




/*!
 * @see         ClkFreqSrc_Virtual::clkPrint
 * @brief       Print debugging info
 *
 * @memberof    ClkBif
 *
 * @param[in]   this        object to print
 * @param[in]   phaseCount  Number of phases for config/program
 *
 */
#if CLK_PRINT_ENABLED
void
clkPrint_Bif
(
    ClkBif          *this,
    ClkPhaseIndex    phaseCount
)
{
    CLK_PRINTF("BIF: GenSpeed: max=%u current=%u target=%u debug=%u errorId=%u\n",
        this->maxGenSpeed, this->lwrrentGenSpeed, this->targetGenSpeed,
        this->debug.bifGenSpeed, this->debug.bifErrorId);
}
#endif

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Validate that the said target speed is supported by the system and
 *        choose the highest genspeed that is equal to or less than the target
 *        based on the capability mask.
 *
 * @param[in] target  Target genspeed to validate
 *
 * @return    target  the highest genspeed that is equal to or less than the
 *                    target based on the capability mask
 */
static RM_PMU_BIF_LINK_SPEED
s_validateTargetGenspeed
(
    RM_PMU_BIF_LINK_SPEED target
)
{
    switch (target)
        {
            //
            // Do not change the order of the cases. Add the highest
            // supported genspeed case at top
            //
            case RM_PMU_BIF_LINK_SPEED_GEN5PCIE:
                 if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN5PCIE, _SUPPORTED, (Bif.bifCaps)))
                 {
                    target = RM_PMU_BIF_LINK_SPEED_GEN5PCIE;
                    return target;
                 }
            case RM_PMU_BIF_LINK_SPEED_GEN4PCIE:
                 if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN4PCIE, _SUPPORTED, (Bif.bifCaps)))
                 {
                    target = RM_PMU_BIF_LINK_SPEED_GEN4PCIE;
                    return target;
                 }
            case RM_PMU_BIF_LINK_SPEED_GEN3PCIE:
                 if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN3PCIE, _SUPPORTED, (Bif.bifCaps)))
                 {
                    target = RM_PMU_BIF_LINK_SPEED_GEN3PCIE;
                    return target;
                 }
            case RM_PMU_BIF_LINK_SPEED_GEN2PCIE:
                 if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN2PCIE, _SUPPORTED, (Bif.bifCaps)))
                 {
                    target = RM_PMU_BIF_LINK_SPEED_GEN2PCIE;
                    return target;
                 }
            case RM_PMU_BIF_LINK_SPEED_GEN1PCIE:
                 if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN1PCIE, _SUPPORTED, (Bif.bifCaps)))
                 {
                    target = RM_PMU_BIF_LINK_SPEED_GEN1PCIE;
                    return target;
                 }
            default:
                 {
                    target = RM_PMU_BIF_LINK_SPEED_ILWALID;
                    return target;
                 }
        }
}

