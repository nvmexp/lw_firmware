/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2001-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
//------------------------------------------------------------------------------
// $Id: //sw/integ/gpu_drv/stage_rel/diag/utils/fakemods.h#1 $
//------------------------------------------------------------------------------
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#pragma once

#ifndef INCLUDED_FAKEMODS_H
#define INCLUDED_FAKEMODS_H

// We need to #define this so that the resman gets our types right
#ifndef LW_MODS
#define LW_MODS
#endif

#include "core/include/types.h"
#include "core/include/bitflags.h"
#include "lwmisc.h"
#include <climits>
#include <string>
#include <vector>
#include "g_lwconfig.h"

class FrameBuffer;
class MemError;
class RegHal;
enum class RegHalDomain;

//----------------------------------------------------------------------------
// Macros
//----------------------------------------------------------------------------

#define MASSERT(x)

// ct_assert is duplicated as we don't use massert.h in mats.
#define ct_assert(test) static_assert(test, #test)

#define CHECK_RC(x)           \
    do                        \
    {                         \
        if (OK != (rc = (x))) \
            return rc;        \
    } while (0)

#define CHECK_RC_CLEANUP(f)   \
    do                        \
    {                         \
        if (OK != (rc = (f))) \
            goto Cleanup;     \
    } while (0)

#define FIRST_RC(f)             \
    do                          \
    {                           \
        if (OK != (rc = (f)))   \
            firstRc = rc;       \
    } while (0)

#define NUMELEMS(array) ( sizeof(array) / sizeof((array)[0]) )

#define ALIGN_DOWN(val, align)  (  (val) - ((val)%(align)) )
#define ALIGN_UP(val, align) ALIGN_DOWN((val) + (align) - 1, align)

