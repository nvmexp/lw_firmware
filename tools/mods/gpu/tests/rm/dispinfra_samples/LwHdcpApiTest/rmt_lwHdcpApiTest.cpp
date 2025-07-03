//!
//!
//! \file rmt_hdcpSampleTest.cpp
//! \brief
//!This test  verifies a sequence(mentioned below) for downstream HDCP and supports the functionalities of LwHdcpApi for upstream HDCP.
//!Sequence :
//!    Setmodeset
//!    Enable Hdcp
//!    Check whether its enabled or not
//!    Setmodeset with different resolution
//!    Check whether Hdcp Enabled or not
//!    Apply scaling
//!    Check if HDCP is enabled or not
//!    Disable HDCP
//!    Check if HDCP is still disabled
//!    Remove scaling
//!    Enable HDCP
//!    Check whether Hdcp is enabled or not
//!    Monito off and on Xp style
//!    Check whether HDCP enabled or not -should be disabled
//!    Monitor off and On vista style
//!    Check whether HDCP is disabled or not -should be disabled
//!    Enable HDCP
//!     Wait for some time
//!    Check HDCP enabled or not.

//!LwHdcpApi functionalities :
//!    DisplayInfo
//!    QueryHeadConfig
//!    Renegotiate
//!    ReadLinkStatus
//!    ValidateLink is not supported  because we are not taking into account repeaters for now. (It basically callwlate V prime)

//!Certain flags that are being used -
//!1.    Upstream - if you just want the upstream functionalities.
//!2.    Downstream - if you just want the upstream functionalities.
//!3.    protocol  - The display out of the various other connected displays, supporting this particular protocol will be taken into consideration  for all the functionalities. If the protocol parameter isn't specified, then all the connected displays will be tested.

//!The following flags if enabled will work only if the Upstream is enabled.
//!1.    displayInfo -  Will give the display info in case the upstream flag is set.
//!2.    ReadLink - Will give the ReadLinkStatus and verify Kp and Kp'
//!3.    QueryHeadConfig - test passes if the RM control command with the desired command for querying head returns OK.
//!4.    Renegotiate - test passes if the RM control command with the desired command for renegotiation  returns OK.

//!If none of the flags is specified then both upstream and downstream will run completely.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/display.h"
#include "gpu/display/evo_disp.h"
#include "gpu/display/evo_dp.h"
#include "gpu/display/evo_chns.h"
#include "ctrl/ctrl0073/ctrl0073specific.h"
#include "random.h"
#include "gpu/tests/gputest.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "core/include/utility.h"
#include "class/cl0073.h"
#include "ctrl/ctrl0073.h"
#include "ctrl/ctrl2080.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "core/include/rc.h"
#include "gpu/tests/rm/utility/hdcputils.h"
#include "core/include/memcheck.h"
#define HBR2 540000000
#define HBR  270000000
#define CHECK_FOR_HDCP_ENABLE  true
#define CHECK_FOR_HDCP_DISABLE false

class LwHdcpApiTest : public RmTest
{
public:
    LwHdcpApiTest();
    virtual ~LwHdcpApiTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    RC RunUpstream(DisplayIDs);
    RC RunDownstream(DisplayIDs);
    RC SetupChannelImageAndMode(DisplayID, DisplayPort::Group *, Surface2D, Surface2D *, DTIUtils::ImageUtils, UINT32, UINT32, UINT32, UINT32, UINT32, bool);
    RC PrintInfoForLwrrentDisplay(UINT32, UINT32, UINT32, DisplayIDs, bool, LW0073_CTRL_SPECIFIC_HDCP_CTRL_PARAMS *);
    RC VerifyHDCPEnable(DisplayID Display);
    RC CheckDispUnderflow();
    RC VerifyHdcpStatus(DisplayID Display, bool verifyOp);
    RC IsHdcpCapable(DisplayID Display, bool *pbIsGpuHdcpCapable, bool *pbIsHdcp1xCapable, bool *pbIsHdcp22Capable);
    bool combinationGenerator(vector<UINT32>& validCombi, UINT32 trialCombi, UINT32 numOfHeads, UINT32 numOfDisps);
    SETGET_PROP(QueryHeadConfig,  bool);
    SETGET_PROP(Renegotiate,  bool);
    SETGET_PROP(ReadLink,  bool);
    SETGET_PROP(displayInfo, bool);
    SETGET_PROP(Upstream,  bool);
    SETGET_PROP(Downstream,  bool);
    SETGET_PROP(runCombiTest,  bool);
    SETGET_PROP(protocol, string);

private:
    UINT32          m_MaxKpToTest;
    DisplayIDs      m_allConnectedDisplays;
    Display*        m_pDisplay;
    DisplayID       m_DisplayToTest;
    bool            m_QueryHeadConfig;
    bool            m_Renegotiate;
    bool            m_ReadLink;
    bool            m_displayInfo;
    bool            m_Upstream;
    bool            m_Downstream;
    bool            m_runCombiTest;
    string          m_protocol;
    LwU64           m_lwrrentCksv;
    LwU64           m_lwrrentCkeyChecksum;
    LwU64           *m_pLwrrentCkeys;
    DPDisplay       *m_dpDisplay;
    DisplayIDs      m_monitorIds;
};

#define BitM64( arg )   (((LwU64)1) << (arg))
#define s_lfsr0 A1_n
#define s_lfsr1 B1_n
#define s_lfsr2 A2_n
#define s_lfsr3 B2_n
#define s_A a1_n
#define s_B a2_n
static LwU8 s_lfsr0[23];
static LwU8 s_lfsr1[24];
static LwU8 s_lfsr2[26];
static LwU8 s_lfsr3[27];
static LwU8 s_A[4];
static LwU8 s_B[4];

static const LwU64 MASK40 = 0xFFFFFFFFFFLL;

static const LwU64 MASK56 = 0xFFFFFFFFFFFFFFLL;

static const LwU64 MSB56  = 0x80000000000000LL;

static UINT32 sleepVal = 0;

// Are we using real keys or test keys?
static bool g_bIsUsingRealKeys = false;

// Keys used in silicon
static LwU64 s_Cksv1 = 0x837aa5b62aLL;
static LwU64 s_Ckey1_DecryptedChecksum = 0x776bf2cc0cd602LL;
static LwU64 s_Ckey1[40] = {
    0x9bec4aee824fdd15ULL,
    0x80cb2537011939f4ULL,
    0x26e1da8139f290c8ULL,
    0x8d475fda2e278b6eULL,
    0xba65885e32f090ffULL,
    0xf89cbb8985658aelwLL,
    0x03bed022fc800fb8ULL,
    0x35911db3b598241aULL,
    0x7a945516e87445a5ULL,
    0x1b803a98796eb1d8ULL,
    0x3f329dd53834bb83ULL,
    0x97c79189d79a5acaULL,
    0x9ae3bc5154a9fd8eULL,
    0x7d401505a1d945b2ULL,
    0xe8b6105a0e4dd80lwLL,
    0x16707cc092713789ULL,
    0x3c990a69e2edaeb8ULL,
    0x0ae65370f3df1693ULL,
    0x44712f106a6d979dULL,
    0x493de4589c9f127aULL,
    0x17ea60a051c0d7b7ULL,
    0x709446801c1445b2ULL,
    0xe083ae630fd20b94ULL,
    0x4d12a989ca2d2ba0ULL,
    0x75b8391a3348cb9fULL,
    0xb51438b48a271d76ULL,
    0xb13f30f33cb910e6ULL,
    0x117af3982e455fa3ULL,
    0xcbacc4b9658eb57aULL,
    0x2b628bc949da789dULL,
    0x5a38117ca710ec42ULL,
    0x3753268cd00ec9daULL,
    0xd8ff87daaf0929elwLL,
    0xf920a655018d111eULL,
    0xdaba7ddb78099a22ULL,
    0x1b0e38a12382f451ULL,
    0x8459c332f7c2bd6bULL,
    0x0def7288bef794d6ULL,
    0x33b54ca0ba502e77ULL,
    0xbfeed27a2e62735eULL
};

// Keys primarily used in emulation and in other simulation platforms
static LwU64 s_Cksv_Emu = 0xb86646fc8cLL;
static LwU64 s_Ckey_Emu_DecryptedChecksum = 0x78efb77dde0f94LL;
static LwU64 s_Ckey_Emu[40] = {
    0xbf9323eba7bd83ULL,
    0x9f4618fb1fddcdULL,
    0xaf9af9f70a4016ULL,
    0xa6486e54e184e5ULL,
    0x9ae6c3b7ab7997ULL,
    0x3fac11e60feab9ULL,
    0x0fc296be6169fdULL,
    0xa60a6b7046bdc7ULL,
    0xd97ab64ded4213ULL,
    0x9a98ca3cb335d8ULL,
    0xaec8d340924cd2ULL,
    0x24063db7783b67ULL,
    0x74ec773c2e5306ULL,
    0xe67ab5293cdb6bULL,
    0x44634e169974eeULL,
    0x675d2f23a50021ULL,
    0x840e353bbe276bULL,
    0xfaf3ea7f7e6fe5ULL,
    0xa66c5ef761f745ULL,
    0x3281b7d3fed3a6ULL,
    0x0c18cf5d267ad9ULL,
    0x3c4bfb02724239ULL,
    0x1e59b589ed36a5ULL,
    0xefd5979c3c8967ULL,
    0x8c8cb0b2c56787ULL,
    0x3271dbf201eee8ULL,
    0x638ad2b0a05aeaULL,
    0xba9d886613d86bULL,
    0xaad73506a50c09ULL,
    0x30b80cc15a98bbULL,
    0x6059cf02f46ca1ULL,
    0x091bfd1f2c67c0ULL,
    0x6ee12b81562882ULL,
    0x7ebeea3a8c94eeULL,
    0x4a7edad3ce070aULL,
    0x90fca1ed41f1ddULL,
    0x2bcb7d6b167472ULL,
    0x91581d525f5130ULL,
    0xcf6531384fe395ULL,
    0xbf745d37be3daeULL,
};

