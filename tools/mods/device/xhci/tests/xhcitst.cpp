/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "xhcitst.h"
#include "device/xhci/xhcictrlmgr.h"
#include "cheetah/include/controller.h"
#include "core/include/xp.h"
#include "device/include/memtrk.h"
#include <unistd.h>

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "XhciTest"

XhciTest::XhciTest()
{
    LOG_ENT();

    m_pDevice       = NULL;
    m_NumSectors    = 1;
    m_Lba           = 0x100;
    m_PerfTestMode  = TEST_MODE_ALL;
    m_SlotId        = 0;
    m_UsedBytes     = 0;

    LOG_EXT();
}

XhciTest::~XhciTest()
{
    LOG_ENT();
    LOG_EXT();
}

//=============================================================================
// Framework stuff above
//=============================================================================
RC XhciTest::WrRdTest(UINT32 Mode)
{
    LOG_ENT();
    RC rc;
    if( !m_IsInit || !m_pDevice )
    {
        Printf(Tee::PriError, "Test hasn't been initialized yes, call Init()\n");
        return RC::WAS_NOT_INITIALIZED;
    }
    if( Mode & (~0x3) )
    {
        Printf(Tee::PriError, "WrRdTest(0x1-Wr Only|0x2-Rd Only|0x3-All)\n");
        return RC::ILWALID_INPUT;
    }
    if(!m_UsedBytes)
    {
        UINT32 sectorSize;
        CHECK_RC(m_pDevice->GetSectorSize(m_SlotId, &sectorSize));
        Printf(Tee::PriLow, "Bytes Per Sector = 0x%08x\n", sectorSize);
        m_UsedBytes = sectorSize * m_NumSectors;
    }

    MemoryFragment::FRAGLIST *pWrFragList = nullptr;
    MemoryFragment::FRAGLIST *pRdFragList = nullptr;

    if(Mode & TEST_MODE_WRITE)
    {
        CHECK_RC(GenerateFragList(m_UsedBytes, &pWrFragList));
        // fill in the pattern
        FillBuffer(pWrFragList);
    }
    if(Mode & TEST_MODE_READ)
    {
        CHECK_RC(GenerateFragList(m_UsedBytes, &pRdFragList));
    }
    CHECK_RC(WriteReadTest(Mode,
                           m_Lba,
                           m_NumSectors,
                           pRdFragList,
                           pWrFragList));
    LOG_EXT();
    Printf(Tee::PriHigh, " %s::%s()\n", CLASS_NAME, FUNCNAME);

    return rc;
}

RC XhciTest::Initialize_Private()
{
    RC rc;
    // Initalize the HW buffer limitation
    CHECK_RC(SetHwBufAttr(16, 65536, 1));
    // Initalize the default prd parameters, change DatasWidth from 2 to 1
    CHECK_RC(SetBufParam(1, 10, 1, 65536, 0, 128, 1));

    vector<UINT32> ctrlIndex;
    CHECK_RC(MaskToIndexes(m_CtrlMask, &ctrlIndex));
    CHECK_RC(XhciControllerMgr::Mgr()->Get(ctrlIndex[0],
                                reinterpret_cast<Controller**>(&m_pDevice)));

    // PrintProperties();
    return OK;
}

RC XhciTest::Uninitialize_Private()
{
    // nothing to do rigth now
    return OK;
}

RC XhciTest::InitProperties_Private()
{
    RC rc;
    JavaScriptPtr pJs;

    CHECK_RC(pJs->GetProperty(m_pTestObj, "iSlotId", &m_SlotId));
    CHECK_RC(pJs->GetProperty(m_pTestObj, "iNumSectors", &m_NumSectors));
    return OK;
}

RC XhciTest::PrintInfo_Private()
{
//    Printf(Tee::PriNormal, "PrintMask: Bit %d - Print Tx Data \n", TX_DATA);
//    Printf(Tee::PriNormal, "           Bit %d - Print Rx Data \n", RX_DATA);
    return OK;
}

RC XhciTest::PrintPropertiesInit_Private(Tee::Priority Pri)
{
    Printf(Pri, "   iSlotId     : %5d    The SlotId assigned for USB testing device \n", m_SlotId);
    Printf(Pri, "   iNumSectors : %5d    The number of sectors to read / write \n", m_NumSectors);
    return OK;
}
RC XhciTest::PrintPropertiesNoInit_Private(Tee::Priority Pri)
{
    Printf(Pri, "    Lba        : %5d    The LBA for WrRdTest() \n", m_Lba);
    return OK;
}

