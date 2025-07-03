/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2014, 2020-2021 by LWPU Corporation.  All rights reserved.  
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "CrcChecker.h"

#include "core/include/platform.h"
#include "mdiag/sysspec.h"
#include "mdiag/utils/utils.h"
#include "mdiag/utils/mdiag_xml.h"
#include "core/utility/errloggr.h"

#include "ProfileKey.h"
#include "VerifUtils.h"

const char SLASH_STRING[2] = {DIR_SEPARATOR_CHAR, '\0'};

#define foreach_surface(it) \
    for(SurfIt2_t it = m_surfaces->begin(); it != m_surfaces->end(); ++it)

#define foreach_alloc_surface(it) \
    for(AllocSurfaceVec2_t::const_iterator it = m_AllocSurfaces->begin(); \
                                         it!= m_AllocSurfaces->end(); ++it)

#define foreach_dmabuf(it) \
    for(DmaBufVec2_t::iterator it = m_DmaBufferInfos->begin(); \
                               it!= m_DmaBufferInfos->end(); ++it)

#define foreach_all_surfaces(it) \
    for(SurfaceInfos::iterator it = m_allSurfaces->begin(); \
                               it!= m_allSurfaces->end(); ++it)

CrcChecker::CrcChecker()
: m_verifier(0), m_params(0), m_surfMgr(0),
  m_DmaUtils(0), m_DumpUtils(0),
  m_crcMode(CrcEnums::CRC_NONE),
  m_forceImages(false), m_forceCrc(false), m_crcMissOK(false),m_blackImageCheck(false),
  m_zlwll_passed(false),
  m_surfaces(0), m_AllocSurfaces(0), m_DmaBufferInfos(0), m_allSurfaces(0)
{
}

void CrcChecker::Setup(GpuVerif* verifier)
{
    m_verifier = verifier;
    m_params = verifier->Params();
    m_surfMgr = verifier->SurfaceManager();
    m_surfaces = &verifier->Surfaces();
    m_crcMode = verifier->CrcMode();

    m_AllocSurfaces = &verifier->AllocSurfaces();
    m_DmaBufferInfos = &verifier->DmaBufferInfos();
    m_allSurfaces = &verifier->AllSurfaces();

    m_DmaUtils = m_verifier->GetDmaUtils();
    m_DumpUtils = m_verifier->GetDumpUtils();

    m_zlwll_passed = true;

    m_crcMissOK = m_params->ParamPresent("-crcMissOK") != 0;
    m_forceCrc = m_params->ParamPresent("-dumpCRCs") != 0;
    m_forceImages = m_params->ParamPresent("-dumpImages") ||
                    m_params->ParamPresent("-dump_all_buffer_after_test");
    m_blackImageCheck = m_params->ParamPresent("-black_image_check") != 0;

    m_checkDynSurfStatus = TestEnums::TEST_SUCCEEDED;

    VerifUtils::WriteVerifXml(m_verifier->Profile());
}

// ---------------------------------------------------------------------------------
//   Top level CRC checking routines
// ---------------------------------------------------------------------------------

TestEnums::TEST_STATUS CrcChecker::Check(ICheckInfo* info)
{
    CrcCheckInfo* ccInfo = (CrcCheckInfo*)info;
    if (ccInfo->CheckType == CCT_PostTest)
    {
        return CheckPostTestCRC((PostTestCrcCheckInfo*)info);
    }
    else if (ccInfo->CheckType == CCT_Dynamic)
    {
        return CheckSurfaceCRC((SurfaceCheckInfo*)info);
    }
    else
    {
        MASSERT(!"Invalid CRC check type.");
        return TestEnums::TEST_FAILED_CRC;
    }
}