enum LfsrInitMode
{
    NoInit,
    InitA,
    InitB
};

static RC CheckCKeyChecksum(const LwU64 * Keys, LwU64 expectedChecksum);

static LwU64 CalcKp
(
    LwU64 Cn,
    LwU64 Cs,
    LwU64 Dksv,
    LwU64 An,
    LwU64 Bksv,
    LwU64 Status,
    const LwU64* Ckey
);

static void SumKu
(
    LwU64 *pKu,
    LwU64 Ksv,
    const LwU64* Keys
);

static void LFSR0
(
    LwU8* lfsr0_output,
    LwU64 data,
    LwU64 key,
    LfsrInitMode init
);

static void LFSR1
(
    LwU8*  lfsr1_output,
    LwU64 data,
    LwU64 key,
    LfsrInitMode init
);

static void LFSR2
(
    LwU8* lfsr2_output,
    LwU64 data,
    LwU64 key,
    LfsrInitMode init
);

static void LFSR3
(
    LwU8*  lfsr3_output,
    LwU64 data,
    LwU64 key,
    LfsrInitMode init
);

static LwU8  OneWay
(
    LwU64 data,
    LwU64 key,
    LfsrInitMode mode
);

static LwU8 Combiner
(
    LwU8* lfsr0_output,
    LwU8* lfsr1_output,
    LwU8* lfsr2_output,
    LwU8* lfsr3_output,
    LfsrInitMode init
);

static LwU8 Shuffle
(
    LwU8 din,
    LwU8 s,
    LfsrInitMode init,
    int i
);

//! \brief LwHdcpApiTest constructor
//!
//! Initalize private member variables to default values.
//!
//! \sa Setup
//------------------------------------------------------------------------------
LwHdcpApiTest::LwHdcpApiTest()
{
    SetName("LwHdcpApiTest");
    m_pDisplay = GetDisplay();
    m_DisplayToTest     = 0;
    m_MaxKpToTest       = 2;
    m_QueryHeadConfig   = false;
    m_Renegotiate       = false;
    m_Upstream          = false;
    m_Downstream        = false;
    m_ReadLink          = false;
    m_displayInfo       = false;
    m_runCombiTest      = false;
    m_protocol          = "";//"CRT,TV,LVDS_LWSTOM,SINGLE_TMDS_A,SINGLE_TMDS_B,DUAL_TMDS,DP_A,DP_B,EXT_TMDS_ENC";
    m_pLwrrentCkeys       = s_Ckey1;
    m_lwrrentCksv         = s_Cksv1;
    m_lwrrentCkeyChecksum = s_Ckey1_DecryptedChecksum;
    g_bIsUsingRealKeys    = true;
}

//! \brief LwHdcpApiTest destructor
//!
//! Doesn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
LwHdcpApiTest::~LwHdcpApiTest()
{

}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string LwHdcpApiTest::IsTestSupported()
{
    RC rc;
    string Protocol = "";

    DisplayIDs tempAllConnectedDisplays;
    // Lets first find out if any of the connected displays
    // support HDCP and also if the GPU supports HDCP

    // remove LVDS_LWSTOM from protocol list
    if((m_protocol.find("LVDS_LWSTOM",0)) != string::npos)
    {
        Printf(Tee::PriHigh, "Removing LVDS from protocol list as HDCP do not support LVDS");

        m_protocol.erase(m_protocol.find("LVDS_LWSTOM",0),strlen("LVDS_LWSTOM"));
    }

    m_pDisplay = GetDisplay();

    // Get all connected displays
    CHECK_RC_CLEANUP(m_pDisplay->GetConnectors(&tempAllConnectedDisplays));
    m_pDisplay->PrintDisplayIds(Tee::PriHigh, tempAllConnectedDisplays);
    tempAllConnectedDisplays.clear();

    CHECK_RC_CLEANUP(m_pDisplay->GetDetected(&tempAllConnectedDisplays));

    if (tempAllConnectedDisplays.size() == 0)
    {
        return "Strange.., no displays detected";
    }
    // Print DisplayIds
    m_pDisplay->PrintDisplayIds(Tee::PriHigh, tempAllConnectedDisplays);

    for (UINT32 loopCount = 0; loopCount < tempAllConnectedDisplays.size(); loopCount++)
    {
        bool            isHdcp1xCapable = false;
        bool            isGpuHdcpCapable = false;

        CHECK_RC_CLEANUP(IsHdcpCapable(tempAllConnectedDisplays[loopCount],
                             &isGpuHdcpCapable, &isHdcp1xCapable, NULL));

        // if GPU itself is not capable, no need to go further
        if (!isGpuHdcpCapable)
        {
            Printf(Tee::PriHigh, "Detected device displayId = 0x%08X is not capable of HDCP \n",
                    (UINT32)tempAllConnectedDisplays[loopCount]);
            continue;
        }

        // if this display is capable, save it and break from here. We got at least
        // one display which is HDCP capable, we'll run our tests on this display.
        if (!isHdcp1xCapable)
        {
            Printf(Tee::PriHigh, "Detected device displayId = 0x%08X is not HDCP1.x Capable \n",
                    (UINT32)tempAllConnectedDisplays[loopCount]);
            continue;
        }

        // Removing display that support LVDS
        CHECK_RC_CLEANUP(m_pDisplay->GetProtocolForDisplayId(tempAllConnectedDisplays[loopCount],Protocol));
        if(!Protocol.compare("LVDS_LWTOM"))
        {
            Printf(Tee::PriHigh, "Removing display = 0x%08X as HDCP do not support LVDS_LWSTOM protocol\n",
                    (UINT32)tempAllConnectedDisplays[loopCount]);
            continue;
        }

        // Removing display that support EDP
        CHECK_RC_CLEANUP(m_pDisplay->GetProtocolForDisplayId(tempAllConnectedDisplays[loopCount],Protocol));
        if(!Protocol.compare("EDP"))
        {
            Printf(Tee::PriHigh, "Removing EDP display = 0x%08X as  do not support LVDS_LWSTOM protocol\n",
                    (UINT32)tempAllConnectedDisplays[loopCount]);
            continue;
        }

        // if given protocol is supported on this display
        if (!DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, tempAllConnectedDisplays[loopCount], m_protocol ))
        {
            Printf(Tee::PriHigh, "Device displayId = 0x%08X  does not support protocol \n ",
                    (UINT32)tempAllConnectedDisplays[loopCount]);
            continue;
        }

         m_allConnectedDisplays.push_back(tempAllConnectedDisplays[loopCount]);
    }

    tempAllConnectedDisplays.clear();

    // if we couldn't find any HDCP capable displays, print msg and return false
    if ( m_allConnectedDisplays.size() == 0)
    {
        return "No displays found which are capable of HDCP";
    }

    return RUN_RMTEST_TRUE;

Cleanup:
    // We failed somewhere, print the RC we got and return false.
    Printf(Tee::PriHigh, "Function %s failed, rc = %s", __FUNCTION__, rc.Message());
    return "See above rc message";
}

//! \brief Just doing Init from Js, nothing else.
//!
//! \return if InitFromJs passed, else corresponding rc
//------------------------------------------------------------------------------
RC LwHdcpApiTest::Setup()
{
    RC                                          rc;
    LwRmPtr                                     pLwRm;
    LW2080_CTRL_GPU_GET_SIMULATION_INFO_PARAMS  simulationInfoParams = {0};

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_pDisplay = GetDisplay();

    CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
        LW2080_CTRL_CMD_GPU_GET_SIMULATION_INFO,
        &simulationInfoParams,
        sizeof(simulationInfoParams)));

    if (simulationInfoParams.type != LW2080_CTRL_GPU_GET_SIMULATION_INFO_TYPE_NONE)
    {
        // Use test keys for all simulation modes
        m_pLwrrentCkeys        = s_Ckey_Emu;
        m_lwrrentCksv          = s_Cksv_Emu;
        m_lwrrentCkeyChecksum  = s_Ckey_Emu_DecryptedChecksum;
        g_bIsUsingRealKeys     = false;
        sleepVal               = 0x20000;
    }
    else
    {
        // Use real keys for silicon
        m_pLwrrentCkeys        = s_Ckey1;
        m_lwrrentCksv          = s_Cksv1;
        m_lwrrentCkeyChecksum  = s_Ckey1_DecryptedChecksum;
        g_bIsUsingRealKeys     = true;
        sleepVal               = 7000;
    }

    return rc;
}