RC XhciTest::PrintTestName_Private(Tee::Priority Pri)
{
    Printf(Pri, "XhciTest");
    return OK;
}

RC XhciTest::BlockRd(UINT32 BlockAddr,
                    UINT32 BlockCnt,
                    MemoryFragment::FRAGLIST* pFragList)
{
    LOG_ENT();
    RC rc = OK;
    MASSERT(m_pDevice);
    MASSERT(pFragList);
    CHECK_RC(m_pDevice->CmdRead10(m_SlotId, BlockAddr, BlockCnt, pFragList));
    LOG_EXT();

    return rc;
}

RC XhciTest::BlockWr(UINT32 BlockAddr,
                    UINT32 BlockCnt,
                    MemoryFragment::FRAGLIST* pFragList)
{
    LOG_ENT();
    RC rc = OK;
    MASSERT(m_pDevice);
    MASSERT(pFragList);
    CHECK_RC(m_pDevice->CmdWrite10(m_SlotId, BlockAddr, BlockCnt, pFragList));
    LOG_EXT();

    return rc;
}

RC XhciTest::InitCCT()
{   // setup DDirect partition in Mem pool
    // fill MEM pool with data pattern
    // allocate systom memory for DMA
    RC rc;
    LOG_ENT();
    JavaScriptPtr pJs;
    CHECK_RC(pJs->GetProperty(m_pTestObj, "iCtrlMask", &m_CtrlMask));

    vector<UINT32> ctrlIndex;
    CHECK_RC(MaskToIndexes(m_CtrlMask, &ctrlIndex));
    CHECK_RC(XhciControllerMgr::Mgr()->Get(ctrlIndex[0],
                                reinterpret_cast<Controller**>(&m_pDevice)));
    // do controller init so we don't rely on JS to do that
    CHECK_RC(m_pDevice->Init(Controller::INIT_BAR));

    // partition MEM pool and fill it with pattern
    if ( !m_vPattern32.size() )
    {
        Printf(Tee::PriError, "Data Pattern not set, call SetPattern32() first\n");
        return RC::BAD_PARAMETER;
    }

    CHECK_RC(m_pDevice->CsbDmaPartition());
    CHECK_RC(m_pDevice->CsbDmaFillPattern(&m_vPattern32));
    // Get the maximum Byte size that DMA engine can handle
    UINT32 bufferSize = m_pDevice->CsbGetBufferSize();
    // Initalize the HW buffer limitation
    CHECK_RC(SetHwBufAttr(1, bufferSize));
    // Initalize the default prd parameters, all default paramters work fine
    CHECK_RC(SetBufParam(1, 1, 16, 65536, 0, 0));

    // create buffer andfragment list
    MemoryFragment::FRAGLIST *pFragList = nullptr;
    CHECK_RC(GenerateFragList(bufferSize, &pFragList));
    MASSERT(pFragList);
    if(pFragList->size()>1)
    {
        Printf(Tee::PriError, "Buffer Build allocated more than one segments\n");
        return RC::SOFTWARE_ERROR;
    }
    m_vpWrFragList.clear();
    // save fraglist
    m_vpWrFragList.push_back(pFragList);
    PHYSADDR pBuf = Platform::VirtualToPhysical((*pFragList)[0].Address);
    Printf(Tee::PriLow, "%d Byte allocated @ 0x%8x \n", (*pFragList)[0].ByteSize, (UINT32)pBuf);

    LOG_EXT();
    return OK;
}

RC XhciTest::UnInitCCT()
{
    LOG_ENT();
    m_vpWrFragList.clear();
    LOG_EXT();
    return OK;
}

RC XhciTest::StartCCTStress()
{   // trigger the DMA write to system MEM and enable the status bit
    LOG_ENT();
    RC rc;
    if(!m_pDevice)
    {
        Printf(Tee::PriError, "Controller pointer hasn't been init");
        return RC::WAS_NOT_INITIALIZED;
    }
    MemoryFragment::FRAGLIST *pFragList = m_vpWrFragList[0];
    PHYSADDR pBuf = Platform::VirtualToPhysical((*pFragList)[0].Address);
    // 'true' for write
    CHECK_RC(m_pDevice->CsbDmaTrigger(pBuf, true));
    LOG_EXT();
    return OK;
}

