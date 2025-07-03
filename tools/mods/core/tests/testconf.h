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

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_TEST_CONFIGURATION_H
#define INCLUDED_TEST_CONFIGURATION_H

#ifndef INCLUDED_STL_SET
#include <set>
#define INCLUDED_STL_SET
#endif
#ifndef INCLUDED_SCRIPT_H
#include "core/include/script.h"
#endif
#ifndef INCLUDED_MEMORYIF_H
#include "core/include/memoryif.h"
#endif
#ifndef INCLUDED_FSAA_H
#include "core/include/fsaa.h"
#endif
#ifndef INCLUDED_TEE_H
#include "core/include/tee.h"
#endif
#ifndef PCI_H__
#include "core/include/pci.h"
#endif

class Channel;
class Surface2D;
class Tsg;

//! Abstract class
class TestConfiguration
{
public:
    enum TestConfigurationType
    {
        TC_Generic,
        TC_GPU,
        TC_WMP,
    };
    enum ChannelType
    {
        GpFifoChannel,
        TegraTHIChannel,
        UseNewestAvailable,

        NUM_CHANNEL_TYPES // must be last
    };

    struct PexThreshold
    {
        UINT32 LocError;
        UINT32 HostError;
    };

public:
    // CREATORS
    TestConfiguration();
    virtual ~TestConfiguration();

    // MANIPULATORS

    // InitFromJs functions are not virtual, they call virtual protected helpers:
    // Reset()
    // GetProperties()
    // GetPerTestOverrides(JSObject *) // S/JS object versions only
    // ValidateProperties()

    RC InitFromJs();

    // Pass your JS test object
    RC InitFromJs(const SObject & tstObj);
    RC InitFromJs(JSObject * tstObj);

    //! Print out information about JS accessable properties
    virtual void PrintJsProperties(Tee::Priority pri);

    // Allow C++ overrides of JavaScript settings. (simple setters)
    // Call these after InitFromJs.
    virtual void SetChannelType(ChannelType ct);
    void SetPushBufferLocation(Memory::Location loc);
    void SetMemoryType(Memory::Location loc);

    // By default, a given TestConfiguration object only allows one
    // Channel to be alive at once, to detect resource leaks.
    // In case someone writes a channel-switching test someday, that test
    // can call this function to disable the leak detection.
    void SetAllowMultipleChannels (bool DoAllow);

    // These are stubs in a generic TestConfiguration.  Don't want them
    // to be pure virtual because we want to be able to use a generic
    // TestConfiguration.                       // LwRm::Handle
    virtual RC AllocateChannel(Channel ** ppChan, UINT32 * phChan);
    virtual RC AllocateErrorNotifier(Surface2D *pErrorNotifier, UINT32 hVASpace);
    virtual RC FreeChannel (Channel * pChan);
    virtual RC IdleAllChannels();

    // ACCESSORS
    bool                AllowMultipleChannels() const;
    UINT32              StartLoop() const;
    UINT32              RestartSkipCount() const;
    UINT32              Loops() const;
    UINT32              Seed() const;
    FLOAT64             TimeoutMs() const;
    UINT32              DisplayWidth() const;
    UINT32              DisplayHeight() const;
    UINT32              DisplayDepth() const;
    UINT32              RefreshRate() const;
    FSAAMode            GetFSAAMode() const;
    UINT32              ZDepth() const;
    UINT32              SurfaceWidth() const;
    UINT32              SurfaceHeight() const;
    virtual ChannelType GetChannelType() const;
    virtual UINT32      ChannelSize() const;
    Memory::Location    PushBufferLocation() const;
    Memory::Location    DstLocation() const;
    Memory::Location    SrcLocation() const;
    Memory::Location    MemoryType() const;
    bool                UseIndMem() const;
    bool                UseTiledSurface() const;
    bool                DisableCrt() const;
    UINT32              HandleBase() const;
    UINT32              ClkChangeThreshold() const;
    const vector<PexThreshold>& PexCorrErrsAllowed() const { return m_PexCorrErrsAllowed; }
    const vector<PexThreshold>& PexUnSuppReqAllowed() const { return m_PexUnSuppReqAllowed; }
    const vector<PexThreshold>& PexNonFatalErrsAllowed() const { return m_PexNonFatalErrsAllowed; }
    UINT32              PexLineErrorsAllowed() const { return m_PexLineErrorsAllowed; }
    UINT32              PexCRCErrorsAllowed() const { return m_PexCRCErrorsAllowed; }
    UINT32              PexNAKsRcvdAllowed() const { return m_PexNAKsRcvdAllowed; }
    UINT32              PexNAKsSentAllowed() const { return m_PexNAKsSentAllowed; }
    UINT32              PexFailedL0sExitsAllowed() const { return m_PexFailedL0sExitsAllowed; }
    bool                EarlyExitOnErrCount() const { return m_EarlyExitOnErrCount; }
    bool                Verbose() const { return m_Verbose; }
    bool                ShortenTestForSim() const {return m_ShortenTestForSim;}

    virtual TestConfigurationType GetTestConfigurationType();

    // MANIPULATORS
    void              SetChannelSize(UINT32 Size);
#ifdef INCLUDE_MDIAG
    void              SetTimeoutMs(FLOAT64 timeoutMs);
#endif

protected:

    virtual void    Reset();
    virtual RC      GetProperties();
    virtual RC      ValidateProperties();
    virtual RC      GetPerTestOverrides(JSObject *valuesObj);
    const char *    ChannelTypeString(ChannelType ct) const;
    const char *    UseProperty(const char* validProperty);

private:
    void SetPexThresholdFromJs(JSObject *pValues, string JsPropStr, vector<PexThreshold> *pThresh);
    //
    // Controls (set from JavaScript, per-test)
    //
    UINT32              m_StartLoop;
    UINT32              m_RestartSkipCount;
    UINT32              m_Loops;
    UINT32              m_Seed;
    FLOAT64             m_TimeoutMs;
    UINT32              m_Width;
    UINT32              m_Height;
    UINT32              m_Depth;
    UINT32              m_RefreshRate;
    UINT32              m_ZDepth;
    FSAAMode            m_FSAAMode;
    UINT32              m_SurfaceWidth;
    UINT32              m_SurfaceHeight;
    Memory::Location    m_MemoryType;
    bool                m_UseIndMem;
    Memory::Location    m_PushBufferLocation;
    Memory::Location    m_DstLocation;
    Memory::Location    m_SrcLocation;
    ChannelType         m_ChannelType;
    UINT32              m_ChannelSize;
    bool                m_UseTiledSurface;
    bool                m_DisableCrt;
    bool                m_AllowMultipleChannels;
    UINT32              m_HandleBase;
    UINT32              m_ClkChangeThreshold;
    vector<PexThreshold> m_PexCorrErrsAllowed;
    vector<PexThreshold> m_PexUnSuppReqAllowed;
    vector<PexThreshold> m_PexNonFatalErrsAllowed;
    UINT32              m_PexLineErrorsAllowed;
    UINT32              m_PexCRCErrorsAllowed;
    UINT32              m_PexNAKsRcvdAllowed;
    UINT32              m_PexNAKsSentAllowed;
    UINT32              m_PexFailedL0sExitsAllowed;
    bool                m_EarlyExitOnErrCount;
    bool                m_Verbose;
    bool                m_ShortenTestForSim;

    enum IsOptional { Required, Optional};
    RC GetPropertiesHelper(JSObject *, IsOptional);

    set<string>         m_UnusedProperties;

    // Used to help generate unique handles.
    static UINT32    s_TstConfigCounter;

};

#endif // INCLUDED_TEST_CONFIGURATION_H
