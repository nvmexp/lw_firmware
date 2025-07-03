/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// CheetAh Consumer Electronics Control (CEC) test

#include "core/include/tee.h"
#include "gpu/include/gpusbdev.h"
#include "gputest.h"
#include "core/include/jscript.h"
#include "core/include/massert.h"
#include "gpu/js/fpk_comm.h"
#include "core/include/fpicker.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#include "cheetah/include/tegrasocdevice.h"
#include "core/include/display.h"
#include "gpu/display/tegra_disp.h"
#include "cheetah/dev/cec/cecdev.h"
#include "core/include/xp.h"
#include <set>
#include <vector>

// TegraCec Class
//
// This test sends random CEC frames on the CEC in a loopback fashion and
// validates that CheetAh receieves the same data that was transmitted as well
// as acknowledges all transmissions
//
// This test was derived from the hardware driver/test from the following
// locations:
//    //hw/ap_tlit1/drv/drvapi/runtest_cec (driver)
//    /home/aurora/traces/cec/so/cec_loopback (test)
//
// The test is divided into frames/loops.
//    Frame : A set of loops that share the same potential CEC addresses
//    Loop  : A single CEC transaction consisting of a header and zero or more
//            bits of data
class TegraCec : public GpuTest
{
public:
    TegraCec();

    bool IsSupported();
    void PrintJsProperties(Tee::Priority pri);
    RC Setup();
    RC Run();
    RC Cleanup();
    RC SetDefaultsForPicker(UINT32 idx);

    SETGET_PROP(UseInterrupts,   bool);
    SETGET_PROP(LoopRetries,     UINT32);
    SETGET_PROP(PollingDelayUs,  INT32);
    SETGET_PROP(OverrideSrcAddr, INT32);
    SETGET_PROP(OverrideDstAddr, INT32);

private:
    //! Test configuration objects
    GpuTestConfiguration   *m_pTestConfig;
    FancyPicker::FpContext *m_pFpCtx;
    FancyPickerArray       *m_pPickers;

    //! Values set from JS
    bool       m_UseInterrupts;
    UINT32     m_LoopRetries;
    INT32      m_PollingDelayUs;
    INT32      m_OverrideSrcAddr;
    INT32      m_OverrideDstAddr;

    //! Per frame/loop randomized data
    set<UINT32>    m_Addrs;      //!< Per frame list of addresses used by CEC
    UINT32         m_SrcAddr;    //!< Per loop source address of the transaction
    UINT32         m_DstAddr;    //!< Per loop dest address of the transaction
    bool           m_bBroadcast; //!< Per loop flag if the transaction is a
                                 //!< broadcast
    vector<UINT08> m_Data;       //!< Per loop data for the transaction

    RC   Restart();
    void RandomizeOncePerLoop();
    RC   DoOneLoop();
    void PrintLoop(Tee::Priority pri, UINT64 timeUs);
};

//------------------------------------------------------------------------------
// Method implementation
//------------------------------------------------------------------------------

//! Default CEC address used by MODS
#define DEFAULT_CEC_ADDR 4

//----------------------------------------------------------------------------
//! \brief Constructor
//!
TegraCec::TegraCec()
 :  m_pTestConfig(nullptr)
   ,m_pFpCtx(nullptr)
   ,m_pPickers(nullptr)
   ,m_UseInterrupts(true)
   ,m_LoopRetries(1)
   ,m_PollingDelayUs(-1)
   ,m_OverrideSrcAddr(-1)
   ,m_OverrideDstAddr(-1)
   ,m_SrcAddr(0)
   ,m_DstAddr(0)
   ,m_bBroadcast(false)
{
    SetName("TegraCec");
    m_pTestConfig = GetTestConfiguration();
    m_pFpCtx = GetFpContext();

    CreateFancyPickerArray(FPK_TEGCEC_NUM_PICKERS);
    m_pPickers = GetFancyPickerArray();
}

//----------------------------------------------------------------------------
//! \brief Determine if the CheetAh CEC test is supported
//!
//! \return true if TegraCec is supported, false if not
bool TegraCec::IsSupported()
{
    Printf(Tee::PriDebug, "TegraCec test not supported on non-CheetAh display\n");
    return false;
}

