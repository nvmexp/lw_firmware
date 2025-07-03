/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/channel.h"
#include "gpu/include/dmawrap.h"
#include "gpu/include/gpudev.h"
#include "gputest.h"
#include "lwRmReg.h"
#include "gpu/include/gralloc.h"
#include "core/include/platform.h"
#include "gpu/utility/surf2d.h"
#include "core/include/utility.h"
#include "gpu/utility/surfrdwr.h"
#include "core/include/registry.h"
#include "ctrl/ctrl2080/ctrl2080flcn.h"
#include "rmlsfm.h"
#include "lwmisc.h"

// Security levels of falcons
#define NS_MODE 0
#define LS_MODE 1
#define HS_MODE 2

class LSFalconTest: public GpuTest
{
public:
    LSFalconTest();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    virtual bool IsSupported();

    RC SetupFalconRegisterBases();
    RC VerifyAcrSetup();
    RC VerifyFalconStatus(UINT32 falconid);

    RC GetExpectedFalconStatus(UINT32 falconId, UINT32 *pStatus);
    RC GetActualFalconStatus(UINT32 falconId, UINT32 *pLevel);
    const char* GetFalconName(UINT32 falconId);

private:

    GpuTestConfiguration *m_pTestConfig;
    UINT32 m_NumFalconsTested;
};

LSFalconTest::LSFalconTest():
    m_pTestConfig(GetTestConfiguration()),
    m_NumFalconsTested(0)

{
    SetName("LSFalconTest");
}

RC EnsureACREnabled()
{
    //Check if acr is enabled or disabled
    UINT32 acrValue;
    if (OK == Registry::Read("ResourceManager", "RMDisableAcr", &acrValue))
    {
        if (acrValue == LW_REG_STR_RM_DISABLE_ACR_TRUE)
        {
            Printf(Tee::PriHigh, "acr has been disabled\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }
    else
    {
        Printf(Tee::PriLow, "Unable to read registry\n");
    }

    return OK;
}

bool LSFalconTest::IsSupported()
{
    if (!(GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_ACR_REGION)))
    {
        return false;
    }

    return true;
}

RC LSFalconTest::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    return rc;
}

RC LSFalconTest::Cleanup()
{
    RC rc;
    CHECK_RC(GpuTest::Cleanup());

    return rc;
}

// Compares ACR setup obtained from RM control call
// with expected setup [readmask, writemask etc..]
RC LSFalconTest::VerifyAcrSetup()
{
    RC rc = OK;
    UINT32 acrRegions;
    UINT32 regionIdx;
    UINT64 startAddr;
    UINT32 readMask;
    UINT32 writeMask;
    LwRmPtr pLwRm;
    GpuSubdevice *pSubDevice;
    LW2080_CTRL_CMD_FB_QUERY_ACR_REGION_PARAMS acrRegion;

    pSubDevice = GetBoundGpuSubdevice();

    // Get total number of ACR regions
    acrRegion.queryType = LW2080_CTRL_CMD_FB_ACR_QUERY_GET_REGION_PROPERTY;
    acrRegion.acrRegionIdProp.regionId = 0;
    CHECK_RC(pLwRm->ControlBySubdevice(pSubDevice,
             LW2080_CTRL_CMD_FB_QUERY_ACR_REGION, &acrRegion, sizeof(acrRegion)));

    acrRegions = acrRegion.acrRegionIdProp.regionId;

    // Query all regions in the ACM and verify the register settings
    for ( regionIdx = 1; regionIdx <= acrRegions; regionIdx++)
    {
        acrRegion.queryType = LW2080_CTRL_CMD_FB_ACR_QUERY_GET_REGION_PROPERTY;
        acrRegion.acrRegionIdProp.regionId = regionIdx;
        CHECK_RC(pLwRm->ControlBySubdevice(pSubDevice,
                 LW2080_CTRL_CMD_FB_QUERY_ACR_REGION, &acrRegion, sizeof(acrRegion)));

        // Get the WPR info for the regionIdx
        CHECK_RC(pSubDevice->GetWPRInfo(regionIdx, &readMask, &writeMask, &startAddr, NULL ));

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
        if ( /* acrRegion.acrRegionIdProp.readMask == readMask && */
            acrRegion.acrRegionIdProp.writeMask == writeMask &&
            acrRegion.acrRegionIdProp.physicalAddress == startAddr
          )
        {
            Printf(GetVerbosePrintPri(),"%s :ACR setup for region %d is valid\n", __FUNCTION__, regionIdx);
        }
        else
        {
            Printf(Tee::PriHigh,"%s :ACR setup for region %d is invalid\n", __FUNCTION__, regionIdx);
            return RC::SOFTWARE_ERROR;
        }
    }

    return OK;
}

// Gives actual level of falcons [NS, LS, HS]
RC LSFalconTest::GetActualFalconStatus (UINT32 falconId, UINT32 *pLevel)
{
    RC rc;

    GpuSubdevice *pSubDevice;
    pSubDevice = GetBoundGpuSubdevice();
    RegHal &regs = pSubDevice->Regs();

    if (falconId >= LSF_FALCON_ID_END || !pLevel )
    {
        return RC::SOFTWARE_ERROR;
    }

    if (falconId == LSF_FALCON_ID_PMU_RISCV)
    {
        *pLevel = pSubDevice->RegRd32(regs.LookupAddress(MODS_FALCON2_PWR_BASE) + 
                  regs.LookupAddress(MODS_PRISCV_RISCV_BCR_DMACFG_SEC));
        *pLevel = regs.GetField(*pLevel, MODS_PRISCV_RISCV_BCR_DMACFG_SEC_WPRID);

        if (*pLevel > 0)
        {
            // indicates ucode level 2
            *pLevel = 2;
        }
    }
    else
    {
            CHECK_RC(pSubDevice->GetFalconStatus(falconId, pLevel));
    }

    return OK;
}

// Gives expected status of the falcons
RC LSFalconTest::GetExpectedFalconStatus(UINT32 falconId, UINT32 *pStatus)
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_CMD_FB_QUERY_ACR_REGION_PARAMS acrQueryRegionParam;
    GpuSubdevice *pSubDevice;
    pSubDevice = GetBoundGpuSubdevice();

    acrQueryRegionParam.queryType = LW2080_CTRL_CMD_FB_ACR_QUERY_GET_FALCON_STATUS;
    acrQueryRegionParam.falconStatus.falconId = falconId;
    CHECK_RC (pLwRm->ControlBySubdevice(pSubDevice,
               LW2080_CTRL_CMD_FB_QUERY_ACR_REGION,
               &acrQueryRegionParam, sizeof(acrQueryRegionParam)));

    if (acrQueryRegionParam.falconStatus.bIsInLs)
    {
        *pStatus = LS_MODE;
    }
    else
    {
        *pStatus = NS_MODE;
    }

    return OK;
}

