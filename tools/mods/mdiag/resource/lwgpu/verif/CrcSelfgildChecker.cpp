/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011, 2020 by LWPU Corporation.  All rights reserved.  
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/utility/surf2d.h"
#include "mdiag/utils/raster_types.h"
#include "mdiag/utils/ICrcProfile.h"
#include "mdiag/tests/test_state_report.h"
#include "mdiag/tests/gpu/trace_3d/selfgild.h"

#include "mdiag/resource/lwgpu/SurfaceReader.h"

#include "ProfileKey.h"
#include "VerifUtils.h"
#include "DumpUtils.h"
#include "CrcSelfgildChecker.h"

#define REGULAR_VERIF() \
    CrcChecker::Verify(key, measuredCrc, surfInfo, subdev, report, crcStatus)

// Read stored crcs and compare with measured crc. Return true if caller should dump image
bool CrcSelfgildChecker::Verify
(
    const char *key, // Key used to find stored crcs
    UINT32 measuredCrc, // Measured crc
    SurfaceInfo* surfInfo,
    UINT32 subdev,
    ITestStateReport *report, // File to touch in case of error
    CrcStatus *crcStatus // Result of comparison
)
{
    if (m_verifier->ReportExists() ||
        surfInfo->GetCheckInfo() ||
        m_params->ParamPresent("-ignore_selfgild"))
        return REGULAR_VERIF();

    ICrcProfile* profile = m_verifier->Profile();
    MdiagSurf* surf = surfInfo->GetSurface();

    LW50Raster *pRaster = m_surfMgr->GetRaster(surf->GetColorFormat());
    UINT32 Width = surf->GetWidth();
    UINT32 Height = surf->GetHeight();
    UINT32 leadCrc = 0;
    bool leadCrcMissing = false; // Crc from lead profile

    // Callwlate profile key string for crc to check (search gpu chain)
    string checkProfileKey("");
    if(!surfInfo->GenProfileKey(key, true, subdev, checkProfileKey))
    {
        ErrPrintf("Illegal CRC chaining detected!!\n");
        report->error("Illegal CRC chaining detected!\n");
        return true;
    }

    // Callwlate profile key string for crc to dump (don't search gpu chain)
    string dumpProfileKey("");
    if(!surfInfo->GenProfileKey(key, false, subdev, dumpProfileKey))
    {
        ErrPrintf("Illegal CRC chaining detected!\n");
        report->error("Illegal CRC chaining detected!\n");
        return true;
    }

    //No test is supposed to produce a black image
    if (SelfgildStrategy::IsImageBlack(pRaster, Width, Height, surf->GetDepth(), surf->GetArraySize()))
    {
        ErrPrintf("The image is all black. Even not trying selfgilding\n");
        // to propery process crc file
        REGULAR_VERIF();
        *crcStatus = CrcEnums::FAIL;
        report->error("GOLD and Test CRC MISMATCH!!\n");

        return true;
    }

    bool self_gild_passed = true;

    SurfInfo2* czSurfInfo = (SurfInfo2*)surfInfo;
    SelfgildStrategies& strategies = m_verifier->GetSelfgildStrategies();
    for (SelfgildStrategies::const_iterator i = strategies.begin(); i != strategies.end(); ++i)
    {
        if (!*i) // running in a mode incompatible with selfgild
            return REGULAR_VERIF();
        InfoPrintf("Selfgilding *%s-%d* with [%s] strategy...\n",
                pRaster->ZFormat() ? "Depth" : "Color", czSurfInfo->ColorIndex(), (*i)->Name().c_str());
        bool strategy_passed = (*i)->Gild(pRaster, Width, Height, surf->GetDepth(), surf->GetArraySize());
        if (strategy_passed)
            InfoPrintf("*%s-%d* passed [%s] strategy\n",
                   pRaster->ZFormat() ? "Depth" : "Color", czSurfInfo->ColorIndex(), (*i)->Name().c_str());
        else
            ErrPrintf("*%s-%d* failed %s strategy\n",
                   pRaster->ZFormat() ? "Depth" : "Color", czSurfInfo->ColorIndex(), (*i)->Name().c_str());

        {
            char comment[128];
            sprintf(comment, "selfgild_%c %s %s",
                pRaster->ZFormat() ? 'z' : VerifUtils::GetCrcColorBufferName(czSurfInfo->ColorIndex()),
                strategy_passed ? "Pass" : "Fail",
                (*i)->Name().c_str());

            czSurfInfo->GetCRCInfo().SelfGildInfo = comment;
        }

        self_gild_passed &= strategy_passed;
    }

    if (checkProfileKey.length() == 0)
    {
        // Couldn't find crc to check against
        m_verifier->WriteToReport("crcReport for %s : key = NULL\n", surf->GetName().c_str());
        m_verifier->WriteToReport("    Error. Could not generate key for %s\n", surf->GetName().c_str());

        *crcStatus = CrcEnums::MISS;
    }
    else
    {
        // Read lead crc
        bool update_profile = false;
        if (profile->ReadStr("trace_dx5", checkProfileKey.c_str(), 0))
        {
            leadCrc = profile->ReadUInt("trace_dx5", checkProfileKey.c_str(), 0);
        }
        else
        {
            leadCrcMissing = true;
            update_profile = true;
        }

        bool crc_ret = true;
        if (!self_gild_passed)
        {
            WarnPrintf("%s failed selfgild, trying CRC approach...\n", checkProfileKey.c_str());
            crc_ret = REGULAR_VERIF();
        }

        if (update_profile)
        {
            profile->WriteHex("trace_dx5", dumpProfileKey.c_str(), measuredCrc, true);
        }

        if (m_forceCrc ||
            (m_crcMode==CrcEnums::CRC_DUMP || m_crcMode==CrcEnums::CRC_UPDATE))
        {
            char comment[64];
            sprintf(comment, "# SELFGILD: %s  CRC=0x%08x", self_gild_passed ? "PASS" : "FAIL", measuredCrc);
            profile->WriteHex("trace_dx5", dumpProfileKey.c_str(), measuredCrc, true);
            profile->WriteComment("trace_dx5", dumpProfileKey.c_str(), comment);
        }

        if (!self_gild_passed)
        {
            if (CrcEnums::MISS == *crcStatus)
            {
                // If selfgild fails, lead and gold are both missing, we mark this test as FAILED
                // See detail at bug 758917
                ErrPrintf("CRC MISS when selfgild failed, test failed\n");
                *crcStatus = CrcEnums::FAIL;
            }
            else if (CrcEnums::LEAD == *crcStatus &&
                     (!m_verifier->LWGpu()->GetGpuSubdevice(subdev)->HasBug(849226) ||
                      m_params->ParamPresent("-failtest_on_selfgildfail_crclead") > 0))
            {
                // Bug 849226 has more details
                // Amodel and gk20a-: disabled by default + switch to enable
                // gk20a and gk20a+: enabled by default.
                ErrPrintf("Test failed! Test with status (selfgild fail + crc pass lead) "
                    "will be marked as FAILED!\n");
                *crcStatus = CrcEnums::FAIL;
            }
            return crc_ret;
        }

        // there are gilding scripts which depends on the following InfoPrintf(s) please change them carefully
        InfoPrintf("Profile key checked: %s\n", checkProfileKey.c_str());

        // Check measured crc against crcs from profile
        if (m_verifier->ReportExists()) // Just report
        {
            m_verifier->WriteToReport("crcReport for %s : key = %s\n", surf->GetName().c_str(), checkProfileKey.c_str());

            if(leadCrcMissing)
            {
                *crcStatus = CrcEnums::MISS;
                m_verifier->WriteToReport("    LEAD crc value is missing\n");
            }
            else
            {
                if (self_gild_passed)
                    *crcStatus = CrcEnums::GOLD;
                else
                    *crcStatus = CrcEnums::FAIL;

                m_verifier->WriteToReport("    LEAD crc value = 0x%08x\n", leadCrc);
            }
        }
        else
        {
            if (leadCrcMissing)
            {
                InfoPrintf("CRC MISS: %s CRC missing\n", surf->GetName().c_str());
                *crcStatus = CrcEnums::MISS;
                return true; // Tell caller to dump image
            }
            else if (measuredCrc != leadCrc)
            {
                InfoPrintf("CRC WRONG!! %s CRC failed, computed 0x%08x != 0x%08x\n",
                        surf->GetName().c_str(), measuredCrc, leadCrc);
                *crcStatus = CrcEnums::FAIL;

                if (!CopyCrcValues(checkProfileKey, dumpProfileKey, profile))
                {
                    return false;
                }
                // Update profile with measured crc value
                profile->WriteHex("trace_dx5", dumpProfileKey.c_str(), measuredCrc, true);
                return true; // Tell caller to dump image
            }
            else
            {
                *crcStatus = CrcEnums::GOLD;
                return false; // Tell caller not to dump image
            }
        }
    }

    return false; // Tell caller not to dump image
}
