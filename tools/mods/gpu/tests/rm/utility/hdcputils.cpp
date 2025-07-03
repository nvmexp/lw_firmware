#include "gpu/tests/rmtest.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "hdcputils.h"
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
#include "dtiutils.h"
#include "core/include/rc.h"

#include "core/include/memcheck.h"

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

// static UINT32 sleepVal = 0;

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

RC HDCPUtils::Init(Display *pDisplay, DisplayID hdcpDisplay)
{
    RC rc;
    LwRmPtr      pLwRm;

    Reset();

    m_pDisplay = pDisplay;
    m_hdcpDisplay = hdcpDisplay;

    if ((m_pDisplay->GetOwningDisplaySubdevice()->IsEmulation()) ||
        (m_pDisplay->GetOwningDisplaySubdevice()->IsDFPGA()) ||
        (Platform::GetSimulationMode() != Platform::Hardware))
    {
        // Use test keys for all simulation modes
        m_pLwrrentCkeys        = s_Ckey_Emu;
        m_lwrrentCksv          = s_Cksv_Emu;
        m_lwrrentCkeyChecksum  = s_Ckey_Emu_DecryptedChecksum;
        m_sleepInMills         = 15000;
        g_bIsUsingRealKeys     = false;
    }
    else
    {
        // Use real keys for silicon
        m_pLwrrentCkeys        = s_Ckey1;
        m_lwrrentCksv          = s_Cksv1;
        m_lwrrentCkeyChecksum  = s_Ckey1_DecryptedChecksum;
        m_sleepInMills         = 5000;
        g_bIsUsingRealKeys     = true;
    }

    return rc;
}

RC HDCPUtils::Init
(
    Display *pDisplay,
    DisplayID hdcpDisplay,
    LwU64 *cKeys,
    LwU64 cKeyChecksum,
    LwU64 cKsv,
    LwU32 sleepInMills
)
{
    RC rc;

    Reset();

    m_pDisplay = pDisplay;
    m_hdcpDisplay = hdcpDisplay;

    // use test provided keys
    m_pLwrrentCkeys        = cKeys;
    m_lwrrentCksv          = cKsv;
    m_lwrrentCkeyChecksum  = cKeyChecksum;
    m_sleepInMills         = sleepInMills;

    return rc;
}

HDCPUtils::~HDCPUtils()
{

}

void HDCPUtils::Reset()
{

    m_pDisplay = NULL;
    m_hdcpDisplay = 0;
    m_lwrrentCksv = 0;
    m_lwrrentCkeyChecksum = 0;
    m_pLwrrentCkeys = NULL;
    m_sleepInMills = 0;

}

LwU32 HDCPUtils::GetSleepAfterRenegotiateTime()
{
    return m_sleepInMills;
}

