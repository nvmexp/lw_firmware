/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "fslib_interface.h"
#include "core/include/rc.h"
#include "core/include/jscript.h"
#include "core/include/script.h"

#include <map>

namespace FsLib
{
    FsMode m_Mode = FsMode::IGNORE_FSLIB;

    map<Device::LwDeviceId, fslib::Chip> m_LwDeviceIdToChip =
    {
#if LWCFG(GLOBAL_ARCH_PASCAL)
        { Device::LwDeviceId::GP102,  fslib::Chip::GP102 },
        { Device::LwDeviceId::GP104,  fslib::Chip::GP104 },
        { Device::LwDeviceId::GP106,  fslib::Chip::GP106 },
        { Device::LwDeviceId::GP107,  fslib::Chip::GP107 },
        { Device::LwDeviceId::GP108,  fslib::Chip::GP108 },
#endif
#if LWCFG(GLOBAL_ARCH_VOLTA)
        { Device::LwDeviceId::GV100,  fslib::Chip::GV100 },
#endif
#if LWCFG(GLOBAL_ARCH_TURING)
        { Device::LwDeviceId::TU102,  fslib::Chip::TU102 },
        { Device::LwDeviceId::TU104,  fslib::Chip::TU104 },
        { Device::LwDeviceId::TU106,  fslib::Chip::TU106 },
        { Device::LwDeviceId::TU116,  fslib::Chip::TU116 },
        { Device::LwDeviceId::TU117,  fslib::Chip::TU117 },
#endif
#if LWCFG(GLOBAL_ARCH_AMPERE)
        { Device::LwDeviceId::GA100,  fslib::Chip::GA100 },
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA102)
        { Device::LwDeviceId::GA102,  fslib::Chip::GA102 },
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA103)
        { Device::LwDeviceId::GA103,  fslib::Chip::GA103 },
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA104)
        { Device::LwDeviceId::GA104,  fslib::Chip::GA104 },
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA106)
        { Device::LwDeviceId::GA106,  fslib::Chip::GA106 },
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA107)
        { Device::LwDeviceId::GA107,  fslib::Chip::GA107 },
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA10B)
        { Device::LwDeviceId::GA10B,  fslib::Chip::GA10B },
#endif
#if LWCFG(GLOBAL_GPU_IMPL_AD102)
        { Device::LwDeviceId::AD102,  fslib::Chip::AD102 },  
#endif
#if LWCFG(GLOBAL_GPU_IMPL_AD103)
        { Device::LwDeviceId::AD103,  fslib::Chip::AD103 },  
#endif
#if LWCFG(GLOBAL_GPU_IMPL_AD104)
        { Device::LwDeviceId::AD104,  fslib::Chip::AD104 },  
#endif
#if LWCFG(GLOBAL_GPU_IMPL_AD106)
        { Device::LwDeviceId::AD106,  fslib::Chip::AD106 },  
#endif
#if LWCFG(GLOBAL_GPU_IMPL_AD107)
        { Device::LwDeviceId::AD107,  fslib::Chip::AD107 },  
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GH100)
        { Device::LwDeviceId::GH100,  fslib::Chip::GH100 },
#endif
    };
}

RC FsLib::SetMode(FsMode mode)
{
    switch (mode)
    {
        // Ignore the library
        case FsMode::IGNORE_FSLIB:
        // Allow non-production/debug configurations, which is still functionally valid
        case FsMode::FUNC_VALID:
        // Allow only production approved configurations
        case FsMode::PRODUCT:
            m_Mode = mode;
            break;
        default:
            Printf(Tee::PriError, "Invalid fslib mode %u\n", static_cast<UINT32>(mode));
            return RC::ILWALID_ARGUMENT;
    }

    return RC::OK;
}

bool FsLib::IsSupported(Device::LwDeviceId id)
{
    return m_LwDeviceIdToChip.count(id) != 0;
}

RC FsLib::LwDeviceIdToChip(Device::LwDeviceId id, fslib::Chip *pChip)
{
    MASSERT(pChip);

    if (!IsSupported(id))
    {
        return RC::BAD_PARAMETER;
    }

    *pChip = m_LwDeviceIdToChip[id];
    return OK;
}

bool FsLib::IsFloorsweepingValid
(
    Device::LwDeviceId devId,
    const GpcMasks& gpcMasks,
    const FbpMasks& fbMasks,
    string &errMsg
)
{
    fslib::Chip chip;
    if (LwDeviceIdToChip(devId, &chip) != RC::OK)
    {
        Printf(Tee::PriLow, "Fslib does not support chip %s. Bypassing fslib checks\n",
                             Device::DeviceIdToString(devId).c_str());
        return true;
    }
    else
    {
        Printf(Tee::PriLow, "Testing with Fslib mode %u\n", static_cast<UINT32>(m_Mode));
    }

    bool isValid;
    isValid = fslib::IsFloorsweepingValid(chip, gpcMasks, fbMasks, errMsg, m_Mode);
    Printf(Tee::PriLow, "FsLib returned %s\n", isValid ? "true" : "false");
    if (!isValid)
    {
        PrintFsMasks(gpcMasks, fbMasks, Tee::PriLow);
    }
    return isValid;
}