//! \brief Run the test
//!
//! Run upstream and downstrean HDCP
//!
//! \return OK if the HDCP APIs succeeded, else corresponding rc
//------------------------------------------------------------------------------
RC LwHdcpApiTest::Run()
{
    RC rc;
    UINT32 numHeads;
    UINT32 numDisps;
    DisplayIDs    ActiveDisplays;

    CHECK_RC(ErrorLogger::StartingTest());
    ErrorLogger::IgnoreErrorsForThisTest();

    CHECK_RC(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    m_dpDisplay = new DPDisplay(GetDisplay());
    CHECK_RC(m_dpDisplay->getDisplay()->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    // Enable the DP library usage
    m_dpDisplay->getDisplay()->SetUseDPLibrary(true);

    //
    // Set correct mode in evoDisp as this is required for displayID allocation.
    // in MST mode we do dynamic display ID while in SST mode we use connectorID
    // Here "0" is SST Mode & "1"  is MST Mode.
    //
    m_dpDisplay->setDpMode(SST);

    //get the no of displays to work with
    numDisps = static_cast<UINT32>(m_allConnectedDisplays.size());

    if(m_runCombiTest && numDisps > 1 )
    {

        // get the head count, thats the highest no of displays we can run at once
        // consider Head Floor Sweeping
        numHeads = Utility::CountBits(m_pDisplay->GetUsableHeadsMask());

        UINT32 allCombinations = static_cast<UINT32>(1<<(numDisps));

        vector<UINT32> validCombi;

        // initialize
        validCombi.resize(numDisps);

        for (UINT32 trialcombi = 3; trialcombi < allCombinations; trialcombi++)
        {
            // if its a valid combination the try the one
            if (!combinationGenerator(validCombi, trialcombi, numHeads, numDisps))
                    continue;

            //Detach all the selected display to get all the heads free for this new combination
            ActiveDisplays.clear();
            m_pDisplay->Selected(&ActiveDisplays);
            if(ActiveDisplays.size() >= 1)
                CHECK_RC(m_pDisplay->DetachDisplay(ActiveDisplays));

            for (UINT32 disp = 0; disp < numDisps; disp ++)
            {
                if(!validCombi[disp])
                    continue;
                Printf(Tee::PriHigh,"   0x%08x    ", (UINT32)m_allConnectedDisplays[disp]);
            }

            for (UINT32 disp = 0; disp < numDisps; disp++)
            {
                if (!validCombi[disp])
                    continue;

                if (m_Downstream)
                {
                    CHECK_RC(RunDownstream(DisplayIDs(1, m_allConnectedDisplays[disp])));
                }
                if (m_Upstream)
                {
                    CHECK_RC(RunUpstream(DisplayIDs(1, m_allConnectedDisplays[disp])));
                }
            }
        }

    }
    else
    {

        for(UINT32 i = 0 ; i <  m_allConnectedDisplays.size() ; i++ )
        {
            if (m_Downstream)
            {
                CHECK_RC(RunDownstream(DisplayIDs(1, m_allConnectedDisplays[i])));
            }
            if (m_Upstream)
            {
                CHECK_RC(RunUpstream(DisplayIDs(1, m_allConnectedDisplays[i])));
            }

            m_dpDisplay->FreeDPConnectorDisplays(m_allConnectedDisplays[i]);
        }
    }

    ErrorLogger::TestCompleted();

    return rc;
}

void GetOptimalResolution(string Protocol, UINT32 *pOutWidth, UINT32 *pOutHeight, UINT32 *pInWidth, UINT32 *pInHeight)
{
    if(!Protocol.compare("DUAL_TMDS"))
    {
        *pOutWidth  = 2560;
        *pOutHeight = 1600;
        *pInWidth   = 2560;
        *pInHeight  = 1600;
    }
    else
    {
        *pOutWidth     = 1024;//For dual DP resolution
        *pOutHeight    = 768; //1024;
        *pInWidth      = 800;
        *pInHeight     = 600;
    }
    return;
}

//! \brief Run the test Downstream
//!
//! Follows this sequence -
//!    Setmodeset
//!    Enable Hdcp
//!    Check whether its enabled or not
//!    Setmodeset with different resolution
//!    Check HDCP enabled or not.
//!    Apply scaling
//!    Check if HDCP is enabled or not
//!    Disable HDCP
//!    Check if HDCP is still enabled
//!    Remove scaling
//!    Enable HDCP
//!    Check whether Hdcp is enabled or not
//!    Monitor off and on Xp style
//!    Check whether HDCP enabled or not -should be disabled
//!    Monitor off and On vista style
//!    Check whether HDCP is disabled or not -should be disabled
//!    Enable HDCP
//!     Wait for some time
//!    Check HDCP enabled or not.
//!
//! \return OK if the test succeeds
//------------------------------------------------------------------------------
RC LwHdcpApiTest :: RunDownstream(DisplayIDs  m_allConnectedDisplays)
{
    RC rc;

    UINT32 outWidth     = 0;
    UINT32 outHeight    = 0;
    UINT32 inWidth      = 0;
    UINT32 inHeight     = 0;
    UINT32 Depth        = 32;
    UINT32 RefreshRate  = 60;
    UINT32 lwrrCombiDispMask = 0;
    LwU32  deviceMask;
    LwU32  displaySubdevice;
    Surface2D * coreSurf = new Surface2D[m_allConnectedDisplays.size()];
    DisplayIDs tempConnectedDisplays;
    DTIUtils::ImageUtils imgArr;
    UINT32        Head = Display::ILWALID_HEAD;
    LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS dispHeadMapParams;
    Surface2D Images;
    bool isDpDisplay = false;
    string Protocol = "";
    bool isHdcp22Capable;

    m_pDisplay->DispIDToDispMask(m_allConnectedDisplays, lwrrCombiDispMask);

    DPConnector *pConnector = NULL;
    DisplayPort::Group * pDpGroup = NULL;

    for (UINT32 i = 0; i <  m_allConnectedDisplays.size(); i++)
    {
        CHECK_RC(m_pDisplay->GetProtocolForDisplayId(m_allConnectedDisplays[i],Protocol));
        GetOptimalResolution(Protocol, &outWidth, &outHeight, &inWidth, &inHeight);

        if (DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, m_allConnectedDisplays[i], "DP_A,DP_B"))
        {
            isDpDisplay = true;

            // Get all detected monitors in connector0
            CHECK_RC(m_dpDisplay->EnumAllocAndGetDPDisplays(m_allConnectedDisplays[i], &pConnector, &m_monitorIds, true));

            // Detach VBIOS screen
            pConnector->notifyDetachBegin(NULL);
            m_dpDisplay->getDisplay()->GetEvoDisplay()->ForceDetachHeads();
            pConnector->notifyDetachEnd();

            pDpGroup = pConnector->CreateGroup(m_monitorIds);
            if (pDpGroup == NULL)
            {
                Printf(Tee::PriHigh, "\n ERROR: Fail to create group\n");
                return RC::SOFTWARE_ERROR;
            }
        }

         // zero out param structs
        memset(&dispHeadMapParams, 0,
                sizeof (LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS));

        // Get Heads for this new Combination
        // Call the GetHeadRouting
        dispHeadMapParams.subDeviceInstance = 0;
        dispHeadMapParams.displayMask = lwrrCombiDispMask;
        dispHeadMapParams.oldDisplayMask = 0;
        dispHeadMapParams.oldHeadRoutingMap = 0;
        dispHeadMapParams.headRoutingMap = 0;

        CHECK_RC(DTIUtils::Mislwtils::GetHeadRoutingMask(m_pDisplay, &dispHeadMapParams));

        if ((rc = DTIUtils::Mislwtils::GetHeadFromRoutingMask(m_allConnectedDisplays[i], dispHeadMapParams, &Head)) != OK)

        {
               Printf(Tee::PriHigh, "GetHeadFromRoutingMask Failed to Get Head for display 0x%08X \n ",(UINT32)m_allConnectedDisplays[i]);
               return RC::SOFTWARE_ERROR;
        }

        // Setup Channel Image and Setimage and modeset with 800 x 600
        imgArr = DTIUtils::ImageUtils::SelectImage(outWidth,outHeight);

        CHECK_RC(SetupChannelImageAndMode(m_allConnectedDisplays[i], pDpGroup, Images, &coreSurf[i], imgArr, outWidth, outHeight, Head, RefreshRate, Depth, isDpDisplay));

        Tasker::Sleep(sleepVal);

        if (OK != rc)
        {
            if ( rc == RC::DISPLAY_NO_FREE_HEAD)
            {
                Printf(Tee::PriHigh, "In function %s, SetMode call failed DISPLAY_NO_FREE_HEAD so considering the return value as OK "
                    "width = %u, height = %u, depth = %u, RR = %u",
                    __FUNCTION__, outWidth, outHeight, Depth ,RefreshRate);

                rc = OK;
            }
            Printf(Tee::PriHigh, "In function %s, SetMode call failed with args "
                "width = %u, height = %u, depth = %u, RR = %u",
                __FUNCTION__, outWidth, outHeight, Depth ,RefreshRate);
        }
        // Check Underflow
        CHECK_RC(CheckDispUnderflow());

        deviceMask = (LwU32) m_allConnectedDisplays[i] ;

        // Enable HDCP on HDCP capable display and verify HDCP is enabled or not

        CHECK_RC(VerifyHDCPEnable(m_allConnectedDisplays[i]));

        CHECK_RC(CheckDispUnderflow());

        if(coreSurf[i].GetMemHandle() != 0)
            coreSurf[i].Free();

        for (UINT32 index = 0; index < 2; index ++)
        {
            if (Images.GetMemHandle() != 0)
            Images.Free();
        }

        imgArr = DTIUtils::ImageUtils::SelectImage(inWidth,inHeight);

        CHECK_RC(SetupChannelImageAndMode(m_allConnectedDisplays[i], pDpGroup, Images, &coreSurf[i], imgArr, outWidth, outHeight, Head, RefreshRate, Depth, isDpDisplay));

        // Wait for sometime.
        Tasker::Sleep(sleepVal);
        if (OK != rc)
        {
            if ( rc == RC::DISPLAY_NO_FREE_HEAD)
            {
                Printf(Tee::PriHigh, "In function %s, SetMode call failed DISPLAY_NO_FREE_HEAD so considering the return value as OK "
                    "width = %u, height = %u, depth = %u, RR = %u",
                    __FUNCTION__, inWidth, inHeight, Depth ,RefreshRate);

                rc = OK;
            }
            Printf(Tee::PriHigh, "In function %s, SetMode call failed with args "
                "width = %u, height = %u, depth = %u, RR = %u",
                __FUNCTION__, inWidth, inHeight, Depth ,RefreshRate);
        }
        // Check Underflow
        CHECK_RC(CheckDispUnderflow());

        // Enable HDCP on HDCP capable display and verify HDCP is enabled or not
        CHECK_RC(VerifyHDCPEnable(m_allConnectedDisplays[i]));

        // Check Underflow
        CHECK_RC(CheckDispUnderflow());

        if(coreSurf[i].GetMemHandle() != 0)
            coreSurf[i].Free();

        for (UINT32 index = 0; index < 2; index ++)
        {
            if (Images.GetMemHandle() != 0)
            Images.Free();
        }

        //  Apply scaling.
        imgArr = DTIUtils::ImageUtils::SelectImage(outWidth,outHeight);
        if(isDpDisplay)
        {
            CHECK_RC(m_dpDisplay->getDisplay()->SetupChannelImage(m_monitorIds[0],
                     outWidth,
                     outHeight,
                     32,
                     Display::CORE_CHANNEL_ID,
                     &Images,
                     imgArr.GetImageName(),
                     0,
                     Head
                     ));
        }
        else
        {
            CHECK_RC(m_pDisplay->SetupChannelImage( m_allConnectedDisplays[i], outWidth, outHeight, Depth, Display::CORE_CHANNEL_ID,
                                                    &coreSurf[i], imgArr.GetImageName(), 0, Head));
        }

        if(!isDpDisplay)
        {
            CHECK_RC(m_pDisplay->SetImage(m_allConnectedDisplays[i], &coreSurf[i], Display::CORE_CHANNEL_ID));
        }

        Tasker::Sleep(sleepVal);

        CHECK_RC(VerifyHdcpStatus(m_allConnectedDisplays[i], CHECK_FOR_HDCP_ENABLE));

        isHdcp22Capable = false;
        CHECK_RC(IsHdcpCapable(m_allConnectedDisplays[i], NULL, NULL, &isHdcp22Capable));

        if (!isHdcp22Capable)
        {
            CHECK_RC(m_pDisplay->GetDisplaySubdeviceInstance(&displaySubdevice));

            LW0073_CTRL_SPECIFIC_HDCP_CTRL_PARAMS hdcpCtrlParams = {0};
            hdcpCtrlParams.subDeviceInstance = displaySubdevice;
            hdcpCtrlParams.displayId = deviceMask;
            hdcpCtrlParams.cmd = LW0073_CTRL_SPECIFIC_HDCP_CTRL_CMD_DISABLE_AUTHENTICATION;

            CHECK_RC(m_pDisplay->RmControl(LW0073_CTRL_CMD_SPECIFIC_HDCP_CTRL,
                &hdcpCtrlParams, sizeof(hdcpCtrlParams)));

            Tasker::Sleep(sleepVal);

            CHECK_RC(VerifyHdcpStatus(m_allConnectedDisplays[i], CHECK_FOR_HDCP_DISABLE));

            CHECK_RC(CheckDispUnderflow());
        }

        // Remove scaling
        if (isDpDisplay)
        {
            rc = m_dpDisplay->SetMode(m_allConnectedDisplays[i], m_monitorIds[0],
                                      pDpGroup,
                                      outWidth,
                                      outHeight,
                                      32,
                                      RefreshRate);
        }
        else
        {
            rc = m_pDisplay-> SetMode( m_allConnectedDisplays[i], outWidth, outHeight, Depth, RefreshRate);
        }

        Tasker::Sleep(sleepVal);

        // Check RC
        if (OK != rc)
        {
            if ( rc == RC::DISPLAY_NO_FREE_HEAD)
            {
                Printf(Tee::PriHigh, "In function %s, SetMode call failed DISPLAY_NO_FREE_HEAD so considering the return value as OK "
                    "width = %u, height = %u, depth = %u, RR = %u",
                    __FUNCTION__, inWidth, inHeight, Depth ,RefreshRate);

                rc = OK;
            }
            Printf(Tee::PriHigh, "In function %s, SetMode call failed with args "
                "width = %u, hp rceight = %u, depth = %u, RR = %u",
                __FUNCTION__, inWidth, inHeight, Depth ,RefreshRate);
        }
        // Check Underflow
        CHECK_RC(CheckDispUnderflow());

        CHECK_RC(m_pDisplay->SetImage( m_allConnectedDisplays[i], NULL, Display::CORE_CHANNEL_ID));

        CHECK_RC(VerifyHDCPEnable(m_allConnectedDisplays[i]));

        // Check Underflow
        CHECK_RC(CheckDispUnderflow());

        // xp style turn monitor off
        CHECK_RC_MSG(m_pDisplay-> SetMonitorPower( m_allConnectedDisplays[i] , true), "Monitor couldn't be turned off!, error = ");

        // xp style turn monitor on
        CHECK_RC_MSG(m_pDisplay-> SetMonitorPower( m_allConnectedDisplays[i], false), "Monitor couldn't be turned on!, error = ");

        Tasker::Sleep(sleepVal);

        // Check Underflow
        CHECK_RC(CheckDispUnderflow());
        //
        // This needs to be checked against HDCP2.2 ?
        //
        // Check if HDCP is enabled or not / should be disabled.
        CHECK_RC(VerifyHdcpStatus(m_allConnectedDisplays[i], CHECK_FOR_HDCP_DISABLE));

        // Enable HDCP.
        CHECK_RC(VerifyHDCPEnable(m_allConnectedDisplays[i]));

        // Check Underflow
        CHECK_RC(CheckDispUnderflow());

        // Vista style monitor off
        tempConnectedDisplays.push_back( m_allConnectedDisplays[i]);
        if (isDpDisplay)
        {
            for (UINT32 disp = 0; disp <  tempConnectedDisplays.size(); disp++)
            {
                CHECK_RC(m_dpDisplay->DetachDisplay(tempConnectedDisplays[disp], m_monitorIds[0], pDpGroup));
            }
        }
        else
        {
            CHECK_RC(m_pDisplay->DetachDisplay(tempConnectedDisplays));
        }
        tempConnectedDisplays.clear();

        // activate it again.
        if (isDpDisplay)
        {

            rc = m_dpDisplay->SetMode(m_allConnectedDisplays[i], m_monitorIds[0],
                                      pDpGroup,
                                      inWidth,
                                      inHeight,
                                      32,
                                      RefreshRate);
        }
        else
        {
            rc = m_pDisplay-> SetMode( m_allConnectedDisplays[i], inWidth, inHeight, Depth, RefreshRate);
        }

        Tasker::Sleep(sleepVal);

        if (OK != rc)
        {
            if ( rc == RC::DISPLAY_NO_FREE_HEAD)
            {
                Printf(Tee::PriHigh, "In function %s, SetMode call failed DISPLAY_NO_FREE_HEAD so considering the return value as OK "
                    "width = %u, height = %u, depth = %u, RR = %u",
                    __FUNCTION__, inWidth, inHeight, Depth ,RefreshRate);

                rc = OK;
            }
            Printf(Tee::PriHigh, "In function %s, SetMode call failed with args "
                "width = %u, height = %u, depth = %u, RR = %u",
                __FUNCTION__, inWidth, inHeight, Depth ,RefreshRate);
        }
        // Check Underflow
        CHECK_RC(CheckDispUnderflow());
        //  Check if HDCP is enabled or not - should be disabled.
        CHECK_RC(VerifyHdcpStatus(m_allConnectedDisplays[i], CHECK_FOR_HDCP_DISABLE));

        // Enable HDCP on HDCP capable display and verify HDCP is enabled or not.
        CHECK_RC(VerifyHDCPEnable(m_allConnectedDisplays[i]));
        // Check Underflow
        CHECK_RC(CheckDispUnderflow());

        // Detach that particular display.
        if (isDpDisplay)
        {
            CHECK_RC(m_dpDisplay->DetachDisplay(m_allConnectedDisplays[i], m_monitorIds[0], pDpGroup));
        }
        else
        {
            CHECK_RC(m_pDisplay->DetachDisplay(DisplayIDs(1, m_allConnectedDisplays[i])));
        }

        // Check if HDCP is disabled or not
        CHECK_RC(VerifyHdcpStatus(m_allConnectedDisplays[i], CHECK_FOR_HDCP_DISABLE));
    }

    for (UINT32 i = 0; i <  m_allConnectedDisplays.size(); i++)
    {
        if(coreSurf[i].GetMemHandle() != 0)
            coreSurf[i].Free();
    }

    if(coreSurf)
        delete [] coreSurf;

    return rc;
}

//! \brief Run the test Upstream
//!
//! Exelwtes options DisplayInfo, QueryHeadConfig, Renegotiate and ReadLinkStatus
//! in the manner similar to LwHdcpApi application.
//!
//! \return OK if the test succeeds
//------------------------------------------------------------------------------

RC LwHdcpApiTest :: RunUpstream(DisplayIDs  m_allConnectedDisplays)
{
    RC          rc ;
    string      Protocol = "";
    UINT64      cN       = 0x2c72677f652c2f27LL;  // Arbitrary integer
    UINT32      linkCount, numAps, i, apIdx;
    UINT64      cS;
    UINT32      RefreshRate  = 60;
    UINT32      outWidth      = 0;
    UINT32      outHeight     = 0;
    UINT32      inWidth      = 0;
    UINT32      inHeight     = 0;
    UINT32      Depth        = 32;
    UINT32      lwrrCombiDispMask = 0;
    vector  <UINT64> status, aN, aKsv, bKsv, bKsvList, dKsv, kP;
    vector  <LwU16> bCaps;
    LwU32       displaySubdevice;
    DisplayIDs     tempConnectedDisplays;
    Surface2D * coreSurf = new Surface2D[m_allConnectedDisplays.size()];
    DTIUtils::ImageUtils imgArr;
    UINT32       Head = Display::ILWALID_HEAD;
    LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS dispHeadMapParams;
    bool isDpDisplay = false;
    Surface2D Images;
    bool hdcpEnabled;

    m_pDisplay->DispIDToDispMask(m_allConnectedDisplays, lwrrCombiDispMask);
    m_dpDisplay->setDpMode(SST);

    for (i=0; i < m_allConnectedDisplays.size(); i++)
    {
        DPConnector *pConnector = NULL;
        DisplayPort::Group * pDpGroup=NULL;

        CHECK_RC(m_pDisplay->GetProtocolForDisplayId(m_allConnectedDisplays[i],Protocol));
        GetOptimalResolution(Protocol, &outWidth, &outHeight, &inWidth, &inHeight);

        if (DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, m_allConnectedDisplays[i], "DP_A,DP_B"))
        {
            isDpDisplay = true;
            //get all detected monitors in connector0
            CHECK_RC(m_dpDisplay->EnumAllocAndGetDPDisplays(m_allConnectedDisplays[i], &pConnector, &m_monitorIds, true));

            //
            // Check if DualDP Mode is requested. If yes then configure dual DP mode
            // & set preferredLinkConfig to make sure linkConfig optimizations should
            // not get triggered
            //

            // Detach VBIOS screen
            pConnector->notifyDetachBegin(NULL);
            m_dpDisplay->getDisplay()->GetEvoDisplay()->ForceDetachHeads();
            pConnector->notifyDetachEnd();

            pDpGroup = pConnector->CreateGroup(m_monitorIds);
            if (pDpGroup == NULL)
            {
                Printf(Tee::PriHigh, "\n ERROR: Fail to create group\n");
                return RC::SOFTWARE_ERROR;
            }
        }

        // zero out param structs
        memset(&dispHeadMapParams, 0,
                sizeof (LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS));

        // Get Heads for this new Combination
        // Call the GetHeadRouting
        dispHeadMapParams.subDeviceInstance = 0;
        dispHeadMapParams.displayMask = lwrrCombiDispMask;
        dispHeadMapParams.oldDisplayMask = 0;
        dispHeadMapParams.oldHeadRoutingMap = 0;
        dispHeadMapParams.headRoutingMap = 0;

        CHECK_RC(DTIUtils::Mislwtils::GetHeadRoutingMask(m_pDisplay,
                                                            &dispHeadMapParams));

        if ((rc = DTIUtils::Mislwtils::GetHeadFromRoutingMask(m_allConnectedDisplays[i],
                                                                dispHeadMapParams, &Head)) != OK)
        {
               Printf(Tee::PriHigh, "GetHeadFromRoutingMask Failed to Get Head for display 0x%08X \n ",(UINT32)m_allConnectedDisplays[i]);
               return RC::SOFTWARE_ERROR;
        }

        // Setup Channel Image and Setimage and set modeset with 1023x768
        imgArr = DTIUtils::ImageUtils::SelectImage(outWidth,outHeight);

        CHECK_RC(SetupChannelImageAndMode(m_allConnectedDisplays[i], pDpGroup, Images, &coreSurf[i], imgArr, outWidth, outHeight, Head, RefreshRate, Depth, isDpDisplay));

        Tasker::Sleep(sleepVal);

        // Enable HDCP on HDCP capable display and verify HDCP is enabled or not.
        CHECK_RC(VerifyHDCPEnable(m_allConnectedDisplays[i]));
        hdcpEnabled = true;
        // Check Underflow
        CHECK_RC(CheckDispUnderflow());

        // DisplayInfo of the current display.
        if (m_displayInfo)
        {
            CHECK_RC(m_pDisplay->GetDisplaySubdeviceInstance(&displaySubdevice));

            LW0073_CTRL_SPECIFIC_HDCP_CTRL_PARAMS hdcpCtrlParams = {0};
            hdcpCtrlParams.subDeviceInstance = displaySubdevice;
            hdcpCtrlParams.displayId =  m_allConnectedDisplays[i];
            hdcpCtrlParams.cmd = LW0073_CTRL_SPECIFIC_HDCP_CTRL_CMD_VALIDATE_LINK;
            hdcpCtrlParams.flags = 0xffffffff;  // We want all available data.
            hdcpCtrlParams.cN = cN;
            hdcpCtrlParams.cKsv = m_lwrrentCksv;

            CHECK_RC(m_pDisplay->RmControl(LW0073_CTRL_CMD_SPECIFIC_HDCP_CTRL,
                &hdcpCtrlParams, sizeof(hdcpCtrlParams)));

            CHECK_RC(m_pDisplay->GetHDCPLinkParams( m_allConnectedDisplays[i], cN, m_lwrrentCksv, &linkCount, &numAps, &cS, &status, &aN, &aKsv, &bKsv, &dKsv, &bKsvList, &kP, (LwU32)LW0073_CTRL_SPECIFIC_HDCP_CTRL_CMD_READ_LINK_STATUS));

            CHECK_RC(PrintInfoForLwrrentDisplay(i, (UINT32)m_allConnectedDisplays[i], linkCount, m_allConnectedDisplays, hdcpEnabled, &hdcpCtrlParams));
        }

        // Query Head Config
        if (m_QueryHeadConfig)
        {
            Printf(Tee::PriHigh, "\n\n***** Querying Head Information for Display %d *****\n", i);
            CHECK_RC(m_pDisplay->GetHDCPLinkParams( m_allConnectedDisplays[i], cN, m_lwrrentCksv, &linkCount, &numAps, &cS, &status, &aN, &aKsv, &bKsv, &dKsv, &bKsvList, &kP, (LwU32)LW0073_CTRL_SPECIFIC_HDCP_CTRL_CMD_QUERY_HEAD_CONFIG));
            Printf(Tee::PriHigh, "     QUERY HEAD CONFIG             : PASS ");
        }

        // Renegotiate Link
        if (m_Renegotiate)
        {
            Printf(Tee::PriHigh, "\n\n***** Renegotiating Link for Display %d ***********\n", i);
            CHECK_RC(m_pDisplay->GetHDCPLinkParams( m_allConnectedDisplays[i], cN, m_lwrrentCksv, &linkCount, &numAps, &cS, &status, &aN, &aKsv, &bKsv, &dKsv, &bKsvList, &kP, (LwU32)LW0073_CTRL_SPECIFIC_HDCP_CTRL_CMD_RENEGOTIATE));
            Printf(Tee::PriHigh, "     RENEGOTIATE                   : PASS ");
        }

        Tasker::Sleep(sleepVal);

        // ReadLink Status.
        if (m_ReadLink)
        {
            Printf(Tee::PriHigh, "\n\n***** Reading Status for Display %d **************\n", i);

            //Verify if the key is ok
            CHECK_RC(CheckCKeyChecksum(m_pLwrrentCkeys, m_lwrrentCkeyChecksum));

            //Read Link Status
            CHECK_RC(m_pDisplay->GetHDCPLinkParams( m_allConnectedDisplays[i], cN, m_lwrrentCksv, &linkCount, &numAps, &cS, &status, &aN, &aKsv, &bKsv, &dKsv, &bKsvList, &kP, (LwU32)LW0073_CTRL_SPECIFIC_HDCP_CTRL_CMD_READ_LINK_STATUS));

            //Need to callwlate Kp' and check if Kp == Kp'
            for (apIdx = 0; apIdx < numAps && apIdx < m_MaxKpToTest; apIdx++)
            {
                LwU64 KP = CalcKp(cN, cS, dKsv[apIdx], aN[apIdx], bKsv[apIdx], status[apIdx], m_pLwrrentCkeys);
                LwU64 oriCs = cS;

                if (KP != kP[apIdx])
                {
                    Printf(Tee::PriHigh, "Kp %0llx and Kp' %0llx different for Display %d. Test FAILED!!\n",KP,kP[i], i);
                    if (cS==0x10)
                    {
                        Printf(Tee::PriHigh, "Changing the CS value from 0x%0llx to 0x10010\n", cS);
                        cS=0x10010;
                    }
                    else if(cS==0x10010)
                    {
                        Printf(Tee::PriHigh, "Changing the CS value from 0x%0llx to 0x10\n", cS);
                        cS=0x10;
                    }
                    else
                        Printf(Tee::PriHigh, "Not Changing the CS value\n");

                    LwU64 KP2 = CalcKp(cN, cS, dKsv[apIdx], aN[apIdx], bKsv[apIdx], status[apIdx], m_pLwrrentCkeys);
                    if (KP2 != kP[apIdx])
                    {
                        Printf(Tee::PriHigh, "Kp %0llx and Kp' %0llx different for Display %d. Test FAILED!!\n",KP2,kP[apIdx], apIdx);
                        cS = oriCs;
                        if (status[apIdx] == 0x6f4c)
                        {
                            Printf(Tee::PriHigh, "Changing the status value from 0x%0llx to 0x6f45\n", status[apIdx]);
                            status[apIdx]=0x6f45;
                        }
                        else if(status[apIdx] == 0x6f45)
                        {
                            Printf(Tee::PriHigh, "Changing the status value from 0x%0llx to 0x6f4c\n", status[apIdx]);
                            status[apIdx]=0x6f4c;
                        }
                        else
                            Printf(Tee::PriHigh, "Not Changing the status value\n");

                        LwU64 KP1 = CalcKp(cN, cS, dKsv[apIdx], aN[apIdx], bKsv[apIdx], status[apIdx], m_pLwrrentCkeys);
                        if (KP1 != kP[apIdx])
                        {
                            Printf(Tee::PriHigh, "Kp %0llx and Kp' %0llx different for Display %d. Test FAILED!!\n",KP1,kP[apIdx], apIdx);
                            return RC::SOFTWARE_ERROR;
                        }
                    }
                }
                else
                {
                    Printf(Tee::PriHigh, "\n\n===============INFO RETURNED FOR LINK %d==================\n", apIdx);
                    Printf(Tee::PriHigh, "     AN (Downstream Session ID)                    : 0x%llx\n", aN[apIdx]);
                    Printf(Tee::PriHigh, "     AKSV (Downstream Transmitter KSV)             : 0x%llx\n", aKsv[apIdx]);
                    Printf(Tee::PriHigh, "     BKSV (Downstream Receiver KSV)                : 0x%llx\n", bKsv[apIdx]);
                    Printf(Tee::PriHigh, "     DKSV (Upstream Transmitter KSV)               : 0x%llx\n", dKsv[apIdx]);
                    Printf(Tee::PriHigh, "     kP' (Hardware Signature)                      : 0x%llx\n", kP[apIdx]);
                    Printf(Tee::PriHigh, "\n     Test Get HDCP Parameters                            :     PASS\n");
                    Printf(Tee::PriHigh, "     Test All Parameters for kP callwlation Present      :     PASS\n");
                    Printf(Tee::PriHigh, "\n     CKSV (Upstream Software KSV)                 : 0x%llx\n", m_lwrrentCksv);
                    //Printf(Tee::PriHigh, "     CN (Upstream Session ID)                      : 0x%11x \n", cN);
                    //Printf(Tee::PriHigh, "     CS (Connection State)                         : 0x%llx \n", cS);
                    Printf(Tee::PriHigh, "     st (Status)                                   : 0x%llx\n", status[apIdx]);
                    Printf(Tee::PriHigh, "     kP (software Signature)                       : 0x%llx\n", KP);
                    Printf(Tee::PriHigh, "\n     Test kP signature verification                      :     PASS\n");
                }
            }
        }

        if (isDpDisplay)
        {
            CHECK_RC(m_dpDisplay->DetachDisplay(m_allConnectedDisplays[i], m_monitorIds[0], pDpGroup));
        }
        else
        {
            CHECK_RC(m_pDisplay->DetachDisplay(DisplayIDs(1, m_allConnectedDisplays[i])));
        }
    }

    for (UINT32 i = 0; i <  m_allConnectedDisplays.size(); i++)
    {
        if(coreSurf[i].GetMemHandle() != 0)
            coreSurf[i].Free();
    }

    if(coreSurf)
        delete [] coreSurf;

    return rc;
}

