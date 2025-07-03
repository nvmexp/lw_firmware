/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \rmt_acrverify.cpp
//! \Verify the ACR setup
//!

#include "core/include/channel.h"
#include "core/include/platform.h"
#include "core/include/jscript.h"
#include "gpu/include/gralloc.h"
#include "gpu/tests/rmtest.h"
#include "lwRmApi.h"
#include "class/cl0040.h" // LW01_MEMORY_LOCAL_USER
#include "class/cl9070.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "lwRmReg.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "lwstatus.h"
#include "lwmisc.h"
#include "gpu/tests/rm/utility/clk/clkbase.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl2080/ctrl2080flcn.h"
#include "rmt_acrverify.h"
#include "hwref/pascal/gp100/dev_falcon_v4.h"
#include "hwref/pascal/gp100/dev_graphics_nobundle.h"
#include "hwref/pascal/gp100/dev_sec_pri.h"
#include "hwref/ampere/ga102/dev_riscv_pri.h"
#include "rmlsfm.h"
#include "core/include/memcheck.h"
#include "mods_reg_hal.h"

//! \brief VerifyFalconStatus
//! \Verifies whether Falcon in booted in expected mode or not
//!
//! \return RC
//------------------------------------------------------------------------------
RC AcrVerify::VerifyFalconStatus (LwU32 falconId, LwBool bIncludeOnDemandFlcns)
{
    RC rc;
    LwU32 flcnUcLevel;
    LwU32 expectFlcnLevel;

    // If we do not want to check on demand Falcons then return OK for them
    if (!bIncludeOnDemandFlcns)
    {
        if (falconId == LSF_FALCON_ID_GPCCS  ||
            falconId == LSF_FALCON_ID_FECS   ||
            falconId == LSF_FALCON_ID_SEC2   ||
            falconId == LSF_FALCON_ID_LWENC0 ||
            falconId == LSF_FALCON_ID_LWENC1 ||
            falconId == LSF_FALCON_ID_LWENC2 ||
            falconId == LSF_FALCON_ID_LWDEC)
        {
            Printf(Tee::PriHigh,
                   "%s: Falcon %s is not checked since it is an OnDemand Falcon\n",
                   __FUNCTION__, GetFalconName(falconId));

            return OK;
        }
    }

    CHECK_RC(LsFalconStatus(falconId, &flcnUcLevel));
    CHECK_RC(GetFalconStatus(falconId, &expectFlcnLevel));

    if(expectFlcnLevel)
    {
        if(flcnUcLevel)
        {
            Printf(Tee::PriHigh, "%s: Falcon %s is booted in Selwred Mode\n", __FUNCTION__, GetFalconName(falconId));
        }
        else
        {
            Printf(Tee::PriHigh, "%s: Falcon %s is booted in NS Mode, Managed by LSFM\n",
                        __FUNCTION__, GetFalconName(falconId));
            return RC::SOFTWARE_ERROR;
        }
    }
    else
    {
        if(flcnUcLevel == NS_MODE)
        {
             Printf(Tee::PriHigh, "%s: Falcon %s is booted in NS Mode, Not managed by LSFM\n",
                        __FUNCTION__, GetFalconName(falconId));
        }
        else
        {
             Printf(Tee::PriHigh, "%s: Falcon %s is booted in LS Mode, Not managed by LSFM\n",
                        __FUNCTION__, GetFalconName(falconId));
        }
    }

    return OK;
}

