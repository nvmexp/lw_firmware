/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dpu_libintfchdcp22wired.c
 * @brief  Container-object for the DPU Hdcp 2.2 routines.  Contains
 *         generic non-HAL interrupt-routines plus logic required to hook-up
 *         chip-specific HAL-routines.
 *
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ OpenRTOS Includes ------------------------------ */

/* ------------------------ Application Includes --------------------------- */
#include "dpusw.h"
#include "dpu_gpuarch.h"
#include "lib_intfchdcp22wired.h"
#ifdef GSPLITE_RTOS
#include "mem_hs.h"
#include "dpu_os.h"
#include "lwosselwreovly.h"
#include "scp_common.h"
#endif
#include "hdcp22wired_selwreaction.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
#ifdef GSPLITE_RTOS
extern FLCN_STATUS dpuSelwreActionHsEntry(SELWRE_ACTION_ARG *pArg);
#endif

/* ------------------------ Function Prototypes ---------------------------- */
LwBool hdcp22WiredIsSec2TypeReqSupported(void)
    GCC_ATTRIB_SECTION("imem_resident", "hdcp22WiredIsSec2TypeReqSupported");

#ifdef GSPLITE_RTOS
inline static void _hdcp22LoadUnloadHSOverlays(SELWRE_ACTION_ARG *pSecActArg, LwBool bIsLoad);
#endif
/* ------------------------ Global Variables ------------------------------- */
static LwU8 gHdcp22wiredHsArg[HDCP22_SELWRE_ACTION_ARG_BUF_SIZE]
    GCC_ATTRIB_SECTION("dmem_hdcp22wiredHs", "gHdcp22wiredHsArg")
#ifdef GSPLITE_RTOS
    GCC_ATTRIB_ALIGN(SCP_BUF_ALIGNMENT);
#else
    GCC_ATTRIB_ALIGN(sizeof(LwU32));   // preVolta compiler doesn't support 16bytes alignment and also no need without HW hash.
#endif

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Staic Functions ------------------------------- */
#ifdef GSPLITE_RTOS
/*!
 * This function is used to load and unload HS Overlay.
 *
 * @param[in]  pSecActArg       Pointer to secure action structure.
 * @param[in]  bIsLoad  Is load or unload request
 */
inline static void
_hdcp22LoadUnloadHSOverlays
(
    SELWRE_ACTION_ARG  *pSecActArg,
    LwBool              bIsLoad
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
        // TODO: move libSecBusHs to libCommonHs after LS/HS reorged.
        OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libSecBusHs));
        OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libCommonHs));
        OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libMemHs));
#ifndef HDCP22_KMEM_ENABLED
        OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libScpCryptHs));
#endif
#ifdef LIB_CCC_PRESENT
        OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libCccHs));
#endif

        switch (pSecActArg->actionType)
        {
            case SELWRE_ACTION_REG_ACCESS:
            case SELWRE_ACTION_SCP_CALLWLATE_HASH:
            case SELWRE_ACTION_SCP_GEN_DKEY:
                // No extra HS libs need to attach.
                break;

            case SELWRE_ACTION_START_SESSION:
                OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22StartSessionHs));
                break;
            case SELWRE_ACTION_SRM_REVOCATION:
            case SELWRE_ACTION_VERIFY_CERTIFICATE:
                OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libShaHs));
                // SRM1X DSA signature needs libBigIntHs even with HW RSA for SRM2X.
                OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libBigIntHs));
                OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22ValidateCertrxSrmHs));
                break;
            case SELWRE_ACTION_KMKD_GEN:
                OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libShaHs));
#ifndef HDCP22_USE_HW_RSA
                OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libBigIntHs));
#endif
                OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22GenerateKmkdHs));
                break;
            case SELWRE_ACTION_HPRIME_VALIDATION:
                OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libShaHs));
                OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22HprimeValidationHs));
                break;
            case SELWRE_ACTION_LPRIME_VALIDATION:
                OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libShaHs));
                OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22LprimeValidationHs));
                break;
            case SELWRE_ACTION_EKS_GEN:
                OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22GenerateSessionkeyHs));
                break;
            case SELWRE_ACTION_CONTROL_ENCRYPTION:
                OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22ControlEncryptionHs));
                break;
            case SELWRE_ACTION_VPRIME_VALIDATION:
                OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libShaHs));
                OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22VprimeValidationHs));
                break;
            case SELWRE_ACTION_MPRIME_VALIDATION:
                OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libShaHs));
                OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22MprimeValidationHs));
            break;
            case SELWRE_ACTION_WRITE_DP_ECF:
                OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22ControlEncryptionHs));
                break;
            case SELWRE_ACTION_END_SESSION:
                OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22EndSessionHs));
                break;
            default:
                break;
        }
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
#ifdef LIB_CCC_PRESENT
        OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libCccHs));