RC LwHdcpApiTest :: SetupChannelImageAndMode
(
    DisplayID m_connectedDisplay,
    DisplayPort::Group *pDpGroup,
    Surface2D Images,
    Surface2D *coreSurf,
    DTIUtils::ImageUtils imgArr,
    UINT32 outWidth,
    UINT32 outHeight,
    UINT32 Head,
    UINT32 RefreshRate,
    UINT32 Depth,
    bool isDpDisplay
)
{
    RC rc;

    if (isDpDisplay)
    {
        CHECK_RC(m_dpDisplay->getDisplay()->SetupChannelImage(m_monitorIds[0],
                 outWidth,
                 outHeight,
                 32,
                 Display::CORE_CHANNEL_ID,
                 &Images,
                 imgArr.GetImageName(),
                 0,
                 Head
                 ));
        CHECK_RC(m_dpDisplay->SetMode(m_connectedDisplay, m_monitorIds[0],
                 pDpGroup,
                 outWidth,
                 outHeight,
                 32,
                 RefreshRate));
    }
    else
    {
        CHECK_RC(m_pDisplay->SetupChannelImage( m_connectedDisplay, outWidth, outHeight, Depth, Display::CORE_CHANNEL_ID,
                                                coreSurf, imgArr.GetImageName(), 0, Head));
        CHECK_RC(m_pDisplay->SetMode( m_connectedDisplay, outWidth, outHeight,
                                  Depth, RefreshRate));
    }

    return rc;
}

