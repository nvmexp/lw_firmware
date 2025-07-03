/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2012,2015, 2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _JTAG2ICD_LWENC_H_
#define _JTAG2ICD_LWENC_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/lwgpu.h"

class jtag2icd_lwenc : public LWGpuSingleChannelTest {
public:
    jtag2icd_lwenc(ArgReader *params);

    virtual ~jtag2icd_lwenc(void);

    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

    virtual void DelayNs(UINT32);

protected:
    int jtag2icd_lwencTest();
    bool host2JtagCfg(int instr, int length, int chiplet, int dw, int status, int req);
    bool setJTAGPat(
                        int cluster_id,
                        int instr_id,
                        int length,
                        UINT32 address,
                        UINT32 data,
                        UINT32 write,
                        UINT32 start,
                        UINT32 start_bit
                   );
    bool getJTAGPat(int cluster_id, int instr_id, int length, UINT32* rdat, UINT32 start_bit);
    bool host2jtag_config_write( UINT32 dword_en, UINT32 reg_length, UINT32 read_or_write, UINT32 capt_dis, UINT32 updt_dis, UINT32 burst_mode);
    bool host2jtag_ctrl_write(UINT32 req_ctrl, UINT32 status, UINT32 chiplet_sel, UINT32 dword_en, UINT32 reg_length, UINT32 instr_id, bool read_or_write);
    bool host2jtag_data_write(UINT32 data_value);
    bool host2jtag_data_read(UINT32& data_value,UINT32 dword_en, UINT32 reg_length);
    bool wait_for_ctrl();
    bool host2jtag_unlock_jtag(int cluster_id, int instr_id, int length);
    bool host2jtag_config_write_multiple( UINT32 dword_en, UINT32 reg_length, UINT32 read_or_write, UINT32 capt_dis, UINT32 updt_dis, UINT32 burst_mode);

private:
    UINT32 arch;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(jtag2icd_lwenc, jtag2icd_lwenc, "jtag2icd_lwenc  Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &jtag2icd_lwenc_testentry
#endif

#endif  // _JTAG2ICD_LWENC_H_
