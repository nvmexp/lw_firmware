/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @brief       Manages all the handling of ClkFreqDomain
 * @see         https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author      Akshata Mahajan
 * @author      Antone Vogt-Varvak
 * @author      Prafull Gupta
 *
 * @details     This file contains the implementations for five classes:
 *                  ClkFreqDomain               Base class
 *                  ClkVolatileFreqDomain       Domains whose state cannot be cached
 *                  ClkNolwolatileFreqDomain    Domains whose state can be cached
 *                  ClkProgrammableFreqDomain   Domains whose state can be programmed and cached
 *                  ClkFixedFreqDomain          Domains whose state can be cached but not programmed
 *
 *              Each of these classes uses the same structure (ClkFreqDomain).
 *              In effect, they are really just alises of one another, which is
 *              to facilitate macro definitions for the subclass virtual tables
 *              and the like.  As such, they are all put into this same file
 *              to simplify maintenance.
 */

/* ------------------------- System Includes ------------------------------- */

#include "pmusw.h"

/* ------------------------- Application Includes -------------------------- */

#include "boardobj/boardobjgrp.h"
#include "pmu_objclk.h"
#include "dmemovl.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"
#include "clk3/sd/clksd.h"

/* ------------------------ Global Variables ------------------------------- */

/*!
 * @brief       Per-class Virtual Table
 *
 * @details     Only concrete classes that are actually instantiated need
 *              virtual tables.
 */
CLK_DEFINE_VTABLE_CLK3__DOMAIN(FixedFreqDomain);

/*!
 * @brief       Double Buffer Control
 */
struct ClkFreqDomain_DoubleBufferControl clkFreqDomain_DoubleBufferControl
        GCC_ATTRIB_SECTION("dmem_clk3x", "_clkFreqDomain_DoubleBufferControl");

/* -------------------- Static Function Prototypes ------------------------- */

static BoardObjGrpIfaceModel10SetHeader (s_clkFreqDomainGrpIfaceModel10SetHeader)
        GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkFreqDomainGrpIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry  (s_clkFreqDomainGrpIfaceModel10SetEntry)
        GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkFreqDomainGrpIfaceModel10SetEntry");
static BoardObjGrpIfaceModel10GetStatusHeader (s_clkFreqDomainGrpIfaceModel10GetStatusHeader)
        GCC_ATTRIB_SECTION("imem_libClkStatus", "s_clkFreqDomainGrpIfaceModel10GetStatusHeader");
static BoardObjIfaceModel10GetStatus              (s_clkFreqDomainIfaceModel10GetStatus)
        GCC_ATTRIB_SECTION("imem_libClkStatus", "s_clkFreqDomainIfaceModel10GetStatus");


/*!
 * @brief       'LW2080_CTRL_CLK_FREQ_DOMAIN_STATUS_BASE' from 'BOARDOBJ' pointer
 * @see         ClkFreqDomain::pStatus
 *
 * @details     Since the domain object immediately follows the 'BOARDOBJ'
 *              object in memory, all this function does is a little pointer
 *              arithmetic and some casting.
 *
 *              The caller must be sure to pass a blob with a 'ClkFreqDomain_Status'
 *              derived payload (as opposed to 'CLkFreqSrc_Status', for example).
 *              No such checking is done by this function.
 *
 *              Note that the actual class of the object is a subclass of
 *              'ClkFreqDomain_Status' with the class specified by the virtual
 *              table 'ClkFreqDomain::pVirtual'.
 *
 *              This function is used by the constructor 's_clkFreqDomainGrpIfaceModel10SetEntry'.
 *
 *              See comments at 'ClkFreqDomain_Virtual::sizeOfStatus' for a
 *              complete explanation.
 */
LW_FORCEINLINE inline
static LW2080_CTRL_CLK_FREQ_DOMAIN_STATUS_BASE *clkStatusFromBoardobj_FreqDomain(BOARDOBJ *pBoardObj)
{
    LwU8 offset = (LW_ALIGN_UP(sizeof(BOARDOBJ), 4));
    return (LW2080_CTRL_CLK_FREQ_DOMAIN_STATUS_BASE *)(((char*) pBoardObj) + offset);
}