RC XhciTest::IsCCTComplete(bool *pIsComplete)
{   // check hte status fifo
    LOG_ENT();
    RC rc;
    if(!m_pDevice)
    {
        Printf(Tee::PriError, "Controller pointer hasn't been init");
        return RC::WAS_NOT_INITIALIZED;
    }
    CHECK_RC(m_pDevice->CsbDmaCheckStatus(pIsComplete));
    LOG_EXT();
    return OK;
}

RC XhciTest::GetCCTBuffer(MemoryFragment::FRAGLIST **ppFragList )
{
    LOG_ENT();
    if(!m_vpWrFragList.size())
    {
        Printf(Tee::PriError, "Buffer not initialized, call InitCCT() \n");
        return RC::WAS_NOT_INITIALIZED;
    }
    *ppFragList = m_vpWrFragList[0];
    LOG_EXT();
    return OK;
}

RC XhciTest::GetStatus(UINT32 *pDataSize, UINT64 *pXferCount)
{
    RC rc;
    if(!m_pDevice)
    {
        Printf(Tee::PriError, "Test hasn't been init");
        return RC::WAS_NOT_INITIALIZED;
    }
    *pDataSize = m_pDevice->CsbGetBufferSize();
    bool isDummy;
    CHECK_RC(m_pDevice->CsbDmaCheckStatus(&isDummy, pXferCount));
    return OK;
}

RC XhciTest::PerfRun(UINT32 MinBW /* = 0 */)
{
    RC rc = OK;
    UINT64 totalBytes;
    UINT32 sectorSize;
    UINT32 loopCount;
    UINT64 kbPerSec;

    if ( !m_pDevice )
    {
        Printf(Tee::PriError, "Controller Index not set");
        return RC::WAS_NOT_INITIALIZED;
    }
    if(m_PerfTestMode == 0)
    {
        Printf(Tee::PriWarn, "PerfTestMode not set, set to 0x2 - Read (0x1 - Write) \n");
        m_PerfTestMode = TEST_MODE_READ;
    }

    CHECK_RC(m_pDevice->GetSectorSize(m_SlotId, &sectorSize));
    Printf(Tee::PriLow, "Bytes Per Sector = 0x%08x\n", sectorSize);

    MemoryFragment::FRAGLIST myFragList;
    CHECK_RC(HandlePerfFraglist(sectorSize * m_NumSectors, &myFragList));
    // fill in the pattern
    FillBuffer(&myFragList);

    totalBytes = static_cast<UINT64>(m_Loops) * static_cast<UINT64>(m_NumSectors) * static_cast<UINT64>(sectorSize);
    Printf(Tee::PriNormal, "%s%s Loops %d, Sectors %d, Total: ",
           (m_PerfTestMode&TEST_MODE_WRITE)?"Write":"",
           (m_PerfTestMode&TEST_MODE_READ)?"Read":"",
           m_Loops, m_NumSectors);
    PrintReportResult(0, totalBytes);
    Printf(Tee::PriNormal, "\n");

    // record time stamp
    UINT64 start = Xp::QueryPerformanceCounter();
    for(loopCount = 0; (loopCount < m_Loops) || (!IsExiting()); loopCount++)
    {
        if(m_PerfTestMode & TEST_MODE_WRITE)
        {
            if( XHCI_DEBUG_MODE_USE_SCSI_CMD16 & m_pDevice->GetDebugMode() )
            {
                CHECK_RC_CLEANUP(m_pDevice->CmdWrite16(m_SlotId, m_Lba, m_NumSectors, &myFragList));
            }
            else
            {
                CHECK_RC_CLEANUP(m_pDevice->CmdWrite10(m_SlotId, m_Lba, m_NumSectors, &myFragList));
            }
            Printf(Tee::PriLow, "Writing Done\n");
        }
        if(m_PerfTestMode & TEST_MODE_READ)
        {
            if( XHCI_DEBUG_MODE_USE_SCSI_CMD16 & m_pDevice->GetDebugMode() )
            {
                CHECK_RC_CLEANUP(m_pDevice->CmdRead16(m_SlotId, m_Lba, m_NumSectors, &myFragList));
            }
            else
            {
                CHECK_RC_CLEANUP(m_pDevice->CmdRead10(m_SlotId, m_Lba, m_NumSectors, &myFragList));
            }
            Printf(Tee::PriLow, "Reading Done\n");
        }
    }
    totalBytes = static_cast<UINT64>(loopCount) * static_cast<UINT64>(m_NumSectors) * static_cast<UINT64>(sectorSize) * (m_PerfTestMode==TEST_MODE_ALL?2:1);
    kbPerSec = PrintReportResult(start, totalBytes);
    if(MinBW && (kbPerSec < MinBW))
    {
        Printf(Tee::PriError,
               "/n Xfer Rate < minBW ( %dKB/s )\n", MinBW);
        CHECK_RC_CLEANUP(RC::HW_STATUS_ERROR);
    }

Cleanup:
    HandlePerfFraglist(0, &myFragList);

    return rc;
}

