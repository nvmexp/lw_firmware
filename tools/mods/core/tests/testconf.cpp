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

// Implementation of (new) TestConfiguration abstract class.

#include "testconf.h"
#include "core/include/jscript.h"

//------------------------------------------------------------------------------
// JS Class, and the one object.
//
JS_CLASS(TestConfiguration);

// Make TestConfiguration_Object global so that gputestc.cpp etc. can see it
SObject TestConfiguration_Object
(
    "TestConfiguration",
    TestConfigurationClass,
    0,
    0,
    "Default controls for Test values (may be overridden per-test)."
);

// Properties (for setting the shared default values)
static SProperty TestConfiguration_StartLoop
(
    TestConfiguration_Object,
    "StartLoop",
    0,
    0,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "Starting loop number."
);
static SProperty TestConfiguration_RestartSkipCount
(
    TestConfiguration_Object,
    "RestartSkipCount",
    0,
    100,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "Loops between restart points."
);
static SProperty TestConfiguration_Loops
(
    TestConfiguration_Object,
    "Loops",
    0,
    1000,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "The loops that the test run through."
);
static SProperty TestConfiguration_Seed
(
    TestConfiguration_Object,
    "Seed",
    0,
    0x12345678,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "The seed for the random test."
);
static SProperty TestConfiguration_TimeoutMs
(
    TestConfiguration_Object,
    "TimeoutMs",
    0,
    1000,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "Timeout in millisecond."
);
static SProperty TestConfiguration_Width
(
    TestConfiguration_Object,
    "DisplayWidth",
    0,
    1024,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "Width of display mode (# of pixels)."
);
static SProperty TestConfiguration_Height
(
    TestConfiguration_Object,
    "DisplayHeight",
    0,
    768,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "Height of display mode (# of pixels)."
);
static SProperty TestConfiguration_Depth
(
    TestConfiguration_Object,
    "DisplayDepth",
    0,
    32,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "bit per pixel (bpp) of display mode."
);
static SProperty TestConfiguration_RefreshRate
(
    TestConfiguration_Object,
    "DisplayRefreshRate",
    0,
    60,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "RefreshRate of display mode."
);
static SProperty TestConfiguration_ZDepth
(
    TestConfiguration_Object,
    "ZDepth",
    0,
    32,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "Depth of Z-buffer."
);
static SProperty TestConfiguration_FSAAMode
(
    TestConfiguration_Object,
    "FSAAMode",
    0,
    FSAADisabled,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "Full Screen Anti-Aliasing mode (0 to 8)"
);
static SProperty TestConfiguration_SurfaceWidth
(
    TestConfiguration_Object,
    "SurfaceWidth",
    0,
    1024,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "Width of test surface (# of pixels)."
);
static SProperty TestConfiguration_SurfaceHeight
(
    TestConfiguration_Object,
    "SurfaceHeight",
    0,
    768,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "Height of test surface (# of pixels)."
);
static SProperty TestConfiguration_PushBufferLocation
(
    TestConfiguration_Object,
    "PushBufferLocation",
    0,
    Memory::Coherent,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "Pushbuffer location: Memory.Fb, Memory.Coherent, or Memory.NonCoherent."
);
static SProperty TestConfiguration_DstLocation
(
    TestConfiguration_Object,
    "DstLocation",
    0,
    Memory::Optimal,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "Destination buffer location: Memory.Fb, Memory.Coherent, Memory.NonCoherent, or Memory.Optimal."
);
static SProperty TestConfiguration_SrcLocation
(
    TestConfiguration_Object,
    "SrcLocation",
    0,
    Memory::Optimal,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "Source buffer location: Memory.Fb, Memory.Coherent, Memory.NonCoherent, or Memory.Optimal."
);
static SProperty TestConfiguration_MemoryType
(
    TestConfiguration_Object,
    "MemoryType",
    0,
    Memory::Coherent,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "Memory type to use: Memory.Fb, Memory.Coherent, or Memory.NonCoherent."
);
static SProperty TestConfiguration_UseIndMem
(
    TestConfiguration_Object,
    "UseIndMem",
    0,
    false,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "Memory Access Method to use: true for indirect, false for direct."
);
static SProperty TestConfiguration_ChannelType
(
    TestConfiguration_Object,
    "ChannelType",
    0,
    TestConfiguration::UseNewestAvailable,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "Force the Channel type to use: GpFifo, TegraTHI, CheetAh, Dma, or Pio (default is UseNewestAvailable)."
);
static SProperty TestConfiguration_ChannelSize
(
    TestConfiguration_Object,
    "ChannelSize",
    0,
    1024*1024,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "Dma Channel Size."
);
static SProperty TestConfiguration_UseTiledSurface
(
    TestConfiguration_Object,
    "UseTiledSurface",
    0,
    true,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "If true, use tiled primary surface, else linear."
);
static SProperty TestConfiguration_DisableCrt
(
    TestConfiguration_Object,
    "DisableCrt",
    0,
    false,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "If true, disable the CRT DAC (or FP \"dac\") during test."
);