//! \brief AcrVerify VerifyAcrSetup
//! \Verifies the register are correctly programmed or not for ACR regions
//!
//! \return RC
//------------------------------------------------------------------------------
RC AcrVerify::VerifyAcrSetup()
{
    RC rc = OK;
    LwU32 acrRegions;
    LwU32 regionIdx;
    LwU64 startAddr;
    LwU32 readMask;
    LwU32 writeMask;
    LwRmPtr pLwRm;
    GpuSubdevice *m_pSubdevice;
    LW2080_CTRL_CMD_FB_QUERY_ACR_REGION_PARAMS acrRegion;

    m_pSubdevice = GetBoundGpuSubdevice();

    // Get total number of ACR regions
    acrRegion.queryType = LW2080_CTRL_CMD_FB_ACR_QUERY_GET_REGION_PROPERTY;
    acrRegion.acrRegionIdProp.regionId = 0;
    CHECK_RC(pLwRm->ControlBySubdevice(m_pSubdevice, LW2080_CTRL_CMD_FB_QUERY_ACR_REGION,
                                    &acrRegion, sizeof(acrRegion)));

    acrRegions = acrRegion.acrRegionIdProp.regionId;

    // Query all regions in the ACM and verify the register settings
    for(regionIdx = 1; regionIdx <= acrRegions; regionIdx++)
    {
        acrRegion.queryType = LW2080_CTRL_CMD_FB_ACR_QUERY_GET_REGION_PROPERTY;
        acrRegion.acrRegionIdProp.regionId = regionIdx;
        CHECK_RC(pLwRm->ControlBySubdevice(m_pSubdevice, LW2080_CTRL_CMD_FB_QUERY_ACR_REGION,
                                    &acrRegion, sizeof(acrRegion)));

        // Get the MMU setup for the regionIdx
        CHECK_RC(m_pSubdevice->GetWPRInfo(regionIdx, &readMask, &writeMask, &startAddr, nullptr));

        /*
         * The check below is disabled temporarily to allow ACR to tighten
         * security on GP10X (marking WPR not readable by level 0). See
         * Bug 1752819.
         *
         * A proper solution requires HALifying the check to allow read
         * protection on GP10X without also force requiring it on GM20X
         * and GP100. Bug 1855221.
         *
         * TODO for dkumar by 02/01/2017: Add hal in RM and uncomment the below
         */
        if( /* acrRegion.acrRegionIdProp.readMask == readMask && */
            acrRegion.acrRegionIdProp.writeMask == writeMask &&
            acrRegion.acrRegionIdProp.physicalAddress == startAddr
          )
        {
            Printf(Tee::PriNormal,"%s :ACR setup for region %d is valid\n", __FUNCTION__, regionIdx);
        }
        else
        {
            Printf(Tee::PriError,"%s :ACR setup for region %d is invalid\n", __FUNCTION__, regionIdx);
            Printf(Tee::PriNormal, "WPR%u readMask: expected=0x%x actual=0x%x\n",
                regionIdx, acrRegion.acrRegionIdProp.readMask, readMask);
            Printf(Tee::PriNormal, "WPR%u writeMask: expected=0x%x actual=0x%x\n",
                regionIdx, acrRegion.acrRegionIdProp.writeMask, writeMask);
            Printf(Tee::PriNormal, "WPR%u startAddr: expected=0x%llx actual=0x%llx\n",
                regionIdx, acrRegion.acrRegionIdProp.physicalAddress, startAddr);
            return RC::SOFTWARE_ERROR;
        }
    }

    return OK;
}

//! \brief AcrVerify LsFalconStatus
//! \verifies whether a falcon is booted in LS mode or not.
//!
//! \return RC
//------------------------------------------------------------------------------

RC AcrVerify::LsFalconStatus (LwU32 falconId, LwU32 *pLevel)
{
    GpuSubdevice *m_pSubdevice;

    m_pSubdevice = GetBoundGpuSubdevice();

    if(falconId >= LSF_FALCON_ID_END || !pLevel )
    {
        return RC::SOFTWARE_ERROR;
    }


    if (falconId == LSF_FALCON_ID_PMU_RISCV)
    {
        //
        // For RISCV engines, the LS mode is enabled by the BootRom when it "exelwtes" the Boot ROM
        // manifest. Reading the registers containing the mode is non-trivial task.
        // Hence we are reading the WPRId that is set in LW_PRISCV_RISCV_BCR_DMACFG_SEC which is set
        // by ACR to confirm that the engine is in LS mode
        //
        //

        /*
         *  TODO : Use the engineArch in the above if ablock so this path applies to all the RISCV
         *  engines.
         */
        
        *pLevel = ACR_REG_RD32(falconRegisterBase[falconId]+LW_PRISCV_RISCV_BCR_DMACFG_SEC);
        *pLevel = DRF_VAL(_PRISCV_RISCV, _BCR_DMACFG_SEC, _WPRID, *pLevel);

        if (*pLevel > 0)
        {
            *pLevel = LW_ENGINE_UCODE_LEVEL_2;
        }
    }
    else
    {
        *pLevel = ACR_REG_RD32(falconRegisterBase[falconId]+LW_PFALCON_FALCON_SCTL);
        *pLevel = DRF_VAL(_FALCON, _SCTL_, UCODE_LEVEL, *pLevel);
    }

    return OK;
}

//! \brief AcrVerify GetFalconName
//!         Colwerts the Falcon ID to string
//!
//! \return string
//------------------------------------------------------------------------------