#endif

        switch (pSecActArg->actionType)
        {
            case SELWRE_ACTION_REG_ACCESS:
            case SELWRE_ACTION_SCP_CALLWLATE_HASH:
            case SELWRE_ACTION_SCP_GEN_DKEY:
                // No extra HS libs need to detach.
                break;

            case SELWRE_ACTION_START_SESSION:
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22StartSessionHs));
                break;
            case SELWRE_ACTION_SRM_REVOCATION:
            case SELWRE_ACTION_VERIFY_CERTIFICATE:
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22ValidateCertrxSrmHs));
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libShaHs));
                // SRM1X DSA signature needs libBigIntHs even with HW RSA for SRM2X.
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libBigIntHs));
                break;
            case SELWRE_ACTION_KMKD_GEN:
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22GenerateKmkdHs));
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libShaHs));
#ifndef HDCP22_USE_HW_RSA
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libBigIntHs));
#endif
                break;
            case SELWRE_ACTION_HPRIME_VALIDATION:
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22HprimeValidationHs));
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libShaHs));
                break;
            case SELWRE_ACTION_LPRIME_VALIDATION:
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22LprimeValidationHs));
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libShaHs));
                break;
            case SELWRE_ACTION_EKS_GEN:
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22GenerateSessionkeyHs));
                break;
            case SELWRE_ACTION_CONTROL_ENCRYPTION:
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22ControlEncryptionHs));
                break;
            case SELWRE_ACTION_VPRIME_VALIDATION:
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22VprimeValidationHs));
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libShaHs));
                break;
            case SELWRE_ACTION_MPRIME_VALIDATION:
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22MprimeValidationHs));
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libShaHs));
                break;
            case SELWRE_ACTION_WRITE_DP_ECF:
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22ControlEncryptionHs));
                break;
            case SELWRE_ACTION_END_SESSION:
                OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22EndSessionHs));
                break;
            default:
                break;
        }
        OSTASK_DETACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(selwreActionHs));
    }
#endif // HS_UCODE_ENCRYPTION
}
#endif // GSPLITE_RTOS

/* ------------------------ Public Functions ------------------------------- */
/*!
 * @brief This is a lib interface function to know Sec2 type
 *        request is supported
 *
 * @return      LwBool          returns LW_TRUE if sec2 type
 *                              request is supported
 */
LwBool
hdcp22WiredIsSec2TypeReqSupported(void)
{
    return DISP_IP_VER_GREATER_OR_EQUAL(DISP_VERSION_V02_08);
}

/*!
 * @brief This is a library interface function to provide secure action argument storage.
 *
 * @return     Address of selwreAction arguement storage.
 */
void*
libIntfcHdcp22wiredGetSelwreActionArgument(void)
{
    return (void *)gHdcp22wiredHsArg;
}

#ifdef GSPLITE_RTOS
/*!
 * @brief This is a library interface function for secure actions in HS mode.
 * @param[in]  pSecActArg       Pointer to secure action structure.
 *
 * @return     FLCN_STATUS      FLCN_OK when succeed else error.
 */
FLCN_STATUS
libIntfcHdcp22wiredSelwreAction
(
    SELWRE_ACTION_ARG  *pSecActArg
)
{
    FLCN_STATUS status = FLCN_OK;

    _hdcp22LoadUnloadHSOverlays(pSecActArg, LW_TRUE);

    //
    // Call entry function (0th offset of overlay). This is the entry into HS
    // mode.
    //
    OS_HS_MODE_SETUP_IN_TASK(OVL_INDEX_IMEM(selwreActionHs), NULL, 0, HASH_SAVING_DISABLE);

    status = dpuSelwreActionHsEntry(pSecActArg);

    //
    // We're now back out of HS mode
    // Cleanup after returning from HS mode
    //
    OS_HS_MODE_CLEANUP_IN_TASK(NULL, 0);

    _hdcp22LoadUnloadHSOverlays(pSecActArg, LW_FALSE);

    return status;
}
#endif