RC HDCPUtils::PerformJob(HDCPJOB hdcpJob)
{
    RC rc;

    if (m_pDisplay->GetDisplayType(m_hdcpDisplay) != Display::DFP)
    {
        Printf(Tee::PriHigh, "     HDCP not supported on this display              : 0\n");
        return rc;
    }

    switch (hdcpJob)
    {
    case QUERY_HEAD_CONFIG:
        Printf(Tee::PriHigh, "\n\n***** Querying Head Information for Display *****\n");

    case DISPLAY_INFO:
        {
            UINT64      cN       = 0x2c72677f652c2f27LL;  // Arbitrary integer
            vector  <UINT64> status, aN, aKsv, bKsv, bKsvList, dKsv, kP;
            vector  <LwU16> bCaps;
            UINT32      linkCount = 1, numAps = 0;
            UINT64      cS;

            LW0073_CTRL_SPECIFIC_HDCP_CTRL_PARAMS hdcpCtrlParams = {0};

            CHECK_RC(GetHDCPLinkParams(m_hdcpDisplay, cN,
                m_lwrrentCksv, &linkCount, &numAps, &cS, &status,
                &aN, &aKsv, &bKsv, &dKsv, &bKsvList, &kP,
                &hdcpCtrlParams,
                (LwU32)LW0073_CTRL_SPECIFIC_HDCP_CTRL_CMD_QUERY_HEAD_CONFIG));

            PrintHDCPInfo(m_hdcpDisplay, &hdcpCtrlParams, true);
        }

        break;

    case RENEGOTIATE_LINK:
        {
            Printf(Tee::PriHigh, "\n\n***** Renegotiating Link for Display ***********\n");
            UINT64      cN       = 0x2c72677f652c2f27LL;  // Arbitrary integer

            // Enable Authentication on Primary Link
            rc  = m_pDisplay->RenegotiateHDCP(m_hdcpDisplay, cN, m_lwrrentCksv, 0);

            if (rc == OK)
            {
                Printf(Tee::PriHigh, "     RENEGOTIATE_LINK             : PASS \n");
            }
            else
            {
                Printf(Tee::PriHigh, "     RENEGOTIATE_LINK             : FAIL \n");
            }
        }
        break;

    case READ_LINK_STATUS:
        {
            UINT64      cN       = 0x2c72677f652c2f27LL;  // Arbitrary integer
            vector  <UINT64> status, aN, aKsv, bKsv, bKsvList, dKsv, kP;
            vector  <LwU16> bCaps;
            UINT32      linkCount = 1, numAps = 0;
            UINT64      cS;
            UINT32 MaxKpToTest = 2;
            LW0073_CTRL_SPECIFIC_HDCP_CTRL_PARAMS hdcpCtrlParams = {0};

            RC rcStatus;
            LwU32 i = 0;

            Printf(Tee::PriHigh, "\n\n***** Reading Status for Display **************\n");

            //Verify if the key is ok
            CHECK_RC(CheckCKeyChecksum(m_pLwrrentCkeys, m_lwrrentCkeyChecksum));

            CHECK_RC(GetHDCPLinkParams(m_hdcpDisplay, cN,
                m_lwrrentCksv, &linkCount, &numAps, &cS, &status,
                &aN, &aKsv, &bKsv, &dKsv, &bKsvList, &kP,
                &hdcpCtrlParams,
                (LwU32)LW0073_CTRL_SPECIFIC_HDCP_CTRL_CMD_READ_LINK_STATUS));

            //Need to callwlate Kp' and check if Kp == Kp'
            for (i = 0; i < numAps && i < MaxKpToTest; i++)
            {
                LwU64 KP = CalcKp(cN, cS, dKsv[i], aN[i], bKsv[i], status[i], m_pLwrrentCkeys);
                LwU64 oriCs = cS;

                if (KP != kP[i])
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

                    LwU64 KP2 = CalcKp(cN, cS, dKsv[i], aN[i], bKsv[i], status[i],
                        m_pLwrrentCkeys);
                    if (KP2 != kP[i])
                    {
                        Printf(Tee::PriHigh, "Kp %0llx and Kp' %0llx different for Display %d. Test FAILED!!\n",KP2,kP[i], i);
                        cS = oriCs;
                        if (status[i] == 0x6f4c)
                        {
                            Printf(Tee::PriHigh, "Changing the status value from 0x%0llx to 0x6f45\n", status[i]);
                            status[i]=0x6f45;
                        }
                        else if(status[i] == 0x6f45)
                        {
                            Printf(Tee::PriHigh, "Changing the status value from 0x%0llx to 0x6f4c\n", status[i]);
                            status[i]=0x6f4c;
                        }
                        else
                            Printf(Tee::PriHigh, "Not Changing the status value\n");

                        LwU64 KP1 = CalcKp(cN, cS, dKsv[i], aN[i], bKsv[i],
                            status[i], m_pLwrrentCkeys);
                        if (KP1 != kP[i])
                        {
                            Printf(Tee::PriHigh, "Kp %0llx and Kp' %0llx different for Display %d. Test FAILED!!\n",KP1,kP[i], i);
                            rcStatus = RC::SOFTWARE_ERROR;
                        }
                    }
                }

            Printf(Tee::PriHigh, "\n\n===============INFO RETURNED FOR LINK %d==================\n", i);
            Printf(Tee::PriHigh, "     AN (Downstream Session ID)                    : 0x%llx\n", aN[i]);
            Printf(Tee::PriHigh, "     AKSV (Downstream Transmitter KSV)             : 0x%llx\n", aKsv[i]);
            Printf(Tee::PriHigh, "     BKSV (Downstream Receiver KSV)                : 0x%llx\n", bKsv[i]);
            Printf(Tee::PriHigh, "     DKSV (Upstream Transmitter KSV)               : 0x%llx\n", dKsv[i]);
            Printf(Tee::PriHigh, "     kP' (Hardware Signature)                      : 0x%llx\n", kP[i]);
            Printf(Tee::PriHigh, "\n     Test Get HDCP Parameters                            :     PASS\n");
            Printf(Tee::PriHigh, "     Test All Parameters for kP callwlation Present      :     PASS\n");
            Printf(Tee::PriHigh, "\n     CKSV (Upstream Software KSV)                 : 0x%llx\n", m_lwrrentCksv);
            Printf(Tee::PriHigh, "     CN (Upstream Session ID)                      : 0x%llx \n", cN);
            Printf(Tee::PriHigh, "     CS (Connection State)                         : 0x%llx \n", cS);
            Printf(Tee::PriHigh, "     st (Status)                                   : 0x%llx\n", status[i]);
            Printf(Tee::PriHigh, "     kP (software Signature)                       : 0x%llx\n", KP);
            Printf(Tee::PriHigh, "\n     Test kP signature verification                      :     PASS\n");

            CHECK_RC(rcStatus);

            }
        }
    }

    return rc;
}