#define REG_RD_DRF(d, r, f) \
    ( (REG_RD32(LW##d##r) >> DRF_SHIFT(LW##d##r##f)) & DRF_MASK(LW##d##r##f) )

const INT32 OK = 0;

UINT32 REG_RD32(UINT32 a);
void   REG_WR32(UINT32 a, UINT32 d);

#define MEM_RD08(a) (*(const volatile UINT08 *)(a))
#define MEM_RD16(a) (*(const volatile UINT16 *)(a))
#define MEM_RD32(a) (*(const volatile UINT32 *)(a))
#define MEM_RD64(a) (*(const volatile UINT64 *)(a))

#define MEM_WR08(a, d) do { *(volatile UINT08 *)(a) = (d); } while (0)
#define MEM_WR16(a, d) do { *(volatile UINT16 *)(a) = (d); } while (0)
#define MEM_WR32(a, d) do { *(volatile UINT32 *)(a) = (d); } while (0)
#define MEM_WR64(a, d) do { *(volatile UINT64 *)(a) = (d); } while (0)

#define SETGET_PROP_LWSTOM(a, b)
#define SETGET_PROP(a, b)
#define SETGET_JSARRAY_PROP_LWSTOM(a)

#define SetName(TestName)

//----------------------------------------------------------------------------
// RC
//----------------------------------------------------------------------------

extern volatile UINT32 g_UserAbort;
class RC
{
protected:
    INT32 m_rc;

public:
    static const INT32 OK                      = 0;
    static const INT32 BAD_MEMORY              = 1;
    static const INT32 USER_ABORT              = 2;
    static const INT32 CANNOT_SET_STATE        = 3;
    static const INT32 ILWALID_RAM_AMOUNT      = 3;
    static const INT32 SOFTWARE_ERROR          = 3;
    static const INT32 FILE_DOES_NOT_EXIST     = 3;
    static const INT32 CANNOT_PARSE_FILE       = 3;
    static const INT32 CANNOT_ALLOCATE_MEMORY  = 3;
    static const INT32 BAD_PARAMETER           = 3;
    static const INT32 FILE_ERROR              = 3;
    static const INT32 UNSUPPORTED_FUNCTION    = 3;
    static const INT32 ILWALID_INPUT           = 3;
    static const INT32 BUFFER_ALLOCATION_ERROR = 3;
    static const INT32 PRIV_LEVEL_VIOLATION    = 3;
    static const INT32 CANNOT_COLWERT_STRING_TO_JSVAL = 3;

    // CREATORS
    RC(INT32 rc = OK) { *this = rc; }
    RC(const RC& rc)  { *this = rc; }
    RC& operator=(INT32 rc)
    {
        m_rc = g_UserAbort ? RC::USER_ABORT : rc;
        return *this;
    }
    RC& operator=(const RC& rc) { *this = rc.Get(); return *this; }

    // ACCESSORS
    INT32    Get()   const { return m_rc; }
    operator INT32() const { return m_rc; }
};

class StickyRC : public RC
{
public:
    StickyRC(INT32 rc = OK) : RC(rc) {}
    StickyRC(const RC& rc)  : RC(rc) {}
    StickyRC& operator=(INT32 rc)
    {
        m_rc = g_UserAbort ? RC::USER_ABORT : m_rc ? m_rc : rc;
        return *this;
    }
    StickyRC& operator=(const RC& rc) { *this = rc.Get(); return *this; }
};

//----------------------------------------------------------------------------
// Tee
//----------------------------------------------------------------------------
namespace Tee
{
    enum Priority
    {
        PriNone      = 0,
        PriDebug     = 1,
        PriLow       = 2,
        PriNormal    = 3,
        PriWarn      = 4,
        PriHigh      = 5,
        PriError     = 6,
        PriAlways    = 7,
        ScreenOnly   = 8,
        FileOnly     = 9,
        SerialOnly   = 10,
        CirlwlarOnly = 11,
        DebuggerOnly = 12,
        EthernetOnly = 13,
        MleOnly      = 14,
    };
    enum Level
    {
        LevLow = 0
    };

    enum ScreenPrintState
    {
        SPS_NORMAL
        ,SPS_FAIL
        ,SPS_PASS
        ,SPS_WARNING
        ,SPS_HIGHLIGHT
        ,SPS_HIGH
        ,SPS_BOTH
        ,SPS_LOW
        ,SPS_END_LIST
    };

    enum
    {
        ModuleNone   = ~0
    };

    UINT32 GetTeeModuleCoreCode();
    class SetLowAssertLevel
    {
    public:
        SetLowAssertLevel() { }
        ~SetLowAssertLevel() { }
    };
};

//----------------------------------------------------------------------------
// LwRm
//----------------------------------------------------------------------------
class LwRm { };

class LwRmPtr
{
public:
    LwRmPtr() { }
    LwRm *Get() { return (LwRm *)nullptr; }
};


//----------------------------------------------------------------------------
// Hardware
//----------------------------------------------------------------------------
namespace Gpu
{
    enum LwDeviceId
    {
        LW0 = 0
#define DEFINE_NEW_GPU( DIDStart, DIDEnd, ChipId, LwId, Constant, ...) \
        ,LwId = Constant
#define DEFINE_OBS_GPU(...) // don't care
#define DEFINE_DUP_GPU(...) // don't care
#include "gpu/include/gpulist.h"
#undef DEFINE_OBS_GPU
#undef DEFINE_DUP_GPU
#undef DEFINE_NEW_GPU
    };
}

extern UINT32 g_ChipId;

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------

#define FEATURE_TYPE_MASK       0x70000
#define GPUSUB_FEATURE_TYPE     0x00000
#define GPUDEV_FEATURE_TYPE     0x10000
#define MCP_FEATURE_TYPE        0x20000
#define SOC_FEATURE_TYPE        0x40000

class Device
{
public:
    enum LwDeviceId
    {
        LW0 = 0
#define DEFINE_NEW_GPU( DIDStart, DIDEnd, ChipId, LwId, Constant, ...) \
        ,LwId = Constant
#define DEFINE_OBS_GPU(...) // don't care
#define DEFINE_DUP_GPU(...) // don't care
#include "gpu/include/gpulist.h"
#undef DEFINE_OBS_GPU
#undef DEFINE_DUP_GPU
#undef DEFINE_NEW_GPU

#define DEFINE_LWL_DEV( DIDStart, DIDEnd, ChipId, LwId, Constant, ...)\
        ,LwId = Constant
#define DEFINE_OBS_LWL_DEV(...) // don't care
#include "core/include/lwlinklist.h"
#undef DEFINE_LWL_DEV

        ,ILWALID_DEVICE = 0xFFFF
    };

    enum Feature
    {
#define DEFINE_GPUSUB_FEATURE(feat, featid, desc)  \
        feat = GPUSUB_FEATURE_TYPE | featid,
#define DEFINE_GPUDEV_FEATURE(feat, featid, desc)  \
        feat = GPUDEV_FEATURE_TYPE | featid,
#define DEFINE_MCP_FEATURE(feat, featid, desc)  \
        feat = MCP_FEATURE_TYPE | featid,
#define DEFINE_SOC_FEATURE(feat, featid, desc)  \
        feat = SOC_FEATURE_TYPE | featid,

#include "core/include/featlist.h"

#undef DEFINE_GPUSUB_FEATURE
#undef DEFINE_GPUDEV_FEATURE
#undef DEFINE_MCP_FEATURE
#undef DEFINE_SOC_FEATURE

        FEATURE_COUNT
    };
    bool HasBug(UINT32 bugNum) const { return false; }
};

// forward reference
class GpuSubdevice;

class FloorsweepImpl
{
public:
    FloorsweepImpl(GpuSubdevice *pSubdev) {}
    UINT32 FbMask() const;
    UINT32 FbpMask() const;
    UINT32 FbioMask() const;
    UINT32 FbioshiftMask() const;
    UINT32 FbpaMask() const;
    UINT32 L2Mask(UINT32 fbpNum) const;
    UINT32 HalfFbpaMask() const;
    UINT32 GetHalfFbpaWidth() const;
    bool   SupportsL2SliceMask(UINT32 hwFbpNum) const;
    UINT32 L2SliceMask(UINT32 hwFbpNum) const;
};

class TestDevice : public Device
{
public:
    virtual UINT32 RegRd32(UINT32 reg) = 0;
    virtual UINT32 RegRd32
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        reg
    ) = 0;
    virtual void RegWr32(UINT32 reg, UINT32 value) = 0;
    virtual void RegWr32
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        reg,
        UINT32        value
    ) = 0;
    virtual void RegWrBroadcast32
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset,
        UINT32        data
    ) = 0;
    virtual void RegWrSync32(UINT32 reg, UINT32 value) = 0;
    virtual void RegWrSync32
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        reg,
        UINT32        value
    ) = 0;
    virtual UINT64 RegRd64(UINT32 offset) = 0;
    virtual UINT64 RegRd64
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset
    ) = 0;
    virtual void RegWr64(UINT32 offset, UINT64 data) = 0;
    virtual void RegWr64
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset,
        UINT64        data
    ) = 0;
    virtual void RegWrBroadcast64
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset,
        UINT64        data
    ) = 0;
    virtual void RegWrSync64(UINT32 offset, UINT64 data) = 0;
    virtual void RegWrSync64
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset,
        UINT64        data
    ) = 0;
    virtual Device::LwDeviceId GetDeviceId() const = 0;
};

