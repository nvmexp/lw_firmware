/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_CRLWTILS_H
#define INCLUDED_CRLWTILS_H

#ifndef INCLUDED_STDIO_H
#include <stdio.h>
#define INCLUDED_STDIO_H
#endif

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

#include "core/include/argread.h"
#include "mdiag/utils/CrcInfo.h"
#include "mdiag/utils/TestInfo.h"
#include "mdiag/utils/ICrcProfile.h"
#include "mdiag/tests/test_state_report.h"

class GpuVerif;
typedef CrcEnums::CRC_STATUS CrcStatus;

struct PteKindMask
{
    UINT32 mask_hi_odd;
    UINT32 mask_hi_evn;
    UINT32 mask_low_odd;
    UINT32 mask_low_evn;
    PteKindMask()
    {
        mask_hi_odd = mask_hi_evn = 0xffffffff;
        mask_low_odd = mask_low_evn = 0xffffffff;
    }
};

namespace VerifUtils
{
    void Setup(GpuVerif* params);

    string ModifySameNames(const string& fname);
    FILE* CreateTestReport(const string& fname);

    void WriteVerifXml(ICrcProfile* profile);
    void WritePostTestCheckXml(const string& profileKey,
            TestEnums::TEST_STATUS status, bool isIntr = false);
    const string& GetImageFileSuffix(UINT32 index);

    void AddGpuTagToDumpName(string *tgaName, const UINT32 subdev);
    PteKindMask GetPtekindMask(UINT32 kind, UINT32 gpuSubdevice);

    UINT32 GetOffsetMask(UINT32 offset, PteKindMask *maskPair);

    CrcStatus MergeCrcStatus(CrcStatus a, CrcStatus b);

    char GetCrcColorBufferName(UINT32 index);

    int p4Action(const char *action, const char *filename);

    string create_imgname
    (
        const char *imgdirname,
        const char *testname,
        const char *key,
        const char *suffix,
        bool chkexist
    );

    void touch_bogus_crc_file (ITestStateReport *report, UINT32 gpu,
            UINT32 crcA, UINT32 crcB, UINT32 crcZ);

    bool CreateGoldCrc(const char* gold_filename, const char* crchain);

    void optionalCreateDir(const char *filename_in, const ArgReader* params);
};

#endif /* CRLWTILS_H_ */
