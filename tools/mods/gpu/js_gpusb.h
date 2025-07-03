/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//-----------------------------------------------------------------------------
// Utility file, keep definitions of GPUSUBDEVICE_READFUNC and
// GPUSUBDEVICE_WRITEFUNC out of js_gpu so they don't confuse doxygen
//-----------------------------------------------------------------------------
#pragma once

#ifndef INCLUDED_JS_GPUSB_H
#define INCLUDED_JS_GPUSB_H

#include "jsapi.h"
#include "core/include/rc.h"
#include <string>
#include <map>
#include "core/include/jscript.h"

class GpuSubdevice;

class JsFrameBuffer;
class JsThermal;
class JsPerf;
class JsPower;
class JsRegHal;
class JsGpio;
class JsPerfmon;
class JsPmu;

#if defined(INCLUDE_DGPU)
class JsFuse;
class JsAvfs;
class JsVolt3x;
#endif

class JsGpuSubdevice
{
public:
    JsGpuSubdevice();
    ~JsGpuSubdevice();

    void            SetGpuSubdevice(GpuSubdevice * pSubdev);
    GpuSubdevice *  GetGpuSubdevice();
    RC              CreateJSObject(JSContext *cx, JSObject *obj);
    JSObject *      GetJSObject() {return m_pJsGpuSubdevObj;}
    void            SetJsFb(JsFrameBuffer *pJsFb)        { m_pJsFb = pJsFb; }
    void            SetJsTherm(JsThermal *pJsTherm)      { m_pJsTherm = pJsTherm; }
    void            SetJsPerf(JsPerf *pJsPerf)           { m_pJsPerf = pJsPerf; }
    void            SetJsRegHal(JsRegHal *pJsRegHal)     { m_pJsRegHal = pJsRegHal; }
    void            SetJsPower(JsPower *pJsPower)        { m_pJsPower = pJsPower; }
    void            SetJsGpio(JsGpio *pJsGpio)           { m_pJsGpio = pJsGpio; }
    void            SetJsPerfmon(JsPerfmon *pJsPerfmon)  { m_pJsPerfmon = pJsPerfmon; }
    void            SetJsPmu(JsPmu *pJsPmu)              { m_pJsPmu = pJsPmu; }
#if defined(INCLUDE_DGPU)
    void            SetJsFuse(JsFuse *pJsFuse)           { m_pJsFuse = pJsFuse; }
    void            SetJsAvfs(JsAvfs *pJsAvfs)           { m_pJsAvfs = pJsAvfs; }
    void            SetJsVolt(JsVolt3x *pJsVolt)         { m_pJsVolt3x = pJsVolt; }
#endif

    static void Resume(GpuSubdevice *pSubdev);