class GpuSubdevice : public TestDevice
{
public:
    GpuSubdevice();
    void *EnsureFbRegionMapped(UINT64 fbOffset, UINT64 size);
    UINT32 RegRd32(UINT32 reg) override { return REG_RD32(reg); }
    UINT32 RegRd32
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        reg
    ) override { return REG_RD32(reg); }
    void RegWr32(UINT32 reg, UINT32 value) override { return REG_WR32(reg, value); }
    void RegWr32
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        reg,
        UINT32        value
    ) override { return REG_WR32(reg, value); }
    void RegWrBroadcast32
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset,
        UINT32        data
    ) override { MASSERT(!"RegWrBroadcast32 not supported\n"); }
    void RegWrSync32(UINT32 reg, UINT32 value) override { return REG_WR32(reg, value); }
    void RegWrSync32
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        reg,
        UINT32        value
    ) override { return REG_WR32(reg, value); }
    UINT64 RegRd64(UINT32 reg) override { MASSERT(!"RegRd64 not supported\n"); return 0; }
    UINT64 RegRd64
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        reg
    ) override { MASSERT(!"RegRd64 not supported\n"); return 0; }
    void RegWr64(UINT32 reg, UINT64 value) override { MASSERT(!"RegWr64 not supported\n"); }
    void RegWr64
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        reg,
        UINT64        value
    ) override { MASSERT(!"RegWr64 not supported\n"); }
    void RegWrBroadcast64
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        offset,
        UINT64        data
    ) override { MASSERT(!"RegWrBroadcast64 not supported\n"); }
    void RegWrSync64(UINT32 reg, UINT64 value) override { MASSERT(!"RegWrSync64 not supported\n"); }
    void RegWrSync64
    (
        UINT32        domIdx,
        RegHalDomain  domain,
        LwRm        * pLwRm,
        UINT32        reg,
        UINT64        value
    ) override { MASSERT(!"RegWrSync64 not supported\n"); }
    FrameBuffer *GetFB();
    bool HasFeature(Feature feature) const;
    bool IsFeatureEnabled(Feature feature);
    FloorsweepImpl *GetFsImpl() { return &m_FsImpl; }
    bool IsSOC() const;
    RegHal &Regs();
    const RegHal &Regs() const;
    Gpu::LwDeviceId DeviceId() const
    {
#define DEFINE_NEW_GPU( DIDStart, DIDEnd, ChipId, GpuId, ...) \
        if (g_ChipId == ChipId)                               \
            return Gpu::GpuId;
#define DEFINE_DUP_GPU( DIDStart, DIDEnd, ChipId, GpuId )     \
        if (g_ChipId == ChipId)                               \
            return Gpu::GpuId;
#define DEFINE_OBS_GPU(...) // don't care
#include "gpu/include/gpulist.h"
#undef DEFINE_OBS_GPU
#undef DEFINE_DUP_GPU
#undef DEFINE_NEW_GPU

        return Gpu::LW0;
    }
    Device::LwDeviceId GetDeviceId() const { return static_cast<Device::LwDeviceId>(DeviceId()); }

    RC GetHBMSiteFbps(UINT32 hbmSite, UINT32* const pFbp0, UINT32* const pFbp1) const
    { MASSERT(!"GetHBMSiteFbps not supported"); return RC::OK; }

    UINT32 NumRowRemapTableEntries() const { return 0; }
    std::string GetName() const;

