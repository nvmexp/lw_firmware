/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011, 2019, 2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "VerifUtils.h"

#include <map>
#include <vector>
using namespace std;

#include "mdiag/utils/utils.h"
#include "core/include/utility.h"
#include "mdiag/sysspec.h"
#include "mdiag/utils/profile.h"
#include "mdiag/tests/testdir.h"
#include "mdiag/tests/test.h"
#include "gpu/include/gpusbdev.h"
#include "mdiag/utils/mdiag_xml.h"

#include "GpuVerif.h"

#ifndef _WIN32
    #include <unistd.h>
    #include <errno.h>
    #include <limits.h>
    #include <stdlib.h>
#else
    #include <windows.h>
    #include <io.h>
    #include <direct.h>
#endif

namespace VerifUtils
{
    static map<string, int>  s_namesInUse;
    static map<string, PteKindMask> s_PteKindNameToMask;
    static vector<string>    s_imageFileSuffixes(MAX_RENDER_TARGETS);
    static GpuVerif*        s_verifier;
    static const char CrcColorBufferName[MAX_RENDER_TARGETS] =
        {'c', 'b', 'C', 'd', 'e', 'f', 'g', 'h'};

    static const char SLASH_STRING[2] = {DIR_SEPARATOR_CHAR, '\0'};
}