/* ----------------------------- Macros ------------------------------------ */
/* ------------------------ Public Functions ------------------------------- */

/*!
 * @brief Pre-inits the SW state of FREQ_DOMAIN_GRP/FREQ_DOMAIN
 */
void
clkFreqDomainPreInit(void)
{
    FREQ_DOMAIN_GRP *pClkFreqDomainGrp = CLK_FREQ_DOMAINS_GET();

    // Set the clock domain semaphore to NULL to indicate it is uninitialized
    pClkFreqDomainGrp->pClkFreqDomainDisableSemaphoreRW = NULL;
}

/*!
 * @brief RM_PMU_BOARDOBJ_CMD_GRP_SET interface handler
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkFreqDomainBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status;
    OSTASK_OVL_DESC ovlDescList_Construct[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLibClk3)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkConstruct)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkReadPerfDaemon)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clk3x)
    };
    OSTASK_OVL_DESC ovlDescList_Semaphore[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libSemaphoreRW)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libInit)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clk3x)
    };

    // Attach overlays
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList_Construct);
    {
        // Construct the BOARDOBJ group and its entries.
        status = BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,    // _grpType
            FREQ_DOMAIN,                                // _class
            pBuffer,                                    // _pBuffer
            s_clkFreqDomainGrpIfaceModel10SetHeader,          // _hdrFunc
            s_clkFreqDomainGrpIfaceModel10SetEntry,           // _entryFunc
            pascalAndLater.clk.clkFreqDomainGrpSet,     // _ssElement
            OVL_INDEX_DMEM(clk3x),                      // _ovlIdxDmem
            BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables

        // Finish Clocks 3.x construction.
        if (status == FLCN_OK)
        {
            status = clkConstruct_SchematicDag();
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList_Construct);

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList_Semaphore);
    {
        // Create the semaphore needed for double-buffering.
        if (status == FLCN_OK)
        {
            clkFreqDomain_DoubleBufferControl.pSemaphore =
                        OS_SEMAPHORE_CREATE_RW(OVL_INDEX_DMEM(clk3x));
            if (clkFreqDomain_DoubleBufferControl.pSemaphore == NULL)
            {
                status = FLCN_ERR_NO_FREE_MEM;
                PMU_HALT();
            }
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList_Semaphore);

    return status;
}


/*!
 * FREQ_DOMAIN_GRP handler for the RM_PMU_BOARDOBJ_CMD_GRP GET_STATUS interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkFreqDomainBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS     status;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clk3x)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS_AUTO_DMA(E32,       // _grpType
            FREQ_DOMAIN,                                    // _class
            pBuffer,                                        // _pBuffer
            s_clkFreqDomainGrpIfaceModel10GetStatusHeader,              // _hdrFunc
            s_clkFreqDomainIfaceModel10GetStatus,                       // _entryFunc
            pascalAndLater.clk.clkFreqDomainGrpGetStatus);  // _ssElement
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief   Helper function to reset output double buffers - both write and read-only
 *
 * @note    SHOULD NOT be called by clients (any code outside of clocks)
 *
 * @note    We need to reset both write and read-only buffers to make sure they're
 *          consistent.The caller of this function must make sure that this function
 *          is called when none of the buffers are being used (no reads/writes are in
 *          progress for the given clock domain).
 *          Lwrrently this is being called during critical entry of GPC-RG (@ref
 *          clkGpcRgDeInit) at which point we're safe to call it as -
 *          #1 - clock domains for which we call this are disabled before engaging GPC-RG
 *          #2 - critical entry of GPC-RG is run at the highest priority in-line
 *               on the LPWR task.
 *
 * @param[in]  clkDomainMask    Mask of clock domains for which the output double
 *                              buffers need to be reset
 *
 * @return FLCN_ERR_NOT_SUPPORTED   Invalid clock domain for which FREQ_DOMAIN
 *                                  object doesn't exist
 */
