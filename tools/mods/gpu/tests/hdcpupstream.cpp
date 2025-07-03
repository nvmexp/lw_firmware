/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// @@@ fix comments for doxygen

// HDCP-upstream sanity check test.
//
// HDCP is High-bandwidth Digital Copyright Protection i.e. HD-DVD encryption.
//
// The main HDCP protocol is for encrypting the digital video/audio data
// between the player and the display.
// This prevents piracy from HW or SW monitorying the data to the display.
//
// The HDCP-upstream protocol encrypts status data from HW past driver/OS
// layers to the ISV DVD player application.  This is to prevent SW or HW in an
// "open" system like a PC from falsifying status about whether encryption is
// enabled on the downstream side.
//
// Names: A is gpu/vaio as hdcp source
//        B is display/repeater as hdcp sink
//        C is mods as hdcp-upstream client
//        D is gpu/vaio as hdcp-upstream server
// For displayless case B is not ilwolved
//
// pseudo-code (displayless upstream):
//  use RM's existing LW0073_CTRL_CMD_SPECIFIC_HDCP_CTRL interface
//    data to RM:
//     command = _READ_LINK_STATUS_NO_DISPLAY
//     cN = C's random key (mods generates random 64-bit number)
//     cKsv = C's key-selection-vector (given, just like C's secret keys )
//    data back from RM:
//     dKsv (D's key-selection-vector)
//     bKsv (B's key-selection-vector, RM returns 0 for displayless case)
//     aN (A's 64-bit random key, RM returns 0 for displayless case)
//     cS (connection state)
//     stStatus
//     kP (D's callwlated signature based on:
//         cKsv,
//         D's upstream secret key array from hdcp ROM,
//         cN, bKsv,
//         aN, cS, stStatus)
//  Check A, C and D Ksv values for 20 0's and 20 1's, else FAIL
//  Callwlate C's version of kP as a complex function of:
//         dKsv,
//         C's secret key array,
//         cN, bKsv,
//         aN, cS, stStatus
//  if D's and C's versions of kP are equal, PASS
//  otherwise FAIL -- defective upstream hdcp HW or bad upstream hdcp key ROM
// end pseudo-code

#include "core/include/display.h"
#include "gpu/include/gpudev.h"
#include "core/include/jscript.h"
#include "core/include/utility.h"
#include "ctrl/ctrl0073.h"
#include "random.h"
#include "hdcpupstream.h"
//-------------------------------------------------------------------------
// LWPU's proprietary, secret C key array and C key-selection-vector.
//
// Note: using obslwred symbol names here, with macros to make it easier
// to read the source code.
#define s_Cksv1 lwA1
#define s_Ckey1 lwB2

// (test key) static const LwU64 s_Cksv1 = 0xb86646fc8cLL;
static const LwU64 s_Cksv1 = 0x837aa5b62aLL;

// This is the secret 56-bit key array xor'd with the first 80 pseudo-random
// values from a mods Random object after being seeded with 0x39425a.
// The secret C keys don't appear in the mods binary or source-code
// as plaintext.