TestEnums::TEST_STATUS
CrcChecker::CheckPostTestCRC(PostTestCrcCheckInfo* ptInfo)
{
    UINT32 gpuSubdevice = ptInfo->GpuSubdevice;
    InfoPrintf("Checking image CRCs on gpu %d ...\n", gpuSubdevice);

    if (m_params->ParamPresent("-flushPriorToCRC") &&
        OK != FlushPriorCRC(gpuSubdevice))
    {
        ErrPrintf("CrcChecker::PostTestCrcCheck: Flush memory failed\n");
        return TestEnums::TEST_FAILED_CRC;
    }

    // Setup -dma_check
    // Only set it up if there is a surface in the trace for CRC check since
    // we do not want to create a DMA channel which will not be used
    if (m_allSurfaces->size() != 0)
    {
        if (m_DmaUtils->SetupDmaCheck(gpuSubdevice) != OK)
        {
            ErrPrintf("%s: SetupDmaCheck() failed!\n", __FUNCTION__);
            return TestEnums::TEST_FAILED;
        }
    }

    if (m_crcMode == CrcEnums::CRC_REPORT)
    {
        m_verifier->OpenReportFile();
    }

    ICrcProfile* profile = m_verifier->Profile();
    ITestStateReport* report = ptInfo->Report;

    CrcStatus crcFinal = CrcEnums::LEAD;  // To match old mdiag status, start with lead for final

    // Come with a CRC-like key, any CRC key. We want to put it in the TGA files
    // to identify the detailed parameters of the format, like multisample mode.
    // The Z and color formats are already identified in the structure at the
    // end of the file.
    string tgaProfileKey("");
    ProfileKey::Generate(m_verifier, "z", NULL, 0, 0, gpuSubdevice, tgaProfileKey);

#define _CHECK_GEN_KEY(name) \
    if(tgaProfileKey.length() < 10) \
        ProfileKey::Generate(m_verifier, #name, NULL, 0, 0, \
                             gpuSubdevice, tgaProfileKey);

    _CHECK_GEN_KEY("c");
    _CHECK_GEN_KEY("c0");
    _CHECK_GEN_KEY("c1");
    _CHECK_GEN_KEY("c2");

    char key[10];
    sprintf(key, "%d", 0);

    // build image filenames
    string cframe = "";
    foreach_all_surfaces(surfIt)
    {
        (*surfIt)->BuildImgFilename();
    }

    // Only grab framebuffer if we are not doing crcReport,
    // if the user wants crc checking, or they want images dumped
    if((m_crcMode != CrcEnums::CRC_REPORT) &&
       (m_forceImages || m_forceCrc || (m_crcMode >= CrcEnums::CRC_CHECK && m_crcMode != CrcEnums::INTERRUPT_ONLY)))
    {
        // Allow -frontdoor_check without blowing up on hardware, since on
        // hardware everything is frontdoor whether we like it or not.
        UINT32 SavedBackdoor = 0;
        bool FrontdoorCheck = m_params->ParamPresent("-frontdoor_check") > 0;
        if (FrontdoorCheck &&
            (Platform::GetSimulationMode() != Platform::Hardware))
        {
            Platform::EscapeRead("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, &SavedBackdoor);
            Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 0);
        }

        // read m_surfaces and callwlate crc's, zlwll sanity check if needed
        if (!ComputeAllCRCs(gpuSubdevice))
        {
            return TestEnums::TEST_FAILED;
        }

        if (FrontdoorCheck &&
            (Platform::GetSimulationMode() != Platform::Hardware))
        {
            Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, SavedBackdoor);
        }

        // there are gilding scripts which depends on the following InfoPrintf(s) please change them carefully
        InfoPrintf("Callwlated CRCs:");
        foreach_all_surfaces(surfIt)
        {
            (*surfIt)->PrintCRC();
        }
        Printf(Tee::PriNormal, "\n");

        if (!SaveMeasuredCrc(key, report, gpuSubdevice))
        {
            return TestEnums::TEST_FAILED_CRC;
        }
    }

    CrcEnums::CRC_MODE saveCrcMode = m_crcMode;
    // check if we're just in the backdoor archive read case.
    if (m_params->ParamPresent("-backdoor_archive") &&
        m_params->ParamUnsigned("-backdoor_archive") == 1)
    {
        m_crcMode = CrcEnums::CRC_NONE;
    }

    // Compare crcs against stored values for pass/fail/miss status
    TestEnums::TEST_STATUS status = TestEnums::TEST_SUCCEEDED;
    if(m_crcMode != CrcEnums::CRC_NONE && m_crcMode != CrcEnums::INTERRUPT_ONLY)
    {
        m_skipSurfaces.insert(ptInfo->SkipSurfaces->begin(),
                              ptInfo->SkipSurfaces->end());

        if(CompareCrcs(status, crcFinal, key, tgaProfileKey,
                report, gpuSubdevice, &m_skipSurfaces) == false)
        {
            ErrPrintf("Illegal crc chaining detected!\n");
            return TestEnums::TEST_FAILED_CRC;
        }

        // account for dynamic checks
        if (status == TestEnums::TEST_SUCCEEDED)
            status = m_checkDynSurfStatus;
    }
    else if(m_crcMode == CrcEnums::INTERRUPT_ONLY)
    {
        InfoPrintf("Skipping CRC check but still doing interrupt check!\n");

        if(m_verifier->CheckIntrPassFail() != TestEnums::TEST_SUCCEEDED)
        {
            ErrPrintf("Interrupt check failed!\n");
            status = TestEnums::TEST_FAILED_CRC;
        }
        else
        {
            status = m_checkDynSurfStatus;
        }
    }
    else
    {
        InfoPrintf("Skipping CRC check and Interrupt check by user request!\n");
        ErrorLogger::IgnoreErrorsForThisTest();
    }

    // InfoPrintf where the reference IMG file name can be found (Doug V's request to
    // support some scripts - the mapping between options and directory names is no
    // longer "trivial"
    if(!BuildProfileKeysAndImgNames(key, report, gpuSubdevice))
    {
        ErrPrintf("Error detected in crc chaining!\n");
        return TestEnums::TEST_FAILED_CRC;
    }

    bool needProfileCleanup = true;

    // now see if we are in crc dump or update mode, and if so, dump the new crc file/images
    if (m_crcMode==CrcEnums::CRC_DUMP || m_crcMode==CrcEnums::CRC_UPDATE)
    {
        m_DumpUtils->DumpUpdateCrcs(status, report, tgaProfileKey, gpuSubdevice);
        needProfileCleanup = false;
    }
    else if ((m_crcMode == CrcEnums::CRC_DUMP) || (m_crcMode == CrcEnums::CRC_UPDATE))
    {
        ErrPrintf("Users specified -crcDump or -crlwpdate, but the GPU does not allow crc dumping!!\n");
        ErrPrintf("The GPU in this case is : 0x%08x\n", m_verifier->LWGpu()->GetArchitecture(gpuSubdevice));
    }

    // cleanup profile
    if (needProfileCleanup && (m_crcMode != CrcEnums::CRC_NONE) && (m_crcMode != CrcEnums::INTERRUPT_ONLY))
        ProfileKey::CleanUpProfile(m_verifier, gpuSubdevice);

    // dump images
    if (m_forceImages && (m_crcMode != CrcEnums::CRC_REPORT))
    {
        m_DumpUtils->DumpImages(tgaProfileKey, gpuSubdevice);
    }

    // Only do the status thing if we actually were told to do the crc check
    if(m_crcMode != CrcEnums::CRC_NONE && m_crcMode != CrcEnums::INTERRUPT_ONLY)
    {
        if (gXML != NULL)
        {
            // Callwlate profile key string for crc to check (search gpu chain)
            string profileKey("");
            bool crcOK = ProfileKey::Generate(
                    m_verifier, key, NULL, 0, 1,
                    gpuSubdevice, profileKey);
            if(!crcOK)
            {
                report->error("Illegal CRC chaining detected!\n");
                return TestEnums::TEST_FAILED;
            }

            VerifUtils::WritePostTestCheckXml(profileKey, status);
        }

        // special case where user asked to make missing crc values become passing tests!
        if(m_crcMissOK && (status == TestEnums::TEST_CRC_NON_EXISTANT))
            status = TestEnums::TEST_SUCCEEDED;

        UINT32 color_crc[2] = {0x0, 0x0};
        SurfIt2_t surfIt = m_surfaces->begin();
        SurfIt2_t surfItEnd = m_surfaces->end();
        for(int i = 0; surfIt != surfItEnd && i < 2; ++surfIt, ++i)
        {
            color_crc[i] = (*surfIt)->GetCRC();
        }

        UINT32 zeta_crc = 0x0;
        if(ZSurfOK() == true)
        {
            zeta_crc = m_verifier->SurfZ()->GetCRC();
        }

        switch (status)
        {
        case TestEnums::TEST_CRC_NON_EXISTANT:
            report->crcNoFileFound();
            if (m_crcMode != CrcEnums::CRC_REPORT)
            {
                report->crcCheckFailed(profile);
            }
            else
            {
                VerifUtils::touch_bogus_crc_file(report,
                        m_verifier->LWGpu()->GetArchitecture(gpuSubdevice),
                        color_crc[0], color_crc[1], zeta_crc);
            }
            break ;

        case TestEnums::TEST_SUCCEEDED:
            if (crcFinal == CrcEnums::GOLD)
                report->crcCheckPassedGold();
            else
                report->crcCheckPassed();

            if (m_forceCrc)
            {
                m_DumpUtils->DumpCRCsPostTest();
            }

            break ;

        case TestEnums::TEST_FAILED_CRC:
            if (m_crcMode != CrcEnums::CRC_REPORT)
            {
                report->crcCheckFailed(profile);
            }
            else
            {
                VerifUtils::touch_bogus_crc_file(
                        report,
                        m_verifier->LWGpu()->GetArchitecture(gpuSubdevice),
                        color_crc[0], color_crc[1], zeta_crc);
            }
            break ;

        default:
            assert (0 && "Unrecognized simulation result.") ;
            break ;
        }
    }
    else if(m_crcMode == CrcEnums::INTERRUPT_ONLY)
    {
        if (gXML != NULL)
        {
            VerifUtils::WritePostTestCheckXml(
                    string(), status, true);
        }
        if(status == TestEnums::TEST_SUCCEEDED)
        {
            if (m_forceCrc)
            {
                m_DumpUtils->DumpCRCsPostTest();
            }
            report->crcCheckPassedGold();
        }
        else
        {
            report->crcCheckFailed("INTR check failed!!!");
        }
    }
    else
    {
        if (m_forceCrc)
        {
            m_DumpUtils->DumpCRCsPostTest();
        }

        // if not doing the crc check, mark test as passing with .yes file, and remove the .noo file
        report->runSucceeded();
    }

    // check if we're just in the backdoor archive read case.
    if (m_params->ParamPresent("-backdoor_archive") &&
        m_params->ParamUnsigned("-backdoor_archive") == 1)
    {
        m_crcMode = saveCrcMode;
    }

    if (m_verifier->ReportExists())
    {
        m_verifier->CloseReportFile();
    }

    if (OK != m_DumpUtils->DumpCRCInfos())
        ErrPrintf("Failed to dump CRC info\n");

    return status;
}