const char * AcrVerify::GetFalconName(LwU32 falconId)
{
    switch(falconId)
    {
        case LSF_FALCON_ID_DPU       : return "DPU";
        case LSF_FALCON_ID_PMU       : return "PMU";
        case LSF_FALCON_ID_LWDEC     : return "LWDEC";
        case LSF_FALCON_ID_LWENC0    : return "LWENC0";
        case LSF_FALCON_ID_LWENC1    : return "LWENC1";
        case LSF_FALCON_ID_LWENC2    : return "LWENC2";
        case LSF_FALCON_ID_GPCCS     : return "GPCCS";
        case LSF_FALCON_ID_FECS      : return "FECS";
        case LSF_FALCON_ID_SEC2      : return "SEC2";
        case LSF_FALCON_ID_PMU_RISCV : return "PMU_RISCV";
        default:
            Printf(Tee::PriHigh,"%s : Invalid Falcon ID passed\n", __FUNCTION__);
            return "Ilwalid_Falcon";
    }
    return "_ERROR_";
}

//! \brief AcrVerify constructor
//! \sa setup
//------------------------------------------------------------------------------

AcrVerify::AcrVerify()
{
    SetName("AcrVerify");
}

//! \brief AcrVerify destructor
//!
//! \sa cleanup
//------------------------------------------------------------------------------

AcrVerify::~AcrVerify()
{
}

//! \This function is to test if ACR is disabled
//! \return bool
//------------------------------------------------------------------------------

bool AcrVerify::IsAcrDisabled()
{
    LwRmPtr pLwRm;

    UINT08  capsAcr[LW2080_CTRL_ACR_CAPS_TBL_SIZE];
    LW2080_CTRL_ACR_GET_CAPS_PARAMS paramsAcr = {0};

    paramsAcr.capsTbl = LW_PTR_TO_LwP64(capsAcr);
    paramsAcr.capsTblSize = LW2080_CTRL_ACR_CAPS_TBL_SIZE;

    if( OK == pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
             LW2080_CTRL_CMD_ACR_GET_CAPS, &paramsAcr, sizeof(paramsAcr)))
    {
        if ( LW2080_CTRL_ACR_GET_CAP(capsAcr, LW2080_CTRL_ACR_CAPS_ACR_DISABLED) )
        {
            return true;
        }
    }

    return false;
}

//! \brief Check for Test Support
//! \This function is to ensure that the test is supported only on GM20X and later chips
//! \return string
//------------------------------------------------------------------------------

string AcrVerify::IsTestSupported()
{
    if(IsAcrDisabled())
        return "ACR disabled";

    if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_ACR_REGION))
    {
        return RUN_RMTEST_TRUE;
    }
    return "Supported only on GM20X+ chips";
}

//! \brief Test Setup Function
//! \This setup is done for Verifying LS client access
//! \Here, Acr region is mapped to BAR1 for verification
//! \return RC
//------------------------------------------------------------------------------