FLCN_STATUS
clkFreqDomainsResetDoubleBuffer
(
    LwU32       clkDomainMask
)
{
    FLCN_STATUS status     = FLCN_OK;
    LwU8        clkDomBit;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clk3x)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Loop over the given clock domain mask
        FOR_EACH_INDEX_IN_MASK(32, clkDomBit, clkDomainMask)
        {
            // Get frequency domain object corresponding to the clock domain
            LwU32          clkDomain   = LWBIT32(clkDomBit);
            ClkFreqDomain *pFreqDomain = CLK_FREQ_DOMAIN_GET(clkDomain);

            // If it doesn't exist, flag the error and exit the loop
            if (pFreqDomain == NULL)
            {
                status = FLCN_ERR_NOT_SUPPORTED;
                PMU_BREAKPOINT();
                goto clkFreqDomainsResetDoubleBuffer_exit; // NJ??
            }

            // Ensure that the entire signal is reset, not just the frequency
            CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(pFreqDomain)     = clkReset_Signal;
            CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN(pFreqDomain) = clkReset_Signal;
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

clkFreqDomainsResetDoubleBuffer_exit:
    return status;
}

/*!
 * @brief   Helper function to acquire access lock for the given clock domains.
 *
 * @note    Client shall never directly call this interface - should use public
 *          access macros instead.
 *          Check @ref CLK_DOMAIN_ACCESS_BEGIN, CLK_DOMAIN_ACCESS_END,
 *          CLK_DOMAIN_IS_ACCESS_ENABLED and CLK_DOMAIN_IS_ACCESS_DISABLED.
 *
 * @param[in]  clkDomainMask   Mask of clock domains to acquire access lock for
 *
 * @return LW_TRUE      Access allowed to all the concerned clock domains
 * @return LW_FALSE     Access not allowed to the given clock domains
 */
LwBool
clkFreqDomainsAcquireAccessLock
(
    LwU32       clkDomainMask
)
{
    FREQ_DOMAIN_GRP *pClkFreqDomainGrp  = CLK_FREQ_DOMAINS_GET();
    LwBool           bAccessAllowed     = LW_FALSE;
    OSTASK_OVL_DESC  ovlDescList[]      = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkAccessControl)
    };

    //
    // Return early if the required feature isn't enabled - access to any domain
    // is always enabled in that case.
    //
    if (!PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAIN_ACCESS_CONTROL))
    {
        return LW_TRUE;
    }

    // Attach overlays
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Sanity check
        if (pClkFreqDomainGrp == NULL)
        {
            PMU_BREAKPOINT();
            bAccessAllowed = LW_FALSE;
            goto clkFreqDomainsAcquireAccessLock_exit;
        }

        //
        // Return early if the semaphore isn't initialized yet.
        // When this check is true it means that the client is trying to acquire
        // access lock even before the semaphore is set up in @ref
        // s_clkFreqDomainGrpIfaceModel10SetHeader. In such a case, the access to the clock
        // domains will always be enabled but no client can acquire the access lock or
        // disable the clock domains as the semaphore isn't ready yet to do so.
        // This is not expected at this point as the case when semaphore is NULL due to
        // the @ref PMU_CLK_DOMAIN_ACCESS_CONTROL feature being disabled is already taken
        // care of above and any failure in creation of semaphore should have been caught
        // much earlier during construct.
        //
        if (pClkFreqDomainGrp->pClkFreqDomainDisableSemaphoreRW == NULL)
        {
            PMU_BREAKPOINT();
            bAccessAllowed = LW_TRUE;
            goto clkFreqDomainsAcquireAccessLock_exit;
        }

        // Grab the read access of the semaphore to read mask of disabled clock domains
        OS_SEMAPHORE_RW_TAKE_RD((pClkFreqDomainGrp)->pClkFreqDomainDisableSemaphoreRW);
        {
            //
            // If all the clock domains in the mask are enabled, provide access lock
            // to the client. Client must retain the access lock until the read/write
            // operation is completed hence head to return from here.
            //
            if ((pClkFreqDomainGrp->clkFreqDomainDisabledMask & clkDomainMask) == 0U)
            {
                bAccessAllowed = LW_TRUE;
                goto clkFreqDomainsAcquireAccessLock_exit;
            }
        }

        //
        // If any of the clock domain in the input mask is disabled, we need to release
        // the semaphore read access of disabled clock domains.
        //
        OS_SEMAPHORE_RW_GIVE_RD((pClkFreqDomainGrp)->pClkFreqDomainDisableSemaphoreRW);

