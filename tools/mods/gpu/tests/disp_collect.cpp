/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
//      disp_collect.cpp - DispTest class / namespace definitions for crc collection functions.
//      Copyright (c) 1999-2007 by LWPU Corporation.
//      All rights reserved.
//
//      Written by: John Weidman
//
//      Routines in this module:
//      CollectLwDpsCrcs                  Collect lwdps 1.5 strip CRCs
//
//

#include "Lwcm.h"
#include "lwcmrsvd.h"

#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "gpu/include/dispchan.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/color.h"
#include "core/include/utility.h"
#include "core/include/imagefil.h"
#include "random.h"
#include "lwos.h"
#include "core/include/gpu.h"
#include "core/include/evntthrd.h"
#include <math.h>
#include <list>
#include <map>
#include <errno.h>

#ifdef USE_DTB_DISPLAY_CLASSES
#include "gpu/js/dispt_sr.h"
#else
#include "gpu/js/dispt_sr.h"
#endif

#include "core/utility/errloggr.h"

#include "gt215/dev_disp.h"

#include "class/cl507a.h"
#include "class/cl507b.h"
#include "class/cl507c.h"
#include "class/cl507d.h"
#include "class/cl507e.h"
#include "ctrl/ctrl5070.h"
#include "ctrl/ctrl0080.h"

#include "class/cl8270.h"
#include "class/cl827a.h"
#include "class/cl827b.h"
#include "class/cl827c.h"
#include "class/cl827d.h"
#include "class/cl827e.h"

#include "class/cl8370.h"
#include "class/cl837c.h"
#include "class/cl837d.h"
#include "class/cl837e.h"

#include "class/cl8870.h"
#include "class/cl887d.h"

#include "class/cl8570.h"
#include "class/cl857a.h"
#include "class/cl857b.h"
#include "class/cl857c.h"
#include "class/cl857d.h"
#include "class/cl857e.h"

#define DISPTEST_PRIVATE
#include "core/include/disp_test.h"
#include "gpu/include/vgacrc.h"

namespace DispTest
{
    // No local ("static") routines needed

    // No access to DispTest member data needed
}

/****************************************************************************************
 * DispTest::CollectLwDpsCrcs
 *
 *  Arguments Description:
 *  - Head:             head the notifier is associated with
 *
 *  Functional Description:
 *    - Collect lwdps 1.5 strip CRCs for the specified head.
 *      Pass them to CollectLwstom for later output
 *
 *    - Note: There is some special ordering here of the reads. We read registers that RTL
 *            will change soonest first. In HW the strip crcs in particular will change in
 *            the next frame as the strips are encountered, and some stats that are based
 *            on the number of strips (strip counter) will also update in HW before loadv.
 *            In fmodel all stats are updated at loadv, so we do this ordering to attempt
 *            to keep HW and fmodel in sync.
 *
 ***************************************************************************************/
