/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    clk3.c
 * @brief   Contains the clocks 3.x public interfaces
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Prafull Gupta
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */

#include "pmu_objclk.h"
#include "clk3/clk3.h"
#include "dev_pwr_csb.h"
#include "pmu/pmumailboxid.h"
#include "g_pmurpc.h"
#include "cmdmgmt/cmdmgmt.h"
#include "g_pmurmifrpc.h"


/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Identify if the given programmable clock domain is sourced out of NAFLL in
 * clocks 3.x.
 * To-do akshatam: Given that the VBIOS no longer has the sourcing details for
 * clock domains, look at having this information stored in each of the clock
 * frequency domain itself so that we have a single place to fetch it for
 * different use-case.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_HOSTCLK_PRESENT)
#define CLK3_IS_PROG_DOM_NAFLL_SOURCED(_clkDomain)                            \
        (((_clkDomain) == LW2080_CTRL_CLK_DOMAIN_GPCCLK)       ||             \
         ((_clkDomain) == LW2080_CTRL_CLK_DOMAIN_XBARCLK)      ||             \
         ((_clkDomain) == LW2080_CTRL_CLK_DOMAIN_HOSTCLK)      ||             \
         ((_clkDomain) == LW2080_CTRL_CLK_DOMAIN_LWDCLK)       ||             \
         ((_clkDomain) == LW2080_CTRL_CLK_DOMAIN_SYSCLK))
#else
#define CLK3_IS_PROG_DOM_NAFLL_SOURCED(_clkDomain)                            \
        (((_clkDomain) == LW2080_CTRL_CLK_DOMAIN_GPCCLK)       ||             \
         ((_clkDomain) == LW2080_CTRL_CLK_DOMAIN_XBARCLK)      ||             \
         ((_clkDomain) == LW2080_CTRL_CLK_DOMAIN_LWDCLK)       ||             \
         ((_clkDomain) == LW2080_CTRL_CLK_DOMAIN_SYSCLK))
#endif

/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */

static void clkFlipBuffer_PerfDaemon(void)
        GCC_ATTRIB_SECTION("imem_clkLibClk3", "clkFlipBuffer_PerfDaemon");

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief   RPC to read the hardware state for the list of clock domains.
 *
 * @note    In general, there are no breakpoints within this function.
 *          The caller should check the return code and break if appropriate.
 *
 * @param[in/out]   pParams Pointer to the clock domain list structure.
 *
 * @retval FLCN_ERR_ILWALID_ARGUMENT    Invalid argument(s) passed as input
 * @retval FLCN_ERR_NOT_SUPPORTED       Unsupported clock domain passed as input
 * @retval FLCN_OK                      Successfully read the requested clock domain(s)
 */
FLCN_STATUS
pmuRpcClkFreqDomainRead
(
    RM_PMU_RPC_STRUCT_CLK_FREQ_DOMAIN_READ *pParams
)
{
    FLCN_STATUS status = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkReadPerfDaemon)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLibClk3)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLibCntr)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64Umul)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfs)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clk3x)
    };

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        if (pParams->clkList.numDomains > LW2080_CTRL_BOARDOBJ_MAX_BOARD_OBJECTS)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto pmuRpcClkFreqDomainRead_exit;
        }
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = clkReadDomainList_PerfDaemon(&pParams->clkList);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

pmuRpcClkFreqDomainRead_exit:
    return status;
}


/*!
 * @brief   RPC to Program the hardware state for the list of clock domains.
 *
 * @note    In general, there are no breakpoints within this function.
 *          The caller should check the return code and break if appropriate.
 *
 * @param[in/out]   pParams Pointer to the clock domain list structure.
 *
 * @retval FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid argument(s) are passed as input.
 * @retval FLCN_ERR_NOT_SUPPORTED
 *      Unsupported clock domain passed as input.
 * @retval FLCN_OK
 *      Successfully program the hardware state for the requested clock domains.
 */
