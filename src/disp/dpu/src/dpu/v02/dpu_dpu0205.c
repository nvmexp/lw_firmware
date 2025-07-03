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
 * @file   dpu_dpu0205.c
 * @brief  DPU 02.05 Hal Functions
 *
 * DPU HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "dpusw.h"
#include "dev_disp.h"
#include "dpu_objdpu.h"
#include "dispflcn_regmacros.h"
#include "lib_intfchdcp_cmn.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_dpu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
// Default Privilege Level of DPU
LwU8    dpuFlcnPrivLevelExtDefault;
LwU8    dpuFlcnPrivLevelCsbDefault;
LwU32   gHdcpOverlayMapping[HDCP_OVERLAYID_MAX];

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */

/*!
 * @brief Early initialization for DPU 02.05
 *
 * Any general initialization code not specific to particular engines should be
 * done here. This function must be called prior to starting the scheduler.
 */
FLCN_STATUS
dpuInit_v02_05(void)
{
#ifndef GSPLITE_RTOS
    //
    // The CSB base address may vary in different falcons, so each falcon ucode
    // needs to set this address based on its real condition so that the RTOS can
    // do the CSB access properly.
    //
    csbBaseAddress = DFLCN_DRF_BASE();
#endif

    // Cache the default privilege level
    dpuFlcnPrivLevelExtDefault = DFLCN_DRF(SCTL1_EXTLVL_MASK_INIT);
    dpuFlcnPrivLevelCsbDefault = DFLCN_DRF(SCTL1_CSBLVL_MASK_INIT);

    return FLCN_OK;
}

/*!
 * @brief Set the falcon privilege level
 *
 * @param[in]  privLevelExt falcon EXT privilege level
 * @param[in]  privLevelCsb falcon CSB privilege level
 */
void
dpuFlcnPrivLevelSet_v02_05
(
    LwU8 privLevelExt,
    LwU8 privLevelCsb
)
{
    LwU32 flcnSctl1 = 0;

    flcnSctl1 = DFLCN_FLD_SET_DRF_NUM(SCTL1, _CSBLVL_MASK, privLevelCsb, flcnSctl1);
    flcnSctl1 = DFLCN_FLD_SET_DRF_NUM(SCTL1, _EXTLVL_MASK, privLevelExt, flcnSctl1);

    DFLCN_REG_WR32_STALL(SCTL1, flcnSctl1);
}

/*!
 * @brief Get the IRQSTAT mask for the OS timer
 */
LwU32
dpuGetOsTickIntrMask_v02_05(void)
{
#if (DPUCFG_FEATURE_ENABLED(DPUTIMER0_FOR_OS_TICKS))
    {
        return DFLCN_DRF_SHIFTMASK(IRQSTAT_EXT_EXTIRQ3);
    }
#else
    {
        return DFLCN_DRF_SHIFTMASK(IRQSTAT_GPTMR);
    }
#endif
}

/*!
 * @brief Return the Timer0 interrupt reg for the OS timer
 */
LwU32
dpuGetOsTickTimer0IntrReg_v02_05(void)
{
#if (DPUCFG_FEATURE_ENABLED(DPUTIMER0_FOR_OS_TICKS))
    {
        return DFLCN_DRF(TIMER_0_INTR);
    }
#else
    {
        return 0;
    }
#endif
}

/*!
 * @brief Return the Timer0 interrupt clear value for the OS timer
 */
LwU32
dpuGetOsTickTimer0IntrClearValue_v02_05(void)
{
#if (DPUCFG_FEATURE_ENABLED(DPUTIMER0_FOR_OS_TICKS))
    {
        return DFLCN_DRF_SHIFTMASK(TIMER_0_INTR_CLR);
    }
#else
    {
        return 0;
    }
#endif
}

/*!
 * @brief Routine to initialize HDCP1X/HDCP22 common part.
 */