// ----------------------------------------------------------------------------
static string PrintFsMask(UINT32 mask, string name)
{
    return Utility::StrPrintf("%sMask=0x%x\n", name.c_str(), mask);
}

static string PrintFsMaskArr(const UINT32 *pMasks, UINT32 arrSize, string name)
{
    string maskPrint;
    for (UINT32 i = 0; i < arrSize; i++)
    {
        maskPrint += Utility::StrPrintf("%sMask[%u]=0x%x ", name.c_str(), i, pMasks[i]);
    }
    maskPrint += "\n";

    return maskPrint;
}

void FsLib::PrintFsMasks
(
    const GpcMasks& gpcMasks,
    const FbpMasks& fbpMasks,
    Tee::Priority pri
)
{
    const UINT32 arrSize = 32; // From fs_lib.h
    string maskPrint = "FsLib Masks:\n";

    // GPC Masks
    maskPrint += PrintFsMask(gpcMasks.gpcMask, "gpc");
    maskPrint += PrintFsMaskArr(gpcMasks.tpcPerGpcMasks, arrSize, "tpc");
    maskPrint += PrintFsMaskArr(gpcMasks.pesPerGpcMasks, arrSize, "pes");
    maskPrint += PrintFsMaskArr(gpcMasks.ropPerGpcMasks, arrSize, "rop");
    maskPrint += PrintFsMaskArr(gpcMasks.cpcPerGpcMasks, arrSize, "cpc");

    // FBP Masks
    maskPrint += PrintFsMask(fbpMasks.fbpMask, "fbp");
    maskPrint += PrintFsMaskArr(fbpMasks.fbioPerFbpMasks,     arrSize, "fbio");
    maskPrint += PrintFsMaskArr(fbpMasks.fbpaPerFbpMasks,     arrSize, "fbpa");
    maskPrint += PrintFsMaskArr(fbpMasks.halfFbpaPerFbpMasks, arrSize, "halfFbpa");
    maskPrint += PrintFsMaskArr(fbpMasks.ltcPerFbpMasks,      arrSize, "ltc");
    maskPrint += PrintFsMaskArr(fbpMasks.l2SlicePerFbpMasks,  arrSize, "l2slice");

    Printf(pri, "%s", maskPrint.c_str());
}

// ----------------------------------------------------------------------------
static RC ParseGpcMasks(JSObject *pGpcSettings, FsLib::GpcMasks *pGpcMasks)
{
    JavaScriptPtr pJs;
    RC rc;

    CHECK_RC(pJs->GetProperty(pGpcSettings, "GpcMask", &(pGpcMasks->gpcMask)));

    JsArray tpcMasks, pesMasks, ropMasks;
    if (pJs->GetProperty(pGpcSettings, "TpcMasks", &tpcMasks) == RC::OK)
    {
        for (UINT32 i = 0; i < tpcMasks.size(); i++)
        {
            CHECK_RC(pJs->FromJsval(tpcMasks[i], &(pGpcMasks->tpcPerGpcMasks[i])));
        }
    }
    if (pJs->GetProperty(pGpcSettings, "PesMasks", &pesMasks) == RC::OK)
    {
        for (UINT32 i = 0; i < pesMasks.size(); i++)
        {
            CHECK_RC(pJs->FromJsval(pesMasks[i], &(pGpcMasks->pesPerGpcMasks[i])));
        }
    }
    if (pJs->GetProperty(pGpcSettings, "RopMasks", &ropMasks) == RC::OK)
    {
        for (UINT32 i = 0; i < ropMasks.size(); i++)
        {
            CHECK_RC(pJs->FromJsval(ropMasks[i], &(pGpcMasks->ropPerGpcMasks[i])));
        }
    }

    return rc;
}