// test on multiple USB flash devices
RC XhciTest::MultiPortTest(vector<UINT32> & vSlotId)
{
    LOG_ENT();
    RC rc;
    if( !m_IsInit || !m_pDevice )
    {
        Printf(Tee::PriError,
               "Test hasn't been correctly initialized yes, set test properties and call Init()\n");
        return RC::WAS_NOT_INITIALIZED;
    }

    UINT32 loopCount;
    UINT32 sectorSize;
    CHECK_RC(m_pDevice->GetSectorSize(m_SlotId, &sectorSize));
    Printf(Tee::PriLow, "Bytes Per Sector = 0x%08x\n", sectorSize);

    MemoryFragment::FRAGLIST myFragList;
    CHECK_RC(HandlePerfFraglist(sectorSize * m_NumSectors, &myFragList));
    // fill in the pattern
    FillBuffer(&myFragList);

    vector<UINT32> compareData;
    if (TEST_MODE_ALL == m_PerfTestMode)
    {   // both write/read enabled, save data for integrity check later
        MemoryFragment::DumpToVector(&myFragList, &compareData);
    }

    if( 0 == m_PerfTestMode )
    {
        Printf(Tee::PriNormal, "PerfTestMode not set, set to 0x2 - Read (0x1 - Write) \n");
        m_PerfTestMode = TEST_MODE_READ;
    }

    UINT64 totalBytes = m_Loops * m_NumSectors * sectorSize;
    Printf(Tee::PriNormal, "%s%s Loops %d, Sectors %d, Total: ",
           (m_PerfTestMode&TEST_MODE_WRITE)?"Write":"",
           (m_PerfTestMode&TEST_MODE_READ)?"Read":"",
            m_Loops, m_NumSectors);
    PrintReportResult(0, totalBytes);
    Printf(Tee::PriNormal, " per Dev (total %d)\n", static_cast<UINT32>(vSlotId.size()));

    // record time stamp
    UINT64 printSecond = 15;
    UINT64 start = Xp::QueryPerformanceCounter();

    for(loopCount = 0; (loopCount < m_Loops) || (!IsExiting()); loopCount++)
    {
        // Print on screen info every 15 seconds
        if ( ((Xp::QueryPerformanceCounter() - start) / Xp::QueryPerformanceFrequency()) >= printSecond  )
        {
            printSecond += 15;
            Printf(Tee::ScreenOnly, "  Loop %d of %d \n", loopCount, m_Loops);
        }
        if (TEST_MODE_WRITE & m_PerfTestMode)
        {
            CHECK_RC_CLEANUP(MultiCmd(vSlotId, true, &myFragList));
        }
        if (TEST_MODE_READ & m_PerfTestMode)
        {
            CHECK_RC_CLEANUP(MultiCmd(vSlotId, false, &myFragList));
        }
    }

    totalBytes = loopCount * m_NumSectors * sectorSize * (m_PerfTestMode==TEST_MODE_ALL?2:1);
    PrintReportResult(start, totalBytes * static_cast<UINT32>(vSlotId.size()));

    if (TEST_MODE_ALL == m_PerfTestMode)
    {   // both write/read enabled, do integrity check
        rc = MemoryFragment::CompareVector(&myFragList, &compareData);
    }

Cleanup:
    HandlePerfFraglist(0, &myFragList);

    if (OK == rc)
    {
        Printf(Tee::PriNormal, "\nMultiPortTest passed... \n");
    }
    else
    {
        Printf(Tee::PriError, "\nMultiPortTest Failed \n");
    }
    LOG_EXT();
    return rc;
}