RC DispTest::CollectLwDpsCrcs( UINT32 Head )
{
    RC rc = OK;

    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();
    unsigned int strip_index;
    UINT32 strip_data[LW_PDISP_COMP_LWDPS_CONTROL_MAX_NUM_STRIPS_INIT];
    const int num_bytes_in_buf = 768;  // If we increase the number of strip crcs we need to increase this.
    char buf[num_bytes_in_buf];
    char *pBuf;
    int nChars;
    UINT32 unFrames;
    UINT32 unStrips;
    UINT32 unDeltaGt;
    UINT32 unStatisticsGatherControl[LW_PDISP_COMP_LWDPS_STATISTICS_GATHER_CONTROL__SIZE_2];
    UINT32 unFilteredStatisticsGatherStatus[LW_PDISP_COMP_LWDPS_FILTERED_STATISTICS_GATHER_STATUS__SIZE_2];
    UINT32 unStripsPerFrame;
    int i, j;

    if (LwrrentDevice()->DoSwapHeads) {
        if (Head == 0) Head = 1;
        else if (Head == 1) Head = 0;
    }

    // check that head number is valid
    if ((Head != 0) && (Head != 1))
    {
        Printf(Tee::PriHigh,"Attempt to collect lwdps CRCs for Invalid Head %u (should be Head 0 or Head 1)\n", Head);
        return RC::BAD_PARAMETER;
    }

    unStrips = pSubdev->RegRd32(LW_PDISP_COMP_LWDPS_STATISTICS_STRIPS(Head));

    for (j = 0; j < LW_PDISP_COMP_LWDPS_STATISTICS_GATHER_CONTROL__SIZE_2; j++) {
        unStatisticsGatherControl[j] = pSubdev->RegRd32(LW_PDISP_COMP_LWDPS_STATISTICS_GATHER_CONTROL(Head,j));
    }

    for (j = 0; j < LW_PDISP_COMP_LWDPS_FILTERED_STATISTICS_GATHER_STATUS__SIZE_2; j++) {
        unFilteredStatisticsGatherStatus[j] = pSubdev->RegRd32(LW_PDISP_COMP_LWDPS_FILTERED_STATISTICS_GATHER_STATUS(Head,j));
    }

    // Read the number of strips per frame from the debug register

    unStripsPerFrame = pSubdev->RegRd32( LW_PDISP_COMP_DEBUG_0_BITS(Head) );
    unStripsPerFrame = REF_VAL(LW_PDISP_COMP_DEBUG_0_BITS_LWDPS_STRIPS_PER_FRAME, unStripsPerFrame);

    MASSERT( unStripsPerFrame <= LW_PDISP_COMP_LWDPS_CONTROL_MAX_NUM_STRIPS_INIT );

    // Collect all the strip crc registers

    if ( unStripsPerFrame > 0 )
    {
        for (strip_index = 0; strip_index < unStripsPerFrame; strip_index++) {
            pSubdev->RegWr32(LW_PDISP_COMP_DEBUG_LWDPS_STRIP_CRC_ADDRESS(Head), strip_index);
            strip_data[strip_index] = pSubdev->RegRd32(LW_PDISP_COMP_DEBUG_LWDPS_STRIP_CRC_DATA(Head));
        }
    }

    // Move the framecounter reads last because they are the least time sensitive.

    unFrames = pSubdev->RegRd32(LW_PDISP_COMP_LWDPS_STATISTICS_FRAMES(Head));
    unDeltaGt = pSubdev->RegRd32(LW_PDISP_COMP_LWDPS_STATISTICS_DELTA_GT(Head));

    // Colwert them into string format

    pBuf = buf;

    nChars = sprintf(pBuf, "head=%d : ", Head);
    if ( nChars > 0 ) pBuf += nChars;

    nChars = sprintf(pBuf, "Frames=0x%04x : Strips=0x%06x : DeltaGt=0x%08x : SGC=", unFrames, unStrips, unDeltaGt);
    if ( nChars > 0 ) pBuf += nChars;

    for (i = 0; i < LW_PDISP_COMP_LWDPS_STATISTICS_GATHER_CONTROL__SIZE_2; i++) {
        nChars = sprintf(pBuf, "0x%08x,", unStatisticsGatherControl[i]);
        if ( nChars > 0 ) pBuf += nChars;
    }

    nChars = sprintf(pBuf, " : FSGS=");
    if ( nChars > 0 ) pBuf += nChars;
    for (i = 0; i < LW_PDISP_COMP_LWDPS_FILTERED_STATISTICS_GATHER_STATUS__SIZE_2; i++) {
        nChars = sprintf(pBuf, "0x%08x,", unFilteredStatisticsGatherStatus[i]);
        if ( nChars > 0 ) pBuf += nChars;
    }

    nChars = sprintf(pBuf, " : Strip CRCs=");
    if ( nChars > 0 ) pBuf += nChars;

    for (strip_index = 0; (int) strip_index < (int) unStripsPerFrame; strip_index++) {
        if ( strip_index == 0 )
            nChars = sprintf(pBuf, "0x%08x", strip_data[strip_index]);
        else
            nChars = sprintf(pBuf, " ,0x%08x", strip_data[strip_index]);

        if ( nChars > 0 ) pBuf += nChars;
    }

    MASSERT( (pBuf - buf) <= num_bytes_in_buf );

    CHECK_RC_MSG
    (
        DispTest::CollectLwstomEvent(string("lwdps"), string(buf)),
        "***ERROR: DispTest::CollectLwDpsCrcs - DispTest::CollectLwstomEvent failed"
    );

    // RC == OK at this point
    return rc;
}