FLCN_STATUS
pmuRpcClkFreqDomainProgram
(
    RM_PMU_RPC_STRUCT_CLK_FREQ_DOMAIN_PROGRAM *pParams
)
{
    FLCN_STATUS status = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfVf)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfDaemon)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLibClk3)
        OSTASK_OVL_DESC_DEFINE_LIB_CLK_FREQ_CONTROLLER
        OSTASK_OVL_DESC_DEFINE_LIB_FREQ_COUNTED_AVG
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clk3x)
    };

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        if (pParams->clkList.numDomains > LW2080_CTRL_BOARDOBJ_MAX_BOARD_OBJECTS)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto pmuRpcClkFreqDomainProgram_exit;
        }
    }

    //
    // Disable Clock Frequency Domain Program bypass path in production
    // to achieve the required level of protection for memory tuning controller.
    //
    // @todo We may want better security mechanism for all our internal APIs
    // via PMU fuse. For now this is disabled based on whether mining throttle
    // PMU fuse is enabled.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_FREQ_DOMAIN_PROGRAM_BYPASS_PATH_SELWRITY))
    {
        if (clkFreqDomainProgramDisabled_HAL(&Clk))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NOT_SUPPORTED;
            goto pmuRpcClkFreqDomainProgram_exit;
        }
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = clkProgramDomainList_PerfDaemon(&pParams->clkList);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

pmuRpcClkFreqDomainProgram_exit:
    return status;
}


/*!
 * @brief       Get the buffered output signal for a clock domain.
 * @public
 *
 * @details     This function does not read the physical hardware, but instead
 *              returns the cached copy of the output signal for the specified
 *              clock domain.  This is done to minimize the amount of time that
 *              the semaphore is held while preventing race conditions.
 *
 *              FLCN_WARN_NOT_QUERIED is returned if the read-only half of
 *              the double-buffered 'ClkFreqDomain::output' does not contain
 *              information.  Broadly speaking, this may happen for one of
 *              two reasons:
 *
 *              1.  The clock domain has not been queried since initialization.
 *              2.  There was an error in the most recent read/config/program.
 *
 *              As such, callers may prefer to handle FLCN_WARN_NOT_QUERIED
 *              with special logic.  One option is to queue a read request via
 *              the PER_DAEMON task, which calls 'clkReadDomainList_PerfDaemon'
 *              to read the physical hardware in these cases.
 *
 *              When this function is called within the single-threaded PERF_DAEMON
 *              task, no semaphore is held.
 *
 * @note        This function is thread-safe and may be called from outside the
 *              PERF_DAEMON task.  It uses the half of the double buffer pointed
 *              to by CLK_OUTPUT_READONLY_BUFFER__FREQDOMAIN.
 *
 * @param[in/out]   pDomain     Pointer to the clock domain list item structure.
 *
 * @retval FLCN_ERR_ILWALID_ARGUMENT    _UNDEFINED clock domain passed as input
 * @retval FLCN_ERR_NOT_SUPPORTED       Unsupported clock domain passed as input
 * @retval FLCN_WARN_NOT_QUERIED        Cache contains no data for the domain
 * @retval FLCN_OK                      Success
 */