RC XhciTest::MultiCmd(vector<UINT32> & vSlotId,
                      bool IsWrite,
                      MemoryFragment::FRAGLIST *pFragList)
{
    RC rc;
    EP_INFO myEpInfo;
    vector<EP_INFO> vEpInfo;

    // issue the CBWs
    for(UINT32 i = 0; i < vSlotId.size(); i++)
    {
        if( IsWrite )
        {
            Printf(Tee::PriLow, "Issuing the Write10 CBW on Slot %d", vSlotId[i]);
            CHECK_RC(m_pDevice->BulkSendCbwWrite10(vSlotId[i], m_Lba, m_NumSectors, &myEpInfo));
        }
        else
        {
            Printf(Tee::PriLow, "Issuing the Read10 CBW on Slot %d", vSlotId[i]);
            CHECK_RC(m_pDevice->BulkSendCbwRead10(vSlotId[i], m_Lba, m_NumSectors, &myEpInfo));
        }
        vEpInfo.push_back(myEpInfo);
    }
    Printf(Tee::PriLow, " \nWait for CBW completion ");
    // wait for all CBWs transfered
    for(UINT32 i = 0; i < vSlotId.size(); i++)
    {
        CHECK_RC(m_pDevice->WaitXfer(vSlotId[i], vEpInfo[i].EpNum, vEpInfo[i].IsDataOut));
        // release buffer for CBW
        MemoryTracker::FreeBuffer(vEpInfo[i].pBufAddress);
        Printf(Tee::PriLow, " Slot %d", vSlotId[i]);
    }
    Printf(Tee::PriLow, "  All done\n");
    // issue all the normal TRBs for data payload
    for(UINT32 i = 0; i < vSlotId.size(); i++)
    {
        if( IsWrite )
        {   // since we use same Endpoint for sending CBW and data payload, we re-use vEpInfo
            CHECK_RC(m_pDevice->SetupTd(vSlotId[i], vEpInfo[i].EpNum, vEpInfo[i].IsDataOut, pFragList));
            // save the EP info for later completion check
            myEpInfo.EpNum = vEpInfo[i].EpNum;
            myEpInfo.IsDataOut = vEpInfo[i].IsDataOut;
        }
        else
        {
            CHECK_RC(m_pDevice->BulkGetInEpInfo(vSlotId[i], &myEpInfo));
            CHECK_RC(m_pDevice->SetupTd(vSlotId[i], myEpInfo.EpNum, myEpInfo.IsDataOut, pFragList));
        }
    }
    // wait for all data transfered (wait for the last TRB)
    for(UINT32 i = 0; i < vSlotId.size(); i++)
    {
        CHECK_RC(m_pDevice->WaitXfer(vSlotId[i], myEpInfo.EpNum, myEpInfo.IsDataOut));
    }
    Printf(Tee::PriLow, "All data TRB completed \n");
    // issue the CSWs
    vEpInfo.clear();
    for(UINT32 i = 0; i < vSlotId.size(); i++)
    {
        CHECK_RC(m_pDevice->BulkSendCsw(vSlotId[i], &myEpInfo));
        vEpInfo.push_back(myEpInfo);
    }
    // wait for all CBWs transfered
    for(UINT32 i = 0; i < vSlotId.size(); i++)
    {
        CHECK_RC(m_pDevice->WaitXfer(vSlotId[i], vEpInfo[i].EpNum, vEpInfo[i].IsDataOut));
        // release buffer for CSW
        MemoryTracker::FreeBuffer(vEpInfo[i].pBufAddress);
    }
    return OK;
}

RC XhciTest::U0U1Test(UINT32 Port)
{
    LOG_ENT();
    RC rc;
    if( !m_IsInit || !m_pDevice )
    {
        Printf(Tee::PriError, "Test hasn't been initialized yes, call Init()\n");
        return RC::WAS_NOT_INITIALIZED;
    }
    UINT32 pls;
    // program the U1 timeout and wait for device goes to U1
    CHECK_RC(m_pDevice->SetU1U2Timeout(Port, 127)); // 127 us

    UINT32 msElapse = 0;
    CHECK_RC(m_pDevice->PortPLS(Port, &pls));
    while( (pls != XHCI_REG_PORTSC_PLS_VAL_U1)
           && (msElapse < 1000))    // 100mS
    {
        Tasker::Sleep(0.1);
        msElapse++;
        CHECK_RC(m_pDevice->PortPLS(Port, &pls));
    }

    if( pls != XHCI_REG_PORTSC_PLS_VAL_U1 )
    {
        Printf(Tee::PriError, "Port is not in U1, PLS = %d\n", pls);
        return RC::HW_STATUS_ERROR;
    }
    Printf(Tee::PriNormal, "Enter U1 after %d loops, PLS = %d, now run WrRdTest\n",
           msElapse, pls);
    Tasker::Sleep(100); //! TODO: WAR for Orin-SLT(F0), will be removed
    CHECK_RC(WrRdTest());

    LOG_EXT();
    return OK;
}