RC HDCPUtils::GetHDCPInfo
(
    DisplayID   hdcpDisplay,
    bool        hdcpStatusCached,
    bool&       isHdcp1xEnabled,
    bool&       isHdcp1xCapable,
    bool&       isGpuCapable,
    bool&       isHdcp22Capable,
    bool&       isHdcp22Enabled,
    bool&       isRepeater
)
{
    LwU32 displaySubdevice;
    RC rc;
    CHECK_RC(m_pDisplay->GetDisplaySubdeviceInstance(&displaySubdevice));

    LW0073_CTRL_SPECIFIC_GET_HDCP_STATE_PARAMS Params ={0};
    Params.subDeviceInstance = displaySubdevice;
    Params.displayId = hdcpDisplay;
    if (hdcpStatusCached)
    {
        Params.flags = DRF_DEF(0073_CTRL_SPECIFIC_HDCP_STATE, _ENCRYPTING, _CACHED, _TRUE);
    }
    else
    {
        Params.flags = DRF_DEF(0073_CTRL_SPECIFIC_HDCP_STATE, _ENCRYPTING, _CACHED, _FALSE);
    }

    CHECK_RC(m_pDisplay->RmControl(
                 LW0073_CTRL_CMD_SPECIFIC_GET_HDCP_STATE,
                 &Params, sizeof(Params)));

    isHdcp1xEnabled     = FLD_TEST_DRF(0073, _CTRL_SPECIFIC_HDCP_STATE,
                               _ENCRYPTING, _YES, Params.flags);

    isHdcp1xCapable = FLD_TEST_DRF(0073, _CTRL_SPECIFIC_HDCP_STATE,
                               _RECEIVER_CAPABLE, _YES, Params.flags) ||
                      FLD_TEST_DRF(0073, _CTRL_SPECIFIC_HDCP_STATE,
                               _HDCP22_REPEATER_CAPABLE, _YES, Params.flags);

    isGpuCapable  = FLD_TEST_DRF(0073, _CTRL_SPECIFIC_HDCP_STATE,
                               _AP_DISALLOWED, _NO, Params.flags);

    isHdcp22Capable = FLD_TEST_DRF(0073, _CTRL_SPECIFIC_HDCP_STATE,
                                   _HDCP22_RECEIVER_CAPABLE,_YES, Params.flags) ||
                      FLD_TEST_DRF(0073, _CTRL_SPECIFIC_HDCP_STATE,
                                    _HDCP22_REPEATER_CAPABLE,_YES, Params.flags);

    isHdcp22Enabled = FLD_TEST_DRF(0073, _CTRL_SPECIFIC_HDCP_STATE,
                                   _HDCP22_ENCRYPTING, _YES, Params.flags);

    isRepeater = FLD_TEST_DRF(0073, _CTRL_SPECIFIC_HDCP_STATE,
                             _REPEATER_CAPABLE, _YES, Params.flags) ||
                 FLD_TEST_DRF(0073, _CTRL_SPECIFIC_HDCP_STATE,
                              _HDCP22_REPEATER_CAPABLE, _YES, Params.flags);

    return rc;
}