static SProperty TestConfiguration_AllowMultipleChannels
(
    TestConfiguration_Object,
    "AllowMultipleChannels",
    0,
    false,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "If true, allows this test to use multiple channels."
);

static SProperty TestConfiguration_EarlyExitOnErrCount
(
    TestConfiguration_Object,
    "EarlyExitOnErrCount",
    0,
    false,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "If true, exit the test early if error count violations are detected."
);

static SProperty TestConfiguration_Verbose
(
    TestConfiguration_Object,
    "Verbose",
    0,
    false,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "If true, enable verbose prints."
);

static SProperty TestConfiguration_ShortenTestForSim
(
    TestConfiguration_Object,
    "ShortenTestForSim",
    0,
    false,
    0,
    0,
    SPROP_VALID_TEST_ARG,
    "If true, test is reduced for Simulation runs."
);

// Constants
static SProperty TestConfiguration_ChannelUseNewestAvailable
(
    TestConfiguration_Object,
    "UseNewestAvailable",
    0,
    TestConfiguration::UseNewestAvailable,
    0,
    0,
    JSPROP_READONLY,
    "CONSTANT: UseNewestAvailablemake channel."
);
static SProperty TestConfiguration_ChannelGpFifo
(
    TestConfiguration_Object,
    "GpFifoChannel",
    0,
    TestConfiguration::GpFifoChannel,
    0,
    0,
    JSPROP_READONLY,
    "CONSTANT: GPFIFO channel."
);
static SProperty TestConfiguration_ChannelTegraTHI
(
    TestConfiguration_Object,
    "TegraTHIChannel",
    0,
    TestConfiguration::TegraTHIChannel,
    0,
    0,
    JSPROP_READONLY,
    "CONSTANT: CheetAh (host1x) channel with a THI wrapper."
);

//------------------------------------------------------------------------------
// Class statics
UINT32 TestConfiguration::s_TstConfigCounter = 0;

//------------------------------------------------------------------------------
// CREATORS
//
TestConfiguration::TestConfiguration()
{
    m_HandleBase    = 0x7CFA0000 + s_TstConfigCounter * 0x100;
    ++s_TstConfigCounter;
    TestConfiguration::Reset();
}

TestConfiguration::~TestConfiguration()
{
    // Nothing for now
}

//------------------------------------------------------------------------------
// MANIPULATORS
//
RC TestConfiguration::InitFromJs()
{
   RC rc;

   Reset();
   CHECK_RC(GetProperties());
   CHECK_RC(ValidateProperties());
   return OK;
}

RC TestConfiguration::InitFromJs
(
   const SObject & tstSObj
)
{
   JSObject * testObj = tstSObj.GetJSObject();
   return InitFromJs(testObj);
}

RC TestConfiguration::InitFromJs
(
   JSObject * testObj
)
{
   JavaScriptPtr pJs;
   RC            rc ;
   JSObject *    valuesObj = 0;

   if (testObj == NULL)
   {
       Printf(Tee::PriHigh,
              "JS Object not set!  Call var.SetJSObject() from JavaScript!\n");
       MASSERT(!"JS Object not set.  Bad instantiation of ModsTest.");
       return RC::SOFTWARE_ERROR;
   }

   // Put all member data to defaults.
   Reset();

   // Get the global test properties.
   CHECK_RC(GetProperties());

   //
   // Get the per-test Test property, if present.
   // Get per-test control overrides, if any.
   //
   rc = pJs->GetProperty(testObj, "TestConfiguration", &valuesObj);

   if (OK == rc)
   {
       CHECK_RC(GetPerTestOverrides(valuesObj));
   }

   CHECK_RC(ValidateProperties());

   if (!valuesObj)
   {
      // Create a TestValue property right now!
      rc = pJs->DefineProperty(testObj, "TestConfiguration", &valuesObj);
   }

   return OK;
}

