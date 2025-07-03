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

#ifndef INCLUDED_XHCITST_H
#define INCLUDED_XHCITST_H

#ifndef INCLUDE_BSCTEST_H
#include "cheetah/include/bsctest.h"
#endif

class XhciController;

class XhciTest: public BscTest
{
public:

    XhciTest();
    virtual ~XhciTest();

    //! \brief Do Write-Read-Compare test on specified USB flash drive
    //! \param [i] Mode : bit mask to specify Read / Write
    RC WrRdTest(UINT32 Mode = 0x3);

    SETGET_TEST_PROP(Lba,           UINT32);
    SETGET_TEST_PROP(PerfTestMode,  UINT08);

    // CCTest function cluster
    RC InitCCT();
    RC UnInitCCT();
    //! \brief trigger a DMA between MEM pool(64KB) and system memory
    RC StartCCTStress();
    //! \brief check if the DMA is complete
    RC IsCCTComplete(bool *pIsComplete);
    RC GetCCTBuffer(MemoryFragment::FRAGLIST **ppFragList );
    RC GetStatus(UINT32 *pDataSize, UINT64 *pXferCount);
    RC PerfRun(UINT32 MinBW = 0);
    RC MultiPortTest(vector<UINT32> & pvSlotId);
    RC U0U1Test(UINT32 Port);
    RC U0U2Test(UINT32 Port);
    RC U0U3Test(UINT32 Port, UINT32 WrRdSleepMs = 1000);
    RC U1U2U3Test(UINT32 Port, UINT32 WrRdSleepMs = 1000);
    RC TimeStampTest(void);
    //! \brief Bus time out test
    RC BtoTest(UINT32 Target);
    //! \brief USB2 LPM test, include L1 and L2
    RC L1L2Test(UINT32 Port, UINT08 LpMode, UINT32 WakeDelay = 1000);

protected:

    //! \brief issue Read10 or Write10 command on multiple slots conlwrrently
    //! \param [i] vSlotId : vector of Slot IDs
    //! \param [i] IsWrite : is write or read
    //! \param [i] pFragList : fragment list to hold data
    RC MultiCmd(vector<UINT32> & vSlotId,
                bool IsWrite,
                MemoryFragment::FRAGLIST *pFragList);

    //! \brief generate fraglist for performance tests.
    //! each fragmens in the fraglist point to the same chunk of memory
    RC HandlePerfFraglist(UINT32 ByteToXfer,
                          MemoryFragment::FRAGLIST *pFragList);

    //! \brief API for block read
    RC BlockRd(UINT32 BlockAddr,
                       UINT32 BlockCnt,
                       MemoryFragment::FRAGLIST* pFragList);

    //! \brief API for block write
    RC BlockWr(UINT32 BlockAddr,
                       UINT32 BlockCnt,
                       MemoryFragment::FRAGLIST* pFragList);

     //! \var Pointer to test controller object
    XhciController * m_pDevice;
    //! \var SlotId to locate the test controller object
    UINT32 m_SlotId;
    //! \var Number of sectors to read write in the test
    UINT32 m_NumSectors;
    //! \var LBA address to read write in the test
    UINT32 m_Lba;
    //! \var test mode for performance test (0 - read, 1 - write)
    UINT08 m_PerfTestMode;
    //! \var buffer size required
    UINT32 m_UsedBytes;

private:
    RC Initialize_Private();
    RC Uninitialize_Private();
    RC InitProperties_Private();
    RC PrintInfo_Private();
    RC PrintPropertiesInit_Private(Tee::Priority Pri);
    RC PrintPropertiesNoInit_Private(Tee::Priority Pri);
    RC PrintTestName_Private(Tee::Priority Pri);

};

#endif