static RC ParseFbMasks(JSObject *pFbSettings, FsLib::FbpMasks *pFbMasks)
{
    JavaScriptPtr pJs;
    RC rc;

    CHECK_RC(pJs->GetProperty(pFbSettings, "FbpMask", &(pFbMasks->fbpMask)));

    JsArray fbioMasks, fbpaMasks, halfFbpaMasks, ltcMasks, l2SliceMasks;
    if (pJs->GetProperty(pFbSettings, "FbioMasks", &fbioMasks) == RC::OK)
    {
        for (UINT32 i = 0; i < fbioMasks.size(); i++)
        {
            CHECK_RC(pJs->FromJsval(fbioMasks[i], &(pFbMasks->fbioPerFbpMasks[i])));
        }
    }
    if (pJs->GetProperty(pFbSettings, "FbpaMasks", &fbpaMasks) == RC::OK)
    {
        for (UINT32 i = 0; i < fbpaMasks.size(); i++)
        {
            CHECK_RC(pJs->FromJsval(fbpaMasks[i], &(pFbMasks->fbpaPerFbpMasks[i])));
        }
    }
    if (pJs->GetProperty(pFbSettings, "HalfFbpaMasks", &halfFbpaMasks) == RC::OK)
    {
        for (UINT32 i = 0; i < halfFbpaMasks.size(); i++)
        {
            CHECK_RC(pJs->FromJsval(halfFbpaMasks[i], &(pFbMasks->halfFbpaPerFbpMasks[i])));
        }
    }
    if (pJs->GetProperty(pFbSettings, "LtcMasks", &ltcMasks) == RC::OK)
    {
        for (UINT32 i = 0; i < ltcMasks.size(); i++)
        {
            CHECK_RC(pJs->FromJsval(ltcMasks[i], &(pFbMasks->ltcPerFbpMasks[i])));
        }
    }
    if (pJs->GetProperty(pFbSettings, "L2SliceMasks", &l2SliceMasks) == RC::OK)
    {
        for (UINT32 i = 0; i < l2SliceMasks.size(); i++)
        {
            CHECK_RC(pJs->FromJsval(l2SliceMasks[i], &(pFbMasks->l2SlicePerFbpMasks[i])));
        }
    }

    return rc;
}

// ----------------------------------------------------------------------------

JS_CLASS(FsLib);

static SObject FsLib_Object
(
   "FsLib",
   FsLibClass,
   0,
   0,
   "FsLib interface."
);

JS_STEST_LWSTOM(FsLib,
                IsSupported,
                1,
                "Check if a FS validity check can be done for the given device id")
{
    STEST_HEADER(1, 1, "Usage: FsLib.IsSupported(DeviceId)");
    STEST_ARG(0, UINT32, DeviceId);

    bool isSupported = FsLib::IsSupported(Device::LwDeviceId(DeviceId));
    if (pJavaScript->ToJsval(isSupported, pReturlwalue) != OK)
    {
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_STEST_LWSTOM(FsLib,
                SetMode,
                1,
                "Set the mode fslib should run in")
{
    STEST_HEADER(1, 1, "Usage: FsLib.SetMode(FsLib.Mode)");
    STEST_ARG(0, UINT32, Mode);

    RETURN_RC(FsLib::SetMode(static_cast<FsLib::FsMode>(Mode)));
}

JS_SMETHOD_LWSTOM(FsLib,
                  IsFloorsweepingValid,
                  4,
                  "Check if floorsweeping settings are valid")
{
    STEST_HEADER(4, 4, "Usage: FsLib.IsFloorsweepingValid(DeviceId, GpcMasks, FbMasks, [ErrMsg])");
    STEST_ARG(0, UINT32, DeviceId);
    STEST_ARG(1, JSObject*, pGpcSettings);
    STEST_ARG(2, JSObject*, pFbSettings);
    STEST_ARG(3, JSObject*, pErrMsg);

    FsLib::GpcMasks gpcMasks = {};
    if (ParseGpcMasks(pGpcSettings, &gpcMasks) != OK)
    {
        Printf(Tee::PriError, "Invalid GPC mask settings\n");
        return JS_FALSE;
    }

    FsLib::FbpMasks fbMasks = {};
    if (ParseFbMasks(pFbSettings, &fbMasks) != OK)
    {
        Printf(Tee::PriError, "Invalid FB mask settings\n");
        return JS_FALSE;
    }

    string errMsg = "";
    bool isValid = FsLib::IsFloorsweepingValid(Device::LwDeviceId(DeviceId),
                                               gpcMasks,
                                               fbMasks,
                                               errMsg);
    if ((pJavaScript->ToJsval(isValid, pReturlwalue) != OK) ||
        (pJavaScript->SetElement(pErrMsg, 0, errMsg) != OK))
    {
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

PROP_CONST(FsLib, MODE_DISABLE,    static_cast<UINT32>(FsLib::FsMode::IGNORE_FSLIB));
PROP_CONST(FsLib, MODE_FUNC_VALID, static_cast<UINT32>(FsLib::FsMode::FUNC_VALID));
PROP_CONST(FsLib, MODE_PRODUCT,    static_cast<UINT32>(FsLib::FsMode::PRODUCT));
