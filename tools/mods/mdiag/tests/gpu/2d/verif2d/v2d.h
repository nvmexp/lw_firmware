/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2013, 2018, 2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _V2D_H_
#define _V2D_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/gpu/2d/lwgpu_2dtest.h"
#include "surf.h"

class Verif2d;

class Verif2dTest : public LWGpu2dTest
{
public:
    Verif2dTest(ArgReader *pParams );

    virtual ~Verif2dTest( void );

    static Test *Factory(ArgDatabase *args);
    virtual int Setup( void );
    virtual void Run( void );
    virtual void CleanUp( void );
    virtual void PostRun();
    virtual TestEnums::TEST_STATUS CheckIntrPassFail();

    virtual void EnableEcoverDumpInSim(bool enabled) {} // bypass the default impl in Test
    virtual bool IsEcoverSupported() { return true; }

    bool dumpTga;   // save dst surface to .tga file
    bool doDmaCheck;
    bool doDmaCheckCE;
    bool doGpuCrcCalc;
    V2dSurface::Flags dstFlags;
    string ctxSurfString;
    bool doCheckCommandLineCrc; // check gpu buffer against CRC on command line
    string commandLineCrc;
    V2dSurface::ImageDumpFormat imageDumpFormat;

private:
    Verif2d *m_v2d;
    vector<string> m_cmdList;
    UINT32 m_rop;
    bool m_doRop;
    UINT32 m_beta1d31;
    bool m_doBeta1;
    UINT32 m_beta4;
    bool m_doBeta4;
    string m_operation;
    bool m_doOperation;
    string m_paramsString;
    string m_scriptFileName;
    string m_scriptDir;
    int m_ctxswMethodCount;
    string m_sectorPromotion;
    string m_pm_trigger_file;
    bool m_useNewPerfmon;
    unique_ptr<EcovVerifier> m_EcovVerifier;
    bool m_IsCtxSwitchTest;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(v2d, Verif2dTest, "2d verification Ginsu knife");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &v2d_testentry
#endif

#endif // _V2D_H_