TestEnums::TEST_STATUS CrcChecker::CheckSurfaceCRC(SurfaceCheckInfo* scInfo)
{
    ITestStateReport* report = scInfo->Report;
    UINT32 gpuSubdevice = scInfo->GpuSubdevice;
    const string& surfaceSuffix = scInfo->SurfaceSuffix;

    // In case dumping image or crc is requested, crc need to be callwlated
    // even in CRC_NONE mode. Otherwise, return directly to save the time
    // of crc callwlation
    if ((m_crcMode == CrcEnums::CRC_NONE || m_crcMode == CrcEnums::INTERRUPT_ONLY) && !m_forceImages && !m_forceCrc)
    {
        InfoPrintf("Skipping CRC callwlation and check by user request!\n");
        if (m_crcMode == CrcEnums::CRC_NONE)
        {
            ErrorLogger::IgnoreErrorsForThisTest();
        }
        report->runSucceeded();

        return TestEnums::TEST_SUCCEEDED;
    }

    // Find the verification surfInfo for the surface.
    AllocSurfaceInfo2* surfInfo = (AllocSurfaceInfo2*)(FindSurfaceInfo(scInfo->CheckName));

    if (surfInfo == 0)
    {
        ErrPrintf("Can't verify surface %s.  Either the surface doesn't exist "
                  "or no CRC/reference check information was declared.\n",
                  scInfo->CheckName.c_str());
        return TestEnums::TEST_FAILED;
    }

    // Setup -dma_check
    if (m_DmaUtils->SetupDmaCheck(gpuSubdevice) != OK)
    {
        ErrPrintf("%s: SetupDmaCheck() failed!\n", __FUNCTION__);
        return TestEnums::TEST_FAILED;
    }

    // Callwlate the CRC.
    if (m_params->ParamPresent("-dma_check") &&
        m_params->ParamUnsigned("-dma_check") == 6)
    {
        if (!surfInfo->GpuComputeCRC(gpuSubdevice))
        {
            ErrPrintf("%s: Error reading surfaces during crc checks!\n",
                __FUNCTION__);
            return TestEnums::TEST_FAILED;
        }
    }
    else
    {
        if (!surfInfo->CpuComputeCRC(gpuSubdevice))
        {
            ErrPrintf("%s: Error reading surfaces during crc checks!\n",
                __FUNCTION__);
            return TestEnums::TEST_FAILED;
        }
    }

    InfoPrintf("Callwlated CRC: %s=0x%08x\n",
            scInfo->CheckName.c_str(), surfInfo->GetCRC());

    //skip the crc compare if it is a BlackImage
    if (m_blackImageCheck)
    {
        if (m_params->ParamPresent("-dma_check") &&
            m_params->ParamUnsigned("-dma_check") == 6)
        {
            WarnPrintf("Black image check is ignored with GPU CRC!\n");
        }
        else
        {
            if (m_params->ParamPresent("-crc_chunk_size") > 0 &&
                m_params->ParamUnsigned("-crc_chunk_size") > 0)
            {
                WarnPrintf("No black image check when surface is too big \n");
            }
            else
            {
                MdiagSurf *surf = surfInfo->GetSurface();
                LW50Raster *pRaster = m_surfMgr->GetRaster(surf->GetColorFormat());
                if (pRaster)
                {
                    pRaster->SetBase(reinterpret_cast<uintptr_t>(surfInfo->GetSurfBase()));
                    if (SelfgildStrategy::IsImageBlack(pRaster, surf->GetWidth(), surf->GetHeight(),
                                                       surf->GetDepth(), surf->GetArraySize()))
                    {
                        if (m_crcMode == CrcEnums::CRC_NONE)
                        {
                            WarnPrintf("The image is all black.\n");
                        }
                        else
                        {
                            ErrPrintf("The image is all black. exit from crc check\n");
                            report->error("All Black, CRC MISMATCH!!\n");
                            return TestEnums::TEST_FAILED_CRC;
                        }
                    }
                }
            }
        }
    }

    // Build profile keys, image names, and other related filenames.
    string key = surfInfo->CreateInitialKey(surfaceSuffix);

    if (!surfInfo->BuildProfKeyImgNameSuffix(key.c_str(), surfaceSuffix, gpuSubdevice))
    {
        report->error("Illegal CRC chaining detected!\n");
        return TestEnums::TEST_FAILED;
    }

    surfInfo->BuildImgFilenameSuffix(surfaceSuffix);

    //
    // In CRC_NONE mode, crc check process can be skipped after the
    // requested image or crc dump.
    if (m_crcMode == CrcEnums::CRC_NONE ||
        m_crcMode == CrcEnums::INTERRUPT_ONLY ||
        (m_params->ParamPresent("-backdoor_archive") &&
         m_params->ParamUnsigned("-backdoor_archive") == 1))
    {
        InfoPrintf("Skipping CRC check by user request!\n");

        if (m_forceImages)
        {
            surfInfo->DumpImageToFile(surfInfo->GetFilename(),
                    surfInfo->GetProfKey(), gpuSubdevice);
        }

        if (m_forceCrc)
        {
            ICrcProfile* profile = m_verifier->Profile();
            profile->WriteHex("trace_dx5",
                    surfInfo->GetProfKey().c_str(),
                    surfInfo->GetCRC(), true);
            profile->SetDump(true);
        }

        if (m_crcMode == CrcEnums::CRC_NONE)
        {
            ErrorLogger::IgnoreErrorsForThisTest();
        }
        report->runSucceeded();
        return TestEnums::TEST_SUCCEEDED;
    }

    // Verify the CRC and dump the image to test directory.
    CrcStatus crcStatus;

    if (surfInfo->Verify(&crcStatus, key.c_str(), report, gpuSubdevice) || m_forceImages)
    {
        surfInfo->DumpImageToFile(surfInfo->GetFilename(),
                surfInfo->GetProfKey(), gpuSubdevice);
    }

    // Update the report and determine the test status.
    TestEnums::TEST_STATUS testStatus = TestEnums::TEST_FAILED_CRC;

    switch (crcStatus)
    {
    case CrcEnums::MISS:
        if (m_crcMissOK)
        {
            report->crcCheckPassed();
            testStatus = TestEnums::TEST_SUCCEEDED;
        }
        else
        {
            report->crcNoFileFound();
            testStatus = TestEnums::TEST_CRC_NON_EXISTANT;
            if (m_crcMode > CrcEnums::CRC_REPORT)
            {
                report->crcCheckFailed(m_verifier->Profile());
            }
        }
        break;

    case CrcEnums::GOLD:
        report->crcCheckPassedGold();
        testStatus = TestEnums::TEST_SUCCEEDED;
        break;

    case CrcEnums::LEAD:
        report->crcCheckPassed();
        testStatus = TestEnums::TEST_SUCCEEDED;
        break;

    case CrcEnums::FAIL:
        report->crcCheckFailed(m_verifier->Profile());
        testStatus = TestEnums::TEST_FAILED_CRC;
        break;
    default:
        MASSERT(!"%s: Should not hit here.");
    }
    // Update the CRC and dump images to the trace directory.
    if (m_crcMode==CrcEnums::CRC_DUMP || m_crcMode==CrcEnums::CRC_UPDATE)
    {
        surfInfo->DumpCRC(testStatus);
        surfInfo->UpdateCRC(surfInfo->GetProfKey(), gpuSubdevice, 0);
    }

    if (m_checkDynSurfStatus == TestEnums::TEST_SUCCEEDED)
        m_checkDynSurfStatus = testStatus;

    return testStatus;
}