private:
    FloorsweepImpl m_FsImpl;
};

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------

class GpuDevice
{
};

//----------------------------------------------------------------------------
// Tasker
//----------------------------------------------------------------------------
namespace Tasker
{
    bool IsInitialized();
    RC   Yield();
    enum { mtxLast, mtxNLast, mtxUnchecked, mtxFirst };
    inline void *AllocMutex(const char*, unsigned) { return nullptr; }
    inline void  FreeMutex(void *) {}

    struct DetachThread
    {
        DetachThread() {} // Prevent "unused variable" compiler warnings
    };
    using AttachThread = DetachThread;
    struct MutexHolder
    {
        MutexHolder(void *pMutex = nullptr) {}
    };
}

//----------------------------------------------------------------------------
// TestConfiguration
//----------------------------------------------------------------------------
class TestConfiguration
{
public:
    bool UseTiledSurface() { return false; }
    UINT32 SurfaceWidth() { return 0; }
    UINT32 SurfaceHeight() { return 0; }
    UINT32 DisplayDepth() { return 0; }
    UINT32 Seed() { return 0; }
};

//----------------------------------------------------------------------------
// Platform
//----------------------------------------------------------------------------
namespace Platform
{
    RC DisableUserInterface(UINT32 w, UINT32 h, UINT32 d, UINT32 r,
                            UINT32 junk1, UINT32 junk2, UINT32 junk3,
                            GpuDevice* junk4 );
    void EnableUserInterface(GpuDevice *p);

