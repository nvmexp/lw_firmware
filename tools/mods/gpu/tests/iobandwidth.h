/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/include/dmawrap.h"
#include "gpu/tests/gputest.h"

class GpuDevice;
class GpuSubdevice;
class GpuTestConfiguration;

extern SObject IOBandwidth_Object;

class IOBandwidth : public GpuTest
{
public:
    IOBandwidth();
    virtual ~IOBandwidth() = default;

    bool IsSupported() override;
    RC Setup() override;
    RC Cleanup() override;
    void PrintJsProperties(Tee::Priority pri) override;

    SETGET_PROP_ENUM(TransferType, UINT08, TransferType, TransferType::READS_AND_WRITES);
    SETGET_PROP(CopiesPerLoop,         UINT32);
    SETGET_PROP(SurfSizeFactor,        UINT32);
    SETGET_PROP(SrcSurfMode,           UINT32);
    SETGET_PROP(SrcSurfModePercent,    UINT32);
    SETGET_PROP(ShowBandwidthData,     bool);
    SETGET_PROP(SkipBandwidthCheck,    bool);
    SETGET_PROP(SkipSurfaceCheck,      bool);
    SETGET_PROP(IgnorePceRequirements, bool);
    SETGET_PROP(NumErrorsToPrint,      UINT32);
    SETGET_PROP(RuntimeMs,             UINT32);
protected:
    enum class TransferType : UINT08
    {
        READS,
        WRITES,
        READS_AND_WRITES
    };
    RC InnerRun(TransferType tt);
    TestDevicePtr GetRemoteTestDevice() { return m_pRemoteDevice; }
private:
    enum class CeLocation : UINT08
    {
        LOCAL,
        REMOTE
    };
    enum class PatternType : UINT08
    {
        BARS
       ,RANDOM
       ,SOLID
       ,LINES
       ,ALTERNATE
    };
    // Used to index a vector so cannot be an enum class
    enum Surfaces
    {
        SRC_SURF_A,
        SRC_SURF_B,
        DST_SURF,
        SURF_COUNT
    };
    struct EndpointConfig
    {
        UINT32                                   engineId    = LW2080_ENGINE_TYPE_COPY(0);
        bool                                     bCeRemapped = false;
        DmaWrapper                               dmaWrap;
        LW2080_CTRL_CE_SET_PCE_LCE_CONFIG_PARAMS ceRestore   = { };
        vector<Surface2D>                        surfaces;
    };

    struct TransferSetup
    {
        Surface2D *pSrcSurfA        = nullptr;
        Surface2D *pSrcSurfB        = nullptr;
        Surface2D *pDstSurf         = nullptr;
        UINT64     srcCtxDmaOffsetA = 0ULL;
        UINT64     srcCtxDmaOffsetB = 0ULL;
        UINT64     dstCtxDmaOffset  = 0ULL;
    };

    GpuTestConfiguration *m_pTestConfig   = nullptr;
    EndpointConfig        m_LocalConfig;
    EndpointConfig        m_RemoteConfig;
    TestDevicePtr         m_pRemoteDevice;
    bool m_bLoopback                      = false;
    Surfaces m_LwrSrcSurf                 = SRC_SURF_A;

    // JS Variables
    TransferType m_TransferType          = TransferType::READS_AND_WRITES;
    UINT32       m_CopiesPerLoop         = 1;
    UINT32       m_SurfSizeFactor        = 1;
    UINT32       m_SrcSurfMode           = 0;
    UINT32       m_SrcSurfModePercent    = 50;
    bool         m_ShowBandwidthData     = false;
    bool         m_SkipBandwidthCheck    = false;
    bool         m_SkipSurfaceCheck      = false;
    bool         m_IgnorePceRequirements = false;
    UINT32       m_NumErrorsToPrint      = 10;
    UINT32       m_RuntimeMs             = 0;

    RC AllocateSurface(Surface2D * pSurf, GpuDevice * pGpuDev, Memory::Location memLoc);
    RC CheckBandwidth(TransferType tt, UINT64 elapsedTimeUs, CeLocation ceLoc, UINT32 loop);
    RC ConfigureCopyEngines(GpuSubdevice * pGpuSubdevice, EndpointConfig * pEndpointConfig);
    RC DoTransfers
    (
        TransferType          tt,
        const TransferSetup & localSetup,
        const TransferSetup & remoteSetup,
        UINT64 *pLocalElapsedTimeUs,
        UINT64 *pRemoteElapsedTimeUs
    );
    RC FillSrcSurfaces(EndpointConfig * pEndpointConfig);
    RC FillSurface
    (
         Surface2D* pSurf
        ,PatternType pt
        ,UINT32 patternData
        ,UINT32 patternData2
        ,UINT32 patternHeight
    );
    RC GetTransferSetup
    (
        TransferType tt,
        EndpointConfig * pSrcConfig,
        EndpointConfig * pDstConfig,
        TransferSetup  * pTransferSetup
    );
    RC QueueCopy
    (
        TransferType          tt,
        DmaWrapper          * pDmaWrap,
        const TransferSetup & transferSetup,
        bool                  bFirstCopy,
        bool                  bLastCopy
    );
    RC SetupPatternSurfaces(Surface2D *pPatternA, Surface2D *pPatternB);
    RC SetupSurfaces(EndpointConfig * pEndpointConfig);
    RC VerifyDestinationSurface(const TransferSetup & setup);

    virtual UINT64 GetBandwidthThresholdPercent(IOBandwidth::TransferType tt) = 0;
    virtual UINT64 GetDataBandwidthKiBps(IOBandwidth::TransferType tt) = 0;
    virtual string GetDeviceString(TestDevicePtr pTestDevice) = 0;
    virtual string GetInterfaceBwSummary() = 0;
    virtual UINT32 GetPixelMask() const = 0;
    virtual UINT64 GetRawBandwidthKiBps() = 0;
    virtual RC GetRemoteTestDevice(TestDevicePtr *pRemoteTestDev) = 0;
    virtual UINT64 GetStartTimeMs() const = 0;
    virtual const char * GetTestedInterface() const = 0;
    virtual void SetupDmaWrap(TransferType tt, DmaWrapper * pDmaWrap) const = 0;
    virtual bool UsePhysAddrCopies() const = 0;
};

