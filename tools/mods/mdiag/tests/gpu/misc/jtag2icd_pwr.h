/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2009, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _JTAG2ICD_PWR_H_
#define _JTAG2ICD_PWR_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/lwgpu.h"

class jtag2icd_pwr : public LWGpuSingleChannelTest {
public:
    jtag2icd_pwr(ArgReader *params);

    virtual ~jtag2icd_pwr(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

    virtual void DelayNs(UINT32);

protected:
    int jtag2icd_pwrTest();
    bool host2JtagCfg(int instr, int length, int chiplet, int dw, int status, int req);
    bool setJTAGPat(int instr, int length, UINT32 address, UINT32 data,  UINT32 write, UINT32 start);
    bool getJTAGPat(int instr, int length, UINT32* rdat);
    bool setJTAGPat_PwrCtrl( UINT32 data);
    bool getJTAGPat_PwrCtrl( UINT32* rdat);
    string m_paramsString;

private:
    UINT32 arch;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(jtag2icd_pwr, jtag2icd_pwr, "jtag2icd_pwr  Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &jtag2icd_pwr_testentry
#endif

#endif  // _JTAG2ICD_PWR_H_