//------------------------------------------------------------------------------
void TestConfiguration::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "TestConfiguration Js Properties:\n");
    Printf(pri, "\tStartLoop:\t\t\t%i\n",        m_StartLoop);
    Printf(pri, "\tRestartSkipCount:\t\t%i\n",   m_RestartSkipCount);
    Printf(pri, "\tLoops:\t\t\t\t%i\n",          m_Loops);
    Printf(pri, "\tSeed:\t\t\t\t%#010x\n",       m_Seed);
    Printf(pri, "\tTimeoutMs:\t\t\t%f\n",        m_TimeoutMs);
    Printf(pri, "\tDisplay:\t\t\t%ix%ix%i %iHz\n", m_Width, m_Height,
            m_Depth, m_RefreshRate);
    Printf(pri, "\tZDepth:\t\t\t\t%i\n",         m_ZDepth);
    Printf(pri, "\tFSAAMode:\t\t\t%s\n",         FsaaName(m_FSAAMode));
    Printf(pri, "\tSurface:\t\t\t%ux%u\n",       m_SurfaceWidth, m_SurfaceHeight);

    Printf(pri, "\tPushBufferLocation:\t\t%s\n", Memory::GetMemoryLocationName(m_PushBufferLocation));
    Printf(pri, "\tDstLocation:\t\t\t%s\n",      Memory::GetMemoryLocationName(m_DstLocation));
    Printf(pri, "\tSrcLocation:\t\t\t%s\n",      Memory::GetMemoryLocationName(m_SrcLocation));
    Printf(pri, "\tMemoryType:\t\t\t%s\n",       Memory::GetMemoryLocationName(m_MemoryType));
    Printf(pri, "\tUseIndMem:\t\t\t%s\n",        m_UseIndMem ? "true" : "false");
    Printf(pri, "\tChannelType:\t\t\t%s (multiple channels %sallowed)\n",
            ChannelTypeString(m_ChannelType), m_AllowMultipleChannels ? "" : "NOT ");

    Printf(pri, "\tChannelSize:\t\t\t%#010x (%i)\n", m_ChannelSize, m_ChannelSize);
    Printf(pri, "\tUseTiledSurface:\t\t%s\n",    m_UseTiledSurface ? "true" : "false");
    Printf(pri, "\tDisableCrt:\t\t\t%s\n",       m_DisableCrt      ? "true" : "false");
    Printf(pri, "\tEarlyExitOnErrCount:\t\t%s\n", m_EarlyExitOnErrCount ? "true" : "false");
    Printf(pri, "\tVerbose:\t\t\t%s\n", m_Verbose ? "true" : "false");
    Printf(pri, "\tShortenTestForSim:\t\t%s\n", m_ShortenTestForSim ? "true" : "false");

}

//------------------------------------------------------------------------------
void TestConfiguration::SetChannelType
(
    TestConfiguration::ChannelType ct
)
{
    m_ChannelType = ct;
}

//------------------------------------------------------------------------------
void TestConfiguration::SetPushBufferLocation
(
    Memory::Location loc
)
{
    m_PushBufferLocation = loc;
}

//-----------------------------------------------------------------------------
void TestConfiguration::SetMemoryType
(
    Memory::Location loc
)
{
    m_MemoryType = loc;
}

RC TestConfiguration::AllocateChannel(Channel **ppChan, UINT32 *phChan)
{
    Printf(Tee::PriHigh,
           "Error:  Cannot AllocateChannel in a generic TestConfiguration!\n");
    MASSERT(false);
    return RC::SOFTWARE_ERROR;
}

RC TestConfiguration::AllocateErrorNotifier(Surface2D *pErrorNotifier, UINT32 hVASpace)
{
    Printf(Tee::PriHigh,
           "Error: Cannot AllocateErrorNotifier in a generic TestConfiguration!\n");
    MASSERT(false);
    return RC::SOFTWARE_ERROR;
}

RC TestConfiguration::FreeChannel(Channel *pChan)
{
    Printf(Tee::PriHigh, "Error:  Cannot FreeChannel in a generic TestConfiguration!\n");
    MASSERT(false);
    return RC::SOFTWARE_ERROR;
}

RC TestConfiguration::IdleAllChannels()
{
    Printf(Tee::PriHigh,
           "Error: Cannot IdleAllChannels in a generic TestConfiguration!\n");
    MASSERT(false);
    return RC::SOFTWARE_ERROR;
}