void
dpuHdcpCmnInit_v02_05(void)
{
    LwU32 i;

    // Initialize all mapping to invalid index.
    for (i=0; i<HDCP_OVERLAYID_MAX; i++)
    {
        gHdcpOverlayMapping[i]                              = ILWALID_OVERLAY_INDEX;
    }

    // HDCP1.X
    gHdcpOverlayMapping[HDCP_AUTH]                          = OVL_INDEX_IMEM(auth);
    gHdcpOverlayMapping[HDCP_BIGINT]                        = OVL_INDEX_IMEM(libBigInt);
    gHdcpOverlayMapping[HDCP_SHA1]                          = OVL_INDEX_IMEM(libSha1);
    gHdcpOverlayMapping[HDCP_BUS]                           = OVL_INDEX_IMEM(pvtbus);

    // HDCP22
    gHdcpOverlayMapping[HDCP22WIRED_REPEATER]               = OVL_INDEX_IMEM(hdcp22Repeater);
    gHdcpOverlayMapping[HDCP22WIRED_TIMER]                  = ILWALID_OVERLAY_INDEX;
    gHdcpOverlayMapping[HDCP22WIRED_DPAUX]                  = OVL_INDEX_IMEM(libDpaux);
    gHdcpOverlayMapping[HDCP22WIRED_I2C]                    = OVL_INDEX_IMEM(libI2c);
    gHdcpOverlayMapping[HDCP22WIRED_LIBI2CSW]               = ILWALID_OVERLAY_INDEX;
    gHdcpOverlayMapping[HDCP22WIRED_CERTRX_SRM]             = OVL_INDEX_IMEM(hdcp22CertrxSrm);
    gHdcpOverlayMapping[HDCP22WIRED_RSA_SIGNATURE]          = OVL_INDEX_IMEM(hdcp22RsaSignature);
    gHdcpOverlayMapping[HDCP22WIRED_DSA_SIGNATURE]          = OVL_INDEX_IMEM(hdcp22DsaSignature);
    gHdcpOverlayMapping[HDCP22WIRED_AES]                    = OVL_INDEX_IMEM(hdcp22Aes);
    gHdcpOverlayMapping[HDCP22WIRED_AKE]                    = OVL_INDEX_IMEM(hdcp22Ake);
    gHdcpOverlayMapping[HDCP22WIRED_AKEH]                   = OVL_INDEX_IMEM(hdcp22Akeh);
    gHdcpOverlayMapping[HDCP22WIRED_LC]                     = OVL_INDEX_IMEM(hdcp22Lc);
    gHdcpOverlayMapping[HDCP22WIRED_SKE]                    = OVL_INDEX_IMEM(hdcp22Ske);
#if SELWRITY_ENGINE_ENABLED
    gHdcpOverlayMapping[HDCP22WIRED_LIB_SE]                 = OVL_INDEX_IMEM(libSE);
#else
    gHdcpOverlayMapping[HDCP22WIRED_LIB_SE]                 = ILWALID_OVERLAY_INDEX;
#endif
    gHdcpOverlayMapping[HDCP22WIRED_LIB_SHA]                = OVL_INDEX_IMEM(libSha);
    gHdcpOverlayMapping[HDCP22WIRED_OVL_SELWRE_ACTION]      = OVL_INDEX_IMEM(hdcp22SelwreAction);
    gHdcpOverlayMapping[HDCP22WIRED_OVL_START_SESSION]      = OVL_INDEX_IMEM(hdcp22StartSession);
    gHdcpOverlayMapping[HDCP22WIRED_OVL_VALIDATE_CERTRX_SRM] = OVL_INDEX_IMEM(hdcp22ValidateCertrxSrm);
    gHdcpOverlayMapping[HDCP22WIRED_OVL_GENERATE_KMKD]      = OVL_INDEX_IMEM(hdcp22GenerateKmkd);
    gHdcpOverlayMapping[HDCP22WIRED_OVL_HPRIME_VALIDATION]  = OVL_INDEX_IMEM(hdcp22HprimeValidation);
    gHdcpOverlayMapping[HDCP22WIRED_OVL_LPRIME_VALIDATION]  = OVL_INDEX_IMEM(hdcp22LprimeValidation);
    gHdcpOverlayMapping[HDCP22WIRED_OVL_GENERATE_SESSIONKEY]= OVL_INDEX_IMEM(hdcp22GenerateSessionkey);
    gHdcpOverlayMapping[HDCP22WIRED_OVL_CONTROL_ENCRYPTION] = OVL_INDEX_IMEM(hdcp22ControlEncryption);
    gHdcpOverlayMapping[HDCP22WIRED_OVL_VPRIME_VALIDATION]  = OVL_INDEX_IMEM(hdcp22VprimeValidation);
    gHdcpOverlayMapping[HDCP22WIRED_OVL_MPRIME_VALIDATION]  = OVL_INDEX_IMEM(hdcp22MprimeValidation);
    gHdcpOverlayMapping[HDCP22WIRED_OVL_END_SESSION]        = OVL_INDEX_IMEM(hdcp22EndSession);
}
