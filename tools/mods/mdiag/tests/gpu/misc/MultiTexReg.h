/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2010, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _MULTITEXREG_H_
#define _MULTITEXREG_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/utils/randstrm.h"

#include "mdiag/utils/cregexcl.h"
#include "mdiag/IRegisterMap.h"

#include <string>
using std::string;
#include <map>
using std::map;

class MultiTexReg : public LWGpuSingleChannelTest {
public:
    MultiTexReg(ArgReader *params);

    virtual ~MultiTexReg(void);

    // a real test needs a factory function
    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);

    string MakeTexRegName           (string base_name, int const gpc_id, int const tpc_id);
    void   SelectTexturePipe        (IRegisterClass * const route_reg, int tex_id);
    void   SaveRegisters            (IRegisterClass * const test_reg, IRegisterClass * const route_reg, UINT32 * vals);
    void   RestoreRegisters         (IRegisterClass * const test_reg, IRegisterClass * const route_reg, UINT32 * vals);
    UINT32 ComputeExpectedReadValue (IRegisterClass * const reg, UINT32 old_val, UINT32 new_val);

    UINT32 TestPerPipeRegs(void);
    UINT32 TestPerTpcRegs(void);

    UINT32 MyRegRd32(UINT32 address);
    void MyRegWr32(UINT32 address, UINT32 data);

private:
    string                  m_TexPipeRoutingRegName;
    string                  m_TexPipeRoutingRegFieldName;
    vector<string>          m_PerPipeRegisterNames;
    vector<string>          m_PerTpcRegisterNames;
    UINT32                  m_NumTexPipesPerTpc;
    UINT32                  m_NumTrmPerTpc;
    UINT32                  m_NumGpc, m_GpcMask;
    UINT32                  m_NumTpc;
    UINT32                  m_verbose;
    UINT32                  m_checkFloorsweep;
    RandomStream *          m_randomStream;
    UINT32                  m_seed1;
    UINT32                  m_seed2;
    UINT32                  m_seed3;
    IRegisterMap *          m_regMap;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(multi_tex_reg, MultiTexReg, "Register test for multiple texture pipes per TPC");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &multi_tex_reg_testentry
#endif

#endif
