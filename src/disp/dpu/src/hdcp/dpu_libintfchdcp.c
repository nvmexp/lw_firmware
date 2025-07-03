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
 * @file   dpu_libintfchdcp.c
 * @brief  Container-object for DPU HDCP routines. Contains generic non-HAL
 *         interrupt-routines plus logic required to hook-up chip-specific
 *         interrupt HAL-routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "dpusw.h"
#include "dpu_objdpu.h"
#ifdef GSPLITE_RTOS
#include "dpu_os.h"
#include "lwosselwreovly.h"
#endif
/* ------------------------- OpenRTOS Includes ------------------------------ */
/* ------------------------- Application Includes --------------------------- */
#include "lib_intfchdcp.h"
// TODO: Move hdcp1.x secure action structure to hdcp1.x header file.
#include "hdcp22wired_selwreaction.h"
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
extern void hdcpSendResponse(HDCP_CONTEXT *pContext, LwU8 eventType);
#ifdef GSPLITE_RTOS
extern FLCN_STATUS dpuSelwreActionHsEntry(SELWRE_ACTION_ARG *pArg);
#endif
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
#ifdef GSPLITE_RTOS
static LwU8 gHdcpHsArg[HDCP1X_SELWRE_ACTION_ARG_BUF_SIZE]
    GCC_ATTRIB_SECTION("dmem_hdcpHs", "_gHdcpHsArg")
    GCC_ATTRIB_ALIGN(SCP_BUF_ALIGNMENT);
#endif

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Function Definitions ------------------- */
#ifdef GSPLITE_RTOS
/*!
 * This function is used to load and unload HS Overlay.
 *
 * @param[in]  bIsLoad  Is load or unload request
 */
inline static void
_hdcpLoadUnloadHSOverlays
(
    LwBool  bIsLoad
)
{
    //
    // When HS ucode encryption is enabled,
    // for any task which gets scheduled in any timer interrupt,
    // OS will always reload the HS overlays attached to that task irrespective
    // of whether context switch or not which is effecting performance.
    // Hence, we didnt attach during task init and is dynamically attaching
    // and detaching HS overlays only when needed.
    //
#ifdef HS_UCODE_ENCRYPTION
    if (bIsLoad)
    {
        OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libSecBusHs));
        OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libCommonHs));
#ifdef LIB_CCC_PRESENT
        OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libCccHs));
#endif
        OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libMemHs));
#ifndef HDCP22_KMEM_ENABLED
        OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libScpCryptHs));
#endif
        OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(selwreActionHs));
    }
    else
    {
        OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libSecBusHs));
        OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libCommonHs));
        OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libMemHs));
#ifndef HDCP22_KMEM_ENABLED
        OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libScpCryptHs));
#endif
        OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(selwreActionHs));
#ifdef LIB_CCC_PRESENT
        OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libCccHs));
#endif
    }
#endif
}

#endif

/* ------------------------- Public Function Definitions ------------------- */

#ifdef GSPLITE_RTOS
/*!
 * @brief This is a library interface function for accessing Secure Bus in HS mode
 *
 * @param[in]  bIsReadReq        Is secure bus read request or write request
 * @param[in]  addr              address of register to be read or written
 * @param[in]  dataIn            Value to be written (not applicable for read request)
 * @param[out] dDataOut          Pointer to be filled with read value (not applicable for write request)
 *
 * @return     FLCN_STATUS       FLCN_OK if read or write request is successful
 */
FLCN_STATUS
libIntfcHdcpSecBusAccess
(
    LwBool bIsReadReq,
    LwU32  addr,
    LwU32  dataIn,
    LwU32  *pDataOut
)
{
    SELWRE_ACTION_HDCP_ARG *pArg    = (SELWRE_ACTION_HDCP_ARG *)gHdcpHsArg;
    FLCN_STATUS             status  = FLCN_OK;

    if(bIsReadReq && pDataOut == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    //
    // Call entry function (0th offset of overlay). This is the entry into HS
    // mode.
    //
    memset(pArg, 0, sizeof(SELWRE_ACTION_HDCP_ARG));
    pArg->actionType                = SELWRE_ACTION_REG_ACCESS;
    pArg->regAccessArg.bIsReadReq   = bIsReadReq;
    pArg->regAccessArg.addr         = addr;
    pArg->regAccessArg.val          = dataIn;

    _hdcpLoadUnloadHSOverlays(LW_TRUE);

    OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(selwreActionHs), NULL, 0, HASH_SAVING_DISABLE);

    status = dpuSelwreActionHsEntry((SELWRE_ACTION_ARG *)pArg);
    if (pDataOut && bIsReadReq)
    {
        // To read request, retVal is filled up in above function.
        if (status == FLCN_OK)
        {
            *pDataOut = pArg->regAccessArg.retVal;
        }
        else
        {
            *pDataOut  = 0xbadfffff;
        }
    }

    // We're now back out of HS mode
    // Cleanup after returning from HS mode
    OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);

    _hdcpLoadUnloadHSOverlays(LW_FALSE);

    memset(pArg, 0, sizeof(SELWRE_ACTION_ARG));

    return status;
}

#endif