RC LwHdcpApiTest :: PrintInfoForLwrrentDisplay
(
    UINT32 i,
    UINT32 m_connectedDisplayID,
    UINT32 linkCount,
    DisplayIDs m_connectedDisplay,
    bool hdcpEnabled,
    LW0073_CTRL_SPECIFIC_HDCP_CTRL_PARAMS *hdcpCtrlParams
)
{
    UINT32 dispMask = 0x00000000;
    RC rc;

    Printf(Tee::PriHigh, "==========DISPLAY INFO OF DISPLAY %d ==========\n\n", i);

    m_pDisplay->DispIDToDispMask( m_connectedDisplay, dispMask);
    Printf(Tee::PriHigh, "     Device Mask                   :  0x%8x\n", dispMask);

    Display::Encoder enc;
    m_pDisplay->GetEncoder(m_connectedDisplayID, &enc);
    if((enc.Signal  == 0) || ((enc.Signal == 0) && (enc.Link == 0) && (enc.Limit == 0))) // second condition for non-DFPs (CRT or TV)
    {
        Printf(Tee::PriHigh, "     Internal Display              : 0\n");
    }
    else if(enc.Signal == 1)
    {
        Printf(Tee::PriHigh, "     Internal Display              : 1\n");
    }

    bool            isHdcp1xEnabled = false;
    bool            isHdcp1xCapable = false;
    bool            isGpuHdcpCapable = false;
    bool            isHdcp22Capable = false;
    bool            isHdcp22Enabled = false;
    bool            isRepeater = false;
    bool            gethdcpStatusCached = true;

    HDCPUtils hdcpUtils;

    CHECK_RC(hdcpUtils.Init(m_pDisplay, m_connectedDisplayID));

    CHECK_RC(hdcpUtils.GetHDCPInfo(m_connectedDisplayID, gethdcpStatusCached, isHdcp1xEnabled,
        isHdcp1xCapable, isGpuHdcpCapable, isHdcp22Capable, isHdcp22Enabled, isRepeater));

    if(isGpuHdcpCapable)
    {
        Printf(Tee::PriHigh, "     HDCP Capable Connection       : 1\n");
    }
    else
    {
        Printf(Tee::PriHigh, "     HDCP Capable Connection       : 0\n");
    }

    if(isHdcp1xEnabled)
    {
        Printf(Tee::PriHigh, "     HDCP 1.x Encryption ON        : 1\n");
    }
    else if(isHdcp22Enabled)
    {
        Printf(Tee::PriHigh, "     HDCP 2.2 Encryption ON        : 1\n");
    }
    else
    {
        Printf(Tee::PriHigh, "     HDCP Encryption ON            : 0\n");
    }

    if(hdcpCtrlParams->bKsvList[0] == 0)
    {
        Printf(Tee::PriHigh, "     REPEATER                      : 0 \n");
    }
    else
    {
        Printf(Tee::PriHigh, "     REPEATER                      : 1\n");
    }

    Printf(Tee::PriHigh, "     Supports Non-HDCP Modes       : 1 \n");

    if(linkCount > 1)
    {
        Printf(Tee::PriHigh, "     Display in Dual-Link          : 1 \n");
        Printf(Tee::PriHigh, "     Attach-point index[0]         : %u\n", hdcpCtrlParams->apIndex[0]);
        Printf(Tee::PriHigh, "     Attach-point index[1]         : %u\n", hdcpCtrlParams->apIndex[1]);
    }
    else
    {
        Printf(Tee::PriHigh, "     Display in Dual-Link          : 0 \n");
        Printf(Tee::PriHigh, "     Attach-point index[0]         : %u\n", hdcpCtrlParams->apIndex[0]);
    }

    return rc;
}

