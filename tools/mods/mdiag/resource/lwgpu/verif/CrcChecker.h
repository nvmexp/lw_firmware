/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2013, 2020 by LWPU Corporation.  All rights reserved.  
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_CRCCHECKER_H
#define INCLUDED_CRCCHECKER_H

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

#include "core/include/argread.h"
#include "mdiag/utils/ICrcProfile.h"
#include "mdiag/utils/TestInfo.h"
#include "mdiag/utils/CrcInfo.h"
#include "mdiag/tests/test_state_report.h"
#include "mdiag/utils/IGpuSurfaceMgr.h"

#include "VerifUtils.h"
#include "DmaUtils.h"
#include "DumpUtils.h"
#include "SurfBufInfo.h"
#include "GpuVerif.h"

enum CrcCheckType
{
    CCT_PostTest,
    CCT_Dynamic
};

struct CrcCheckInfo: public ICheckInfo
{
    CrcCheckInfo (
        CrcCheckType checkType,
        UINT32 gpuSubdevice,
        ITestStateReport* report
    ) : ICheckInfo(report),
        CheckType(checkType),
        GpuSubdevice(gpuSubdevice)
    {
    }

    CrcCheckType CheckType;
    UINT32       GpuSubdevice;
};

struct PostTestCrcCheckInfo: public CrcCheckInfo
{
    PostTestCrcCheckInfo (
        ITestStateReport* report,
        UINT32 gpuSubdevice,
        const SurfaceSet *SkipSurfaces
    )
        : CrcCheckInfo(CCT_PostTest, gpuSubdevice, report),
          SkipSurfaces(SkipSurfaces)
    {
    }

    const SurfaceSet *SkipSurfaces;
};

struct SurfaceCheckInfo: public CrcCheckInfo
{
    const string&     CheckName;
    const string&     SurfaceSuffix;

    SurfaceCheckInfo(
        ITestStateReport* report,
        const string& checkName,
        const string& surfaceSuffix,
        UINT32 gpuSubdevice)
        : CrcCheckInfo(CCT_Dynamic, gpuSubdevice, report),
          CheckName(checkName),
          SurfaceSuffix(surfaceSuffix)
    {
    }
};

class CrcChecker: public ICrcVerifier
{
public:
    CrcChecker();
    virtual ~CrcChecker() {}

    // IVerifChecker impls
    virtual void Setup(GpuVerif* verifier);
    virtual TestEnums::TEST_STATUS Check(ICheckInfo* info);

    // ICrcVerifier impls
    virtual bool Verify
    (
        const char *key, // Key used to find stored crcs
        UINT32 measuredCrc, // Measured crc
        SurfaceInfo* surfInfo,
        UINT32 gpuSubdevice,
        ITestStateReport *report, // File to touch in case of error
        CrcStatus *crcStatus // Result of comparison
    );

    virtual UINT32 CallwlateCrc(const string& checkName, UINT32 subdev,
            const SurfaceAssembler::Rectangle *rect = 0);

protected:
    // Post test checking
    TestEnums::TEST_STATUS CheckPostTestCRC(PostTestCrcCheckInfo* ptInfo);

    // On demand surface checking
    TestEnums::TEST_STATUS CheckSurfaceCRC(SurfaceCheckInfo* scInfo);

    // CRC computations
    bool ComputeAllCRCs(UINT32 gpuSubdevice);
    bool GpuComputeAllCRCs(UINT32 gpuSubdevice);
    bool CpuComputeAllCRCs(UINT32 gpuSubdevice);

    // CRC verification
    bool CompareCrcs(TestEnums::TEST_STATUS& status, CrcStatus& crcFinal, char* key,
                     const string& tgaProfileKey, ITestStateReport* report,
                     UINT32 gpuSubdevice, const SurfaceSet *skipSurfaces);

    // CRC management
    bool SaveMeasuredCrc(const char* key, ITestStateReport* report, UINT32 subdev);
    bool CopyCrcValues(const string& prevKey, const string& newKey, ICrcProfile* profile);

    // Image filenames and profile keys
    void BuildImgFileNames(const string& cframe);
    bool BuildProfileKeysAndImgNames(char* key, ITestStateReport* report, UINT32 subdev);

    // States
    bool ZSurfOK() const;

    // Helpers
    RC FlushPriorCRC(UINT32 subdev);
    SurfInfo2* FindSurfaceInfo(const string &checkName);
    DmaBufferInfo2* FindDmaBufferInfo(const string &checkName);

    //TODO: find a good place for Zlwll
    bool ZLwllSanityCheck(UINT32 gpuSubdevice);

protected:
    GpuVerif*          m_verifier;
    const ArgReader*    m_params;
    IGpuSurfaceMgr*     m_surfMgr;

    DmaUtils*           m_DmaUtils;
    DumpUtils*          m_DumpUtils;

    CrcEnums::CRC_MODE  m_crcMode;

    bool                m_forceImages;
    bool                m_forceCrc;
    bool                m_crcMissOK;
    bool                m_blackImageCheck;

    TestEnums::TEST_STATUS  m_checkDynSurfStatus;

    bool                m_zlwll_passed;

    vector<SurfInfo2*>*          m_surfaces;
    vector<AllocSurfaceInfo2*>*  m_AllocSurfaces;
    vector<DmaBufferInfo2*>*     m_DmaBufferInfos;
    vector<SurfaceInfo*>*        m_allSurfaces; // all xxInfos linked together
    // SurfZ is not cached b/c it's n/a in Setup()

    SurfaceSet m_skipSurfaces;
};

#endif /* INCLUDED_CRCCHECKER_H */