FLCN_STATUS
clkReadDomain_AnyTask
(
    LW2080_CTRL_CLK_CLK_DOMAIN_LIST_ITEM *pDomainItem
)
{
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clk3x)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libSemaphoreRW)
    };

    FLCN_STATUS status = FLCN_OK;

    // Required parameter
    if (pDomainItem == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkReadDomain_AnyTask_exit;
    }

    // Required
    if (pDomainItem->clkDomain == LW2080_CTRL_CLK_DOMAIN_UNDEFINED)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkReadDomain_AnyTask_exit;
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        //
        // Get frequency domain object corresponding to the clock domain.
        // If it is NULL, then we've been asked to query a domain which is not
        // supported on this chip.  Note that 'pDomainItem->clkDomain' should have
        // exactly one bit set; otherwise the behaviour here is not defined.
        //
        ClkFreqDomain *pFreqDomain = CLK_FREQ_DOMAIN_GET(pDomainItem->clkDomain);
        if (pFreqDomain == NULL)
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_ERROR_LOLWS ),
                DRF_NUM(_MAILBOX, _CLK_ERROR_LOLWS , _STATUS,      status)          |
                DRF_NUM(_MAILBOX, _CLK_ERROR_LOLWS , _DOMAIN_IDX,  BIT_IDX_32(pDomainItem->clkDomain)) |
                DRF_DEF(_MAILBOX, _CLK_ERROR_LOLWS , _ACTION,      _FREQ_DOMAIN_NULL));
            PMU_BREAKPOINT();
            goto clkReadDomain_AnyTask_detach;
        }

        //
        // Take the semaphore since it's possible that the PERF_DAEMON task may update
        // 'bIsWriteIndexOne' which is used by CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN.
        //
        OS_SEMAPHORE_RW_TAKE_RD(clkFreqDomain_DoubleBufferControl.pSemaphore);

        // Get information from the read-only half of the double buffer.
        pDomainItem->clkFreqKHz = CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN(pFreqDomain).freqKHz;
        pDomainItem->regimeId   = CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN(pFreqDomain).regimeId;
        pDomainItem->source     = CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN(pFreqDomain).source;
        pDomainItem->dvcoMinFreqMHz = CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN(pFreqDomain).dvcoMinFreqMHz;

        // Release the semaphore.
        OS_SEMAPHORE_RW_GIVE_RD(clkFreqDomain_DoubleBufferControl.pSemaphore);

clkReadDomain_AnyTask_detach:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

clkReadDomain_AnyTask_exit:
    return status;
}


/*!
 * @brief       Flip the double-buffered output for each domain.
 * @see         clkFreqDomain_DoubleBufferControl
 *
 * @details     This function flips the write v. read-only halves of the
 *              double-buffered 'ClkFreqDomain::output' while holding a
 *              semaphore.
 *
 *              On entry, the write half should be up-to-date with the
 *              state of the hardware, which becomes the read-only half
 *              when this function flips the double buffer so that it can
 *              be accessed without race conditions via 'clkReadDomain_AnyTask'.
 *
 *              After flipping, this function copies the newly read-only half
 *              to the write half so that they are in sync, and future calls to
 *              'clkReadDomainList_PerfDaemon' and 'clkProgramDomainList_PerfDaemon'
 *              will have up-to-date information.
 *
 * @note        This function should be called only from the single-threaded
 *              PERF_DAEMON task since it manipulates the double-buffered
 *              'ClkFreqDomain::output' members.
 */
void clkFlipBuffer_PerfDaemon(void)
{
    LwU8 d;             ///< Domain array index

    OSTASK_OVL_DESC ovlDescList_Semaphore[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libSemaphoreRW)
    };

    //
    // Flip.  In other words, exchange the write side of the double-buffered
    // output with the read side so that the read side has the most recently
    // updated information.
    //
    // Use a semaphore since it is possible that the flip indicator is being
    // used in client code from some other task (and therefore thread).
    //
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList_Semaphore);
    {
        OS_SEMAPHORE_RW_TAKE_WR(clkFreqDomain_DoubleBufferControl.pSemaphore);
        {
            clkFreqDomain_DoubleBufferControl.bIsWriteIndexOne =
                        !clkFreqDomain_DoubleBufferControl.bIsWriteIndexOne;
        }
        OS_SEMAPHORE_RW_GIVE_WR(clkFreqDomain_DoubleBufferControl.pSemaphore);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList_Semaphore);

    //
    // Copy the most recently updated information (on the read-only side) to
    // the write side so that the updated info is available for the next call
    // to clkReadDomainList_PerfDaemon or clkProgramDomainList_PerfDaemon.
    //
    // Skip over NULL since the array is not perfectly dense.
    //
    for (d = 0; d < LW_ARRAY_ELEMENTS(clkFreqDomainArray); ++d)
    {
        ClkFreqDomain *pDomain = clkFreqDomainArray[d];
        if (pDomain != NULL)
        {
            CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(pDomain) =
                CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN(pDomain);
        }
    }
}