void TestConfiguration::SetAllowMultipleChannels (bool DoAllow)
{
    m_AllowMultipleChannels = DoAllow;
}

//------------------------------------------------------------------------------
// ACCESSORS
//
bool TestConfiguration::AllowMultipleChannels() const
{
    return m_AllowMultipleChannels;
}
UINT32 TestConfiguration::StartLoop() const
{
   return m_StartLoop;
}
UINT32 TestConfiguration::RestartSkipCount() const
{
   if (m_RestartSkipCount < 1)
      return 1;

   return m_RestartSkipCount;
}
UINT32 TestConfiguration::Loops() const
{
   return m_Loops;
}
UINT32 TestConfiguration::Seed() const
{
   return m_Seed;
}
FLOAT64 TestConfiguration::TimeoutMs() const
{
   return m_TimeoutMs;
}
UINT32 TestConfiguration::DisplayWidth() const
{
   return m_Width;
}
UINT32 TestConfiguration::DisplayHeight() const
{
   return m_Height;
}
UINT32 TestConfiguration::DisplayDepth() const
{
   return m_Depth;
}
UINT32 TestConfiguration::RefreshRate() const
{
   return m_RefreshRate;
}
FSAAMode TestConfiguration::GetFSAAMode() const
{
   return m_FSAAMode;
}
UINT32 TestConfiguration::ZDepth() const
{
   return m_ZDepth;
}
UINT32 TestConfiguration::SurfaceWidth() const
{
   return m_SurfaceWidth;
}
UINT32 TestConfiguration::SurfaceHeight() const
{
   return m_SurfaceHeight;
}
TestConfiguration::ChannelType TestConfiguration::GetChannelType() const
{
   return m_ChannelType;
}
UINT32 TestConfiguration::ChannelSize() const
{
   return m_ChannelSize;
}
Memory::Location TestConfiguration::PushBufferLocation() const
{
   return m_PushBufferLocation;
}
Memory::Location TestConfiguration::DstLocation() const
{
   return m_DstLocation;
}
Memory::Location TestConfiguration::SrcLocation() const
{
   return m_SrcLocation;
}
Memory::Location TestConfiguration::MemoryType() const
{
   return m_MemoryType;
}
bool TestConfiguration::UseIndMem() const
{
   return m_UseIndMem;
}
bool TestConfiguration::UseTiledSurface() const
{
    return (0 != m_UseTiledSurface);
}
bool TestConfiguration::DisableCrt() const
{
    return (0 != m_DisableCrt);
}
UINT32 TestConfiguration::HandleBase() const
{
    return m_HandleBase;
}
UINT32 TestConfiguration::ClkChangeThreshold() const
{
    return m_ClkChangeThreshold;
}
TestConfiguration::TestConfigurationType TestConfiguration::GetTestConfigurationType()
{
    return TestConfiguration::TC_Generic;
}

//------------------------------------------------------------------------------
// MANIPULATORS
//
void TestConfiguration::SetChannelSize(UINT32 Size)
{
   m_ChannelSize = Size;
}

#ifdef INCLUDE_MDIAG
void TestConfiguration::SetTimeoutMs(FLOAT64 timeoutMs)
{
    m_TimeoutMs = timeoutMs;
}
#endif

//------------------------------------------------------------------------------
// Protected helpers
//
void TestConfiguration::Reset()
{
    m_AllowMultipleChannels = false;

    m_StartLoop          = 0;
    m_RestartSkipCount   = 100;
    m_Loops              = 1000;
    m_Seed               = 0x12345678;
    m_TimeoutMs          = 1000;

    m_Width              = 1024;
    m_Height             =  768;
    m_Depth              =  32;
    m_RefreshRate        =  60;

    m_FSAAMode           = FSAADisabled;

    m_ZDepth             = m_Depth;

    m_SurfaceWidth       = 1024;
    m_SurfaceHeight      =  768;

    m_ChannelType        = UseNewestAvailable;
    m_ChannelSize        = 1024 * 1024 ;

    m_PushBufferLocation = Memory::Coherent;
    m_DstLocation        = Memory::Optimal;
    m_SrcLocation        = Memory::Optimal;
    m_MemoryType         = Memory::Coherent;
    m_UseIndMem          = false;

    m_UseTiledSurface    = true;
    m_DisableCrt         = false;
    m_ClkChangeThreshold = 0;

    m_PexLineErrorsAllowed = 0;
    m_PexCRCErrorsAllowed = 0;
    m_PexNAKsRcvdAllowed  = 0;
    m_PexNAKsSentAllowed  = 0;
    m_PexFailedL0sExitsAllowed = 0;
    m_EarlyExitOnErrCount = false;
    m_Verbose = false;
    m_ShortenTestForSim = false;

    m_UnusedProperties.clear();
}