    void DelayNS(UINT32 ns);

    UINT64 GetTimeMS();

    inline bool IsPhysFunMode() { return true; }
    inline bool IsVirtFunMode() { return false; }
};

extern UINT32 Printf ( INT32 priority, const char * format, ... );
extern UINT32 Printf ( INT32 priority, UINT32 module, UINT32 sps,
                       const char * format, ... );
//----------------------------------------------------------------------------
// Random class
//----------------------------------------------------------------------------
class Random
{
public:
    void   Shuffle ( UINT32 deckSize, UINT32 * pDeck, UINT32 numSwaps =0 );
    void   SeedRandom(UINT32 x);
    UINT32 GetRandom ();
    UINT32 GetRandom ( UINT32 milwalue, UINT32 maxValue );
};

//----------------------------------------------------------------------------
// FP Context (needed to get a pointer to the above Random class)
//-----------------------------------------------------------------------------
struct FpContext
{
    Random Rand;
};
FpContext * GetFpContext();

//----------------------------------------------------------------------------
// Utility
//----------------------------------------------------------------------------
namespace Utility
{
    INT32 CountBits ( UINT32 value );

    INT32 BitScanForward(UINT32 value, INT32 start = 0);
    INT32 BitScanForward64(UINT64 value, INT32 start = 0);
    INT32 BitScanReverse(UINT32 x);
    INT32 FindNthSetBit(UINT32 value, INT32 n);
    UINT32 Log2i(UINT32 x);

    void CheckMaxLimit(UINT64 value, UINT64 maxLimit, const char *file, int line);

    std::string StrPrintf(const char * format, ...)
    #ifdef __GNUC__
    // GCC can type-check printf like 'Arguments' against the 'Format' string.
    __attribute__ ((format (printf, 1, 2)))
    #endif
    ;
}

#define UNSIGNED_CAST(type, value) (Utility::CheckMaxLimit(value, (std::numeric_limits<type>::max)(), __FILE__, __LINE__), static_cast<type>(value))

//----------------------------------------------------------------------------
// GpuUtility
//----------------------------------------------------------------------------
namespace GpuUtility
{
    struct MemoryChunkDesc
    {
        UINT64 size;
        UINT32 pteKind;
        UINT32 partStride;
        UINT32 pageSizeKB;
        UINT64 fbOffset;
    };
    typedef std::vector<MemoryChunkDesc> MemoryChunks;

    RC AllocateEntireFramebuffer(UINT64 minChunkSize,
                                 UINT64 maxChunkSize,
                                 UINT64 maxSize,
                                 MemoryChunks *pChunks,
                                 bool blockLinear,
                                 UINT64 minPageSize,
                                 UINT64 maxPageSize,
                                 UINT32 hChannel,
                                 GpuDevice *pGpuDev,
                                 UINT32 minPercentReqd,
                                 bool contiguous);

    RC FreeEntireFramebuffer(MemoryChunks *pChunks);
}

const UINT64 ONE_MB = 1U << 20;

//----------------------------------------------------------------------------
// Memory
//----------------------------------------------------------------------------
namespace Memory
{
    RC FillRgb ( void * address,
                 UINT32 width,
                 UINT32 height,
                 UINT32 depth,
                 UINT32 pitch );
}

//----------------------------------------------------------------------------
// ErrorMap
//----------------------------------------------------------------------------

static constexpr INT32 s_UnknownTestId = -1;

class ErrorMap
{
public:
    static INT32 Test() { return s_UnknownTestId; }
};

//----------------------------------------------------------------------------
// RowRemapper
//---------------------------------------------------------------------------
class RowRemapper
{
public:
    RowRemapper(GpuSubdevice* pSubdev, LwRm* pLwRm) {}