/*!
 * @brief       Read the hardware state for the list of clock domains.
 *
 * @note        This function should be called only from the single-threaded
 *              PERF_DAEMON task since it uses the write half of the double=
 *              buffered 'ClkFreqDomain::output' member via the functions
 *              it calls.
 *
 * @param[in/out]   pDomainList      Pointer to the clock domain list structure.
 *
 * @retval FLCN_ERR_ILWALID_ARGUMENT Invalid argument(s) are passed as input
 * @retval FLCN_ERR_NOT_SUPPORTED    Unsupported clock domain passed as input
 * @retval FLCN_OK                   Successfully read the hardware state
 */
FLCN_STATUS
clkReadDomainList_PerfDaemon
(
    LW2080_CTRL_CLK_CLK_DOMAIN_LIST *pDomainList
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        idx    = 0;

    //
    // This logic (including functions called from this function) uses the
    // write side of the double buffer, which must be single-threaded by
    // calling it only from the single-thread PERF_DAEMON task.
    //
    PMU_HALT_COND(osTaskIdGet() == RM_PMU_TASK_ID_PERF_DAEMON);

    for (idx = 0; idx < pDomainList->numDomains; idx++)
    {
        //
        // Default mask = XBAR as PMU needs it up to perform any register
        // access outside SYS cluster.
        // To-do akshatam: generalize this so that we can choose for a different
        // default domain mask for future chips if needed.
        //
        LwU32 clkDomainMask = LW2080_CTRL_CLK_DOMAIN_XBARCLK;

        // Element of the list which has the info we want to query/set.
        LW2080_CTRL_CLK_CLK_DOMAIN_LIST_ITEM *pDomainItem = &pDomainList->clkDomains[idx];

        // It's okay if the caller sends us zero-filled (i.e. unused) elements.
        if (pDomainItem->clkDomain == LW2080_CTRL_CLK_DOMAIN_UNDEFINED)
        {
            continue;
        }

        // Get frequency domain object corresponding to the clock domain.
        ClkFreqDomain *pFreqDomain = CLK_FREQ_DOMAIN_GET(pDomainItem->clkDomain);

        // If it doesn't exist, flag the error and exit the loop
        if (pFreqDomain == NULL)
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_ERROR_LOLWS ),
                DRF_NUM(_MAILBOX, _CLK_ERROR_LOLWS , _STATUS,      status)          |
                DRF_NUM(_MAILBOX, _CLK_ERROR_LOLWS , _DOMAIN_IDX,  BIT_IDX_32(pDomainItem->clkDomain)) |
                DRF_DEF(_MAILBOX, _CLK_ERROR_LOLWS , _ACTION,      _FREQ_DOMAIN_NULL));
            PMU_BREAKPOINT();
            break;
        }

        // Set clock domain required for read operation
        clkDomainMask |= pDomainItem->clkDomain;

        CLK_DOMAIN_ACCESS_BEGIN()
        {
            CLK_DOMAIN_IS_ACCESS_ENABLED(clkDomainMask)
            {
                // Read domain info from the hardware.
                status = clkRead_FreqDomain_VIP(pFreqDomain);

                // Exit the loop if there is an error.
                if (status != FLCN_OK)
                {
                    REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_ERROR_LOLWS ),
                        DRF_NUM(_MAILBOX, _CLK_ERROR_LOLWS , _STATUS,      status)          |
                        DRF_NUM(_MAILBOX, _CLK_ERROR_LOLWS , _DOMAIN_IDX,  BIT_IDX_32(pDomainItem->clkDomain)) |
                        DRF_DEF(_MAILBOX, _CLK_ERROR_LOLWS , _ACTION,      _READ));
                    PMU_BREAKPOINT();
                    break;
                }

                // Copy the result to the input parameter structure.
                pDomainItem->clkFreqKHz =
                    CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(pFreqDomain).freqKHz;
                pDomainItem->regimeId   =
                    CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(pFreqDomain).regimeId;
                pDomainItem->source     =
                    CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(pFreqDomain).source;
                pDomainItem->dvcoMinFreqMHz =
                    CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(pFreqDomain).dvcoMinFreqMHz;
            }
            CLK_DOMAIN_IS_ACCESS_DISABLED()
            {
                // Get information from the read-only half of the double buffer
                pDomainItem->clkFreqKHz =
                    CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN(pFreqDomain).freqKHz;
                pDomainItem->regimeId   =
                    CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN(pFreqDomain).regimeId;
                pDomainItem->source     =
                    CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN(pFreqDomain).source;
                pDomainItem->dvcoMinFreqMHz =
                    CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN(pFreqDomain).dvcoMinFreqMHz;
            }
        }
        CLK_DOMAIN_ACCESS_END();
    }

    // Flip the double-buffered output.
    clkFlipBuffer_PerfDaemon();

    // Done
    return status;
}


