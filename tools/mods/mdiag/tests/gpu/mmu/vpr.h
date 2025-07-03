/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2013, 2015-2017, 2019-2021 by LWPU Corporation.  All rights 
 * reserved.  All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
 //! \file vpr.h
 //! \brief Definition of a class to test CPU VPR accesses.
 //!
 //!

#ifndef _VPR_H_
#define _VPR_H_

#include "mdiag/utils/types.h"
#include "mdiag/tests/test.h"
#include "mdiag/utils/buf.h"

class RandomStream;
class BuffInfo;

//#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/tests/gpu/lwgpu_basetest.h"

//! \brief VPR test
class vprTest : public LWGpuBaseTest
{
public:
    vprTest(ArgReader *reader);

    virtual ~vprTest(void);

    static Test *Factory(ArgDatabase *args);

    //! a little extra setup to be done
    virtual int Setup(void);

    //! Run() overridden - now we're a real test!
    virtual void Run(void);

    //! Runs the test on older chips that do not have non-indexed VPR registers
    void RunLegacy(void);

    //! a little extra cleanup to be done
    virtual void CleanUp(void);

protected:
    //! A gpu
    LWGpuResource *lwgpu;

    // BuffInfo Object to store info about allocated buffers, and do a nice
    // alloc print like trace3d
    BuffInfo *m_buffInfo;

    MdiagSurf m_vprSurf;
    MdiagSurf m_nolwprSurf;

    GpuSubdevice *m_pGpuSub;

    ArgReader *m_params;

    LwU32 m_bar0WindowSave;

    void DestroyMembers();

    TestEnums::TEST_STATUS CheckIntrPassFail(void);

    //! \brief Enables the backdoor access
    void EnableBackdoor(void);
    //! \brief Disable the backdoor access
    void DisableBackdoor(void);

    //! Sets up the surface object in memory.
    int SetupSurface(MdiagSurf *surface, UINT32 size,
                      Memory::Location memLoc, bool bProtected);

    void SetBar0Window(const MdiagSurf &surface, LwU32 offset);
    uint GetBar0WindowTarget(const MdiagSurf& surface);
    void ContextReset(int count, bool useContextReset);

    //!interrupt_file name for IFB error recognition
    string m_vprIntFileName;
    string m_vprIntDir;

    bool m_checkAutoFetchDisabled;
    bool m_checkInUseIsLow;
    bool m_gen5Capable;
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(vpr, vprTest, "VPR access memory test");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &vpr_testentry
#endif

#endif //_VPR_H_