void VerifUtils::Setup(GpuVerif* verifier)
{
    s_verifier = verifier;

    s_imageFileSuffixes[0] = ".tga";
    s_imageFileSuffixes[1] = "_b.tga";
    s_imageFileSuffixes[2] = "_c.tga";
    s_imageFileSuffixes[3] = "_d.tga";
    s_imageFileSuffixes[4] = "_e.tga";
    s_imageFileSuffixes[5] = "_f.tga";
    s_imageFileSuffixes[6] = "_g.tga";
    s_imageFileSuffixes[7] = "_h.tga";
    for (int i = 0; i < MAX_RENDER_TARGETS; i++)
    {
        if (!s_verifier->Params()->ParamPresent("-no_tga_gz"))
            s_imageFileSuffixes[i] += ".gz";
    }

    // build the map between PTE name and mask
    PteKindMask maskPair;
    maskPair.mask_hi_odd = maskPair.mask_hi_evn = 0xffffff;
    maskPair.mask_low_odd = maskPair.mask_low_evn = 0xffff;
    s_PteKindNameToMask["X8Z24_X16V8S8_MS4_VC12"] = maskPair;
    s_PteKindNameToMask["X8Z24_X16V8S8_MS4_VC12_2CS"] = maskPair;
    s_PteKindNameToMask["X8Z24_X16V8S8_MS4_VC12_2CSZV"] = maskPair;
    s_PteKindNameToMask["X8Z24_X16V8S8_MS8_VC24"] = maskPair;
    s_PteKindNameToMask["X8Z24_X16V8S8_MS8_VC24_2CS"] = maskPair;
    s_PteKindNameToMask["X8Z24_X16V8S8_MS8_VC24_2CSZV"] = maskPair;
    s_PteKindNameToMask["X8ZF24_X16V8S8_MS4_VC12"] = maskPair;

    maskPair.mask_hi_odd = maskPair.mask_hi_evn = 0xffffff;
    maskPair.mask_low_odd = 0xff;
    maskPair.mask_low_evn = 0xffff;
    s_PteKindNameToMask["X8Z24_X16V8S8_MS4_VC4"] = maskPair;
    s_PteKindNameToMask["X8Z24_X16V8S8_MS4_VC4_2CS"] = maskPair;
    s_PteKindNameToMask["X8Z24_X16V8S8_MS4_VC4_2CSZV"] = maskPair;
    s_PteKindNameToMask["X8Z24_X16V8S8_MS8_VC8"] = maskPair;
    s_PteKindNameToMask["X8Z24_X16V8S8_MS8_VC8_2CS"] = maskPair;
    s_PteKindNameToMask["X8Z24_X16V8S8_MS8_VC8_2CSZV"] = maskPair;
    s_PteKindNameToMask["X8ZF24_X16V8S8_MS4_VC4"] = maskPair;
    s_PteKindNameToMask["X8ZF24_X16V8S8_MS8_VC8"] = maskPair;

    maskPair.mask_hi_odd = maskPair.mask_hi_evn = 0xffffffff;
    maskPair.mask_low_odd = maskPair.mask_low_evn = 0xff;
    s_PteKindNameToMask["ZF32_X24S8"] = maskPair;
    s_PteKindNameToMask["ZF32_X24S8_2CS"] = maskPair;
    s_PteKindNameToMask["ZF32_X24S8_2CSZV"] = maskPair;
    s_PteKindNameToMask["ZF32_X24S8_MS2"] = maskPair;
    s_PteKindNameToMask["ZF32_X24S8_MS2_2CS"] = maskPair;
    s_PteKindNameToMask["ZF32_X24S8_MS2_2CSZV"] = maskPair;
    s_PteKindNameToMask["ZF32_X24S8_MS4"] = maskPair;
    s_PteKindNameToMask["ZF32_X24S8_MS4_2CS"] = maskPair;
    s_PteKindNameToMask["ZF32_X24S8_MS4_2CSZV"] = maskPair;
    s_PteKindNameToMask["ZF32_X24S8_MS8"] = maskPair;
    s_PteKindNameToMask["ZF32_X24S8_MS8_2CS"] = maskPair;
    s_PteKindNameToMask["ZF32_X24S8_MS8_2CSZV"] = maskPair;

    maskPair.mask_hi_odd = maskPair.mask_hi_evn = 0xffffffff;
    maskPair.mask_low_odd = 0xff;
    maskPair.mask_low_evn = 0xffff;
    s_PteKindNameToMask["ZF32_X16V8S8_MS4_VC4"] = maskPair;
    s_PteKindNameToMask["ZF32_X16V8S8_MS4_VC4_2CS"] = maskPair;
    s_PteKindNameToMask["ZF32_X16V8S8_MS4_VC4_2CSZV"] = maskPair;
    s_PteKindNameToMask["ZF32_X16V8S8_MS8_VC8"] = maskPair;
    s_PteKindNameToMask["ZF32_X16V8S8_MS8_VC8_2CS"] = maskPair;
    s_PteKindNameToMask["ZF32_X16V8S8_MS8_VC8_2CSZV"] = maskPair;

    maskPair.mask_hi_odd = maskPair.mask_hi_evn = 0xffffffff;
    maskPair.mask_low_odd = maskPair.mask_low_evn = 0xffff;
    s_PteKindNameToMask["ZF32_X16V8S8_MS4_VC12"] = maskPair;
    s_PteKindNameToMask["ZF32_X16V8S8_MS4_VC12_2CS"] = maskPair;
    s_PteKindNameToMask["ZF32_X16V8S8_MS4_VC12_2CSZV"] = maskPair;
    s_PteKindNameToMask["ZF32_X16V8S8_MS8_VC24"] = maskPair;
    s_PteKindNameToMask["ZF32_X16V8S8_MS8_VC24_2CS"] = maskPair;
    s_PteKindNameToMask["ZF32_X16V8S8_MS8_VC24_2CSZV"] = maskPair;

    maskPair.mask_hi_odd = maskPair.mask_low_odd = 0xffffff;
    maskPair.mask_hi_evn = maskPair.mask_low_evn = 0xffffffff;
    s_PteKindNameToMask["V8Z24_MS4_VC4"] = maskPair;
    s_PteKindNameToMask["V8Z24_MS4_VC4_2CS"] = maskPair;
    s_PteKindNameToMask["V8Z24_MS4_VC4_2CZV"] = maskPair;
    s_PteKindNameToMask["V8Z24_MS4_VC4_2ZV"] = maskPair;
    s_PteKindNameToMask["V8Z24_MS8_VC8"] = maskPair;
    s_PteKindNameToMask["V8Z24_MS8_VC8_2CS"] = maskPair;
    s_PteKindNameToMask["V8Z24_MS8_VC8_2CZV"] = maskPair;
    s_PteKindNameToMask["V8Z24_MS8_VC8_2ZV"] = maskPair;

    maskPair.mask_hi_odd = maskPair.mask_low_odd = 0xffffff00;
    maskPair.mask_hi_evn = maskPair.mask_low_evn = 0xffffffff;
    s_PteKindNameToMask["Z24V8_MS4_VC4"] = maskPair;
    s_PteKindNameToMask["Z24V8_MS4_VC4_2CS"] = maskPair;
    s_PteKindNameToMask["Z24V8_MS4_VC4_2CZV"] = maskPair;
    s_PteKindNameToMask["Z24V8_MS4_VC4_2ZV"] = maskPair;
    s_PteKindNameToMask["Z24V8_MS8_VC8"] = maskPair;
    s_PteKindNameToMask["Z24V8_MS8_VC8_2CS"] = maskPair;
    s_PteKindNameToMask["Z24V8_MS8_VC8_2CZV"] = maskPair;
    s_PteKindNameToMask["Z24V8_MS8_VC8_2ZV"] = maskPair;
}