/****************************************************************************************
 * DispTest::WriteLwstomEventFiles
 *
 *  Functional Description:
 *  - Write the custom event files collected with CollectLwstomEvent
 ***************************************************************************************/
RC DispTest::WriteLwstomEventFiles()
{
    string filename;

    map< string, list<string> * >::iterator ce_map_iter;
    for (ce_map_iter = LwrrentDevice()->pLwstomEventMap->begin();
         ce_map_iter != LwrrentDevice()->pLwstomEventMap->end();
         ce_map_iter++)
    {
        filename = "test." + ce_map_iter->first;  // put the extension on the filename

        // open the file
        FILE *fp = NULL;
        RC rc = Utility::OpenFile(filename.c_str(), &fp, "w+");
        if (rc != OK)
        {
            Printf(Tee::PriHigh,"Unable to Open Custom Event File: %s\n",
                   filename.c_str());
            return rc;
        }

        // write subtest header
        fprintf(fp, "[filetype=%s]\n", ce_map_iter->first.c_str());
        fprintf(fp, "[owner=%s]\n",   LwrrentDevice()->pCrcInfo->p_owner->c_str());
        fprintf(fp, "[test=%s]\n",    LwrrentDevice()->pCrcInfo->p_test_name->c_str());
        fprintf(fp, "[subtest=%s]\n\n", LwrrentDevice()->pCrcInfo->p_subtest_name->c_str());

        // write data for each custom event
        // list<string> *ce_list = ce_map_iter->second;
        list<string>::iterator e_iter;
        for (e_iter = ce_map_iter->second->begin();
             e_iter != ce_map_iter->second->end();
             e_iter++)
        {
            // print the associated event string
            // write the event
            fprintf(fp, "%s\n", e_iter->c_str());
        }

        // close the file
        fclose(fp);
    }

    return OK;
}

/****************************************************************************************
 * DispTest::LwstomInitialize
 *
 *  Arguments Description:
 *
 *  Functional Description:
 *  - Initialize the custom event capture
 ***************************************************************************************/
RC DispTest::LwstomInitialize()
{

    // initialize custom lookup maps
    LwrrentDevice()->pLwstomEventMap = new map< string, list<string> * >();

    return OK;
}

/****************************************************************************************
 * DispTest::LwstomCleanup
 *
 *  Functional Description:
 *  - Deallocate memory used for custom event capture data structures
 ***************************************************************************************/
void DispTest::LwstomCleanup()
{
    /* cleanup custom event info */

    map< string, list<string> * >::iterator ce_map_iter;
    for (ce_map_iter = LwrrentDevice()->pLwstomEventMap->begin();
         ce_map_iter != LwrrentDevice()->pLwstomEventMap->end();
         ce_map_iter++)
    {
        list<string> *ce_list = ce_map_iter->second;
        delete ce_list;
    }
    delete LwrrentDevice()->pLwstomEventMap;

    return;
}

/****************************************************************************************
 * DispTest::CollectLwstomEvent
 *
 *  Functional Description:
 *  - Collect the custom event strings into the capture data structures
 ***************************************************************************************/
RC DispTest::CollectLwstomEvent(string  LwstomFileName, string LwstomEvent)
{
    // If this is the first time we've seen this filename, create the mapping from filename to data
    if (LwrrentDevice()->pLwstomEventMap->find(LwstomFileName) == LwrrentDevice()->pLwstomEventMap->end())
    {
        list<string> * pLines = new list<string>;
        (*LwrrentDevice()->pLwstomEventMap)[LwstomFileName] = pLines;
    }

    // save the custom event string in our local list
    (*LwrrentDevice()->pLwstomEventMap)[LwstomFileName]->push_back(LwstomEvent);

    return OK;
}

//
//      Copyright (c) 1999-2005 by LWPU Corporation.
//      All rights reserved.
//