RC TestConfiguration::ValidateProperties()
{
    switch (m_Depth)
    {
        case 15:
        case 16:
        case 24:
        case 32:
            break;
        default:
            Printf(Tee::PriError, "DisplayDepth must be 15, 16, 24, or 32.\n");
            return RC::UNSUPPORTED_DEPTH;
    }

    switch (m_MemoryType)
    {
        case Memory::NonCoherent:
        case Memory::Coherent:
        case Memory::Fb:
        case Memory::Optimal:
            break;
        default:
            Printf(Tee::PriError, "MemoryType must be .NonCoherent, .Coherent, "
                   ".Optimal, or .Fb.\n");
            return RC::BAD_MEMLOC;
    }

    if (m_ChannelType >= NUM_CHANNEL_TYPES)
    {
        Printf(Tee::PriError, "ChannelType must be Pio, Dma, TegraSW, CheetAh, GpFifo, "
               "or UseNewestAvailable.\n");
        return RC::TEST_CONFIGURATION_ILWALID_CHANNEL_TYPE;
    }

    if (m_FSAAMode >= FSAA_NUM_MODES)
    {
        Printf(Tee::PriError, "FSAAMode must be 0 (disabled) to %d.\n",
               FSAA_NUM_MODES-1);
        return RC::UNSUPPORTED_FSAA_MODE;
    }

    if (!m_UnusedProperties.empty())
    {
        for (const auto& prop : m_UnusedProperties)
        {
            Printf(Tee::PriError,
                   "Unrecognized TestConfiguration property: %s\n", prop.c_str());
        }
        return RC::ILWALID_ARGUMENT;
    }

    return OK;
}

const char * TestConfiguration::ChannelTypeString(TestConfiguration::ChannelType ct) const
{
    const char * channelTypeNames[NUM_CHANNEL_TYPES] =
    {
        "GpFifoChannel",
        "TegraTHIChannel",
        "UseNewestAvailable"
    };

    if (ct < NUM_CHANNEL_TYPES)
        return channelTypeNames[ct];
    else
        return "?";
}

void TestConfiguration::SetPexThresholdFromJs
(
    JSObject *pValues,
    string JsPropStr,
    vector<PexThreshold> *pThresh
)
{
    JavaScriptPtr pJs;
    JsArray JsArrayPexAllowed;
    if (OK == pJs->GetProperty(pValues, JsPropStr.c_str(), &JsArrayPexAllowed))
    {
        pThresh->clear();
        for (UINT32 i = 0; i < JsArrayPexAllowed.size(); i++ )
        {
            JsArray JsArrayPexAllowedPerLink;
            if ( OK == pJs->FromJsval(JsArrayPexAllowed[i], &JsArrayPexAllowedPerLink))
            {
                PexThreshold AllowedError = {0};
                UINT32 tmpI;

                if ( OK == pJs->FromJsval(JsArrayPexAllowedPerLink[Pci::LOCAL_NODE], &tmpI))
                {
                    AllowedError.LocError = tmpI;
                }
                if ( OK == pJs->FromJsval(JsArrayPexAllowedPerLink[Pci::HOST_NODE], &tmpI))
                {
                    AllowedError.HostError = tmpI;
                }
                pThresh->push_back(AllowedError);
            }
        }
    }
}

RC TestConfiguration::GetProperties()
{
    //
    // Get the global Test control properties.
    //
    return GetPropertiesHelper(TestConfiguration_Object.GetJSObject(), Required);
}
RC TestConfiguration::GetPerTestOverrides(JSObject *valuesObj)
{
    //
    // Get the per-test control properties.
    //
    return GetPropertiesHelper(valuesObj, Optional);
}