/*!
 * @brief       Configure and program the list of clock domains.
 *
 * @details     To reduce overlay swapping and minimize IMEM usage, this
 *              implementation configures all the domains, then programs all
 *              the domains because clkConfig and clkProgram implementations
 *              exist in separate IMEM overlays.
 *
 * @pre         This function should be called only from the single-threaded
 *              PERF_DAEMON task since it uses the write half of the double=
 *              buffered 'ClkFreqDomain::output' member via the functions
 *              it calls.
 *
 * @pre         Caller of this function *MUST* ensure that it acquires the
 *              access lock of all the domains it is trying to program.
 *              @ref CLK_DOMAIN_IS_ACCESS_ENABLED or ensure that all the
 *              low power features are disabled.
 *
 *              As of CL 28076464 (Mar 2020), this function is called from 2 places:
 *
 *              1) Change sequencer - This is the production path and is
 *              ilwoked as a part of a VF switch around which low power features
 *              are disabled.
 *
 *              2) RPC from RM - This is a testing-only path
 *              To-do akshatam: Check in a fix for this separately which would
 *              be to disable all the supported clock domains before calling
 *              into this function.
 *
 * @param[in/out]   pDomainList      Pointer to the clock domain list structure.
 *
 * @retval FLCN_ERR_ILWALID_ARGUMENT Invalid argument(s) passed as input
 * @retval FLCN_ERR_NOT_SUPPORTED    Unsupported clock domain passed as input
 * @retval FLCN_OK                   Successfully programmed the clock domain(s)
 */