// Keep track of file names and add numbers like _2, _3, etc at the end
// of file name if same output file name was used in several tests
string VerifUtils::ModifySameNames(const string& fname)
{
    int nameCount = ++s_namesInUse[fname];
    if(nameCount > 1)
    {
        return fname + Utility::StrPrintf("_%d", nameCount);
    }

    return fname;
}

void VerifUtils::WriteVerifXml(ICrcProfile* profile)
{
    if (gXML != NULL)
    {
        XD->XMLStartStdInline("<MDiagTestDir");
        XD->outputAttribute("now", gXML->GetRuntimeTimeInMilliSeconds());
        XD->outputAttribute("SeqId", Test::FindTestForThread(Tasker::LwrrentThread())->GetSeqId());
        XD->outputAttribute("TestDir", profile->GetTestDir().c_str());
        XD->XMLEndLwrrent();
    }
}

void VerifUtils::WritePostTestCheckXml(const string& profileKey,
        TestEnums::TEST_STATUS status, bool isIntr)
{
    XD->XMLStartStdInline("<MDiagTestKey");
    XD->outputAttribute("now", gXML->GetRuntimeTimeInMilliSeconds());
    XD->outputAttribute("SeqId", Test::FindTestForThread(Tasker::LwrrentThread())->GetSeqId());
    XD->outputAttribute("ErrCount", 0);
    if (!isIntr)
        XD->outputAttribute("Key", profileKey.c_str());
    XD->outputAttribute("Status", status);
    switch(status) {

    case TestEnums::TEST_CRC_NON_EXISTANT:
        XD->outputAttribute("StatusString", "TEST_CRC_NON_EXISTANT");
        break;
    case TestEnums::TEST_SUCCEEDED:
        XD->outputAttribute("StatusString", "TEST_SUCCEEDED");
        break;
    case TestEnums::TEST_FAILED_CRC:
        XD->outputAttribute("StatusString", "TEST_FAILED_CRC");
        break;
    default:
        XD->outputAttribute("StatusString", "TEST_UNKNOWN_OR_ILWALID_STATUS");
    }
    XD->outputAttribute("WarningCount", 0);
    XD->XMLEndLwrrent();
}

FILE* VerifUtils::CreateTestReport(const string& outFilename)
{
    string rptname;

    rptname = TestStateReport::create_status_filename(".rpt",
            outFilename.c_str());

    // Remove any left-over files from previous runs
    unlink (rptname.c_str());

    InfoPrintf("Running crcReport, output report name is %s\n", rptname.c_str());
    FILE* rptFile = fopen(rptname.c_str(), "w");
    if(!rptFile)
    {
        ErrPrintf("Couldn't open report file\n");
    }

    return rptFile;
}