RC TestConfiguration::GetPropertiesHelper(JSObject * pObj, IsOptional isOpt)
{
    RC rc;
    JavaScript * pJs = JavaScriptPtr().Instance();

    vector<string> objectProperties;
    pJs->GetProperties(pObj, &objectProperties);
    m_UnusedProperties.insert(objectProperties.begin(), objectProperties.end());

    // gcc 2.9 nullptr-compat broken here, member-function-pointer.
    RC (JavaScript::*pfnUnpack)(JSObject *, const char*, ...) = 0; // nullptr
    if (isOpt == Optional)
        pfnUnpack = &JavaScript::UnpackFieldsOptional;
    else
        pfnUnpack = &JavaScript::UnpackFields;

    CHECK_RC((pJs->*pfnUnpack)(pObj, "IIIId",
                               UseProperty("StartLoop"), &m_StartLoop,
                               UseProperty("RestartSkipCount"), &m_RestartSkipCount,
                               UseProperty("Loops"), &m_Loops,
                               UseProperty("Seed"), &m_Seed,
                               UseProperty("TimeoutMs"), &m_TimeoutMs));

    CHECK_RC((pJs->*pfnUnpack)(pObj, "IIIIIIII",
                               UseProperty("DisplayWidth"), &m_Width,
                               UseProperty("DisplayHeight"), &m_Height,
                               UseProperty("DisplayDepth"), &m_Depth,
                               UseProperty("DisplayRefreshRate"), &m_RefreshRate,
                               UseProperty("ZDepth"), &m_ZDepth,
                               UseProperty("FSAAMode"), &m_FSAAMode,
                               UseProperty("SurfaceWidth"), &m_SurfaceWidth,
                               UseProperty("SurfaceHeight"), &m_SurfaceHeight));

    UINT32 pbloc = PushBufferLocation();
    CHECK_RC((pJs->*pfnUnpack)(pObj, "III",
                               UseProperty("ChannelType"), &m_ChannelType,
                               UseProperty("ChannelSize"), &m_ChannelSize,
                               UseProperty("PushBufferLocation"), &pbloc));
    SetPushBufferLocation((Memory::Location)pbloc);

    UINT32 mtype = MemoryType();
    CHECK_RC((pJs->*pfnUnpack)(pObj, "III",
                               UseProperty("DstLocation"), &m_DstLocation,
                               UseProperty("SrcLocation"), &m_SrcLocation,
                               UseProperty("MemoryType"), &mtype));
    SetMemoryType((Memory::Location)mtype);

    CHECK_RC((pJs->*pfnUnpack)(pObj, "??????",
                               UseProperty("UseIndMem"), &m_UseIndMem,
                               UseProperty("UseTiledSurface"), &m_UseTiledSurface,
                               UseProperty("DisableCrt"), &m_DisableCrt,
                               UseProperty("EarlyExitOnErrCount"), &m_EarlyExitOnErrCount,
                               UseProperty("Verbose"), &m_Verbose,
                               UseProperty("ShortenTestForSim"), &m_ShortenTestForSim));

    if (isOpt == Optional)
    {
        // These are not present as globals, only per-test.  Always optional.
        CHECK_RC(pJs->UnpackFieldsOptional(
                     pObj, "?I",
                     UseProperty("AllowMultipleChannels"), &m_AllowMultipleChannels,
                     UseProperty("ClkChangeThreshold"), &m_ClkChangeThreshold));

        SetPexThresholdFromJs(pObj, UseProperty("PexCorrErrsAllowed"), &m_PexCorrErrsAllowed);
        SetPexThresholdFromJs(pObj, UseProperty("PexUnSuppReqAllowed"), &m_PexUnSuppReqAllowed);
        SetPexThresholdFromJs(pObj, UseProperty("PexNonFatalErrsAllowed"), &m_PexNonFatalErrsAllowed);

        CHECK_RC(pJs->UnpackFieldsOptional(
                     pObj, "IIIII",
                     UseProperty("PexLineErrorsAllowed"), &m_PexLineErrorsAllowed,
                     UseProperty("PexCRCErrorsAllowed"), &m_PexCRCErrorsAllowed,
                     UseProperty("PexNAKsRcvdAllowed"), &m_PexNAKsRcvdAllowed,
                     UseProperty("PexNAKsSentAllowed"), &m_PexNAKsSentAllowed,
                     UseProperty("PexFailedL0sExitsAllowed"), &m_PexFailedL0sExitsAllowed));
    }

    return rc;
}

const char* TestConfiguration::UseProperty(const char* property)
{
    set<string>::iterator iter = m_UnusedProperties.find(property);
    if (iter != m_UnusedProperties.end())
        m_UnusedProperties.erase(iter);
    return property;
}