    string      GetDeviceName();
    RC          GetChipSkuNumber(string* pVal);
    RC          GetChipSkuModifier(string* pVal);
    RC          GetChipSkuNumberModifier(string* pVal);
    string      GetBoardNumber();
    string      GetBoardSkuNumber();
    string      GetCDPN();
    UINT32      GetDevInst();
    UINT32      GetDeviceId();
    bool        GetIsPrimary();
    string      GetBootInfo();
    UINT32      GetPciDeviceId();
    UINT32      GetChipArchId();
    UINT32      GetSubdeviceInst();
    UINT32      GetGpuInst();
    UINT64      GetFbHeapSize();
    UINT64      GetFramebufferBarSize();
    JSObject   *GetParentDevice();
    string      GetPdiString();
    string      GetRawEcidString();
    string      GetGHSRawEcidString();
    string      GetEcidString();
    string      GetChipIdString();
    string      GetUUIDString();
    string      GetChipIdCooked();
    RC          GetRevision(UINT32* pVal);
    RC          GetPrivateRevision(UINT32* pVal);
    string      GetRevisionString();
    string      GetPrivateRevisionString();
    UINT32      GetSubsystemVendorId();
    UINT32      GetSubsystemDeviceId();
    UINT32      GetBoardId();
    PHYSADDR    GetPhysLwBase();
    PHYSADDR    GetPhysFbBase();
    PHYSADDR    GetPhysInstBase();
    UINT32      GetIrq();
    UINT32      GetRopCount();
    UINT32      GetShaderCount();
    UINT32      GetUGpuCount();
    UINT32      GetUGpuMask();
    UINT32      GetVpeCount();
    UINT32      GetFrameBufferUnitCount();
    UINT32      GetFrameBufferIOCount();
    UINT32      GetGpcCount();
    UINT32      GetMaxGpcCount();
    UINT32      GetTpcCount();
    UINT32      GetMaxTpcCount();
    UINT32      GetMaxFbpCount();
    UINT32      GetGpcMask();
    UINT32      GetTpcMask();
    UINT32      GetTpcMaskOnGpc(UINT32 GpcNum);
    bool        IsGpcGraphicsCapable(UINT32 hwGpcNum);
    UINT32      GetGraphicsTpcMaskOnGpc(UINT32 hwGpcNum);
    bool        SupportsReconfigTpcMaskOnGpc(UINT32 gpcNum);
    UINT32      GetReconfigTpcMaskOnGpc(UINT32 GpcNum);
    UINT32      GetPesMaskOnGpc(UINT32 GpcNum);
    bool        SupportsRopMaskOnGpc(UINT32 gpcNum);
    UINT32      GetRopMaskOnGpc(UINT32 GpcNum);
    bool        SupportsCpcMaskOnGpc(UINT32 gpcNum);
    UINT32      GetCpcMaskOnGpc(UINT32 GpcNum);
    UINT32      GetFbMask();
    UINT32      GetFbpMask();
    UINT32      GetFbpaMask();
    UINT32      GetL2Mask();
    UINT32      GetL2MaskOnFbp(UINT32 FbpNum);
    bool        SupportsL2SliceMaskOnFbp(UINT32 FbpNum);
    UINT32      GetL2SliceMaskOnFbp(UINT32 FbpNum);
    UINT32      GetFbioMask();
    UINT32      GetFbioShiftMask();
    UINT32      GetLwdecMask();
    UINT32      GetLwencMask();
    UINT32      GetLwjpgMask();
    UINT32      GetPceMask();
    UINT32      GetMaxPceCount();
    UINT32      GetOfaMask();
    UINT32      GetSyspipeMask();
    UINT32      GetSmMask();
    UINT32      GetSmMaskOnTpc(UINT32 GpcNum, UINT32 TpcNum);
    UINT32      GetXpMask();
    UINT32      GetMevpMask();
    bool        GetHasVbiosModsFlagA();
    bool        GetHasVbiosModsFlagB();
    UINT32      GetNumAteSpeedo();
    bool        GetIsMXMBoard();
    UINT32      GetMXMVersion();
    UINT32      GetHQualType();
    bool        GetIsEmulation();
    bool        GetIsEmOrSim();
    bool        GetIsDFPGA();
    UINT32      GetPciDomainNumber();
    UINT32      GetPciBusNumber();
    UINT32      GetPciDeviceNumber();
    UINT32      GetPciFunctionNumber();
    string      GetRomPartner();
    string      GetRomProjectId();
    string      GetRomVersion();
    string      GetRomType();
    UINT32      GetRomTypeEnum();
    UINT32      GetRomTimestamp();
    UINT32      GetRomExpiration();
    UINT32      GetRomSelwrityStatus();
    string      GetRomOemVendor();
    string      GetInfoRomVersion();
    string      GetBoardSerialNumber();
    string      GetBoardMarketingName();
    string      GetBoardProjectSerialNumber();
    UINT32      GetFoundry();
    string      GetFoundryString();
    UINT32      GetAspmState();
    UINT32      GetAspmCyaState();
    bool        GetIsASLMCapable();
    UINT32      GetL2CacheSize();
    UINT32      GetL2CacheSizePerUGpu(UINT32 uGpu);
    UINT32      GetMaxL2SlicesPerFbp();
    UINT32      GetMaxCeCount();
    RC          SetPexAerEnable(bool bEnable);
    bool        GetPexAerEnable();
    bool        GetHasAspmL1Substates();
    bool        GetCanL2BeDisabled();
    bool        GetIsEccFuseEnabled();
    string      GetVbiosImageHash();
    bool        IsMfgModsSmcEnabled();
    UINT32      GetSmbusAddress();