// ---------------------------------------------------------------------------------
//   Supporting methods
// ---------------------------------------------------------------------------------

bool CrcChecker::ComputeAllCRCs(UINT32 gpuSubdevice)
{
    if (m_params->ParamPresent("-dma_check") &&
        m_params->ParamUnsigned("-dma_check") == 6)
    {
        if(!GpuComputeAllCRCs(gpuSubdevice))
        {
            ErrPrintf("Error reading surfaces during crc checks!\n");
            return false;
        }
    }
    else
    {
        if(!CpuComputeAllCRCs(gpuSubdevice) ||
           !ZLwllSanityCheck(gpuSubdevice))
        {
            ErrPrintf("Error reading surfaces during crc checks!\n");
            return false;
        }
    }

    return true;
}

bool CrcChecker::GpuComputeAllCRCs(UINT32 gpuSubdevice)
{
    foreach_all_surfaces(surfIt)
    {
        if (!(*surfIt)->GpuComputeCRC(gpuSubdevice))
        {
            return false; // abort immediately
        }
    }

    return true;
}

// ---------------------------------------------------------------------------------
//  read and callwlate crc's for all buffers
// ---------------------------------------------------------------------------------
bool CrcChecker::CpuComputeAllCRCs(UINT32 gpuSubdevice)
{
    foreach_all_surfaces(surfIt)
    {
        if (!(*surfIt)->CpuComputeCRC(gpuSubdevice))
        {
            return false; // abort immediately
        }
    }

    return true;
}

