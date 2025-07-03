/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007,2018, 2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _REGTEST_H_
#define _REGTEST_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/utils/randstrm.h"

#include "mdiag/utils/cregexcl.h"
#include "mdiag/IRegisterMap.h"

#include <string>
using std::string;
#include <map>
using std::map;

class CRange
{
public:
    CRange(UINT32 start, UINT32 stop) : m_start(start), m_stop(stop) {};
    ~CRange();

    UINT32 GetStart(void) { return m_start; };
    UINT32 GetStop(void) { return m_stop; };

    bool InRangeInclusive(UINT32 tval) { return ((tval >= m_start) && (tval <= m_stop)); };

private:
    UINT32 m_start;
    UINT32 m_stop;
};

class CConstVal
{
public:
    CConstVal(UINT32 cval, UINT32 cmask) : m_cval(cval), m_cmask(cmask) {};
    ~CConstVal() {};
    UINT32 GetConstVal(void) { return m_cval; };
    UINT32 GetConstMask(void) { return m_cmask; };
    const void operator |= (const CConstVal &a)
    {
        m_cval  |= a.m_cval;
        m_cmask |= a.m_cmask;
    }

private:
    UINT32 m_cval;
    UINT32 m_cmask;
};

enum SPECIAL_ADJ
{
    SADJ_NONE =         0x0,
    SADJ_CONST =        0x1,
    SADJ_EXCL =         0x2,
    SADJ_ALL =          SADJ_CONST | SADJ_EXCL
};

class CRegTestGpu : public LWGpuSingleChannelTest {
public:
    CRegTestGpu(ArgReader *params);

    virtual ~CRegTestGpu(void);

    // a real test needs a factory function
    static Test *Factory(ArgDatabase *args);

    // a little extra setup to be done
    virtual int Setup(void);

    // Run() overridden - now we're a real test!
    virtual void Run(void);

    // a little extra cleanup to be done
    virtual void CleanUp(void);
    void   ResetDisplay();
    bool   GetInitValue        (IRegisterClass* reg, UINT32 testAddr, UINT32 *initVal, UINT32 *initMask);
    UINT32 SaveRegValue        (IRegisterClass* reg, UINT32 testAddr, UINT32 &lwrVal);
    UINT32 RestoreRegValue     (IRegisterClass* reg, UINT32 testAddr, UINT32 lwrVal);
    UINT32 DoReadInitValuesTest(IRegisterClass* reg, UINT32 testAddr);
    UINT32 DoWriteTest         (IRegisterClass* reg, UINT32 testAddr, UINT32 &lwrVal, UINT32 writeVal, const char *tType);
    UINT32 DoWrite0Test        (IRegisterClass* reg, UINT32 testAddr);
    UINT32 DoWrite1Test        (IRegisterClass* reg, UINT32 testAddr);
    UINT32 DoWalkTest          (IRegisterClass* reg, UINT32 testAddr, UINT32 val, const char *tType);
    UINT32 DoRandomTest        (IRegisterClass* reg, UINT32 testAddr, UINT32 count);
    UINT32 DoA5Test            (IRegisterClass* reg, UINT32 testAddr);
    bool   GetActualInitValue        (IRegisterClass* reg, UINT32 testAddr, UINT32 *initVal, UINT32 *initMask);
    UINT32 DoReadActualInitValuesTest(IRegisterClass* reg, UINT32 testAddr);
    bool   GetProdValue        (IRegisterClass* reg, UINT32 testAddr, UINT32 *prodVal, UINT32 *prodMask);
    UINT32 DoReadProdValuesTest(IRegisterClass* reg, UINT32 testAddr);
    UINT32 LocalRegRd32(UINT32 address);
    void LocalRegWr32(UINT32 address, UINT32 data);
    UINT32 RMARegRd32(UINT32 address);
    void RMARegWr32(UINT32 address, UINT32 data);
    void setGpio8(int en_, int mode);

    void SaveLWAccessState();
    void RestoreLWAccessState();
    UINT32 DoHwInitTest(void);
    UINT32 DoClkCntrTest(void);

    void PrintRegisterMap();
    void PrintRegister(IRegisterClass* reg);
    void DecodeRegister(IRegisterClass* ireg, UINT32 numIdx, UINT32 idx1, UINT32 idx2);

    bool DoSpecialAdjustments(IRegisterClass* reg, UINT32 which, UINT32* expVal, UINT32* readVal);
    bool DoWriteAdjustments(IRegisterClass* reg, UINT32 which, UINT32 initialValue, UINT32* writeVal);
    UINT32 ApplyWriteMasks(IRegisterClass* reg, UINT32 rdVal, UINT32 wrVal);
    UINT32 ComputeExpectedReadValue(IRegisterClass* reg, UINT32 rdValOld, UINT32 wrVal, UINT32 rdValNew);

    const char* PassGroupCheck(IRegisterClass* reg, bool allowNoGroups);

    bool AddrInRange(UINT32 addr);

    UINT32 PTVOSpecialTest();
    UINT32 TMDSSpecialTest();

    UINT32 ProcessExceptionFile(const char* skipFile);
    bool PrintExceptions(void);

private:
    UINT32 Tolerance(IRegisterClass* reg);

    ArgReader *params;

    RandomStream*                   m_rndStream;
    UINT32                          m_seed1;
    UINT32                          m_seed2;
    UINT32                          m_seed3;

    UINT32                          m_verbose;
    UINT32                          m_only_prod;
    bool                            AddOffset;
    UINT32                          a_offset;

    CRange*                         m_ArrayRange1;
    CRange*                         m_ArrayRange2;

    bool                            m_fbTest;
    UINT32                          m_fbTestBegin;
    UINT32                          m_fbTestEnd;

    UINT32                          cr38Addr;
    UINT16                          cr38LswOfWrite;
    UINT08                          cr38Val;

    UINT32                          m_printLevel;
    int                             m_printClass;
    UINT32                          m_printArrayLwtoff;

    IRegisterMap*                   m_regMap;
    IRegisterClass*                 m_theReg;
    CRegExclusions                  m_exclusions;

    const vector<string>*           m_regGroups;
    vector<CRange*>                 m_regRanges;

    map<string, int            > m_skipRegsAll;  // all regs that appear in the skip files(s)
    map<string, int            > m_skipRegs;
    map<string, UINT32         > m_excludeBits;
    map<string, CConstVal *    > m_constBits;
    map<string, CConstVal *    > m_forceBits;
    map<string, UINT32         > m_Tolerances;
    map<string, UINT32         > m_InitialVals;
    map<string, CConstVal *    > m_InitialOverride;
    map<string, map<int, bool> > m_skipidxs;     // list of array indices to skip for each register
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(RegTestGpu, CRegTestGpu, " Register Test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &RegTestGpu_testentry
#endif

#endif