static LwU8 Shuffle
(
    LwU8 din,
    LwU8 s,
    LfsrInitMode init,
    int i
)
{
    LwU8  dout;

    if (s==0)
    {
        dout = s_A[i];
    }
    else
    {
        dout = s_B[i];
    }
    if (init != NoInit)
    {
        s_A[i] = 0;
        s_B[i] = 1;
    }
    else
    {
        if (s==0)
        {
            s_A[i] = s_B[i];
            s_B[i] = din;
        }
        else
        {
            s_B[i] = s_A[i];
            s_A[i] = din;
        }
    }
    return(dout);
}

static LwU8 Combiner
(
    LwU8* lfsr0_output,
    LwU8* lfsr1_output,
    LwU8* lfsr2_output,
    LwU8* lfsr3_output,
    LfsrInitMode init
)
{
    LwU8 din0;
    LwU8 dout0, dout1, dout2, dout3;

    din0 = lfsr0_output[0] ^ lfsr1_output[0] ^ lfsr2_output[0] ^ lfsr3_output[0];

    dout0 = Shuffle(din0,   lfsr0_output[1], init, 0);
    dout1 = Shuffle(dout0,  lfsr1_output[1], init, 1);
    dout2 = Shuffle(dout1,  lfsr2_output[1], init, 2);
    dout3 = Shuffle(dout2,  lfsr3_output[1], init, 3);

    dout0 = dout3 ^ lfsr0_output[2] ^ lfsr1_output[2] ^ lfsr2_output[2] ^ lfsr3_output[2];

    return(dout0);
}

static RC CheckCKeyChecksum(const LwU64 * Keys, LwU64 expectedChecksum)
{
    int i;
    LwU64 lwSum = 0;

    // Running decrypt here, so we don't have to store the secret key-vector
    // in plaintext in the .exe file.
    Random rnd;
    rnd.SeedRandom(0x39425a);  // arbitrary, must match array at top of file.

    for (i=0;i<40;i++)
    {
        LwU64 key;
        if (g_bIsUsingRealKeys)
        {
            LwU64 x = rnd.GetRandom();
            x = (x << 32) | rnd.GetRandom();
            key = Keys[i] ^ x;

            //Printf(Tee::PriLow,
            //    "Key[%d] = 0x%llxLL == 0x%llxLL ^ 0x%llxLL,\n",
            //    i, Keys[i], key, x);
        }
        else
            key = Keys[i];

        lwSum += key;
        bool rollover = lwSum > MASK56;
        lwSum = rollover + (lwSum & MASK56);
    }

    Printf(Tee::PriLow,
        " Checksum for decrypted keys 0x%llx %s expected 0x%llx\n",
        lwSum,
        lwSum == expectedChecksum ? "matches" : "DOES NOT MATCH",
        expectedChecksum);

    if (lwSum != expectedChecksum)
        return RC::SOFTWARE_ERROR;

    return OK;
}