const string& VerifUtils::GetImageFileSuffix(UINT32 index)
{
    MASSERT(index < MAX_RENDER_TARGETS);
    return s_imageFileSuffixes[index];
}

void VerifUtils::AddGpuTagToDumpName(string *tgaName, const UINT32 subdev)
{
    if (subdev > 0)
    {
        string gpuTag = Utility::StrPrintf("_GPU%d", subdev);
        tgaName->insert(tgaName->rfind(".tga"), gpuTag);
    }
}

PteKindMask VerifUtils::GetPtekindMask(UINT32 kind, UINT32 gpuSubdevice)
{
    MASSERT(s_verifier);

    PteKindMask pteMask;
    GpuSubdevice* pGpuSubdev = s_verifier->LWGpu()->GetGpuSubdevice(gpuSubdevice);
    MASSERT(pGpuSubdev != 0);

    const char* pteName = pGpuSubdev->GetPteKindName(kind);
    if (pteName != 0)
    {
        map<string, PteKindMask>::const_iterator it;
        it = s_PteKindNameToMask.find(pteName);
        if (it != s_PteKindNameToMask.end())
        {
            pteMask = it->second;
        }
    }

    return pteMask;
}

UINT32 VerifUtils::GetOffsetMask(UINT32 offset, PteKindMask *pMaskPair)
{
    MASSERT(offset % 4 == 0);

    if (offset % 8 == 0)
        return (offset/(8*8) & 0x1) ? pMaskPair->mask_hi_odd : pMaskPair->mask_hi_evn;
    else if (offset % 8 == 4)
        return (offset/(8*8) & 0x1) ? pMaskPair->mask_low_odd : pMaskPair->mask_low_evn;
    else
        return 0;
}

// Fail < Missing < Lead < Gold
CrcStatus VerifUtils::MergeCrcStatus(CrcStatus a, CrcStatus b)
{
    if ((a == CrcEnums::FAIL) || (b == CrcEnums::FAIL))
        return CrcEnums::FAIL;
    if ((a == CrcEnums::MISS) || (b == CrcEnums::MISS))
        return CrcEnums::MISS;
    if ((a == CrcEnums::LEAD) || (b == CrcEnums::LEAD))
        return CrcEnums::LEAD;
    return CrcEnums::GOLD;
}

int VerifUtils::p4Action(const char *action, const char *filename)
{
    int retval = P4Action(action, filename);
    if (retval)
    {
        ErrPrintf("p4 %s %s failed\n", action, filename);
        ErrPrintf("%s\n", strerror(retval));
    }
    else
    {
        InfoPrintf ("p4 %s %s ... done\n", action, filename);
    }
    return retval;
}

char VerifUtils::GetCrcColorBufferName(UINT32 index)
{
    MASSERT(index < MAX_RENDER_TARGETS);
    return CrcColorBufferName[index];
}

string VerifUtils::create_imgname
(
    const char *imgdirname,
    const char *testname,
    const char *key,
    const char *suffix,
    bool chkexist
)
{
    // imgname = imgdirname + IMG_ + key + '/' + testname [+ '=' scene] + suffix
    string imgname = imgdirname;
    imgname += "IMG_";
    imgname += key;

    imgname += SLASH_STRING;

    if (chkexist)
    {
        // check to make sure our image directory exists
        if (access(imgname.c_str(), 0) == -1)
        {
#ifndef _WIN32
            mkdir(imgname.c_str(), 0777);
#else
            mkdir(imgname.c_str());
#endif
        }
    }

    imgname += testname;
    imgname += suffix;

    return imgname;
}