RC XhciTest::U0U2Test(UINT32 Port)
{
    LOG_ENT();
    RC rc;
    if( !m_IsInit || !m_pDevice )
    {
        Printf(Tee::PriError, "Test hasn't been initialized yes, call Init()\n");
        return RC::WAS_NOT_INITIALIZED;
    }

    UINT32 pls;
    CHECK_RC(m_pDevice->SetU1U2Timeout(Port, 0, 128)); // Set U2 timeout to be 32ms

    UINT32 msElapse = 0;
    CHECK_RC(m_pDevice->PortPLS(Port, &pls));
    while( (pls != XHCI_REG_PORTSC_PLS_VAL_U2)
           && (msElapse < 300) )    // 300mS
    {
        Tasker::Sleep(1);
        msElapse++;
        CHECK_RC(m_pDevice->PortPLS(Port, &pls));
    }

    if( pls != XHCI_REG_PORTSC_PLS_VAL_U2 )
    {
        Printf(Tee::PriError, "Port is not in U2, PLS = %d\n", pls);
        return RC::HW_STATUS_ERROR;
    }
    Printf(Tee::PriNormal, "Enter U2 after %d loops, PLS = %d, now run WrRdTest\n",
           msElapse, pls);
    Tasker::Sleep(100); //! TODO: WAR for Orin-SLT(F0), will be removed
    CHECK_RC(WrRdTest());

    LOG_EXT();
    return OK;
}

RC XhciTest::U0U3Test(UINT32 Port, UINT32 WrRdSleepMs /*= 1000*/)
{
    LOG_ENT();
    RC rc;
    if( !m_IsInit || !m_pDevice )
    {
        Printf(Tee::PriError, "Test hasn't been initialized yes, call Init()\n");
        return RC::WAS_NOT_INITIALIZED;
    }

    UINT32 pls;
    CHECK_RC(m_pDevice->SetU1U2Timeout(Port, 0, 0)); //Clear U1, U2 timeout
    //Go back to U0
    CHECK_RC(m_pDevice->PortPLS(Port, nullptr, XHCI_REG_PORTSC_PLS_VAL_U0));
    Tasker::Sleep(10);

    for(UINT32 i = 0; i < 10; i++)
    {
        Printf(Tee::PriNormal, "Loop %d\n", i);
        CHECK_RC(m_pDevice->PortPLS(Port, nullptr, XHCI_REG_PORTSC_PLS_VAL_U3));
        rc = m_pDevice->WaitPLS(Port, XHCI_REG_PORTSC_PLS_VAL_U3, 2000);
        CHECK_RC(m_pDevice->PortPLS(Port, &pls));
        if( OK != rc )
        {
            Printf(Tee::PriError, "Port is not in U3, PLS = %d\n", pls);
            return RC::HW_STATUS_ERROR;
        }
        // Stay in U3 for 10ms
        Printf(Tee::PriNormal, "Link stay in U3 for 10ms...\n");
        Tasker::Sleep(10);
        // then wake to U0
        Printf(Tee::PriNormal, "PLS = %d, Now wake to U0. \n", pls);
        CHECK_RC(m_pDevice->PortPLS(Port, nullptr, XHCI_REG_PORTSC_PLS_VAL_U0));
        // wait PLS become U0
        rc = m_pDevice->WaitPLS(Port, XHCI_REG_PORTSC_PLS_VAL_U0, 2000);
        if( OK != rc )
        {
            CHECK_RC(m_pDevice->PortPLS(Port, &pls));
            Printf(Tee::PriError, "Port is not in U0, PLS = %d\n", pls);
            return RC::HW_STATUS_ERROR;
        }
        Printf(Tee::PriNormal, "Link stay in U0 for 10ms...\n");
        Tasker::Sleep(10);
    }
    m_pDevice->PortPLS(Port, &pls); //dummy read to clear PLC
    Printf(Tee::PriNormal, "Wait %d ms before write/read test \n", WrRdSleepMs);
    Tasker::Sleep(WrRdSleepMs);
    CHECK_RC(WrRdTest());

    LOG_EXT();
    return OK;
}