//----------------------------------------------------------------------------
//! \brief Print the JS properties of the CheetAh CEC test
//!
//! \param pri : Print priority to use for the JS properties
//!
void TegraCec::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);

    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tUseInterrupts   : %s\n", m_UseInterrupts ? "true" : "false");
    Printf(pri, "\tLoopRetries     : %u\n", m_LoopRetries);
    Printf(pri, "\tPollingDelayUs  : %d\n", m_PollingDelayUs);
    Printf(pri, "\tOverrideSrcAddr : %d\n", m_OverrideSrcAddr);
    Printf(pri, "\tOverrideDstAddr : %d\n", m_OverrideDstAddr);
}

//----------------------------------------------------------------------------
//! \brief Setup the CheetAh CEC test
//!
//! \return OK if successful, not OK otherwise
//!
RC TegraCec::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());

    m_Addrs.insert(DEFAULT_CEC_ADDR);
    CHECK_RC(CheetAh::SocPtr()->GetCec()->Initialize(m_Addrs, m_UseInterrupts));
    CheetAh::SocPtr()->GetCec()->SetTimeoutMs(m_pTestConfig->TimeoutMs());
    if (m_PollingDelayUs != -1)
        CheetAh::SocPtr()->GetCec()->SetPollingDelayUs(m_PollingDelayUs);
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Run the CheetAh CEC test
//!
//! \return OK if successful, not OK otherwise
//!
RC TegraCec::Run()
{
    RC     rc;
    UINT32 StartLoop        = GetTestConfiguration()->StartLoop();
    UINT32 RestartSkipCount = GetTestConfiguration()->RestartSkipCount();
    UINT32 EndLoop          = StartLoop + GetTestConfiguration()->Loops();

    for (m_pFpCtx->LoopNum = StartLoop; m_pFpCtx->LoopNum < EndLoop;
         ++m_pFpCtx->LoopNum)
    {
        if ((m_pFpCtx->LoopNum == StartLoop) ||
            ((m_pFpCtx->LoopNum % RestartSkipCount) == 0))
        {
            m_pFpCtx->RestartNum = m_pFpCtx->LoopNum / RestartSkipCount;
            CHECK_RC( Restart() );
        }

        RandomizeOncePerLoop();

        const UINT64 startTime = Xp::GetWallTimeUS();
        UINT32 lwrRetries = m_LoopRetries;
        do
        {
            rc.Clear();
            rc = DoOneLoop();
        } while ((OK != rc) && --lwrRetries);

        // Print the number of attempts if it was more than one.  Need to
        // try multiple times because MODS cannot always meet the strict timing
        // requirements for handling CEC processing
        if ((m_LoopRetries - lwrRetries) > 1)
        {
            Printf((OK == rc) ? GetVerbosePrintPri() : Tee::PriHigh,
                   "Loop %d %s after %d attempts\n",
                   m_pFpCtx->LoopNum, (OK == rc) ? "passed" : "failed",
                   m_LoopRetries - lwrRetries);
        }
        PrintLoop((OK == rc) ? GetVerbosePrintPri() : Tee::PriHigh,
                  Xp::GetWallTimeUS() - startTime);
        CHECK_RC(rc);

        // When not using interrupts, the CEC device uses tight polling for
        // performing the transactions.  To avoid starving other threads,
        // yield after every loop
        if (!m_UseInterrupts)
            Tasker::Yield();
    }

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Cleanup the CheetAh CEC test
//!
//! \return OK if successful, not OK otherwise
//!
RC TegraCec::Cleanup()
{
    StickyRC rc;

    rc = CheetAh::SocPtr()->GetCec()->Shutdown();
    rc = GpuTest::Cleanup();

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Initialize the fancy pickers for the test.
//!
//! \param idx : fancy picker index to set the defaults for
//!
//! \return OK if set succeeded, not OK otherwise
//!
RC TegraCec::SetDefaultsForPicker(UINT32 idx)
{
    FancyPickerArray * pPickers = GetFancyPickerArray();
    FancyPicker * pFp = &((*pPickers)[idx]);

    switch (idx)
    {
        case FPK_TEGCEC_NUM_ADDRS:
            pFp->ConfigRandom();
            pFp->AddRandItem(1, CheetAh::Cec::MaxAddrs);
            pFp->AddRandItem(3, 1);
            pFp->AddRandRange(6, 1, CheetAh::Cec::MaxAddrs);
            pFp->CompileRandom();
            break;

        case FPK_TEGCEC_ADDR:
            pFp->ConfigRandom();
            pFp->AddRandRange(1, 0, CheetAh::Cec::MaxAddrs - 1);
            pFp->CompileRandom();
            break;

        case FPK_TEGCEC_BROADCAST:
            pFp->ConfigRandom();
            pFp->AddRandItem(9, 0);
            pFp->AddRandItem(1, 1);
            pFp->CompileRandom();
            break;

        case FPK_TEGCEC_NUM_DATA:
            pFp->ConfigRandom();
            pFp->AddRandItem(1, 0);
            pFp->AddRandItem(1, CheetAh::Cec::MaxData);
            pFp->AddRandRange(8, 0, CheetAh::Cec::MaxData);
            pFp->CompileRandom();
            break;

        case FPK_TEGCEC_DATA:
            pFp->ConfigRandom();
            pFp->AddRandRange(1, 0, 0xff);
            pFp->CompileRandom();
            break;

        default:
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Called once per "frame" to reseed the random generator and choose
//!        the CEC addresses that will be used
//!
//! \return OK if restart succeeded, not OK otherwise
//!
RC TegraCec::Restart()
{
    RC rc = OK;

    // Re-seed the random and restart all pickers
    m_pFpCtx->Rand.SeedRandom(m_pTestConfig->Seed() + m_pFpCtx->LoopNum);
    m_pPickers->Restart();

    Printf(GetVerbosePrintPri(),
           "\n\tRestart: loop %d, seed %#010x\n",
           m_pFpCtx->LoopNum,
           m_pTestConfig->Seed() + m_pFpCtx->LoopNum);

    UINT32 numAddrs = (*m_pPickers)[FPK_TEGCEC_NUM_ADDRS].Pick();
    m_Addrs.clear();
    for (UINT32 i = 0; i < numAddrs; i++)
    {
        UINT32 lwrAddr = i;
        if (numAddrs != CheetAh::Cec::MaxAddrs)
        {
            UINT32 retries = 0;
            do
            {
                lwrAddr = (*m_pPickers)[FPK_TEGCEC_ADDR].Pick();
            } while (m_Addrs.count(lwrAddr) && (retries++ < 10));
        }
        m_Addrs.insert(lwrAddr);
    }

    // Ensure that any overridden addresses are in the set of possible addresses
    // to be used by the CEC
    if (m_OverrideSrcAddr != -1)
        m_Addrs.insert(m_OverrideSrcAddr);
    if (m_OverrideDstAddr != -1)
        m_Addrs.insert(m_OverrideDstAddr);

    CHECK_RC(CheetAh::SocPtr()->GetCec()->SetAddrs(m_Addrs));
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Called once per "loop" to create a random CEC transaction
//!
void TegraCec::RandomizeOncePerLoop()
{
    vector<UINT32> addrVect(m_Addrs.begin(), m_Addrs.end());
    const UINT32 numAddrs = static_cast<UINT32>(addrVect.size());

    m_pFpCtx->Rand.Shuffle(numAddrs, &addrVect[0]);
    m_SrcAddr = addrVect[m_pFpCtx->Rand.GetRandom(0, numAddrs - 1)];
    m_DstAddr = addrVect[m_pFpCtx->Rand.GetRandom(0, numAddrs - 1)];
    m_bBroadcast = (*m_pPickers)[FPK_TEGCEC_BROADCAST].Pick() != 0;

    // Override the addresses as necessary
    if (m_OverrideSrcAddr != -1)
        m_SrcAddr = m_OverrideSrcAddr;
    if (m_OverrideDstAddr != -1)
    {
        // If overriding to the broadcast address, ensure that the CEC
        // transaction is flagged as broadcast
        m_DstAddr = m_OverrideDstAddr;
        if (m_DstAddr == CheetAh::Cec::BroadcastAddr)
            m_bBroadcast = true;
    }
    else if (m_bBroadcast)
    {
        // Otherwise if no overrides, ensure that if broadcasting that the
        // destination address is the broadcast address
        m_DstAddr = CheetAh::Cec::BroadcastAddr;
    }

    UINT32 numData = (*m_pPickers)[FPK_TEGCEC_NUM_DATA].Pick();
    m_Data.clear();
    for (UINT32 i = 0; i < numData; i++)
    {
        m_Data.push_back((*m_pPickers)[FPK_TEGCEC_DATA].Pick());
    }
}

//-----------------------------------------------------------------------------
//! \brief Called once per "loop" to send a random CEC transaction
//!
//! \return OK if the transaction succeeded and results were as expected, not
//!         OK otherwise
RC TegraCec::DoOneLoop()
{
    RC rc;
    CheetAh::Cec *pCec = CheetAh::SocPtr()->GetCec();
    vector<CheetAh::Cec::RxData> rxData;
    rxData.resize(m_Data.size());

    // Since doing a loopback and the CEC engine has only a single byte deep
    // fifo and we will potentially be sending up to 16 bytes, it is necessary
    // to start the transaction and then immediately begin receiving data
    // before waiting for the transmission to complete
    //
    // Loopback is achieved by selecting both a source and a destination
    // address that the CheetAh CEC engine will answer to
    size_t rxDataCount = 0;
    CHECK_RC(pCec->TransmitData(m_SrcAddr, m_DstAddr, m_Data));
    CHECK_RC(pCec->ReceiveData(&rxData));
    CHECK_RC(pCec->WaitTransmitComplete());
    CHECK_RC(pCec->WaitReceiveComplete(&rxDataCount));

    // Immediately fail if not enough data was received (received data includes
    // the header)
    bool   bFailed = ((m_Data.size() + 1) != rxDataCount);
    if (bFailed)
    {
        // Expect received data bytes to be the same as transmitted.
        //
        // All data sent as part of a broadcast transaction is "nak'd" otherwise
        // expect all data to be "ack'd"
        for (size_t i = 0; i < rxDataCount; i++)
        {
            UINT08 expectedData;
            if (i == 0)
            {
                expectedData =
                    DRF_NUM(_CEC, _TX_HEADER, _INITIATOR, m_SrcAddr) |
                    DRF_NUM(_CEC, _TX_HEADER, _DESTINATION, m_DstAddr);
            }
            else
            {
                expectedData = m_Data[i - 1];
            }
            if ((rxData[i].data != expectedData) ||
                (rxData[i].bAck != m_bBroadcast))
            {
                bFailed = true;
            }
        }
    }

    if (bFailed)
    {
        Printf(Tee::PriHigh, "%s : Failed at loop %d\n",
               GetName().c_str(), m_pFpCtx->LoopNum);
        rc = RC::GOLDEN_VALUE_MISCOMPARE;
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Print the loop information of a random CEC transaction
//!
void TegraCec::PrintLoop(Tee::Priority pri, UINT64 timeUs)
{
    Printf(pri,
           "\tLoop %d of %d:\n"
           "\t\tTimeUs    : %lld\n"
           "\t\tSrcAddr   : %u\n"
           "\t\tDstAddr   : %u\n"
           "\t\tBroadcast : %s\n"
           "\t\tData      : ",
           m_pFpCtx->LoopNum + 1,
           m_pTestConfig->StartLoop() + m_pTestConfig->Loops(),
           timeUs,
           m_SrcAddr,
           m_DstAddr,
           m_bBroadcast ? "true" : "false");
    for (size_t i = 0; i < m_Data.size(); i++)
    {
        if ((i != 0) && ((i % 8) == 0))
        {
            Printf(pri, "\n\t\t            ");
        }
        Printf(pri, "0x%02x ", m_Data[i]);
    }
    Printf(pri, "\n");
}

//------------------------------------------------------------------------------
// TegraCec object, properties and methods.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(TegraCec, GpuTest, "TegraCec Test");
CLASS_PROP_READWRITE(TegraCec, UseInterrupts, bool,
                     "Use interrupts with the CEC engine (default = false)");
CLASS_PROP_READWRITE(TegraCec, LoopRetries, UINT32,
                     "Number of times attempt a transaction (default = 1)");
CLASS_PROP_READWRITE(TegraCec, PollingDelayUs, INT32,
                     "When not using interrupts, delay in uS between interrupt "
                     "processing (default = -1 (HW default))");
CLASS_PROP_READWRITE(TegraCec, OverrideSrcAddr, INT32,
                     "Override the source address of the CEC transaction");
CLASS_PROP_READWRITE(TegraCec, OverrideDstAddr, INT32,
                     "Override the destination address of the CEC transaction");
