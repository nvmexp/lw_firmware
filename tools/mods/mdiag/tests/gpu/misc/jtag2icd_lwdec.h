/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2012,2015-2016, 2021 by LWPU Corporation.  All rights 
 * reserved.  All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _JTAG2ICD_LWDEC_H_
#define _JTAG2ICD_LWDEC_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/lwgpu.h"

class jtag2icd_lwdec : public LWGpuSingleChannelTest
{
public:
    jtag2icd_lwdec(ArgReader *params);

    virtual ~jtag2icd_lwdec(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

    virtual void DelayNs(UINT32);

protected:
    int Jtag2icdLwdecTest();
    bool Host2JtagCfg(int instr, int length, int chiplet, int dw, int status, int req);
    bool SetJTAGPat(int clusterID, int instrID, int length, UINT32 address, UINT32 data, UINT32 write, UINT32 start, UINT32 startBit);
    bool GetJTAGPat(int clusterID, int instrID, int length, UINT32* rdat, UINT32 startBit);
    bool Host2jtagConfigWrite( UINT32 dwordEn, UINT32 regLength, UINT32 readOrWrite, UINT32 captDis, UINT32 updtDis, UINT32 burstMode, bool lastClusterIlw);
    bool Host2jtagCtrlWrite(UINT32 reqCtrl, UINT32 status, UINT32 chipletSel, UINT32 dwordEn, UINT32 regLength, UINT32 instrID, bool readOrWrite);
    bool Host2jtagDataWrite(UINT32 dataValue);
    bool Host2jtagDataRead(UINT32& dataValue, UINT32 dwordEn, UINT32 regLength);
    bool WaitForCtrl();
    bool Host2jtagUnlockJtag(int clusterID, int instrID, int length);

private:
    UINT32 arch;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(jtag2icd_lwdec, jtag2icd_lwdec, "jtag2icd_lwdec  Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &jtag2icd_lwdec_testentry
#endif

#endif  // _JTAG2ICD_LWDEC_H_