const char * LSFalconTest::GetFalconName(UINT32 falconId)
{
    switch(falconId)
    {
        case LSF_FALCON_ID_DPU    : return "DPU";
        case LSF_FALCON_ID_PMU    : return "PMU";
        case LSF_FALCON_ID_LWDEC  : return "LWDEC";
        case LSF_FALCON_ID_LWENC0 : return "LWENC0";
        case LSF_FALCON_ID_LWENC1 : return "LWENC1";
        case LSF_FALCON_ID_LWENC2 : return "LWENC2";
        case LSF_FALCON_ID_GPCCS  : return "GPCCS";
        case LSF_FALCON_ID_FECS   : return "FECS";
        case LSF_FALCON_ID_PMU_RISCV : return "PMU_RISCV";
        default:
            Printf(Tee::PriHigh,"%s : Invalid Falcon ID passed\n", __FUNCTION__);
            return "Ilwalid_Falcon";
    }
}

RC LSFalconTest::VerifyFalconStatus (UINT32 falconId)
{
    RC rc;
    UINT32 flcnUcLevel;
    UINT32 expectFlcnLevel;

    // Only PMU is tested
    if ((falconId != LSF_FALCON_ID_PMU_RISCV) && (falconId != LSF_FALCON_ID_PMU))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Checking if Falcon or RSIC V
    LwRmPtr pLwRm;
    LW2080_CTRL_FLCN_GET_ENGINE_ARCH_PARAMS engArchParams = {};
    engArchParams.engine = LW2080_ENGINE_TYPE_PMU;
    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                            LW2080_CTRL_CMD_FLCN_GET_ENGINE_ARCH,
                            &engArchParams,
                            sizeof(engArchParams)));

    if ((engArchParams.engineArch == LW2080_CTRL_FLCN_GET_ENGINE_ARCH_RISCV && 
         falconId == LSF_FALCON_ID_PMU) || 
        (engArchParams.engineArch == LW2080_CTRL_FLCN_GET_ENGINE_ARCH_FALCON &&
         falconId == LSF_FALCON_ID_PMU_RISCV))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    CHECK_RC(GetActualFalconStatus(falconId, &flcnUcLevel));
    CHECK_RC(GetExpectedFalconStatus(falconId, &expectFlcnLevel));

    if (expectFlcnLevel)
    {
        if (flcnUcLevel)
        {
            Printf(GetVerbosePrintPri(), "%s: Falcon %s is booted in Selwred Mode\n",
                                     __FUNCTION__, GetFalconName(falconId));
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
        if (flcnUcLevel == NS_MODE)
        {
             Printf(GetVerbosePrintPri(), "%s: Falcon %s is booted in NS Mode, "
                     "Not managed by LSFM\n", __FUNCTION__, GetFalconName(falconId));
        }
        else
        {
             Printf(GetVerbosePrintPri(), "%s: Falcon %s is booted in LS Mode, "
                     "Not managed by LSFM\n", __FUNCTION__, GetFalconName(falconId));
        }
    }

    m_NumFalconsTested++;
    return OK;
}

RC LSFalconTest::Run()
{
    RC rc;
    UINT32 falconId;

    //Check if ACR is enabled
    CHECK_RC(EnsureACREnabled());

    // Check whether ACR setup is correct or not
    CHECK_RC(VerifyAcrSetup());

    // Verify whether falcons are booted in LS mode or not
    for (falconId = 0; falconId < LSF_FALCON_ID_END; falconId++)
    {
        RC falconRC = VerifyFalconStatus(falconId);
        if (falconRC == RC::UNSUPPORTED_FUNCTION)
        {
            Printf(Tee::PriLow, "falconId %d not supported lwrrently.\n", falconId);
        }
        else
        {
            CHECK_RC(falconRC);
        }
    }

    if (m_NumFalconsTested == 0)
    {
        Printf(Tee::PriHigh, "No Falcons Tested\n");
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

JS_CLASS_INHERIT(LSFalconTest, GpuTest, "LSFalconTest test.");