clkFreqDomainsAcquireAccessLock_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return bAccessAllowed;
}

/*!
 * @brief   Enable/disable access to the given clock domains
 *
 * @param[in]  clkDomainMask    Mask of clock domains to be enabled/disabled
 * @param[in]  clientId         ID of the client which wants to enable/disable
 *                              the given clock domains - @ref LW2080_CTRL_CLK_CLIENT_ID_<xyz>
 * @param[in]  bDisable         TRUE = disable the given domains; FALSE = enable the given domains
 * @param[in]  bConditional     Set to TRUE if the request is conditional (not wait infinitely for
 *                              acquiring the access lock)
 *
 * @return FLCN_OK                      Given clock domains successfully enabled/disabled
 * @return FLCN_ERR_TIMEOUT             Timed out while waiting for access lock
 * @return FLCN_ERR_ILWALID_STATE       FREQ_DOMAIN_GRP pointer is NULL
 * @return FLCN_ERR_ILWALID_ARGUMENT    Given client ID or clkDomainMask is invalid
 * @return FLCN_ERR_NOT_SUPPORTED       Clock domain access control feature not supported
 */
FLCN_STATUS
clkFreqDomainsDisable
(
    LwU32       clkDomainMask,
    LwU8        clientId,
    LwBool      bDisable,
    LwBool      bConditional
)
{
    FREQ_DOMAIN_GRP *pClkFreqDomainGrp = CLK_FREQ_DOMAINS_GET();
    FLCN_STATUS      status            = FLCN_OK;
    LwBool           bSemTakeWrSuccess = LW_FALSE;
    LwU8             clkDomBit;
    OSTASK_OVL_DESC  ovlDescList[]     = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkAccessControl)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clk3x)
    };

    // Assert early if the required feature isn't enabled
    if (!PMUCFG_FEATURE_ENABLED(PMU_CLK_DOMAIN_ACCESS_CONTROL))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_NOT_SUPPORTED;
    }

    // Sanity check clock freq domain group
    if (pClkFreqDomainGrp == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_STATE;
    }

    // Sanity check if client ID/clock domain is valid (is one of the known client IDs)
    if ((clkDomainMask == 0U) || ((LWBIT_TYPE(clientId, LwU32) &
         LW2080_CTRL_CLK_CLIENT_ID_VALID_MASK) == 0U))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    //
    // Return early if the semaphore isn't initialized yet.
    // When this check is true it means that the client is trying to disable
    // clock domains even before the semaphore is set up in
    // @ref s_clkFreqDomainGrpIfaceModel10SetHeader. In such a case, the domains
    // will always be considered as enabled but no client can disable them
    // as the semaphore isn't ready yet to do so.
    //
    if (pClkFreqDomainGrp->pClkFreqDomainDisableSemaphoreRW == NULL)
    {
        //
        // To-do akshatam: Return FLCN_ERR_NOT_INITIALIZED instead for pre-silicon
        // platforms as well once bug 2908724 is fixed.
        // This is just to work around one of the issue found in AXL which needs
        // to be fixed from LPWR side. Landing here means that GPC-RG is getting
        // engaged even before clock domains have been set up which shouldn't be
        // allowed - check bug 2908724 for more details.
        // Although we're WAR-ing this issue on pre-silicon platforms as this is
        // specific to non-pstate roms and also to keep AXL happy, we shouldn't
        // let this happen on silicon hence fail out loudly here if we're on
        // silicon.
        //
        if (IsSilicon())
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_NOT_INITIALIZED;
        }
        else
        {
            return FLCN_OK;
        }
    }

    // Attach overlays
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Grab the write access to update mask of disabled clock domains
        if (bConditional)
        {
            OS_SEMAPHORE_RW_TAKE_WR_COND(pClkFreqDomainGrp->pClkFreqDomainDisableSemaphoreRW, &bSemTakeWrSuccess);
            if (!bSemTakeWrSuccess)
            {
                status = FLCN_ERR_TIMEOUT;
                goto clkFreqDomainsDisable_exit;
            }
        }
        else
        {
            OS_SEMAPHORE_RW_TAKE_WR((pClkFreqDomainGrp)->pClkFreqDomainDisableSemaphoreRW);
            bSemTakeWrSuccess = LW_TRUE;
        }

        // Update the domain masks to enable/disable it as needed
        FOR_EACH_INDEX_IN_MASK(32, clkDomBit, clkDomainMask)
        {
            // Get frequency domain object corresponding to the clock domain
            LwU32          clkDomain   = LWBIT32(clkDomBit);
            ClkFreqDomain *pFreqDomain = CLK_FREQ_DOMAIN_GET(clkDomain);

            // If it doesn't exist, flag the error and exit the loop
            if (pFreqDomain == NULL)
            {
                status = FLCN_ERR_NOT_SUPPORTED;
                PMU_BREAKPOINT();
                goto clkFreqDomainsDisable_exit;
            }

            if (bDisable)
            {
                pClkFreqDomainGrp->clkFreqDomainDisabledMask |= clkDomain;
                pFreqDomain->clkFreqDomainDisabledClientMask |= LWBIT_TYPE(clientId, LwU32);
            }
            else
            {
                pFreqDomain->clkFreqDomainDisabledClientMask &= ~LWBIT_TYPE(clientId, LwU32);

                // Enable the domain only when all of its clients have enabled it
                if (pFreqDomain->clkFreqDomainDisabledClientMask == 0)
                {
                    pClkFreqDomainGrp->clkFreqDomainDisabledMask &= (~clkDomain);
                }
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;

clkFreqDomainsDisable_exit:

        if (bSemTakeWrSuccess)
        {
            // Release write access
            OS_SEMAPHORE_RW_GIVE_WR((pClkFreqDomainGrp)->pClkFreqDomainDisableSemaphoreRW);
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}


/*!
 * @brief       Print dynamic state for all clock domains.
 * @memberof    ClkFreqDomain
 * @see         clkPrint_FreqSrc_VIP
 * @see         CLK_PRINT_ALL
 *
 * @details     In builds for which it is enabled, this function prints available
 *              dynamic state for both software and hardware for all domains.
 *
 *              This function should be called only via CLK_PRINT_ALL and only
 *              before a breakpoint is fired because it prints too much info to
 *              be useful for any other scenario.
 *
 * @param[in]   file        __FILE__ macro value
 * @param[in]   line        __LINE__ macroe value
 * @param[in]   phase       Phase number lwrrently being configured or programmed
 */
#if CLK_PRINT_ENABLED
void
clkPrintAll_FreqDomain(const char *file, unsigned line, ClkPhaseIndex phase)
{
    LwU8 d;             ///< Domain array index

    CLK_PRINTF("============ Clocks 3.x: Current Domain State Information ============\n\n");

    for (d = 0; d < LW_ARRAY_ELEMENTS(clkFreqDomainArray); ++d)
    {
        ClkFreqDomain *pDomain = clkFreqDomainArray[d];
        if (pDomain != NULL)
        {
            CLK_PRINTF("=== Clock Domain id=0x%08x\n", pDomain->clkDomain);     // LW2080_CTRL_CLK_DOMAIN_<xyz>
            clkPrint_Signal(&CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN(pDomain), "Read buffer");
            clkPrint_Signal(&CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(pDomain), "Write buffer");
            clkPrint_FreqSrc_VIP(pDomain->pRoot, pDomain->phaseCount);
            CLK_PRINTF("\n");
        }
    }

    CLK_PRINTF("%s(%u): phase=%u\n", file, line, (unsigned) phase);
}
#endif


/* ------------------------- Private Functions ------------------------------ */

/*!
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
static FLCN_STATUS
s_clkFreqDomainGrpIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    FREQ_DOMAIN_GRP *pFreqDomainGrp = NULL;
    const RM_PMU_CLK_FREQ_DOMAIN_BOARDOBJGRP_SET_HEADER *pSet = NULL;
    FLCN_STATUS         status;
    OSTASK_OVL_DESC     ovlDescList_Semaphore[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libSemaphoreRW)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libInit)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkAccessControl)
    };

    //
    // Call into BOARDOBJGRP super-constructor, which will do actual memory
    // allocation.
    //
    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkFreqDomainGrpIfaceModel10SetHeader_exit;
    }

    pFreqDomainGrp              = (FREQ_DOMAIN_GRP *) pBObjGrp;
    pSet                        = (const RM_PMU_CLK_FREQ_DOMAIN_BOARDOBJGRP_SET_HEADER *)pHdrDesc;

    // Copy global FREQ_DOMAIN_GRP state.
    pFreqDomainGrp->initFlags   = pSet->initFlags;

    // Initialize clock domain disable mask to all enabled
    pFreqDomainGrp->clkFreqDomainDisabledMask = 0;

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList_Semaphore);
    {
        // Create clock domain disable mask read-write semaphore
        pFreqDomainGrp->pClkFreqDomainDisableSemaphoreRW =
            OS_SEMAPHORE_CREATE_RW(OVL_INDEX_DMEM(clkAccessControl));

        if (pFreqDomainGrp->pClkFreqDomainDisableSemaphoreRW == NULL)
        {
            status = FLCN_ERR_NO_FREE_MEM;
            PMU_BREAKPOINT();
            goto s_clkFreqDomainGrpIfaceModel10SetHeader_exit;
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList_Semaphore);

s_clkFreqDomainGrpIfaceModel10SetHeader_exit:
    return status;
}


/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_clkFreqDomainGrpIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,         // out
    RM_PMU_BOARDOBJ    *pBuf                // in
)
{
    const RM_PMU_CLK_FREQ_DOMAIN_BOARDOBJ_SET *pSet =
                    (const RM_PMU_CLK_FREQ_DOMAIN_BOARDOBJ_SET *)pBuf;
    ClkFreqDomain *pClkFreqDomain = CLK_FREQ_DOMAIN_GET(pSet->clkDomain);
    LW2080_CTRL_CLK_FREQ_DOMAIN_STATUS_BASE *pBoardObjStatus;
    FLCN_STATUS status;

    //
    // NULL indicates a domain that doesn't apply to this chip.
    //
    // TODO_CLK3: Consider creating a 'ClkUnsupportedDomain' class that returns
    // 'FLCN_ERR_NOT_SUPPORTED' from each of its virtual functions.  Then, if
    // null here proves problematic, we would construct a 'ClkUnsupportedDomain'
    // object instead.
    //
    if (pClkFreqDomain == NULL)
    {
        *ppBoardObj = NULL;
        status = FLCN_ERR_NOT_SUPPORTED;
        PMU_BREAKPOINT();
        goto s_clkFreqDomainGrpIfaceModel10SetEntry_exit;
    }

    //
    // Call into BOARDOBJ super-constructor, which will do actual memory
    // allocation.
    //
    // CLK3_TODO: Evaluate the data being allocated and trim away anything that
    // is not needed.  Specifically, the data being pointed to by 'pBoardObjStatus'
    // goes unused.  Verify this.  It may be that 'clkDomain' is used somewhere.
    // This memory was used in Clocks 3.0 and copied to framebuffeer memory by
    // 's_clkFreqDomainIfaceModel10GetStatus'.  However, as of this changelist for
    // Clocks 3.1, this data structure has been replaced with statically allocated
    // data in 'ClkFreqDomain' per bug 2138863 to facilitate double-buffering per
    // bug 2585557.
    //
    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj,
                (LW_ALIGN_UP(sizeof(BOARDOBJ), 4) +
                sizeof(LW2080_CTRL_CLK_FREQ_DOMAIN_STATUS_CLK3)),
                pBuf);

    if (status != FLCN_OK)
    {
        goto s_clkFreqDomainGrpIfaceModel10SetEntry_exit;
    }

    // Get the pointer to the newly allocated memory.
    pBoardObjStatus = clkStatusFromBoardobj_FreqDomain(*ppBoardObj);

    // Copy LW2080_CTRL_CLK_DOMAIN_[dom] value.
    pBoardObjStatus->clkDomain = pSet->clkDomain;

    // Initialize the clock output signal to be reset.
    pBoardObjStatus->output    = clkReset_Signal;

    //
    // Fill in the enabled domains mask with the current clock domain per
    // LW2080_CTRL_CLK_DOMAIN_[dom].  This should be the bit posisition
    // of the index.
    //
    Clk.freqDomainGrp.clkFreqDomainSupportedMask |= pBoardObjStatus->clkDomain;

    // Initialize the clock domain inside the ClkFreqDomain object.
    pClkFreqDomain->clkDomain = pSet->clkDomain;

    // Initialize the client mask to indicate that the domain is enabled
    pClkFreqDomain->clkFreqDomainDisabledClientMask =
                          LW2080_CTRL_CLK_CLIENT_ID_MASK_RESET_VALUE;

s_clkFreqDomainGrpIfaceModel10SetEntry_exit:
    return status;
}


/*!
 * @copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
static FLCN_STATUS
s_clkFreqDomainGrpIfaceModel10GetStatusHeader
(
    BOARDOBJGRP_IFACE_MODEL_10         *pModel10,
    RM_PMU_BOARDOBJGRP  *pBuf,              // out
    BOARDOBJGRPMASK     *pMask              // in/out
)
{
    return FLCN_OK;
}


/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
static FLCN_STATUS
s_clkFreqDomainIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pBuf        // out
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    const ClkFreqDomain *pDomain;
    LW2080_CTRL_CLK_FREQ_DOMAIN_STATUS_BASE *pStatus;

    FLCN_STATUS status = boardObjIfaceModel10GetStatus(pModel10, pBuf);
    if (status != FLCN_OK)
    {
        goto s_clkFreqDomainIfaceModel10GetStatus_exit;
    }

    // Get the pointer to the BOARDOBJ-allocated memory.
    pStatus = clkStatusFromBoardobj_FreqDomain(pBoardObj);

    // Get the clock domain which contains the up-to-date status info.
    pDomain = CLK_FREQ_DOMAIN_GET(pStatus->clkDomain);

    //
    // Copy output signal info into FB memory directly from Clocks 3.1.
    //
    // Since the output signal is double-buffered, use the reading buffer (i.e.
    // not the writing buffer.
    //
    ((RM_PMU_CLK_FREQ_DOMAIN_BOARDOBJ_GET_STATUS *)pBuf)->domain.clkDomain =
                        pStatus->clkDomain;
    ((RM_PMU_CLK_FREQ_DOMAIN_BOARDOBJ_GET_STATUS *)pBuf)->domain.output =
                        CLK_OUTPUT_READ_ONLY_BUFFER__FREQDOMAIN(pDomain);

s_clkFreqDomainIfaceModel10GetStatus_exit:
    return status;
}


/* ------------------ Protected Implementations of clkRead ------------------ */

/*!
 * @brief       Read hardware unless cache is valid.
 *
 * @details     This function is approprite for programmable and fixed
 *              domains, but not volatile domains.
 */
FLCN_STATUS clkRead_NolwolatileFreqDomain(ClkFreqDomain *this)
{
    //
    // We already have a valid configuration, so there is little to do.
    //
    // 'output' should be reset anytime there is an error returned
    // from 'clkRead', 'clkConfig', 'clkProgram', or 'clkCleanup'.
    // It should also be initialized to zero at construction.
    //
    // Update the debug counts only on cache hit since 'readCall' is
    // incremented by 'clkRead_VolatileFreqDomain'.
    //
    if (CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(this).freqKHz)
    {
        return FLCN_OK;
    }

    // Read the actual hardward.
    return clkRead_VolatileFreqDomain(this);
}


/*!
 * @brief       Read hardware without checking cache.
 *
 * @details     This function is appropriate for volatile domains, but not
 *              programmable and fixed domains.
 */
FLCN_STATUS clkRead_VolatileFreqDomain(ClkFreqDomain *this)
{
    FLCN_STATUS status;

    //
    // Clear the result of any prior 'clkConfig' call since that
    // call was made based on different phase-zero data.
    //
    // Phase array now has only one valid phase, phase zero, which is
    // about to be read.
    //
    this->phaseCount = 1;

    // Ensure that the entire signal is reset, not just the frequency.
    CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(this) = clkReset_Signal;

    // Read not supported if schematic dag is uninitialized
    if (this->pRoot == NULL)
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }

    // Read the root input.
    status = clkRead_FreqSrc_VIP(this->pRoot, &CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(this), LW_TRUE);

    // Make sure the output signal is reset in case of error.
    if (status != FLCN_OK)
    {
        CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(this) = clkReset_Signal;
    }

    // Done
    return status;
}


/* ----------------- Protected Implementations of clkConfig ----------------- */

FLCN_STATUS clkConfig_FixedFreqDomain(ClkFreqDomain *this, const ClkTargetSignal *pTarget)
{
    return FLCN_ERR_ILLEGAL_OPERATION;
}


/* ----------------- Protected Implementations of clkProgram ---------------- */

FLCN_STATUS clkProgram_FixedFreqDomain(ClkFreqDomain *this)
{
    return FLCN_ERR_ILLEGAL_OPERATION;
}

/*!
 * @brief       Program hardware.
 */
FLCN_STATUS clkProgram_ProgrammableFreqDomain(ClkProgrammableFreqDomain *this)
{
    ClkPhaseIndex p;
    FLCN_STATUS status;

    //
    // If one of these assertions fail, it may mean that clkRead and clkConfig
    // were not successfully called since these function set 'phaseCount'.
    //
    PMU_HALT_COND(this->phaseCount);
    PMU_HALT_COND(this->phaseCount <= CLK_MAX_PHASE_COUNT);

    // clkConfig sets 'phaseCount = 1' to indicate that there is nothing to do.
    if (this->phaseCount == 1)
    {
        return FLCN_OK;
    }

    //
    // Program each phase in order, starting with phase 1.  Exit on any error.
    // Program the root input which daisy-chains along the schematic as needed.
    //
    status = FLCN_OK;
    for (p = 1; p < this->phaseCount && status == FLCN_OK; ++p)
    {
        status = clkProgram_FreqSrc_VIP(this->pRoot, p);
    }

    //
    // If programming wen't okay, clean up the hardware and copy the last phase
    // to all the other phases to get ready for the next clkRead or clkConfig.
    //
    if (status == FLCN_OK)
    {
        status = clkCleanup_FreqSrc_VIP(this->pRoot, LW_TRUE);
    }

    //
    // There is an error in either clkProgram or clkCleanup.
    // Make sure the output signal is reset since it is unknown.
    // This ilwalidates the cache and causes clkRead_NolwolatileFreqDomain
    // be read again from hardware.
    //
    if (status != FLCN_OK)
    {
        CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(this) = clkReset_Signal;
    }

    // Done
    return status;
}