    struct Request {};
    struct MaxRemapsPolicy {};
    struct ClearSelection {};
    enum class Source;
    enum class RemapStatus;

    RC Initialize() { return RC::OK; }
    bool IsEnabled() { return false; }
};

//----------------------------------------------------------------------------
// Misc
//----------------------------------------------------------------------------

MemError &GetMemError(UINT32 x);
RC ValidateSoftwareTree();
GpuSubdevice *GetBoundGpuSubdevice();
GpuDevice *GetBoundGpuDevice();

class GpuTest
{
public:
    RC AllocDisplay();
    RC GetDisplayPitch(UINT32 *pPitch) { *pPitch = 4096; return OK; }
    RC EndLoop() { return OK; }
protected:
    TestConfiguration *GetTestConfiguration() { return &m_TestConfig; }
    TestConfiguration m_TestConfig;
};

class GpuMemTest : public GpuTest
{
public:
    GpuMemTest();
    RC InitFromJs() { return OK; };
    RC Cleanup();
    RC Setup();
    UINT64 GetStartLocation() { return m_StartLocation; }
    UINT64 GetEndLocation() { return m_EndLocation; }
    UINT32 GetMinFbMemPercent() { return m_MinFbMemPercent; }
    UINT32 GetMaxFbMb() { return m_MaxFbMb; }
    RC AllocateEntireFramebuffer
    (
        bool blockLinear,
        UINT32 hChannel
    );
    GpuUtility::MemoryChunks * GetChunks() { return &m_MemoryChunks; }

public:
    UINT64 m_EndLocation;
    UINT64 m_StartLocation;
    UINT32 m_MaxErrors;
    UINT32 m_MinFbMemPercent;
    UINT32 m_MaxFbMb;
    GpuUtility::MemoryChunks  m_MemoryChunks;
};

class MapFb
{
public:
    MapFb() {};
    ~MapFb() {};
    void * MapFbRegion(GpuUtility::MemoryChunks *pChunks,
                       UINT64 fbOffset,
                       UINT64 size,
                       GpuSubdevice *pGpuSubdevice = 0)
    {
        return GetBoundGpuSubdevice()->EnsureFbRegionMapped(fbOffset, size);
    }
    void UnmapFbRegion()
    {
        return;
    }

};

class GpuDevMgr
{
public:
    UINT32 NumGpus() { return 1; }
};

namespace DevMgrMgr
{
    extern GpuDevMgr* d_GraphDevMgr;
};

struct jsval;
typedef std::vector<jsval> JsArray;

struct jsval
{
    jsval()                     { clear(); }
    jsval(UINT64 v)             { clear(); ival = v; }
    jsval(UINT32 v)             { clear(); ival = UINT64(v); }
    jsval(INT64 v)              { clear(); ival = UINT64(v); }
    jsval(INT32 v)              { clear(); ival = UINT64(v); }
    jsval(float v)              { clear(); fval = v; }
    jsval(const std::string &v) { clear(); sval = v; }

    void clear()
    {
        ival = 0ULL;
        fval = 0.0f;
        sval = "";
        jval.clear();
    }

    UINT64 ival;
    float fval;
    std::string sval;
    JsArray jval;
};

class JavaScriptPtr
{
public:
    RC FromJsval(const jsval &in, UINT32 *out) const
    {
        MASSERT(out != NULL);
        *out = UINT32(in.ival);
        return OK;
    }
    RC FromJsval(const jsval &in, UINT64 *out) const
    {
        MASSERT(out != NULL);
        *out = in.ival;
        return OK;
    }
    RC FromJsval(const jsval &in, std::string *out) const
    {
        MASSERT(out != NULL);
        *out = in.sval;
        return OK;
    }
    RC FromJsval(const jsval &in, JsArray *out) const
    {
        MASSERT(out != NULL);
        *out = in.jval;
        return OK;
    }

    JavaScriptPtr *operator->() { return this; }
};

#define JSVAL_IS_STRING(x) 0

#endif