static LwU64 CalcKp
(
    LwU64 Cn,
    LwU64 Cs,
    LwU64 Dksv,
    LwU64 An,
    LwU64 Bksv,
    LwU64 Status,
    const LwU64* Ckey
)
{
    LwU64 Ku;
    LwU64 Kp = 0;
    LwU64 K1 = 0;
    LwU64 K2 = 0;
    LwU64 K3 = 0;
    LwU64 K4 = 0;
    LwU64 StatCn;
    LwU64 Cn56;
    LwU64 PartCn;
    int i;

Printf(Tee::PriHigh, "CALCKP CS    0x%0llx\n", Cs);
Printf(Tee::PriHigh, "CALCKP Cn    0x%0llx\n", Cn);
Printf(Tee::PriHigh, "CALCKP Dksv  0x%0llx\n", Dksv);
Printf(Tee::PriHigh, "CALCKP An    0x%0llx\n", An);
Printf(Tee::PriHigh, "CALCKP Bksv  0x%0llx\n", Bksv);
Printf(Tee::PriHigh, "CALCKP Status 0x%0llx\n", Status);
Printf(Tee::PriHigh, "CALCKP Ckey   0x%0llx\n", *Ckey);
    //Callwlate Ku
    SumKu(&Ku, Dksv, Ckey);

Printf(Tee::PriHigh, "CALCKP Ku 0x%0llx\n", Ku);

    // K1' = oneWay-A56(Ku', lsb40(Cn))
    Cn56 = Cn;
    PartCn = Cn56 & MASK40;
Printf(Tee::PriHigh, "CALCKP PartCn 0x%0llx\n", PartCn);
    OneWay(PartCn, Ku, InitA);
Printf(Tee::PriHigh, "CALCKP Ku 0x%0llx\n", Ku);

    for (i=0; i<56+32; i++)
    {
        K1 = K1 >> 1;
        K1 |= OneWay(PartCn, Ku, NoInit) ? MSB56 : 0;
    }

Printf(Tee::PriHigh, "CALCKP K1 0x%0llx\n", K1);
    // K2' = oneWay-A56(K1', Bksv)
    OneWay(Bksv, K1, InitA);
Printf(Tee::PriHigh, "CALCKP K1 0x%0llx\n", K1);
Printf(Tee::PriHigh, "CALCKP Bksv 0x%0llx\n", Bksv);

    for (i=0; i<56+32; i++)
    {
        K2 = K2 >> 1;
        K2 |= OneWay(Bksv, K1, NoInit) ? MSB56 : 0;
    }

Printf(Tee::PriHigh, "CALCKP K2 0x%0llx\n", K2);
    // K3' = oneWay-A56(K2', msb40(An))
    An >>= 24;
    OneWay(An, K2, InitA);
Printf(Tee::PriHigh, "CALCKP K2 0x%0llx\n", K2);
Printf(Tee::PriHigh, "CALCKP An 0x%0llx\n", An);
    for (i=0; i<56+32; i++)
    {
        K3 = K3 >> 1;
        K3 |= OneWay(An, K2, NoInit) ? MSB56 : 0;
    }

Printf(Tee::PriHigh, "CALCKP K3 0x%0llx\n", K3);
    // K4' = oneWay-A56(K3', status || msb24(Cn))
    StatCn = ((Status & 0x0000ffffLL) << 24) | (Cn >> 40);

Printf(Tee::PriHigh, "CALCKP StatCn 0x%0llx\n", StatCn);
    OneWay(StatCn, K3, InitA);
Printf(Tee::PriHigh, "CALCKP K3 0x%0llx\n", K3);
Printf(Tee::PriHigh, "CALCKP StatCn 0x%0llx\n", StatCn);
    for (i=0; i<56+32; i++)
    {
        K4 = K4 >> 1;
        K4 |= OneWay(StatCn, K3, NoInit) ? MSB56 : 0;
    }

Printf(Tee::PriHigh, "CALCKP K4 0x%0llx\n", K4);
Printf(Tee::PriHigh, "CALCKP StatCn 0x%0llx\n", StatCn);
    if (1 == DRF_VAL(0073, _CTRL_SPECIFIC_HDCP_CTRL_STATUS_S, _CS_CAPABLE, Status))
    {
        // Kp' = oneWay-A56(K4', Cs)
Printf(Tee::PriHigh, "CALCKP CS    0x%0llx\n", Cs);
Printf(Tee::PriHigh, "CALCKP InitA 0x%0x\n", InitA);
        OneWay(Cs, K4, InitA);

Printf(Tee::PriHigh, "CALCKP K4 0x%0llx\n", K4);
Printf(Tee::PriHigh, "CALCKP CS 0x%0llx\n", Cs);
        for (i=0; i<56+32; i++)
        {
            Kp = Kp >> 1;
            Kp |= OneWay(Cs, K4, NoInit) ? MSB56 : 0;
        }
Printf(Tee::PriHigh, "CALCKP Kp 0x%0llx\n", Kp);
    }
    else
    {
        Kp = K4;
    }
Printf(Tee::PriHigh, "CALCKP Kp 0x%0llx\n", Kp);

    return(Kp);
}

static void SumKu
(
    LwU64        *pKu,
    LwU64        Ksv,
    const LwU64* Keys
)
{
    int i;
    LwU64 lwSum = 0;

    // Running decrypt here, so we don't have to store the secret key-vector
    // in plaintext in the .exe file.
    Random rnd;
    rnd.SeedRandom(0x39425a);  // arbitrary, must match array at top of file.

    for (i=0;i<40;i++)
    {
        LwU64 key;

        if (g_bIsUsingRealKeys)
        {
            LwU64 x = rnd.GetRandom();
            x = (x << 32) | rnd.GetRandom();
            key = Keys[i] ^ x;
        }
        else
        {
            key = Keys[i];
        }

        if (Ksv & BitM64(i))
            lwSum += key;
    }

    lwSum &= MASK56;
    *pKu = lwSum;

    return;
}

static LwU8  OneWay
(
    LwU64 data,
    LwU64 key,
    LfsrInitMode mode
)
{
    LwU8  Result;
    LwU8  lfsr0_output[3];
    LwU8  lfsr1_output[3];
    LwU8  lfsr2_output[3];
    LwU8  lfsr3_output[3];

    LFSR0(lfsr0_output, data, key, mode);
    LFSR1(lfsr1_output, data, key, mode);
    LFSR2(lfsr2_output, data, key, mode);
    LFSR3(lfsr3_output, data, key, mode);

    Result = Combiner(lfsr0_output, lfsr1_output, lfsr2_output, lfsr3_output, mode);

// #if defined(DEBUG_HDCP_UP)
//     Printf(Tee::PriDebug,
//         " OneWay %s data 0x%llx key 0x%llx returns %d\n",
//         mode == InitA ? "initA" : mode == InitB ? "initB" : "",
//         data,
//     PrintLfsr(false, 0, lfsr0_output, sizeof(lfsr0_output));
//     PrintLfsr(false, 1, lfsr1_output, sizeof(lfsr1_output));
//     PrintLfsr(false, 2, lfsr2_output, sizeof(lfsr2_output));
//     PrintLfsr(false, 3, lfsr3_output, sizeof(lfsr3_output));
// #endif

    return(Result);
}

static void LFSR0
(
    LwU8* lfsr0_output,
    LwU64 data,
    LwU64 key,
    LfsrInitMode init
)
{
    int i;

    lfsr0_output[2] = s_lfsr0[22];
    lfsr0_output[1] = s_lfsr0[14];
    lfsr0_output[0] = s_lfsr0[7];

    if (init==InitA)
    {
        for (i=22;i>19;i--)
            s_lfsr0[i] = data & BitM64(i-15) ? 1 : 0;

        s_lfsr0[19]= key & BitM64(10) ? 0 : 1;

        for (i=18;i>13;i--)
            s_lfsr0[i] = data & BitM64(i-14) ? 1 : 0;

        for (i=13;i>6;i--)
            s_lfsr0[i] = key & BitM64(i) ? 1 : 0;

        for (i=6;i>=0;i--)
            s_lfsr0[i] = key & BitM64(i) ? 1 : 0;

    }
    else if (init==InitB)
    {
        for (i=22;i>19;i--)
            s_lfsr0[i] = data & BitM64(i-20) ? 0 : 1;

        s_lfsr0[19]= key & BitM64(3) ? 0 : 1;

        for (i=18;i>13;i--)
            s_lfsr0[i] = data & BitM64(i+21) ? 1 : 0;

        for (i=13;i>6;i--)
            s_lfsr0[i] = key & BitM64(i-7) ? 1 : 0;

        for (i=6;i>=0;i--)
            s_lfsr0[i] = key & BitM64(i+49) ? 1 : 0;

    }
    else
    {
        LwU8  temp = s_lfsr0[5] ^ s_lfsr0[8] ^ s_lfsr0[11] ^ s_lfsr0[15] ^ s_lfsr0[19] ^ s_lfsr0[22];

        for (i=22;i>0;i--)
            s_lfsr0[i]=s_lfsr0[i-1];

        s_lfsr0[0] = temp;
    }
    return;
}

//----------------------------------------------------------------------------
static void LFSR1
(
    LwU8*  lfsr1_output,
    LwU64 data,
    LwU64 key,
    LfsrInitMode init
)
{
    int i;
    lfsr1_output[2] = s_lfsr1[23];
    lfsr1_output[1] = s_lfsr1[15];
    lfsr1_output[0] = s_lfsr1[7];

    if (init==InitA)
    {
        for (i=23;i>18;i--)
            s_lfsr1[i] = data & BitM64(i-7) ? 1 : 0;

        s_lfsr1[18] = key & BitM64(19) ? 0 : 1;

        for (i=17;i>13;i--)
            s_lfsr1[i] = data & BitM64(i-6) ? 1 : 0;

        for (i=13;i>=0;i--)
            s_lfsr1[i] = key & BitM64(i+14) ? 1 : 0;
    }
    else if (init==InitB)
    {
        for (i=23;i>18;i--)
            s_lfsr1[i] = data & BitM64(i-12) ? 0 : 1;

        s_lfsr1[18] = key & BitM64(12) ? 0 : 1;

        for (i=17;i>13;i--)
            s_lfsr1[i] = data & BitM64(i-11) ? 1 : 0;

        for (i=13;i>=0;i--)
            s_lfsr1[i] = key & BitM64(i+7) ? 1 : 0;;
    }
    else
    {
        LwU8  temp = s_lfsr1[6] ^ s_lfsr1[9] ^ s_lfsr1[13] ^ s_lfsr1[17] ^ s_lfsr1[20] ^ s_lfsr1[23];

        for (i=23; i>0; i--)
            s_lfsr1[i] = s_lfsr1[i-1];

        s_lfsr1[0] = temp;
    }
    return;
}