// ---------------------------------------------------------------------------------
//  zlwll sanity check
// ---------------------------------------------------------------------------------
bool CrcChecker::ZLwllSanityCheck(UINT32 gpuSubdevice)
{
    SurfInfo2* surfZ = m_verifier->SurfZ();

    // check if zeta target exists
    if (surfZ == NULL)
        return true;

    // sanity check surface
    MASSERT(surfZ->IsValid());

    return surfZ->ZLwllSanityCheck(gpuSubdevice, &m_zlwll_passed);
}

// ---------------------------------------------------------------------------------
//  check if zeta buffer is valid
// ---------------------------------------------------------------------------------
bool CrcChecker::ZSurfOK() const
{
    return m_verifier->ZSurfOK();
}

// Read stored crcs and compare with measured crc. Return true if caller should dump image
bool CrcChecker::Verify
(
    const char *key, // Key used to find stored crcs
    UINT32 measuredCrc, // Measured crc
    SurfaceInfo* surfInfo,
    UINT32 subdev,
    ITestStateReport *report, // File to touch in case of error
    CrcStatus *crcStatus // Result of comparison
)
{
    m_verifier->IncrementCheckCounter();

    UINT32 leadCrc = 0;
    bool leadCrcMissing = false; // Crc from lead profile
    UINT32 goldCrc = 0;
    bool goldCrcMissing = false; // Crc from golden profile

    ICrcProfile* profile = m_verifier->Profile();

    // Callwlate profile key string for crc to check (search gpu chain)
    string checkProfileKey("");
    if (!surfInfo->GenProfileKey(key, true, subdev, checkProfileKey))
    {
        ErrPrintf("Illegal CRC chaining detected!!\n");
        report->error("Illegal CRC chaining detected!\n");
        return true;
    }

    // Callwlate profile key string for crc to dump (don't search gpu chain)
    string dumpProfileKey("");
    if (!surfInfo->GenProfileKey(key, false, subdev, dumpProfileKey))
    {
        ErrPrintf("Illegal CRC chaining detected!\n");
        report->error("Illegal CRC chaining detected!\n");
        return true;
    }

    string surfaceName = surfInfo->GetSurfaceName();
    const char *SurfName = surfaceName.c_str();
    bool mustMatchCrc = surfInfo->MustMatchCrc();

    if (checkProfileKey.length() == 0)
    {
        // Couldn't find crc to check against
        m_verifier->WriteToReport("crcReport for %s : key = NULL\n", SurfName);
        m_verifier->WriteToReport("    Error. Could not generate key for %s\n", SurfName);

        *crcStatus = CrcEnums::MISS;
    }
    else
    {
#if 0 // not needed yet, but want to check file in...
        CheckAllInChain(checkProfileKey, true);
#endif

        const ArgReader* params = m_verifier->Params();
        ICrcProfile* gold_profile = m_verifier->GoldProfile();

        bool crcMissingAllowed = m_crcMode == CrcEnums::CRC_UPDATE ||
                                 m_crcMode == CrcEnums::CRC_DUMP ||
                                 params->ParamPresent("-crcMissOK") > 0;

        if (crcMissingAllowed)
        {
            if (!profile->Loaded())
            {
                leadCrcMissing = true;
                InfoPrintf("Lead CRC file missing.\n");
            }

            if (!gold_profile->Loaded())
            {
                goldCrcMissing = true;
                InfoPrintf("Golden CRC file missing.\n");
            }
        }
        else
        {
            if (!profile->Loaded())
            {
                leadCrcMissing = true;
                WarnPrintf("Lead CRC file missing.\n");
            }

            if (!gold_profile->Loaded())
            {
                goldCrcMissing = true;
                WarnPrintf("Golden CRC file missing.\n");
            }
        }

        // Read lead crc
        if (!leadCrcMissing && profile->ReadStr("trace_dx5", checkProfileKey.c_str(), 0))
        {
            leadCrc = profile->ReadUInt("trace_dx5", checkProfileKey.c_str(), 0);
        }
        else
        {
            leadCrcMissing = true;
            profile->WriteHex("trace_dx5", dumpProfileKey.c_str(), measuredCrc, true);
        }

        // Read gold crc
        if (gold_profile->Loaded() && gold_profile->ReadStr("trace_dx5", checkProfileKey.c_str(), 0))
        {
            goldCrc = gold_profile->ReadUInt("trace_dx5", checkProfileKey.c_str(), 0);
        }
        else
        {
            goldCrcMissing = true;
            report->goldCrcMissing();
        }

        // Check that gold and lead crcs agree
        if (!leadCrcMissing && !goldCrcMissing && goldCrc != leadCrc)
        {
            ErrPrintf("GOLD and Test CRC MISMATCH!! key %s: golden.crc Value = 0x%08x  test.crc = 0x%08x\n",
                      checkProfileKey.c_str(), goldCrc, leadCrc);
            if (measuredCrc==leadCrc)
                ErrPrintf("GOLD and Test CRC MISMATCH!! test.crc value passed, but golden.crc value was different\n");
            report->error("GOLD and Test CRC MISMATCH!!\n");
            *crcStatus = CrcEnums::FAIL;
            return true;
        }

        InfoPrintf("Profile key checked: %s\n", checkProfileKey.c_str());

        // Check measured crc against crcs from profile
        if (m_verifier->ReportExists()) // Just report
        {
            m_verifier->WriteToReport("crcReport for %s : key = %s\n", SurfName, checkProfileKey.c_str());
            *crcStatus = CrcEnums::GOLD;
            if(goldCrcMissing)
            {
                *crcStatus = CrcEnums::LEAD;
                m_verifier->WriteToReport("    GOLD crc value is missing\n");
            }
            else
                m_verifier->WriteToReport("    GOLD crc value = 0x%08x\n", goldCrc);
            if(leadCrcMissing)
            {
                *crcStatus = CrcEnums::MISS;
                m_verifier->WriteToReport("    LEAD crc value is missing\n");
            }
            else
                m_verifier->WriteToReport("    LEAD crc value = 0x%08x\n", leadCrc);
        }
        else if (!goldCrcMissing && measuredCrc==goldCrc)
        {
            if (mustMatchCrc)
            {
                InfoPrintf("CRC OK: %s CRC passed GOLD\n", SurfName);
                *crcStatus = CrcEnums::GOLD;
            }
            else
            {
                ErrPrintf("CRC of %s matches, but mismatch is expected\n", SurfName);
                *crcStatus = CrcEnums::FAIL;
                return true;
            }
        }
        else if (!leadCrcMissing && measuredCrc==leadCrc)
        {
            if (mustMatchCrc)
            {
                InfoPrintf("CRC OK: %s CRC passed LEAD\n", SurfName);
                *crcStatus = CrcEnums::LEAD;
            }
            else
            {
                ErrPrintf("CRC of %s matches, but mismatch is expected\n", SurfName);
                *crcStatus = CrcEnums::FAIL;
                return true;
            }
        }
        else if (leadCrcMissing && goldCrcMissing)
        {
            WarnPrintf("CRC MISS: %s CRC missing\n", SurfName);
            *crcStatus = CrcEnums::MISS;
            return true; // Tell caller to dump image
        }
        else
        {
            if (mustMatchCrc)
            {
                if (leadCrcMissing)
                {
                    ErrPrintf("CRC WRONG!! %s CRC failed, computed 0x%08x != 0x%08x\n",
                        SurfName, measuredCrc, goldCrc);
                }
                else
                {
                    ErrPrintf("CRC WRONG!! %s CRC failed, computed 0x%08x != 0x%08x\n",
                        SurfName, measuredCrc, leadCrc);
                }
                *crcStatus = CrcEnums::FAIL;

                if (!CopyCrcValues(checkProfileKey, dumpProfileKey, profile))
                    return false;

                // Update profile with measured crc value
                profile->WriteHex("trace_dx5", dumpProfileKey.c_str(), measuredCrc, true);
                return true; // Tell caller to dump image
            }
            else
            {
                InfoPrintf("Expected CRC MISMATCH of %s, computed 0x%08x != 0x%08x\n",
                            SurfName, measuredCrc, leadCrc);
                if (!goldCrcMissing)
                {
                    InfoPrintf("CRC OK: %s CRC passed GOLD\n", SurfName);
                    *crcStatus = CrcEnums::GOLD;
                }
                else if (!leadCrcMissing)
                {
                    InfoPrintf("CRC OK: %s CRC passed LEAD\n", SurfName);
                    *crcStatus = CrcEnums::LEAD;
                }
            }
        }
    }

    return false; // Tell caller not to dump image
}