RC XhciTest::U1U2U3Test(UINT32 Port, UINT32 WrRdSleepMs /*= 1000*/)
{
    LOG_ENT();
    RC rc;
    if( !m_IsInit || !m_pDevice )
    {
        Printf(Tee::PriError, "Test hasn't been initialized yes, call Init()\n");
        return RC::WAS_NOT_INITIALIZED;
    }

    CHECK_RC(U0U1Test(Port));
    CHECK_RC(U0U2Test(Port));
    CHECK_RC(U0U3Test(Port, WrRdSleepMs));

    Printf(Tee::PriNormal, "$$$ PASSED $$$ XhciTest::U1U2U3Test() \n");
    LOG_EXT();
    return OK;
}

RC XhciTest::HandlePerfFraglist(UINT32 ByteToXfer,
                                MemoryFragment::FRAGLIST *pFragList)
{
    RC rc;
    MASSERT(pFragList);

    void * pBuffer;
    if( pFragList->size() )
    {
        // caller wants to release the buffer allocated
        // first check if the passed in fraglist is allocated by this function
        pBuffer = pFragList->at(0).Address;
        for(UINT32 i = 0; i < pFragList->size(); i++)
        {
            if(pFragList->at(i).Address != pBuffer)
            {
                Printf(Tee::PriError, "Not a Fraglist allocated by this \n");
                return RC::SOFTWARE_ERROR;
            }
        }
        MemoryTracker::FreeBuffer(pBuffer);
        pFragList->clear();
        return OK;
    }

    UINT32 allocSize = m_pDevice->GetOptimisedFragLength();
    if (!ByteToXfer)
    {
        Printf(Tee::PriError, "Invalid TotalByte 0 \n");
        return RC::BAD_PARAMETER;
    }
    if((ByteToXfer) % allocSize)
    {
        Printf(Tee::PriError,
               "Invalid TotalByte %d, should be multiple of max TRB xfer size %d (128 Sectors) \n",
                ByteToXfer, allocSize);
        return RC::BAD_PARAMETER;
    }

    // allocate memory, we re-use this chunck for all subsequent operations
    CHECK_RC( MemoryTracker::AllocBuffer(allocSize, &pBuffer, true, 32, Memory::UC) );

    // create buffer and fragment list
    MemoryFragment::FRAGMENT myFrag;
    myFrag.ByteSize = allocSize;
    myFrag.Address = pBuffer;
    for(UINT32 i = 0; i < ( ByteToXfer ) / allocSize; i++)
    {   // we duplicate fragments
        pFragList->push_back(myFrag);
    }

    return OK;
}

RC XhciTest::TimeStampTest(void)
{
    RC rc = OK;

    // Get extended property -- time stamp
    CHECK_RC(m_pDevice->GetTimeStamp());

    return rc;
}

RC XhciTest::BtoTest(UINT32 Target)
{
    RC rc = OK;
    UINT32 loopCount = 0;
    UINT32 busErrCount = 0;
    const UINT16 devDescLen = 8;
    void * pDummy;
    CHECK_RC(MemoryTracker::AllocBufferAligned( devDescLen,
                                                &pDummy,
                                                16, 32,
                                                Memory::UC));
    DEFER
    {
        MemoryTracker::FreeBuffer(pDummy);
    };

    for(loopCount = 0; (loopCount < m_Loops) || (!IsExiting()); loopCount++)
    {
        CHECK_RC(m_pDevice->ReqGetDescriptor(m_SlotId,
                                             UsbDescriptorDevice,
                                             0,
                                             pDummy,
                                             devDescLen));
        busErrCount = m_pDevice->GetBusErrCount();
        if (Target && (Target <= busErrCount))
        {
            Printf( Tee::PriNormal,
                    "The bus error counter reach to the target (%d) in loop(%d).\n",
                    Target, loopCount);
            return OK;
        }
    }

    if (Target)
    {
        Printf( Tee::PriNormal,
                "Bus error counter: %d, BTO target: %d in loop(%d) \n",
                busErrCount, Target, m_Loops);
        Printf( Tee::PriError,
                "The bus error counter doesn't met the user target. \n");
        return RC::TEGRA_TEST_FAIL;
    }
    else // Target = 0
    {
        Printf( Tee::PriNormal,
                "Bus error counter: %d in loop(%d) \n",
                busErrCount, m_Loops);
        return OK;
    }
}

