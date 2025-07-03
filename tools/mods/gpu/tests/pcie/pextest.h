/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2013,2017,2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_PEXTEST_H
#define INCLUDED_PEXTEST_H

#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

#ifndef INCLUDED_GPUTEST_H
#include "gpu/tests/gputest.h"
#endif
#ifndef PCI_H__
#include "core/include/pci.h"
#endif

#ifndef INCLUDED_PEXDEV_H
#include "gpu/utility/pcie/pexdev.h"
#endif

class Pcie;

class PexTest : public GpuTest
{
public:
    PexTest();
    virtual ~PexTest();
    virtual RC Run();
    virtual RC Setup();
    virtual RC Cleanup();
    virtual bool IsSupported();
    virtual RC RunTest() = 0;
    virtual bool IsTestType(TestType tt);

    SETGET_PROP(AllowCoverageHole, bool);
    SETGET_PROP(UpStreamDepth, UINT32);
    SETGET_PROP(ExtraAspmToTest, UINT32);
    SETGET_PROP(ForceAspm, string);
    SETGET_PROP(PexVerbose, bool);
    SETGET_PROP(UseInternalCounters, bool);
    SETGET_PROP(UsePollPcieStatus, bool);
    SETGET_PROP(CheckCountAfterGetError, bool);
    SETGET_PROP(PexInfoToRestore, UINT32);
    SETGET_PROP(PStateLockRequired, bool);
    SETGET_PROP(EnablePexGenArbitration, bool);
    SETGET_PROP(IgnorePexErrors, bool);
    SETGET_PROP(IgnorePexHwCounters, bool);
    SETGET_PROP(DisplayPexCounts, bool);

protected:
    //! Setup PCIE error counts for the given subdevice.
    RC SetupPcieErrors(GpuSubdevice* pSubdev);
    RC SetupPcieErrors(TestDevicePtr pTestDevice);

    //! Check for PCIE errors for the given subdevice.
    //! GpuSubdevice is being passed as an argument here since there might be
    // multiple calls to it for different subdevices, like in CheckDma.
    RC GetPcieErrors(GpuSubdevice* pSubdevice);
    RC GetPcieErrors(TestDevicePtr pTestDevice);

    //! This one is called at the end of RunTest() to see if the test should
    //! fail for
    RC CheckPcieErrors(GpuSubdevice* pSubdevice);
    RC CheckPcieErrors(TestDevicePtr pTestDevice);

    UINT32 UpStreamDepth();
    UINT32 AllowCoverageHole(UINT32 GpuSpeedCap);

    virtual RC LockGpuPStates();
    virtual void ReleaseGpuPStates();

    RC DisableAspm();

    Pcie* GetGpuPcie() { return m_pPcie; }
    Pcie* GetPcie() { return m_pPcie; }

private:
    struct PexErrors
    {
        UINT32 LocErrors;
        UINT32 HostErrors;
    };
    struct PexErrorsList
    {
        PexErrorsList(UINT32 Length);
        void ReSize(UINT32 Length);
        void ClearCount();
        UINT32 m_ListLength;

        vector<PexErrorCounts> m_LocalCounts;
        vector<PexErrorCounts> m_HostCounts;
    };
    struct ForcedAspm
    {
        UINT32 Depth; // depth from GPU
        UINT32 LocAspm;
        UINT32 HostAspm;
    };
    RC ParseForceAspmArgs(vector<ForcedAspm> *pForcedAspmList);

    //! Retrieve error counters for the given testdevice.
    RC GetPexErrors(TestDevicePtr pTestDevice, PexErrorsList** ppErrorList);

    RC CountPcieCtrlErrors(TestDevicePtr  pTestDevice,
                           PexDevice    * pPexDev,
                           PexErrorsList* pErrorList,
                           UINT32         Depth);

    Pcie *                                 m_pPcie;
    UINT32                                 m_SavedId;
    UINT32                                 m_UpStreamDepth;   // depth of GPU on one card - eg a dagwood with 1 BR04 has depth of 1
    UINT32                                 m_ExtraAspmToTest; // JS setting - add to what is lwrrently there
    string                                 m_ForceAspm;       // JS setting - override current setting
    bool                                   m_PexVerbose;    // Pex Verbose
    bool                                   m_UseInternalCounters; // Show internal only PEX counters
    bool                                   m_UsePollPcieStatus;
    bool                                   m_CheckCountAfterGetError;    // continue to accumulate PEX errors until end of the test
    bool                                   m_PStateLockRequired;
    bool                                   m_EnablePexGenArbitration;
    bool                                   m_IgnorePexErrors;
    bool                                   m_IgnorePexHwCounters;
    bool                                   m_DisplayPexCounts;
    bool                                   m_AllowCoverageHole;
    UINT32                                 m_PexInfoToRestore;
    vector<PexErrorsList>                  m_PexErrors;     // a list for each GPU
    vector<TestConfiguration::PexThreshold> m_AllowedCorrLinkError;
    vector<TestConfiguration::PexThreshold> m_AllowedUnSuppReq;
    vector<TestConfiguration::PexThreshold> m_AllowedNonFatalError;
    UINT32                                 m_AllowedLineErrors;
    UINT32                                 m_AllowedCRC;
    UINT32                                 m_AllowedFailedL0sExits;
    UINT32                                 m_AllowedNAKsRcvd;
    UINT32                                 m_AllowedNAKsSent;
    vector<UINT32>                         m_UpStreamAspmCap;
    vector<UINT32>                         m_OldLinkWidth;
    PStateOwner                            m_PStateOwner;
};

extern SObject PexTest_Object;

#endif // INCLUDED_PEXTEST_H