UINT32 CrcChecker::CallwlateCrc(const string& checkName, UINT32 subdev,
        const SurfaceAssembler::Rectangle *rect)
{
    class SwapRect
    {
        typedef SurfaceAssembler::Rectangle SurfRect;

    public:
        SwapRect(SurfRect* pOld, const SurfRect* pNew)
            : m_pOld(pOld), m_bak(*pOld)
        {
            if (pNew)
            {
                *pOld = *pNew;
            }
        }
        ~SwapRect() { *m_pOld = m_bak; }
    private:
        SurfaceAssembler::Rectangle* m_pOld;
        SurfaceAssembler::Rectangle  m_bak;
    };

    UINT32 crcValue = 0;
    SurfInfo2* pInfo = 0;
    DmaBufferInfo2* pBufInfo = 0;

    //NOTE: only color and alloc surfs are searched for now
    if ((pInfo = FindSurfaceInfo(checkName)))
    {
        if (m_DmaUtils->SetupDmaCheck(subdev) != OK)
        {
            ErrPrintf("%s: SetupDmaCheck() failed!\n", __FUNCTION__);
            return 0;
        }

        if (m_params->ParamPresent("-dma_check") &&
            m_params->ParamUnsigned("-dma_check") == 6)
        {
            SwapRect swp(pInfo->GetRect(), rect);
            if(!pInfo->GpuComputeCRC(subdev))
            {
                ErrPrintf("Error reading surfaces during crc checks!\n");
            }
        }
        else
        {
            SwapRect swp(pInfo->GetRect(), rect);
            if(!pInfo->CpuComputeCRC(subdev))
            {
                ErrPrintf("Error reading surfaces during crc checks!\n");
            }
        }
        crcValue = pInfo->GetCRC();

        // remember the surface for skipping
        m_skipSurfaces.insert(pInfo->GetSurface());
    }
    else if ((pBufInfo = FindDmaBufferInfo(checkName)))
    {
        if (m_DmaUtils->SetupDmaCheck(subdev) != OK)
        {
            ErrPrintf("%s: SetupDmaCheck() failed!\n", __FUNCTION__);
            return 0;
        }

        if (m_params->ParamPresent("-dma_check") &&
            m_params->ParamUnsigned("-dma_check") == 6)
        {
            if(!pBufInfo->GpuComputeCRC(subdev))
            {
                ErrPrintf("Error reading surfaces during crc checks!\n");
            }
        }
        else
        {
            if(!pBufInfo->CpuComputeCRC(subdev))
            {
                ErrPrintf("Error reading surfaces during crc checks!\n");
            }
        }
        crcValue = pBufInfo->GetCRC();

        // remember the surface for skipping
        m_skipSurfaces.insert(pBufInfo->GetSurface());
    }
    else
    {
        ErrPrintf("Cannot find \"%s\" for CRC callwlation.\n", checkName.c_str());
    }
    return crcValue;
}