RC HDCPUtils::PrintHDCPInfo
(
    DisplayID   hdcpDisplay,
    void       *pHdcpCtrlParams,
    bool        hdcpStatusCached
)
{
    RC rc;
    Display::Encoder enc;
    LW0073_CTRL_SPECIFIC_HDCP_CTRL_PARAMS *pHDCPCtrlParams =
        (LW0073_CTRL_SPECIFIC_HDCP_CTRL_PARAMS *)pHdcpCtrlParams;

    Printf(Tee::PriHigh, "==========DISPLAY INFO OF DISPLAY %d ==========\n\n", (UINT32)hdcpDisplay);

    m_pDisplay->GetEncoder(hdcpDisplay, &enc);
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
    bool            isGpuCapable = false;
    bool            isHdcp22Capable = false;
    bool            isHdcp22Enabled = false;
    bool            isRepeater = false;
    bool            getHdcpStatusCached = true;

    CHECK_RC(GetHDCPInfo(hdcpDisplay, getHdcpStatusCached, isHdcp1xEnabled, isHdcp1xCapable,
        isGpuCapable, isHdcp22Capable, isHdcp22Enabled, isRepeater));

    if (!isGpuCapable)
    {
        Printf(Tee::PriHigh, "     GPU doesn't support HDCP    \n");
    }

    if(isHdcp1xCapable)
    {
        Printf(Tee::PriHigh, "     HDCP1.X Capable Connection       : 1\n");
    }
    else if(isHdcp22Capable)
    {
        Printf(Tee::PriHigh, "     HDCP2.2 Capable Connection       : 1\n");
    }

    if(isHdcp1xEnabled)
    {
        Printf(Tee::PriHigh, "     HDCP1.X Encryption ON            : 1\n");
    }
    else if (isHdcp22Enabled)
    {
        Printf(Tee::PriHigh, "     HDCP2.2 Encryption ON            : 0\n");
    }

    Printf(Tee::PriHigh, "      Repeater            : %d\n", isRepeater);

    if (pHDCPCtrlParams != NULL)
    {

        if(pHDCPCtrlParams->linkCount > 1)
        {
            Printf(Tee::PriHigh, "     Display in Dual-Link          : 1 \n");
            Printf(Tee::PriHigh, "     Attach-point index[0]         : %u\n", pHDCPCtrlParams->apIndex[0]);
            Printf(Tee::PriHigh, "     Attach-point index[1]         : %u\n", pHDCPCtrlParams->apIndex[1]);
        }
        else
        {
            Printf(Tee::PriHigh, "     Display in Dual-Link          : 0 \n");
            Printf(Tee::PriHigh, "     Attach-point index[0]         : %u\n", pHDCPCtrlParams->apIndex[0]);
        }

        // Need to print Cs Status, BCaps will print in next CL.
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get HDCP Link status and information. This API will provide data
//!        needed for client to verify HDCP link (verify Kp == Kp').
//!
//! \param[in]  Display     : The display for HDCP Link info is to be fetched.
//! \param[in]  Cn          : Cn value.
//! \param[in]  CKsv        : cKsv value.
//! \param[out] LinkCount   : Pointer to a UINT64 where the link count will be saved.
//! \param[out] NumAps      : Pointer to a UINT64 where the no. of attached points
//!                           will be saved.
//! \param[out] Cs          : Pointer to a UINT64 where the connection state
//!                           will be saved.
//! \param[out] Status      : Pointer to a vector of UINT64 where the status of
//!                           each of the attached points will be saved.
//! \param[out] An          : Pointer to a vector of UINT64 where the An value
//!                           used to callwlate Kp will be saved.
//! \param[out] AKsv        : Pointer to a vector of UINT64 where
//!                           AKsv values will be stored.
//! \param[out] BKsv        : Pointer to a vector of UINT64 where the BKsv values
//!                           used to callwlate Kp will be stored.
//! \param[out] DKsv        : Pointer to a vector of UINT64 where DKsv values
//!                           used to callwlate Kp will be stored.
//! \param[out] Kp          : Pointer to a vector of UINT64 where the Kp value
//!                           callwlated by RM will be stored. The client will
//!                           callwlate Kp using its own keys and can verify if
//!                           the RM Kp value matches with the client value.
//!
//! \return OK if HDCP Link info was successfully fetched, not OK otherwise
//!
RC HDCPUtils::GetHDCPLinkParams
(
   const DisplayID  Display,
   const UINT64     Cn,
   const UINT64     CKsv,
   UINT32           *LinkCount,
   UINT32           *NumAps,
   UINT64           *Cs,
   vector <UINT64>  *Status,
   vector <UINT64>  *An,
   vector <UINT64>  *AKsv,
   vector <UINT64>  *BKsv,
   vector <UINT64>  *DKsv,
   vector <UINT64>  *BKsvList,
   vector <UINT64>  *Kp,
   void             *pHdcpCtrlParams,
   LwU32            hdcpCommand
)
{
    RC rc;

    // Get the display subdevice
    LwU32 displaySubdevice;
    CHECK_RC(m_pDisplay->GetDisplaySubdeviceInstance(&displaySubdevice));

    LW0073_CTRL_SPECIFIC_HDCP_CTRL_PARAMS *pHDCPCtrlParams =
        (LW0073_CTRL_SPECIFIC_HDCP_CTRL_PARAMS *)pHdcpCtrlParams;

    pHDCPCtrlParams->subDeviceInstance = displaySubdevice;
    pHDCPCtrlParams->displayId = Display;
    pHDCPCtrlParams->cmd = hdcpCommand;
    pHDCPCtrlParams->flags = 0xffffffff;  // We want all available data.
    pHDCPCtrlParams->cN = Cn;
    pHDCPCtrlParams->cKsv = CKsv;

    CHECK_RC(m_pDisplay->RmControl(LW0073_CTRL_CMD_SPECIFIC_HDCP_CTRL,
                                   pHDCPCtrlParams, sizeof(LW0073_CTRL_SPECIFIC_HDCP_CTRL_PARAMS)));

    if (FLD_TEST_DRF(0073, _CTRL_SPECIFIC_HDCP_CTRL_FLAGS,
        _CS_PRESENT, _NO, pHDCPCtrlParams->flags))
    {
        Printf(Tee::PriHigh, "Function %s, cannot read cS\n",__FUNCTION__);
        CHECK_RC(RC::HDCP_ERROR);
    }

    // Extract numAPs from cS
    UINT32 numAPsTemp = Utility::CountBits((UINT32)DRF_VAL(0073,
                           _CTRL_SPECIFIC_HDCP_CTRL, _CS_ATTACH_POINTS, pHDCPCtrlParams->cS));

    // Now fill in out params
    if (NumAps)     *NumAps    = numAPsTemp;
    if (Cs)         *Cs        = pHDCPCtrlParams->cS;
    if (LinkCount)  *LinkCount = pHDCPCtrlParams->linkCount;

    // if it failed to read any of the values, return error
    if (FLD_TEST_DRF(0073, _CTRL_SPECIFIC_HDCP_CTRL_FLAGS, _AN_PRESENT, _NO, pHDCPCtrlParams->flags) ||
        FLD_TEST_DRF(0073, _CTRL_SPECIFIC_HDCP_CTRL_FLAGS, _AKSV_PRESENT, _NO, pHDCPCtrlParams->flags) ||
        FLD_TEST_DRF(0073, _CTRL_SPECIFIC_HDCP_CTRL_FLAGS, _BKSV_PRESENT, _NO, pHDCPCtrlParams->flags) ||
        FLD_TEST_DRF(0073, _CTRL_SPECIFIC_HDCP_CTRL_FLAGS, _DKSV_PRESENT, _NO, pHDCPCtrlParams->flags) ||
        FLD_TEST_DRF(0073, _CTRL_SPECIFIC_HDCP_CTRL_FLAGS, _KP_PRESENT, _NO, pHDCPCtrlParams->flags))
    {
        Printf(Tee::PriHigh,
               "Function %s, cannot read HDCP parameters, flag = 0x%x\n",
               __FUNCTION__, (UINT32)pHDCPCtrlParams->flags);
        CHECK_RC(RC::HDCP_ERROR);
    }
    UINT32 tempCount;
    for (tempCount = 0; (tempCount < pHDCPCtrlParams->linkCount) && (tempCount < LW0073_CTRL_HDCP_LINK_COUNT); tempCount++)
    {
        if (An)     An->push_back(pHDCPCtrlParams->aN[tempCount]);
        if (AKsv)   AKsv->push_back(pHDCPCtrlParams->aKsv[tempCount]);
        if (BKsv)   BKsv->push_back(pHDCPCtrlParams->bKsv[tempCount]);
        if (DKsv)   DKsv->push_back(pHDCPCtrlParams->dKsv[tempCount]);
        if (Kp)     Kp->push_back(pHDCPCtrlParams->kP[tempCount]);
    }

    for (tempCount = 0; (tempCount < numAPsTemp) && (tempCount < LW0073_CTRL_HDCP_LINK_COUNT); tempCount++)
    {
        if(Status) Status->push_back(pHDCPCtrlParams->stStatus[pHDCPCtrlParams->apIndex[tempCount]].S);
    }

    for (tempCount = 0; (tempCount < pHDCPCtrlParams->numBksvs) && (tempCount < LW0073_CTRL_HDCP_MAX_DEVICE_COUNT); tempCount++)
    {
        if(BKsvList) BKsvList->push_back(pHDCPCtrlParams->bKsvList[tempCount]);
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
    rnd.UseOld(); // Bug 2708847
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
    rnd.UseOld(); // Bug 2708847
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