FLCN_STATUS
clkProgramDomainList_PerfDaemon
(
    LW2080_CTRL_CLK_CLK_DOMAIN_LIST *pDomainList
)
{
    LwU8        idx;
    FLCN_STATUS status = FLCN_OK;

    OSTASK_OVL_DESC ovlDescList_Config[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkConfig)
    };

    OSTASK_OVL_DESC ovlDescList_Program[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkProgram)
    };

    //
    // This logic (including functions called from this function) uses the
    // write side of the double buffer, which must be single-threaded by
    // calling it only from the single-thread PERF_DAEMON task.
    //
    PMU_HALT_COND(osTaskIdGet() == RM_PMU_TASK_ID_PERF_DAEMON);

    // Check to make sure the domains exist.
    for (idx = 0; idx < pDomainList->numDomains; idx++)
    {
        // Ignore unused entries.
        if (pDomainList->clkDomains[idx].clkDomain == LW2080_CTRL_CLK_DOMAIN_UNDEFINED)
        {
            continue;
        }

        // Check frequency domain object corresponding to the clock domain.
        if (!CLK_FREQ_DOMAIN_GET(pDomainList->clkDomains[idx].clkDomain))
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_ERROR_LOLWS ),
                DRF_NUM(_MAILBOX, _CLK_ERROR_LOLWS , _STATUS,          status)          |
                DRF_NUM(_MAILBOX, _CLK_ERROR_LOLWS , _DOMAIN_IDX,      BIT_IDX_32(pDomainList->clkDomains[idx].clkDomain)) |
                DRF_DEF(_MAILBOX, _CLK_ERROR_LOLWS , _ACTION,          _FREQ_DOMAIN_NULL)    |
                DRF_NUM(_MAILBOX, _CLK_ERROR_LOLWS , _TARGET_FREQ_MHz, pDomainList->clkDomains[idx].clkFreqKHz / 1000));
            PMU_BREAKPOINT();
            goto clkProgramDomainList_PerfDaemon_exit;
        }
    }                                       // end check loop

    // Configure all domains.
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList_Config);
    {
        for (idx = 0; idx < pDomainList->numDomains; idx++)
        {
            ClkTargetSignal target;

            // Ignore unused entries.
            if (pDomainList->clkDomains[idx].clkDomain == LW2080_CTRL_CLK_DOMAIN_UNDEFINED)
            {
                continue;
            }

            // Copy target parameters.
            target                = clkReset_TargetSignal;
            target.super.freqKHz  = pDomainList->clkDomains[idx].clkFreqKHz;
            target.super.regimeId = pDomainList->clkDomains[idx].regimeId;
            target.super.source   = pDomainList->clkDomains[idx].source;
            target.super.dvcoMinFreqMHz = pDomainList->clkDomains[idx].dvcoMinFreqMHz;

            // Configure all the required phases for the target
            status = clkConfig_FreqDomain_VIP(CLK_FREQ_DOMAIN_GET(
                                pDomainList->clkDomains[idx].clkDomain), &target);

            //
            // FLCN_ERR_ILWALID_STATE means that OBJBIF is not yet initialized.
            //
            // TODO_CLK3: Remove this CYA once we can ensure that OBJBIF is initialized
            // before this logic is called.
            //
            if (status == FLCN_ERR_ILWALID_STATE &&
                pDomainList->clkDomains[idx].clkDomain == LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK)
            {
                continue;
            }

            // Other error
            if (status != FLCN_OK)
            {
                REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_ERROR_LOLWS ),
                    DRF_NUM(_MAILBOX, _CLK_ERROR_LOLWS , _STATUS,      status)          |
                    DRF_NUM(_MAILBOX, _CLK_ERROR_LOLWS , _DOMAIN_IDX,  BIT_IDX_32(pDomainList->clkDomains[idx].clkDomain)) |
                    DRF_DEF(_MAILBOX, _CLK_ERROR_LOLWS , _ACTION,      _CONFIG)         |
                    DRF_NUM(_MAILBOX, _CLK_ERROR_LOLWS , _TARGET_FREQ_MHz, pDomainList->clkDomains[idx].clkFreqKHz / 1000));
                PMU_BREAKPOINT();
                break;
            }
        }                                               // end config loop
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList_Config);

    // Program all domains.
    if (status == FLCN_OK)
    {
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList_Program);
        {
            for (idx = 0; idx < pDomainList->numDomains; idx++)
            {
                // Ignore unused entries.
                if (pDomainList->clkDomains[idx].clkDomain == LW2080_CTRL_CLK_DOMAIN_UNDEFINED)
                {
                    continue;
                }

                // Program all the phases.
                status = clkProgram_FreqDomain_VIP(CLK_FREQ_DOMAIN_GET(
                                        pDomainList->clkDomains[idx].clkDomain));

                // Failure
                if (status != FLCN_OK)
                {
                    REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_CLK_ERROR_LOLWS ),
                        DRF_NUM(_MAILBOX, _CLK_ERROR_LOLWS , _STATUS,      status)          |
                        DRF_NUM(_MAILBOX, _CLK_ERROR_LOLWS , _DOMAIN_IDX,  BIT_IDX_32(pDomainList->clkDomains[idx].clkDomain)) |
                        DRF_DEF(_MAILBOX, _CLK_ERROR_LOLWS , _ACTION,      _PROGRAM)        |
                        DRF_NUM(_MAILBOX, _CLK_ERROR_LOLWS , _TARGET_FREQ_MHz, pDomainList->clkDomains[idx].clkFreqKHz / 1000));
                    PMU_BREAKPOINT();
                    break;
                }
            }                                               // end program loop
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList_Program);
    }

    //
    // Flip the double buffer.  This function is smart enough to check for errors
    // for each individual domain.
    //
    clkFlipBuffer_PerfDaemon();

    // Exit point for errors before config/program.