RC AcrVerify::Setup()
{
    RC rc = OK;
    UINT32 i;
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS  engineParams = {0};
    LwRmPtr pLwRm;
    const RegHal &regs  = GetBoundGpuSubdevice()->Regs();

    CHECK_RC( pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                               LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
                               &engineParams,
                               sizeof (engineParams)) );

    for(i=0; i < LSF_FALCON_ID_END; i++)
    {
        falconRegisterBase[i] = 0;
    }

    for ( i = 0; i < engineParams.engineCount; i++ )
    {
        switch(engineParams.engineList[i])
        {
            case LW2080_ENGINE_TYPE_LWENC0:
                falconRegisterBase[LSF_FALCON_ID_LWENC0] = LW_FALCON_LWENC0_BASE;
            break;
            case LW2080_ENGINE_TYPE_LWENC1:
                falconRegisterBase[LSF_FALCON_ID_LWENC1] = LW_FALCON_LWENC1_BASE;
            break;
            case LW2080_ENGINE_TYPE_LWENC2:
                falconRegisterBase[LSF_FALCON_ID_LWENC2] = LW_FALCON_LWENC2_BASE;
            break;
            case LW2080_ENGINE_TYPE_SEC2:
                falconRegisterBase[LSF_FALCON_ID_SEC2] = regs.LookupAddress(MODS_PSEC_FALCON_IRQSSET);
            break;
            case LW2080_ENGINE_TYPE_GRAPHICS:
                falconRegisterBase[LSF_FALCON_ID_FECS]  = LW_PGRAPH_PRI_FECS_FALCON_IRQSSET;
                falconRegisterBase[LSF_FALCON_ID_GPCCS] = regs.LookupAddress(MODS_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQSSET);
            break;
            case LW2080_ENGINE_TYPE_BSP:
                falconRegisterBase[LSF_FALCON_ID_LWDEC] = regs.LookupAddress(MODS_PLWDEC_FALCON_IRQSSET, 0);
            break;
        }
    }

    /*
     *  TODO : call the LW2080_CTRL_CCMD_FLCN_GET_ENGINE_ARCH for each engine if a combination of both 
     *  FALCON and RISCV engines are going to be present. 
     *
     *  Since in GA10x only PMU is RISCV, this API is being called only for PMU.
     *
     */
    LW2080_CTRL_FLCN_GET_ENGINE_ARCH_PARAMS engArchParams;

    engArchParams.engine = LW2080_ENGINE_TYPE_PMU;
    CHECK_RC( pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                             LW2080_CTRL_CMD_FLCN_GET_ENGINE_ARCH,
                             &engArchParams,
                             sizeof (engArchParams)) );

    // PMU is always present
    if (engArchParams.engineArch == LW2080_CTRL_FLCN_GET_ENGINE_ARCH_RISCV)
    {
        falconRegisterBase[LSF_FALCON_ID_PMU_RISCV] = LW_FALCON2_PWR_BASE;
    }
    else if (engArchParams.engineArch == LW2080_CTRL_FLCN_GET_ENGINE_ARCH_FALCON)
    {
        falconRegisterBase[LSF_FALCON_ID_PMU] = LW_FALCON_PWR_BASE;
    }
    else
    {
        Printf(Tee::PriError, "%s : The given engine (PMU) is of neither Falcon or RISCV arch.\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;

    }

    // check whether DPU is present or not
    Display    *pDisplay     = GetDisplay();
    UINT32      displayClass = pDisplay->GetClass();

    if ((pDisplay->GetDisplayClassFamily() == Display::EVO) &&
            (displayClass >= LW9070_DISPLAY))
    {
        falconRegisterBase[LSF_FALCON_ID_DPU] = LW_FALCON_DISP_BASE;
    }
    else
    {
        Printf(Tee::PriHigh,"%s : DPU is not supported on current platform\n", __FUNCTION__);
    }

    return rc;
}

//! \brief Test Exelwtion Control Function.
//!
//! \return RC
//------------------------------------------------------------------------------

RC AcrVerify::Run()
{
    RC rc = OK;
    LwU32 falconId;

    // Check whetber ACR setup is correct or not
    CHECK_RC(VerifyAcrSetup());

    // Verify whether falcons are booted in LS mode or not
    for(falconId = LSF_FALCON_ID_PMU; falconId < LSF_FALCON_ID_END; falconId++)
    {
        if (!falconRegisterBase[falconId])
        {
            Printf(Tee::PriHigh,"%s : Falcon %d is not supported \n", __FUNCTION__, falconId);
        }
        else
        {
            Printf(Tee::PriHigh,"%s : Verifying falcon status for falconId %d\n", __FUNCTION__, falconId);
            CHECK_RC(VerifyFalconStatus(falconId, LW_FALSE));
        }
    }

    return rc;
}

//! \brief Cleanup
//!
//! \return RC
//------------------------------------------------------------------------------

RC AcrVerify::Cleanup()
{
    return OK;
}

RC AcrVerify::GetFalconStatus(LwU32 falconId, LwU32 *pStatus)
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_CMD_FB_QUERY_ACR_REGION_PARAMS acrQueryRegionParam;
    GpuSubdevice *m_pSubdevice;
    m_pSubdevice = GetBoundGpuSubdevice();

    acrQueryRegionParam.queryType = LW2080_CTRL_CMD_FB_ACR_QUERY_GET_FALCON_STATUS;
    acrQueryRegionParam.falconStatus.falconId = falconId;
    CHECK_RC ( pLwRm->ControlBySubdevice(m_pSubdevice, LW2080_CTRL_CMD_FB_QUERY_ACR_REGION,
                                    &acrQueryRegionParam, sizeof(acrQueryRegionParam)));

    if(acrQueryRegionParam.falconStatus.bIsInLs)
    {
        *pStatus = LS_MODE;
    }
    else
    {
        *pStatus = NS_MODE;
    }

    return OK;
}
//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(AcrVerify, RmTest,"AcrVerify RM test - Acr Verification Test");