static const LwU64 s_Ckey1[40] = {
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
// (xor'd test key)
// static const LwU64 s_Ckey1[40] = {
//    0x9b71bd1b845e316dULL,
//    0x80a607167d65bebeULL,
//    0x2692df4c594dbdcfULL,
//    0x8d2d3dc9b262db58ULL,
//    0xbadb4ad5c8861150ULL,
//    0xf88a8d40940c35d2ULL,
//    0x03e0cdb720e774a3ULL,
//    0x35485a32424870aaULL,
//    0x7a451e32839f370eULL,
//    0x1b9a9e51660a334eULL,
//    0x3fdb258209e60459ULL,
//    0x97161626ebe1f51fULL,
//    0x9a19cfab4247c249ULL,
//    0x7db9ed4df69e1b51ULL,
//    0xe8278df8dacebfbfULL,
//    0x1662f805a7971ccfULL,
//    0x3cd5560aad80c153ULL,
//    0x0a56738b1df0f211ULL,
//    0x4420884f6f56014dULL,
//    0x49237c062d364381ULL,
//    0x176946001ce165dbULL,
//    0x70f0246919262fc9ULL,
//    0xe05365d52f57894eULL,
//    0x4d8ceecac6d52983ULL,
//    0x75a1b9b8daa6267fULL,
//    0xb590d6a146a2ccb1ULL,
//    0xb1b40051fc74ed7fULL,
//    0x11866d592a577634ULL,
//    0xcb949fd1883984c0ULL,
//    0x2bd7d69572f8af21ULL,
//    0x5a940dd543197f62ULL,
//    0x37dd4f758869cb99ULL,
//    0xd84518d0c3d6fc6bULL,
//    0xf9aaf4a7263e4ae2ULL,
//    0xdaf2b42b912840f4ULL,
//    0x1beb92921f5fd81dULL,
//    0x8486ac7de751a9b7ULL,
//    0x0de3cfad36a5f250ULL,
//    0x33cdf881629a3098ULL,
//    0xbf5b30199f91ef16ULL,
// };

#define BitM64( arg )   (((LwU64)1) << (arg))
static const LwU64 MASK40 = 0xFFFFFFFFFFLL;
static const LwU64 MASK56 = 0xFFFFFFFFFFFFFFLL;
static const LwU64 MSB56  = 0x80000000000000LL;

//---------------------------------------------------------------------------

// The following functions are local and static, to avoid revealing details
// in the object code (i.e. linker symbol-name data).
// The function names are also obslwred with macros just in case.
#define CalcKp lwa1
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

#define SumKu lwb2
static void SumKu
(
    LwU64 *pKu,
    LwU64 Ksv,
    const LwU64* Keys
);

#define CheckCsv a1
static RC   CheckKsv
(
    LwU64 ksv,
    const char * name,
    UINT32 num
);

#define Shuffle b2
static LwU8 Shuffle
(
    LwU8 din,
    LwU8 s,
    HDCPUpstream::LfsrInitMode init,
    int i
);

#define Combiner A1
static LwU8 Combiner
(
    LwU8* lfsr0_output,
    LwU8* lfsr1_output,
    LwU8* lfsr2_output,
    LwU8* lfsr3_output,
    HDCPUpstream::LfsrInitMode init
);

#define LFSR0 B2
static void LFSR0
(
    LwU8* lfsr0_output,
    LwU64 data,
    LwU64 key,
    HDCPUpstream::LfsrInitMode init
);

#define LFSR1 lw_A1
static void LFSR1
(
    LwU8*  lfsr1_output,
    LwU64 data,
    LwU64 key,
    HDCPUpstream::LfsrInitMode init
);

#define LFSR2 lw_B2
static void LFSR2
(
    LwU8* lfsr2_output,
    LwU64 data,
    LwU64 key,
    HDCPUpstream::LfsrInitMode init
);

#define LFSR3 a1_
static void LFSR3
(
    LwU8*  lfsr3_output,
    LwU64 data,
    LwU64 key,
    HDCPUpstream::LfsrInitMode init
);

#define OneWay b2_
static LwU8  OneWay
(
    LwU64 data,
    LwU64 key,
    HDCPUpstream::LfsrInitMode mode
);

// For debugging the math...
// #define DEBUG_HDCP_UP 1

#if defined(DEBUG_HDCP_UP)
// (test key) static const LwU64 s_Ckey1_DecryptedChecksum = 0x78efb77dde0f94LL;
static const LwU64 s_Ckey1_DecryptedChecksum = 0x776bf2cc0cd602LL;

static RC CheckCKeyChecksum(const LwU64 * Keys, LwU64 expectedChecksum);
#endif

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

//----------------------------------------------------------------------------
// Script interface.

JS_CLASS_INHERIT(HDCPUpstream, GpuTest,
                 "HDCP Upstream test.");

//----------------------------------------------------------------------------
HDCPUpstream::HDCPUpstream()
{
    SetName("HDCPUpstream");
}

//----------------------------------------------------------------------------
HDCPUpstream::~HDCPUpstream()
{
}

//----------------------------------------------------------------------------
RC HDCPUpstream::Setup()
{
    // Although we run this test without a connected display, we still ilwoke some display
    // specific code which would fail if -null_display is used.
    // We want fail test explicity if null display is used to avoid confusion
    if (GetBoundGpuDevice()->GetDisplay()->GetDisplayClassFamily() ==
            Display::NULLDISP)
    {
        Printf(Tee::PriError, "Test cannot be run with null display\n");
        return RC::ILWALID_DISPLAY_TYPE;
    }
 
    return RC::OK;
}

//------------------------------------------------------------------------------
RC HDCPUpstream::Run()
{
    RC rc;
#if defined(DEBUG_HDCP_UP)

    // Verify that C-key array obfuscation is OK.
    CHECK_RC(CheckCKeyChecksum(s_Ckey1, s_Ckey1_DecryptedChecksum));

    // Only works with test keys:
    // Run the first example from page 20 of the hdcp-upstream spec,
    // from table A-3 column 1:
    // UINT64 tmp_S = 0x0105LL;
    // UINT64 tmp_cN = 0x2c72677f652c2f27LL;
    // UINT64 tmp_bKsv = 0xe72697f401LL;
    // UINT64 tmp_aN = 0x34271c130c000000LL;
    // UINT64 tmp_cS = 0LL; // not used, bit _CS_CAPABLE of Status is clear
    // UINT64 tmp_dKsv = 0xfc5d32906cLL;
    // UINT64 tmp_Kp = 0x03e6205ba71568LL;
    // UINT64 tmp_KpPrime = CalcKp(tmp_cN, tmp_cS, tmp_dKsv, tmp_aN, tmp_bKsv, tmp_S, s_Ckey1);
    //
    // Printf(Tee::PriLow,
    //     " sample kP callwlated as 0x%llx, (%s expected 0x%llx).\n",
    //     tmp_KpPrime,
    //     tmp_KpPrime == tmp_Kp ? "matches" : "DOES NOT match",
    //     tmp_Kp
    //     );
#endif

    // Get hdcp data from the RM on current GPU.
    LW0073_CTRL_SPECIFIC_HDCP_CTRL_PARAMS * pparams;
    pparams = new LW0073_CTRL_SPECIFIC_HDCP_CTRL_PARAMS;

    MASSERT(pparams != NULL);

    // Cleanup
    unique_ptr<LW0073_CTRL_SPECIFIC_HDCP_CTRL_PARAMS> AutoParams(pparams);

    memset(pparams, 0, sizeof(LW0073_CTRL_SPECIFIC_HDCP_CTRL_PARAMS));
    pparams->subDeviceInstance = GetBoundGpuSubdevice()->GetSubdeviceInst();
    pparams->cmd = LW0073_CTRL_SPECIFIC_HDCP_CTRL_CMD_READ_LINK_STATUS_NO_DISPLAY;
    pparams->flags = 0xffffffff;  // We want all available data.
    pparams->cN = 0x12345678abcdef01LL;
    pparams->cKsv = s_Cksv1;

    {
        CHECK_RC(GetBoundGpuDevice()->GetDisplay()->
            RmControl(LW0073_CTRL_CMD_SPECIFIC_HDCP_CTRL,
                      pparams,
                      sizeof(LW0073_CTRL_SPECIFIC_HDCP_CTRL_PARAMS)));

        for (UINT32 i = 0; i < m_ApsToTest; i++)
        {
            // Check the Key-Selection-Vectors to be sure they all have
            // exactly 20 1's in them.
            // Report all the errors before exiting the test.
            // For displayless case, we are checking aksv, cksv and dksv only
            const map<string, LwU64>& keysToTest =
            {
                { "A", pparams->aKsv[i] },
                { "C", pparams->cKsv },
                { "D", pparams->dKsv[i] }
            };

            RC tempRc = CheckKsvs(keysToTest, i);
            if (rc == RC::OK)
            {
                rc = tempRc;
            }

            // Callwlate C's version of kP:
            //   kU = sum(cKeyArray, dKsv)
            //   k1 = OneWay(kU, lsb40(cN))
            //   k2 = OneWay(k1, bKsv)
            //   k3 = OneWay(k2, msb40(aN))
            //   k4 = OneWay(k3, append(status, msb24(cN)))
            //   kP = OneWay(k4, cS)

            LwU64 S = pparams->stStatus[pparams->apIndex[i]].S;

            LwU64 kP = CalcKp(
                    pparams->cN,
                    pparams->cS,
                    pparams->dKsv[i],
                    pparams->aN[i],
                    pparams->bKsv[i],
                    S,
                    s_Ckey1);

            if (kP != pparams->kP[i])
            {
                Printf(Tee::PriError, " kP[%d] doesn't match: %llx %llx\n",
                       i, kP, pparams->kP[i]);
                tempRc = RC::HDCP_ERROR;
                if (rc == RC::OK)
                {
                    rc = tempRc;
                }
            }
            else
            {
                Printf(Tee::PriDebug, " kP[%d] matched: %llx %llx\n",
                        i, kP, pparams->kP[i]);
            }
        }
    }

    return rc;
}

//----------------------------------------------------------------------------
bool HDCPUpstream::IsSupported()
{
    RC rc;
    UINT32 hdcpEn;
    rc = GetBoundGpuDevice()->GetDisplay()->GetHdcpEnable(&hdcpEn);
    if ((rc != RC::OK) || (hdcpEn == 0))
    {
        Printf(Tee::PriLow, "Test is not supported as HDCP is not enabled on this device");
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------
RC HDCPUpstream::CheckKsvs
(
   const map<string, LwU64>& keysToTest,
   UINT32 index
)
{
    RC rc, tempRc;
    for (auto key : keysToTest)
    {
        tempRc = CheckKsv(key.second, key.first.c_str(), index);
        if (rc == RC::OK)
        {
            rc = tempRc;
        }
    }

    return rc;
}

//----------------------------------------------------------------------------
static RC CheckKsv(LwU64 ksv, const char * name, UINT32 num)
{
    int nOnes = Utility::CountBits(LwU64_LO32(ksv)) +
                Utility::CountBits(LwU64_HI32(ksv) & 0xff);

    if (20 != nOnes)
    {
        Printf(Tee::PriHigh,
                " %sksv[%d] has %d bits set, should be 20 (0x%llx)\n",
                name, num, nOnes, ksv);

        if (0 == nOnes && (0 == strcmp(name, "D")))
        {
            Printf(Tee::PriNormal,
                    "NOTE: revision 002 of HDCP ROM present.\n");
        }
        return RC::ILWALID_ENCRYPTION_KEY;
    }
    return OK;
}

//----------------------------------------------------------------------------
static LwU8 Shuffle
(
    LwU8 din,
    LwU8 s,
    HDCPUpstream::LfsrInitMode init,
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
    if (init != HDCPUpstream::LfsrInitMode::NoInit)
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

//----------------------------------------------------------------------------
static LwU8 Combiner
(
    LwU8* lfsr0_output,
    LwU8* lfsr1_output,
    LwU8* lfsr2_output,
    LwU8* lfsr3_output,
    HDCPUpstream::LfsrInitMode init
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

//----------------------------------------------------------------------------
static void LFSR0
(
    LwU8* lfsr0_output,
    LwU64 data,
    LwU64 key,
    HDCPUpstream::LfsrInitMode init
)
{
    int i;

    lfsr0_output[2] = s_lfsr0[22];
    lfsr0_output[1] = s_lfsr0[14];
    lfsr0_output[0] = s_lfsr0[7];

    if (init==HDCPUpstream::LfsrInitMode::InitA)
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
    else if (init==HDCPUpstream::LfsrInitMode::InitB)
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
    HDCPUpstream::LfsrInitMode init
)
{
    int i;
    lfsr1_output[2] = s_lfsr1[23];
    lfsr1_output[1] = s_lfsr1[15];
    lfsr1_output[0] = s_lfsr1[7];

    if (init==HDCPUpstream::LfsrInitMode::InitA)
    {
        for (i=23;i>18;i--)
            s_lfsr1[i] = data & BitM64(i-7) ? 1 : 0;

        s_lfsr1[18] = key & BitM64(19) ? 0 : 1;

        for (i=17;i>13;i--)
            s_lfsr1[i] = data & BitM64(i-6) ? 1 : 0;

        for (i=13;i>=0;i--)
            s_lfsr1[i] = key & BitM64(i+14) ? 1 : 0;
    }
    else if (init==HDCPUpstream::LfsrInitMode::InitB)
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
    HDCPUpstream::LfsrInitMode init
)
{
    int i;

    lfsr2_output[2] = s_lfsr2[25];
    lfsr2_output[1] = s_lfsr2[16];
    lfsr2_output[0] = s_lfsr2[8];

    if (init==HDCPUpstream::LfsrInitMode::InitA)
    {
        for (i=25;i>21;i--)
            s_lfsr2[i] = data & BitM64(i+2) ? 1 : 0;

        s_lfsr2[21] = key & BitM64(36) ? 0 : 1;

        for (i=20;i>13;i--)
            s_lfsr2[i] = data & BitM64(i+3) ? 1 : 0;

        for (i=13;i>=0;i--)
            s_lfsr2[i] = key & BitM64(i+28) ? 1 : 0;
    }
    else if (init==HDCPUpstream::LfsrInitMode::InitB)
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
    HDCPUpstream::LfsrInitMode init
)
{
    int i;

    lfsr3_output[2] = s_lfsr3[26];
    lfsr3_output[1] = s_lfsr3[17];
    lfsr3_output[0] = s_lfsr3[8];

    if (init==HDCPUpstream::LfsrInitMode::InitA)
    {
        for (i=26;i>21;i--)
            s_lfsr3[i] = data & BitM64(i+13) ? 1 : 0;

        s_lfsr3[21] = key & BitM64(51) ? 0 : 1;

        for (i=20;i>13;i--)
            s_lfsr3[i] = data & BitM64(i+14) ? 1 : 0;

        for (i=13;i>=0;i--)
            s_lfsr3[i] = key & BitM64(i+42) ? 1 : 0;
    }
    else if (init==HDCPUpstream::LfsrInitMode::InitB)
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

// #if defined(DEBUG_HDCP_UP)
// static void PrintLfsr
// (
//     bool isGlobal,
//     int lfsrNum,
//     LwU8* pLfsr,
//     size_t lfsrSize
// )
// {
//     int i;
//     Printf(Tee::PriDebug, "  %slfsr%d%s is (ms) ",
//             isGlobal ? "s_" : "",
//             lfsrNum,
//             isGlobal ? "" : "_output");
//
//     int iStart = isGlobal ? 27 : 3;
//
//     for (i = iStart; i >= 0; i--)
//     {
//         if (i < (int)lfsrSize)
//             Printf(Tee::PriDebug, "%d", pLfsr[i]);
//         else
//             Printf(Tee::PriDebug, " ");
//
//         if (0 == i%4)
//             Printf(Tee::PriDebug, " ");
//     }
//     Printf(Tee::PriDebug, " (ls)\n");
// }
// static void PrintGlobalState()
// {
//     PrintLfsr(true, 0, s_lfsr0, sizeof(s_lfsr0));
//     PrintLfsr(true, 1, s_lfsr1, sizeof(s_lfsr1));
//     PrintLfsr(true, 2, s_lfsr2, sizeof(s_lfsr2));
//     PrintLfsr(true, 3, s_lfsr3, sizeof(s_lfsr3));
//
//     Printf(Tee::PriDebug, "  s_A is (ms) %d %d %d %d (ls)",
//             s_A[3], s_A[2], s_A[1], s_A[0]);
//     Printf(Tee::PriDebug, "  s_B is (ms) %d %d %d %d (ls)\n",
//             s_B[3], s_B[2], s_B[1], s_B[0]);
// }
// #endif

//----------------------------------------------------------------------------
static LwU8  OneWay
(
    LwU64 data,
    LwU64 key,
    HDCPUpstream::LfsrInitMode mode
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
//         mode == HDCPUpstream::LfsrInitMode::InitA ? "initA" :
//             mode == HDCPUpstream::LfsrInitMode::InitB ? "initB" : "",
//         data,
//         key,
//         Result);
//     PrintGlobalState();
//     PrintLfsr(false, 0, lfsr0_output, sizeof(lfsr0_output));
//     PrintLfsr(false, 1, lfsr1_output, sizeof(lfsr1_output));
//     PrintLfsr(false, 2, lfsr2_output, sizeof(lfsr2_output));
//     PrintLfsr(false, 3, lfsr3_output, sizeof(lfsr3_output));
// #endif

    return(Result);
}

//----------------------------------------------------------------------------
static void SumKu
(
    LwU64 *pKu,
    LwU64 Ksv,
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
        LwU64 x = rnd.GetRandom();
        x = (x << 32) | rnd.GetRandom();
        LwU64 key = Keys[i] ^ x;

        if (Ksv & BitM64(i))
            lwSum += key;
    }

    lwSum &= MASK56;
    *pKu = lwSum;

    return;
}

#if defined(DEBUG_HDCP_UP)
//
// The purpose of this function is only to confirm that the xor
// obfuscation scheme of the C key array is working correctly.
//
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
        LwU64 x = rnd.GetRandom();
        x = (x << 32) | rnd.GetRandom();
        LwU64 key = Keys[i] ^ x;

        //Printf(Tee::PriLow,
        //    "Key[%d] = 0x%llxLL == 0x%llxLL ^ 0x%llxLL,\n",
        //    i, Keys[i], key, x);

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
#endif

//----------------------------------------------------------------------------
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

    Printf(Tee::PriLow, " CalcKp: input Cn 0x%llx\n", Cn);
    Printf(Tee::PriLow, "         input Cs 0x%llx\n", Cs);
    Printf(Tee::PriLow, "         input Dksv 0x%llx\n", Dksv);
    Printf(Tee::PriLow, "         input An 0x%llx\n", An);
    Printf(Tee::PriLow, "         input Bksv 0x%llx\n", Bksv);
    Printf(Tee::PriLow, "         input Status 0x%llx\n", Status);
    Printf(Tee::PriLow, "An and Bksv can be 0 in case of displayless upstream test\n");
    //Callwlate Ku
    SumKu(&Ku, Dksv, Ckey);

#if defined(DEBUG_HDCP_UP)
    Printf(Tee::PriLow,
        " CalcKp: Ku 0x%llx\n", Ku);
#endif

    // K1' = oneWay-A56(Ku', lsb40(Cn))
    Cn56 = Cn;
    PartCn = Cn56 & MASK40;
    OneWay(PartCn, Ku, HDCPUpstream::LfsrInitMode::InitA);

    for (i=0; i<56+32; i++)
    {
        K1 = K1 >> 1;
        K1 |= OneWay(PartCn, Ku, HDCPUpstream::LfsrInitMode::NoInit) ? MSB56 : 0;
    }

#if defined(DEBUG_HDCP_UP)
    Printf(Tee::PriLow,
        " CalcKp: K1 0x%llx\n", K1);
#endif

    // K2' = oneWay-A56(K1', Bksv)
    OneWay(Bksv, K1, HDCPUpstream::LfsrInitMode::InitA);
    for (i=0; i<56+32; i++)
    {
        K2 = K2 >> 1;
        K2 |= OneWay(Bksv, K1, HDCPUpstream::LfsrInitMode::NoInit) ? MSB56 : 0;
    }

#if defined(DEBUG_HDCP_UP)
    Printf(Tee::PriLow,
        " CalcKp: K2 0x%llx\n", K2);
#endif

    // K3' = oneWay-A56(K2', msb40(An))
    An >>= 24;
    OneWay(An, K2, HDCPUpstream::LfsrInitMode::InitA);
    for (i=0; i<56+32; i++)
    {
        K3 = K3 >> 1;
        K3 |= OneWay(An, K2, HDCPUpstream::LfsrInitMode::NoInit) ? MSB56 : 0;
    }

#if defined(DEBUG_HDCP_UP)
    Printf(Tee::PriLow,
        " CalcKp: K3 0x%llx\n", K3);
#endif

    // K4' = oneWay-A56(K3', status || msb24(Cn))
    StatCn = ((Status & 0x0000ffffLL) << 24) | (Cn >> 40);

    OneWay(StatCn, K3, HDCPUpstream::LfsrInitMode::InitA);
    for (i=0; i<56+32; i++)
    {
        K4 = K4 >> 1;
        K4 |= OneWay(StatCn, K3, HDCPUpstream::LfsrInitMode::NoInit) ? MSB56 : 0;
    }

#if defined(DEBUG_HDCP_UP)
    Printf(Tee::PriLow,
        " CalcKp: K4 0x%llx\n", K4);
#endif

    if (1 == DRF_VAL(0073, _CTRL_SPECIFIC_HDCP_CTRL_STATUS_S, _CS_CAPABLE, Status))
    {
        // Kp' = oneWay-A56(K4', Cs)
        OneWay(Cs, K4, HDCPUpstream::LfsrInitMode::InitA);

        for (i=0; i<56+32; i++)
        {
            Kp = Kp >> 1;
            Kp |= OneWay(Cs, K4, HDCPUpstream::LfsrInitMode::NoInit) ? MSB56 : 0;
        }
    }
    else
    {
        Kp = K4;
    }

#if defined(DEBUG_HDCP_UP)
    Printf(Tee::PriLow,
        " CalcKp: Kp 0x%llx\n", Kp);
#endif

    return(Kp);
}