//----------------------------------------------------------------------------
static void LFSR2
(
    LwU8* lfsr2_output,
    LwU64 data,
    LwU64 key,
    LfsrInitMode init
)
{
    int i;

    lfsr2_output[2] = s_lfsr2[25];
    lfsr2_output[1] = s_lfsr2[16];
    lfsr2_output[0] = s_lfsr2[8];

    if (init==InitA)
    {
        for (i=25;i>21;i--)
            s_lfsr2[i] = data & BitM64(i+2) ? 1 : 0;

        s_lfsr2[21] = key & BitM64(36) ? 0 : 1;

        for (i=20;i>13;i--)
            s_lfsr2[i] = data & BitM64(i+3) ? 1 : 0;

        for (i=13;i>=0;i--)
            s_lfsr2[i] = key & BitM64(i+28) ? 1 : 0;
    }
    else if (init==InitB)
    {
        for (i=25;i>21;i--)
            s_lfsr2[i] = data & BitM64(i-3) ? 0 : 1;

        s_lfsr2[21] = key & BitM64(29) ? 0 : 1;

        for (i=20;i>13;i--)
            s_lfsr2[i] = data & BitM64(i-2) ? 1 : 0;

        for (i=13;i>=0;i--)
            s_lfsr2[i] = key & BitM64(i+21) ? 1 : 0;
    }
    else
    {
        LwU8  temp = s_lfsr2[7] ^ s_lfsr2[11] ^ s_lfsr2[14] ^ s_lfsr2[17] ^ s_lfsr2[22] ^ s_lfsr2[25];

        for (i=25;i>0;i--)
            s_lfsr2[i]=s_lfsr2[i-1];

        s_lfsr2[0] = temp;
    }
    return;
}

//----------------------------------------------------------------------------
static void LFSR3
(
    LwU8*  lfsr3_output,
    LwU64 data,
    LwU64 key,
    LfsrInitMode init
)
{
    int i;

    lfsr3_output[2] = s_lfsr3[26];
    lfsr3_output[1] = s_lfsr3[17];
    lfsr3_output[0] = s_lfsr3[8];

    if (init==InitA)
    {
        for (i=26;i>21;i--)
            s_lfsr3[i] = data & BitM64(i+13) ? 1 : 0;

        s_lfsr3[21] = key & BitM64(51) ? 0 : 1;

        for (i=20;i>13;i--)
            s_lfsr3[i] = data & BitM64(i+14) ? 1 : 0;

        for (i=13;i>=0;i--)
            s_lfsr3[i] = key & BitM64(i+42) ? 1 : 0;
    }
    else if (init==InitB)
    {
        for (i=26;i>21;i--)
            s_lfsr3[i] = data & BitM64(i+8) ? 0 : 1;

        s_lfsr3[21] = key & BitM64(44) ? 0 : 1;

        for (i=20;i>13;i--)
            s_lfsr3[i] = data & BitM64(i+9) ? 1 : 0;

        for (i=13;i>=0;i--)
            s_lfsr3[i] = key & BitM64(i+35) ? 1 : 0;
    }
    else
    {
        LwU8  temp = s_lfsr3[7] ^ s_lfsr3[12] ^ s_lfsr3[16] ^ s_lfsr3[20] ^ s_lfsr3[23] ^ s_lfsr3[26];

        for (i=26;i>0;i--)
            s_lfsr3[i] = s_lfsr3[i-1];

        s_lfsr3[0] = temp;
    }
    return;
}

//! \brief Cleanup, does nothing for this simple test
//------------------------------------------------------------------------------
RC LwHdcpApiTest::Cleanup()
{
    return OK;
}

//! \brief combinationGenerator, does
//! takes a integer as input colwerts  it to a combination. Returns true if
//! the combination is valid
bool LwHdcpApiTest::combinationGenerator(  vector<UINT32>& validCombi,
                                            UINT32 trialCombi,
                                            UINT32 numOfHeads,
                                            UINT32 numOfDisps)
{
    UINT32 limit = 0;

    for(UINT32 loopCount = 0; loopCount < numOfDisps; loopCount++)
    {
        if((validCombi[loopCount] = (trialCombi & 1)))
            limit++;
        trialCombi = (trialCombi >> 1);
    }

    if(limit <= numOfHeads && limit > 1)
        return true;
    else
        return false;
}

//! Gets HDCP capabilty
RC LwHdcpApiTest:: IsHdcpCapable(DisplayID Display,
                                 bool *pbIsGpuHdcpCapable,
                                 bool *pbIsHdcp1xCapable,
                                 bool *pbIsHdcp22Capable)
{
    RC rc;
    HDCPUtils hdcpUtils;

    CHECK_RC(hdcpUtils.Init(m_pDisplay, Display));

    bool isHdcp1xEnabled = false;
    bool isHdcp1xCapable = false;
    bool isGpuHdcpCapable = false;
    bool isHdcp22Enabled = false;
    bool isHdcp22Capable = false;
    bool isRepeater = false;
    bool gethdcpStatusCached = false;

    CHECK_RC(hdcpUtils.GetHDCPInfo(Display, gethdcpStatusCached, isHdcp1xEnabled, isHdcp1xCapable,
                isGpuHdcpCapable, isHdcp22Capable, isHdcp22Enabled, isRepeater));

    if(pbIsGpuHdcpCapable != NULL)
        *pbIsGpuHdcpCapable = isGpuHdcpCapable;

    if(pbIsHdcp1xCapable != NULL)
        *pbIsHdcp1xCapable = isHdcp1xCapable;

    if(pbIsHdcp22Capable != NULL)
        *pbIsHdcp22Capable = isHdcp22Capable;

    return rc;
}

//! Verifies HDCP status (disabled or enabled)
RC LwHdcpApiTest:: VerifyHdcpStatus(DisplayID Display, bool bverifyOp)
{
    RC rc;
    HDCPUtils hdcpUtils;

    CHECK_RC(hdcpUtils.Init(m_pDisplay, Display));

    bool isHdcp1xEnabled = false;
    bool isHdcp1xCapable = false;
    bool isGpuHdcpCapable = false;
    bool isHdcp22Capable = false;
    bool isHdcp22Enabled = false;
    bool isRepeater = false;
    bool gethdcpStatusCached = false;

    CHECK_RC(hdcpUtils.GetHDCPInfo(Display, gethdcpStatusCached, isHdcp1xEnabled, isHdcp1xCapable,
                isGpuHdcpCapable, isHdcp22Capable, isHdcp22Enabled, isRepeater));

    if (bverifyOp == CHECK_FOR_HDCP_ENABLE)
    {
        if(isHdcp22Capable)
        {
            if(!isHdcp22Enabled)
            {
                rc = RC::SOFTWARE_ERROR;
            }
        }
        else
        {
            if(!isHdcp1xCapable || !isHdcp1xEnabled)
            {
                rc = RC::SOFTWARE_ERROR;
            }
        }
    }
    else
    {
        if(isHdcp1xEnabled || isHdcp22Enabled)
        {
            rc = RC::SOFTWARE_ERROR;
        }
    }

    return rc;
}

//! Enables HDCP and verifies that it is enabled
RC LwHdcpApiTest::VerifyHDCPEnable(DisplayID Display)
{
    RC rc;
    HDCPUtils hdcpUtils;

    CHECK_RC(hdcpUtils.Init(m_pDisplay, Display));

    CHECK_RC(hdcpUtils.PerformJob(HDCPUtils::RENEGOTIATE_LINK));

    // Emulation sleep is taking more time
    Tasker::Sleep(sleepVal);

    CHECK_RC(VerifyHdcpStatus(Display, CHECK_FOR_HDCP_ENABLE));

    // Check for upstream
    CHECK_RC(hdcpUtils.PerformJob(HDCPUtils::READ_LINK_STATUS));

    return rc;
}

//! Check for display under flow
RC LwHdcpApiTest::CheckDispUnderflow()
{
    RC rc;
    UINT32 underflowHead;
    UINT32 m_numHeads;

    m_numHeads = m_pDisplay->GetHeadCount();
    // Check Underflow
    if ((rc = DTIUtils::VerifUtils::checkDispUnderflow(GetDisplay(), m_numHeads, &underflowHead)) == RC::LWRM_ERROR)
    {
        Printf(Tee::PriHigh, "ERROR: Display underflow was detected on head %u after enabling HDCP\n",
               (UINT32)underflowHead);

        return RC::SOFTWARE_ERROR;
    }

    return rc;
}
//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(LwHdcpApiTest, RmTest,
    "Simple test to demonstrate usage of HDCP related APIs in MODS");
CLASS_PROP_READWRITE(LwHdcpApiTest, QueryHeadConfig, bool,
    "Flag to use QueryHeadConfig option.");
CLASS_PROP_READWRITE(LwHdcpApiTest, Renegotiate, bool,
    "Flag to use RenegotiateLink option.");
CLASS_PROP_READWRITE(LwHdcpApiTest, ReadLink, bool,
    "Flag to use ReadLinkStatus option.");
CLASS_PROP_READWRITE(LwHdcpApiTest, protocol, string,
                     "OR protocol to use for the test");
CLASS_PROP_READWRITE(LwHdcpApiTest, displayInfo, bool,
                     "Flag to use DisplayInfo option.");
CLASS_PROP_READWRITE(LwHdcpApiTest, Upstream, bool,
    "Flag to use Upstream option.");
CLASS_PROP_READWRITE(LwHdcpApiTest, Downstream, bool,
    "Flag to use Downstream option.");
CLASS_PROP_READWRITE(LwHdcpApiTest, runCombiTest, bool,
    "Flag to use runCombiTest option.");