    // Specific clock access functions
    RC          SetMemoryClock(UINT64 Hz);
    UINT64      GetMemoryClock();
    UINT64      GetTargetMemoryClock();

    bool        HasBug(UINT32 bugNum);
    bool        HasFeature(UINT32 feature);
    bool        IsFeatureEnabled(UINT32 feature);
    bool        IsSOC();
    bool        HasDomain(UINT32 domain);
    bool        HasGpcRg();
    bool        CanClkDomainBeProgrammed(UINT32 domain);
    UINT32      GetPcieLinkWidth();
    UINT32      GetPcieLinkSpeed();
    UINT32      GetPcieSpeedCapability();
    UINT32      GetBusType();
    UINT32      GetMemoryIndex();
    UINT32      GetRamConfigStrap();
    UINT32      GetTvEncoderStrap();
    UINT32      GetIo3GioConfigPadStrap();
    UINT32      GetUserStrapVal();

    RC          SetVerboseJtag(bool enable);
    bool        GetVerboseJtag();

    RC          SetGpuLocId(UINT32 locId);
    UINT32      GetGpuLocId();

    string      GetGpuIdentStr();
    string      GetChipSelwrityFuse();
    string      GetPackageStr();
    bool        IsChipSltFused();

    RC          SetGspFwImg(string gspFwImg);
    string      GetGspFwImg();

    RC          SetGspLogFwImg(string gspLogFwImg);
    string      GetGspLogFwImg();

    bool GetUseRegOps();
    RC   SetUseRegOps(bool bUseRegOps);

    RC          GetNamedProp(string Name, UINT32  *pValue);
    RC          GetNamedProp(string Name, bool    *pValue);
    RC          GetNamedProp(string Name, string  *pValue);
    RC          GetNamedProp(string Name, JsArray *pArray);
    RC          SetNamedProp(string Name, UINT32   Value);
    RC          SetNamedProp(string Name, bool     Value);
    RC          SetNamedProp(string Name, const string &Value);
    RC          SetNamedProp(string Name, JsArray *pArray);

    RC CreateNamedProperties(JSContext *cx, JSObject *obj);
    static void RegisterNamedProperty(const char * Name,
                                      JSPropertyOp GetMethod,
                                      JSPropertyOp SetMethod);

    enum AspmCounterIdx
    {
        GPU_XMIT_L0S_ID      = 0,
        UPSTREAM_XMIT_L0S_ID = 1,
        L1_ID                = 2,
        L1P_ID               = 3,
        DEEP_L1_ID           = 4,
        L1_1_SS_ID           = 5,
        L1_2_SS_ID           = 6
    };

    RC DumpCieData();
    RC RunFpfFub();
    RC RunFpfFubUseCase(UINT32 useCase);
    RC RunRirFrb();

    RC SetSkipAzaliaInit(bool bSkip);
    bool GetSkipAzaliaInit();
    RC EnableGpcRg(bool bEnable);

    bool GetIsMIGEnabled();
    bool GetSkipEccErrCtrInit();
    RC SetSkipEccErrCtrInit(bool);

private:
    GpuSubdevice *  m_pGpuSubdevice;
    JSObject *      m_pJsGpuSubdevObj;

    // Sub-objects
    JsFrameBuffer  *m_pJsFb;
    JsThermal      *m_pJsTherm;
    JsPerf         *m_pJsPerf;
    JsRegHal       *m_pJsRegHal;
    JsPower        *m_pJsPower;
    JsGpio         *m_pJsGpio;
    JsPerfmon      *m_pJsPerfmon;
    JsPmu          *m_pJsPmu;

#if defined(INCLUDE_DGPU)
    JsFuse         *m_pJsFuse;
    JsAvfs         *m_pJsAvfs;
    JsVolt3x       *m_pJsVolt3x;
#endif