RC XhciTest::L1L2Test(UINT32 Port, UINT08 LpMode, UINT32 WakeDelay /*= 1000*/)
{
    LOG_ENT();
    RC rc;
    if( !m_IsInit || !m_pDevice )
    {
        Printf(Tee::PriError, "Test hasn't been initialized yes, call Init()\n");
        return RC::WAS_NOT_INITIALIZED;
    }

    if (LpMode != LOWPOWER_MODE_L1 && LpMode != LOWPOWER_MODE_L2)
    {
        Printf(Tee::PriError, "Ilwaild low power mode %d for HS \n", LpMode);
        return RC::BAD_PARAMETER;
    }

    // Start from L0-ENABLED
    Printf(Tee::PriNormal, "Directing Link to L0-ENABLED ...\n");
    CHECK_RC(m_pDevice->SetLowPowerMode(Port, LOWPOWER_MODE_U0_Idle));

    // Enter L1/L2 mode
    Printf(Tee::PriNormal,
           "Directing Link to L%d ...\n",
           (LpMode == LOWPOWER_MODE_L1) ? 1 : 2);
    CHECK_RC(m_pDevice->SetLowPowerMode(Port, LpMode));
    if (LpMode == LOWPOWER_MODE_L2)
    {
        // Resume device from suspend (L2) mode
        Printf(Tee::PriNormal, "Resume the device on port %d \n", Port);
        CHECK_RC(m_pDevice->Usb2ResumeDev(Port));
    }

    // Back to L0-ENABLED
    Printf(Tee::PriNormal, "Directing Link back to L0-ENABLED ... \n");
    CHECK_RC(m_pDevice->SetLowPowerMode(Port, LOWPOWER_MODE_U0_Idle));
    Tasker::Sleep(WakeDelay);

    Printf(Tee::PriNormal, "PLS = %d, now run WrRdTest\n", XHCI_REG_PORTSC_PLS_VAL_U0);
    CHECK_RC(WrRdTest());

    Printf(Tee::PriNormal, "$$$ PASSED $$$ XhciTest::L1L2Test() \n");
    LOG_EXT();
    return OK;
}

//=============================================================================
// Framework stuff below
//=============================================================================
EXPOSE_MODS_TESTS_ADV(XhciTest, "XHCI Test");

JSCLASS_PROPERTY(XhciTest, iSlotId,     0, "The SlotId assigned for USB testing device");
JSCLASS_PROPERTY(XhciTest, iNumSectors, 0, "Number of sectors for Read / Write");
CLASS_PROP_READWRITE(XhciTest, Lba, UINT32, "LBA for WrRdTest()");
CLASS_PROP_READWRITE(XhciTest, PerfTestMode, UINT32, "Test Mode for Performance Test (0-read,1-write)");

JS_STEST_LWSTOM(XhciTest, WrRdTest, 1, "Write / Read Test on USB Storage devices, with data compare")
{
    JSIF_INIT_VAR(XhciTest, WrRdTest, 0, 1, *WrRdMask[0x1-WrOnly|0x2-RdOnly|0x3-both]);
    JSIF_GET_ARG_OPT(0, UINT32, Mask, 3);
    JSIF_CALL_TEST(XhciTest, WrRdTest, Mask);
    JSIF_RETURN(rc);
}


JS_STEST_LWSTOM(XhciTest, PerfRun, 1, "Xhci Performance test")
{
    JSIF_INIT_VAR(XhciTest, PerfRun, 0, 1, *MinBW);
    JSIF_GET_ARG_OPT(0, UINT32, MinBW, 0);
    JSIF_CALL_TEST(XhciTest, PerfRun, MinBW);
    JSIF_RETURN(rc);
}

JSIF_TEST_1(XhciTest, MultiPortTest, vector<UINT32>, SlotIds, "Xhci Multi Device test (requires iSlotId, Lba, iNumSectors");
JSIF_TEST_2(XhciTest, U1U2U3Test, UINT32, PortIndex, UINT32, WrRdSleepMs, "Xhci U1U2U3 test on specified port");
JSIF_TEST_0(XhciTest, TimeStampTest, "Get time-stamp test");
JSIF_TEST_1(XhciTest, BtoTest, UINT32, Target, "Xhci BTO test on specified port");
JSIF_TEST_3(XhciTest, L1L2Test, UINT32, PortIndex, UINT08, LpMode, UINT32, WakeDelay, "Xhci L1L2 test on specified port");