bool CrcChecker::CopyCrcValues
(
    const string& prevKey,
    const string& newKey,
    ICrcProfile* profile
)
{
    const string &gpuKey = m_verifier->GetGpuKey();
    CrcChainManager* crcChainMgr = m_verifier->CrcChain_Manager();

    if (gpuKey != *crcChainMgr->GetCrcChain().begin())
    {
        // remove trailing _0c/_0z/_0C/etc chars
        string::size_type pos = prevKey.length() - 3;
        const string& prevPrefix = prevKey.substr(0, pos);

        pos = newKey.length() - 3;
        const string& newPrefix = newKey.substr(0, pos);

        if(profile->CopyCrcValues("trace_dx5", prevPrefix.c_str(), newPrefix.c_str()) == false)
        {
            ErrPrintf("Failed to copy crc values from: %s to: %s\n",
                prevPrefix.c_str(), newPrefix.c_str());
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------------
//  compare measured crc's w/ lead/gold values
// ---------------------------------------------------------------------------------
bool CrcChecker::CompareCrcs
(
    TestEnums::TEST_STATUS& status,
    CrcStatus& crcFinal,
    char* key,
    const string& tgaProfileKey,
    ITestStateReport* report,
    UINT32 subdev,
    const SurfaceSet *skipSurfaces
)
{
    TestEnums::TEST_STATUS intrStatus = m_verifier->CheckIntrPassFail();
    CrcStatus crcAll = CrcEnums::GOLD;
    CrcStatus crcLwrrent;
    bool dump_all = false;
    bool dump_lwr = false;

    foreach_all_surfaces(surfIt)
    {
        if (!(*surfIt)->IsValid() ||
            skipSurfaces->count((*surfIt)->GetSurface()))
            continue;
        //skip the crc compare if it is a BlackImage
        MdiagSurf *surf = (*surfIt)->GetSurface();
        LW50Raster *pRaster = m_surfMgr->GetRaster(surf->GetColorFormat());
        if(pRaster)
        {
            pRaster->SetBase(reinterpret_cast<uintptr_t>((*surfIt)->GetSurfBase()));
            if (m_blackImageCheck &&
                SelfgildStrategy::IsImageBlack(pRaster, surf->GetWidth(), surf->GetHeight(),
                                               surf->GetDepth(), surf->GetArraySize()))
            {
                ErrPrintf("The image is all black. exit from crc compare\n");
                crcLwrrent = CrcEnums::FAIL;
                report->error("All Black, CRC MISMATCH!!\n");
                return false;
            }
        }

        dump_lwr = (*surfIt)->CompareCRC(key, subdev, report, crcLwrrent);
        if (dump_lwr && (*surfIt)->IsColorOrZ())
        {
            // dump here in case -dump_images_on_crc_fail is not specified
            SurfInfo2* info = (SurfInfo2*)(*surfIt);
            info->DumpSurface(subdev, tgaProfileKey);
            dump_all = true;
        }

        crcAll = VerifUtils::MergeCrcStatus(crcAll, crcLwrrent);
    }
    crcAll = VerifUtils::MergeCrcStatus(crcAll, m_zlwll_passed ? CrcEnums::GOLD : CrcEnums::FAIL);

    // dump all other passing surfaces when there is one failure
    if (dump_all && m_params->ParamPresent("-dump_images_on_crc_fail") > 0)
    {
        // Dump all color images and zeta image if one CRC fails
        foreach_surface (surfIt)
        {
            (*surfIt)->DumpSurface(subdev, tgaProfileKey);
        }

        SurfInfo2* surfZ = m_verifier->SurfZ();
        if (surfZ && surfZ->IsValid())
        {
            surfZ->DumpSurface(subdev, tgaProfileKey);
        }
    }

    // chiplib may perform a custom verification step. lwrrently supported by amodel only
    crcAll = VerifUtils::MergeCrcStatus(crcAll,
                 Platform::PassAdditionalVerification
                 (
                     m_verifier->LWGpu()->GetGpuDevice(),
                     m_verifier->Profile()->GetFileName().c_str()
                 )
                 ? CrcEnums::GOLD : CrcEnums::FAIL
             );

    if(crcAll == CrcEnums::MISS)
        status = TestEnums::TEST_CRC_NON_EXISTANT;
    else if((crcAll == CrcEnums::FAIL) || (intrStatus != TestEnums::TEST_SUCCEEDED))
        status = TestEnums::TEST_FAILED_CRC;
    else if(crcAll == CrcEnums::LEAD)
        crcFinal = CrcEnums::LEAD;
    else
        crcFinal = CrcEnums::GOLD;

    return true;
}

bool CrcChecker::SaveMeasuredCrc(const char* key, ITestStateReport* report, UINT32 subdev)
{
    foreach_all_surfaces(surfIt)
    {
        if (!(*surfIt)->SaveMeasuredCRC(key, subdev))
        {
            report->error("Illegal CRC chaining detected!\n");
            return false;
        }
    }

    return true;
}

bool CrcChecker::BuildProfileKeysAndImgNames
(
    char* key,
    ITestStateReport* report,
    UINT32 gpuSubdevice
)
{
    ICrcProfile* profile = m_verifier->Profile();

    if (profile->GetImgDir() == "" || profile->GetTestName() == "")
        return true;

    foreach_all_surfaces(surfIt)
    {
        if (!(*surfIt)->BuildProfKeyImgName(key, gpuSubdevice))
        {
            report->error("Illegal CRC chaining detected!\n");
            return false;
        }
    }

    return true;
}

RC CrcChecker::FlushPriorCRC(UINT32 subdev)
{
    //Platform::EscapeWrite("CRC_CHECK", 1, 4, 0);
    LwRm* pLwRm = m_verifier->GetLwRmPtr();
    LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS param;
    memset(&param, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));
    // passing in a null param; flags say it all
    param.flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _FULL_CACHE) |
                   DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
                   DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES);

    // const GpuSubdevice *
    GpuSubdevice* pGpuSubdev = m_verifier->LWGpu()->GetGpuSubdevice(subdev);

    return pLwRm->Control(pLwRm->GetSubdeviceHandle(pGpuSubdev),
                          LW2080_CTRL_CMD_FB_FLUSH_GPU_CACHE,
                          &param,
                          sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));
}

//--------------------------------------------------------------------------
// Search the surface info lists using the given CRC check name to
// find verification information about a surface.
//
SurfInfo2* CrcChecker::FindSurfaceInfo(const string &checkName)
{
    foreach_alloc_surface(allocSurfaceIter)
    {
        if ((*allocSurfaceIter)->CrcName() == checkName)
        {
            return *allocSurfaceIter;
        }
    }

    foreach_surface(surfaceIter)
    {
        if ((*surfaceIter)->GetSurfaceName()  == checkName)
        {
            return *surfaceIter;
        }
    }

    if (m_verifier->SurfZ()->GetSurfaceName() == checkName)
    {
        return m_verifier->SurfZ();
    }

    return 0;
}

DmaBufferInfo2* CrcChecker::FindDmaBufferInfo(const string &checkName)
{
    foreach_dmabuf(bufIter)
    {
        if ((*bufIter)->GetSurfaceName() == checkName)
        {
            return *bufIter;
        }
    }

    return 0;
}