    static map<string, pair<JSPropertyOp, JSPropertyOp> > s_NamedProperties;
};

#ifndef DOXYGEN_IGNORE
#define GPUSUBDEVICE_READFUNC(funcname,helpmsg)                                         \
    C_(JsGpuSubdevice_##funcname);                                                      \
    C_(JsGpuSubdevice_##funcname)                                                       \
    {                                                                                   \
        UINT32 Offset;                                                                  \
        JavaScriptPtr pJavaScript;                                                      \
        if ((NumArguments != 1) ||                                                      \
            (OK != pJavaScript->FromJsval(pArguments[0], &Offset)))                     \
        {                                                                               \
            JS_ReportError(pContext, "Usage: GpuSubdevice." #funcname "(Offset)");      \
            return JS_FALSE;                                                            \
        }                                                                               \
                                                                                        \
        UINT32 ReadData;                                                                \
        JsGpuSubdevice * pJsSubdev;                                                     \
                                                                                        \
        if ((pJsSubdev = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject, "GpuSubdevice")) != NULL) \
        {                                                                               \
            ReadData = (UINT32)pJsSubdev->GetGpuSubdevice()->funcname(Offset);          \
                                                                                        \
            if (OK != pJavaScript->ToJsval( ReadData, pReturlwalue))                    \
            {                                                                           \
                JS_ReportError( pContext, "Error oclwrred in GpuSubdevice." #funcname); \
                *pReturlwalue = JSVAL_NULL;                                             \
                return JS_FALSE;                                                        \
            }                                                                           \
            return JS_TRUE;                                                             \
        }                                                                               \
        return JS_FALSE;                                                                \
    }                                                                                   \
    static SMethod JsGpuSubdevice_##funcname                                            \
    (                                                                                   \
        JsGpuSubdevice_Object,                                                          \
        #funcname,                                                                      \
        C_JsGpuSubdevice_##funcname,                                                    \
        1,                                                                              \
        helpmsg                                                                         \
    )

#define GPUSUBDEVICE_WRITEFUNC(funcname,helpmsg)                                            \
    C_(JsGpuSubdevice_##funcname);                                                          \
    C_(JsGpuSubdevice_##funcname)                                                           \
    {                                                                                       \
        JavaScriptPtr pJavaScript;                                                          \
        UINT32 Offset;                                                                      \
        UINT32 Data;                                                                        \
        if ((NumArguments != 2) ||                                                          \
            (OK != pJavaScript->FromJsval(pArguments[0], &Offset)) ||                       \
            (OK != pJavaScript->FromJsval(pArguments[1], &Data)))                           \
        {                                                                                   \
            JS_ReportError(pContext, "Usage: GpuSubdevice." #funcname "(Offset, Data)");    \
            return JS_FALSE;                                                                \
        }                                                                                   \
                                                                                            \
        JsGpuSubdevice * pJsSubdev;                                                         \
                                                                                            \
        if ((pJsSubdev = JS_GET_PRIVATE(JsGpuSubdevice, pContext, pObject, "GpuSubdevice")) != NULL) \
        {                                                                                   \
            pJsSubdev->GetGpuSubdevice()->funcname(Offset, Data);                           \
            RETURN_RC(OK);                                                                  \
        }                                                                                   \
        return JS_FALSE;                                                                    \
    }                                                                                       \
    static STest JsGpuSubdevice_##funcname                                                  \
    (                                                                                       \
        JsGpuSubdevice_Object,                                                              \
        #funcname,                                                                          \
        C_JsGpuSubdevice_##funcname,                                                        \
        2,                                                                                  \
        helpmsg                                                                             \
    )

#endif // DOXYGEN_IGNORE
#endif // INCLUDED_JS_GPUSB_H