clkProgramDomainList_PerfDaemon_exit:
    return status;
}

/*!
 * @brief Given a target frequency quantize it to nearest feasible clock
 * frequency based on bFloor.
 *
 * @param[in]     clkDomain  Clock domain ID @ref LW2080_CTRL_CLK_DOMAIN_<xyz>
 * @param[in,out] pfreqMHz   Non-quantized frequency in MHz will be input and
 *                           quantized frequency in MHz value will be output
 * @param[in]     bFloor     Boolean indicating whether the frequency should be
 *                           quantized to the floor (LW_TRUE) or ceiling (LW_FALSE)
 *                           of the closest supported value.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *      If the clock domain is invalid or the input pFreqMHz pointer is NULL
 * @return FLCN_ERR_NOT_SUPPORTED
 *      When the input domain doesn't support this functionality (usually non
 *      AVFS clock domains like DISPCLK/MCLK)
 * @return other
 *      Descriptive status code from sub-routines on success/failure
 */
FLCN_STATUS
clkQuantizeFreq
(
    LwU32   clkDomain,
    LwU16  *pFreqMHz,
    LwBool  bFloor
)
{
    FLCN_STATUS status;

    // Sanity check the input arguments
    if ((clkDomain == LW2080_CTRL_CLK_DOMAIN_UNDEFINED) || (pFreqMHz == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkQuantizeFreq_exit;
    }

    if (CLK3_IS_PROG_DOM_NAFLL_SOURCED(clkDomain))
    {
        status = clkNafllFreqMHzQuantize(clkDomain, pFreqMHz, bFloor);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkQuantizeFreq_exit;
        }
    }
    else
    {
        status = FLCN_ERR_NOT_SUPPORTED;
    }

clkQuantizeFreq_exit:
    return status;
}

/*!
 * @brief Given the input frequency, find the next lower freq supported by the
 * given clock domain.
 *
 * @param[in]       clkDomain  Clock domain ID @ref LW2080_CTRL_CLK_DOMAIN_<xyz>
 * @param[in,out]   pFreqMHz   Pointer to the buffer which is used to iterate
 *                             over the frequency.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *      If the clock domain is invalid or the input pFreqMHz pointer is NULL
 * @return FLCN_ERR_NOT_SUPPORTED
 *      When the input domain doesn't support this functionality (usually non
 *      AVFS clock domains like DISPCLK/MCLK)
 * @return other
 *      Descriptive status code from sub-routines on success/failure
 */
FLCN_STATUS
clkGetNextLowerSupportedFreq
(
    LwU32   clkDomain,
    LwU16  *pFreqMHz
)
{
    FLCN_STATUS status;

    // Sanity check the input arguments
    if ((clkDomain == LW2080_CTRL_CLK_DOMAIN_UNDEFINED) || (pFreqMHz == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkGetNextLowerSupportedFreq_exit;
    }

    if (CLK3_IS_PROG_DOM_NAFLL_SOURCED(clkDomain))
    {
        status = clkNafllFreqsIterate(clkDomain, pFreqMHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkGetNextLowerSupportedFreq_exit;
        }
    }
    else
    {
        status = FLCN_ERR_NOT_SUPPORTED;
    }

clkGetNextLowerSupportedFreq_exit:
    return status;
}