void VerifUtils::touch_bogus_crc_file (ITestStateReport *report,
        UINT32 gpu, UINT32 crcA, UINT32 crcB, UINT32 crcZ)
{
    char str[256];
    DebugPrintf("Writing to bogus crc file\n");

    sprintf(str, "This is a bogus crc file, since the GPU (0x%02x) you are running\n", gpu);
    report->crcCheckFailed(str);
    report->crcCheckFailed("doesn't support crc dumping!!!  Following are the crc's generated\n");
    report->crcCheckFailed("from this test, they are for your colwenience only, please do not\n");
    report->crcCheckFailed("check these crc values into the tree!!!!\n\n");

    report->crcCheckFailed("If however, you know what you are doing, you can use the following options:\n\n");
    report->crcCheckFailed("-crlwpdate          Also perform p4 edit/add if necessary, works only on linux system\n");
    report->crcCheckFailed("-crcAllowLocalDump  This will dump crc to local directory, you must manually copy the crc to the appropriate place in trace tree, p4 edit/add if necessary.\n");

    sprintf(str, "colorA=0x%08x colorB=0x%08x z=0x%08x\n", crcA, crcB, crcZ);
    report->crcCheckFailed(str);
}

bool VerifUtils::CreateGoldCrc(const char* gold_filename, const char* crchain)
{
    FILE *fd = fopen(gold_filename, "w");
    if(fd)
    {
        char *user;
        InfoPrintf("Created default golden.crc file: %s\n", gold_filename);

        fprintf(fd, "[trace_dx5]\n");
        user = getelw("P4USER");
        if (user==NULL) user = getelw("USERNAME");
        if (user==NULL) user = getelw("USER");
        if (user==NULL) user = getelw("LOGNAME");
        if (user)
            fprintf(fd, "%s_approver=%s\n\n", crchain, user);
        else
            ErrPrintf("   Could not get a valid username to add approver to golden.crc file\n");

        fclose(fd);

        return true;
    }

    return false;
}

void VerifUtils::optionalCreateDir(const char *filename_in, const ArgReader* params)
{
    char prefix_in[4096];       // prefix of files that should have directory auto-created
    memset(prefix_in, 0, 4096);
    if (params->ParamPresent("-crcLocal") > 0)
    {
        strcpy(prefix_in, params->ParamStr("-crcLocal"));
    }

    char dirname[4096], prefix[4096];
    memset(dirname, 0, 4096);
    memset(prefix, 0, 4096);
    clean_filename_slashes(dirname, filename_in);
    clean_filename_slashes(prefix, prefix_in);

    char match_list_data[4096]; // this will be the name we are matching, like O:\traces
    char *match_list = match_list_data;
    char sub[4096]; // this will be the name we are substituting, like L:\perforce\arch\traces
    extract_sub_and_match_list(sub, match_list, prefix);

    // so sub is the cleaned path below which we auto-create directories
    // and dirname is the cleaned path of the file we're considering
    InfoPrintf("crcLocal: sub = %s\n", sub);
    InfoPrintf("crcLocal: dirname = %s\n", dirname);
    if (0 == memcmp(sub, dirname, strlen(sub))) {
        InfoPrintf("crcLocal target directory match: YES\n");
        char *last = strchr(dirname, DIR_SEPARATOR_CHAR);       // skip first /--never create root directory
        if (last == NULL)
            return;             // could not find any / characters
        for (last = strchr(last+1, DIR_SEPARATOR_CHAR); last != NULL; last = strchr(last+1, DIR_SEPARATOR_CHAR)) {
            char savechar = last[0];
            last[0] = '\0';
            // InfoPrintf("crcLocal: considering directory at %s\n", dirname);
            // check to make sure our directory exists
            if(access(dirname, 0) == -1) {
                InfoPrintf("crcLocal: creating directory at %s\n", dirname);
#ifndef _WIN32
                mkdir(dirname, 0777);
#else
                mkdir(dirname);
#endif
            }
            last[0] = savechar;
        }
    } else {
        InfoPrintf("crcLocal target directory match: NO\n");
        return;
    }
}

