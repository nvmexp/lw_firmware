/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2016,2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file crccomparison.cpp
//! \brief File to handle Display CRC comparison of XML format
//!

#include "core/include/display.h"
#include "gpu/include/gpudev.h"
#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "xmlwrapper.h"
#include "crccomparison.h"
#include "core/include/utility.h"
#include "core/include/massert.h"
#include "string.h"
#include "class/cl5070.h" // For IsSupported()
#include "class/cl8270.h" // For IsSupported()
#include "class/cl8370.h" // For IsSupported()
#include "class/cl8870.h" // For IsSupported()
#include "class/cl8570.h" // For IsSupported()
#include "class/cl9070.h" // For IsSupported()
#include "class/cl9170.h" // For IsSupported()
#include "class/cl9270.h" // For IsSupported()
#include "class/cl9470.h" // For IsSupported()
#include "class/cl9570.h" // For IsSupported()
#include "class/cl9770.h" // For IsSupported()
#include "class/cl9870.h" // For IsSupported()
#include "class/cl507dcrcnotif.h" // Core channel crc notifier
#include "class/cl907dcrcnotif.h" // Core channel crc notifier
#include <sys/stat.h>
#include "core/include/memcheck.h"
#include "gpu/display/lwdisplay/js_lwcctx.h"
#include "class/clc37d.h" // LWC37D_CORE_CHANNEL_DMA
#include "class/clc37e.h" // LWC37E_WINDOW_CHANNEL_DMA
#include "class/clc370.h" // For IsSupported()
#include "class/clc37dcrcnotif_sw.h"

#define DUMP2FILE(filePointer, buff)                  \
if (filePointer)                                      \
{                                                     \
    fprintf(filePointer,"%s", buff);                  \
}                                                     \

//!
//! DumpCrcInFile : Public Interface to dump the CRC Notifier's Data into a XML file
//! Arguments :
//! FileName : FileName where user of the API wants to Dump the data. If File
//!            already exists it will just append the new Notifier's data else
//!            create the file
//! pGpuDev : Get Lwrrently Bound Device
//! bIsCoreChannel : To specify whether captured CRC was for Base or Core channel
//!                  so that it can be specified in XML file
//! Return RC::OK on Success
//! \sa CompareCrcFiles
//!
//------------------------------------------------------------------------------
RC CrcComparison::DumpCrcToXml(GpuDevice *pGpuDev,
                               const char *filename,
                               void *pCrcSettings,
                               bool CreateNewFile,
                               bool PrintAllCrcs)
{
    RC rc;
    UINT32 DisplayClass;

    Display *pDisplay = pGpuDev->GetDisplay();

    DisplayClass = pDisplay->GetClass();

    if (pDisplay->GetDisplayClassFamily() == Display::EVO)
    {
        if ((DisplayClass == LW50_DISPLAY)  ||
            (DisplayClass == G82_DISPLAY)   ||
            (DisplayClass == G94_DISPLAY)   ||
            (DisplayClass == GT200_DISPLAY) ||
            (DisplayClass == GT214_DISPLAY) ||
            (DisplayClass == LW9070_DISPLAY) ||
            (DisplayClass == LW9170_DISPLAY) ||
            (DisplayClass == LW9270_DISPLAY) ||
            (DisplayClass == LW9470_DISPLAY) ||
            (DisplayClass == LW9570_DISPLAY) ||
            (DisplayClass == LW9770_DISPLAY) ||
            (DisplayClass == LW9870_DISPLAY))
        {
            CHECK_RC(Crc01XNotifier2Xml(Gpu::DeviceIdToInternalString(pGpuDev->DeviceId()).c_str(),
                                        filename,
                                        pCrcSettings,
                                        CreateNewFile,
                                        PrintAllCrcs));
        }
        else
        {
               Printf(Tee::PriHigh,"DumpCrcToXml: Invalid or Not supported class 0x%x\n", DisplayClass);
               return RC::SOFTWARE_ERROR;
        }
    }
    else if (pDisplay->GetDisplayClassFamily() == Display::LWDISPLAY)
    {
            CHECK_RC(Crc03XNotifier2Xml(Gpu::DeviceIdToInternalString(pGpuDev->DeviceId()).c_str(),
                                        filename,
                                        pCrcSettings,
                                        CreateNewFile,
                                        PrintAllCrcs));
     }
    else
    {
        Printf(Tee::PriHigh,"DumpCrcToXml: This Operation is only supported for this Display family \n");
        return RC::SOFTWARE_ERROR;
    }
    return rc;
}

//! FileExists: Checks if file exists
//! Returns 1 : when File Exists
//! Returns 0 : When File Does not Exits
int CrcComparison::FileExists(const char *fname, vector<string> *pSearchPath)
{
    bool flag=false;

    if (!fname)
    {
        Printf(Tee::PriHigh,"\n Filename cannot be NULL.");
        return 0;
    }
    if (!pSearchPath)
    {
        pSearchPath = new vector<string>();
        pSearchPath->push_back (".");
        Utility::AppendElwSearchPaths(pSearchPath,"LD_LIBRARY_PATH");
        Utility::AppendElwSearchPaths(pSearchPath,"MODS_RUNSPACE");
        flag=true;
    }

    string FileNameFromPath = Utility::FindFile(fname, *pSearchPath);

    if(flag)
        delete pSearchPath;

    if (FileNameFromPath.size() == 0)
    {
        Printf(Tee::PriHigh," \n%s: File %s not found",
            __FUNCTION__, FileNameFromPath.c_str());
        return 0;
    }

    return 1;
}

//!
//! Crc01XNotifier2Xml : Function which actually Dump DISP01X CRC notifier to XML file
//! Arguments :
//! GpuDevID : GpuID of Lwrrently Bound Device
//! FileName : FileName where user of the API wants to Dump the data. If File
//!            already exists it will just append the new Notifier's data else
//!            create the file
//! pCrcSettings : EvoCRCSettings variable that stores Crc buffer and notifier details
//! CreateNewFile : TRUE = Create A New Crc XML File
//!                 FALSE = Append to existing XML file
//! Return RC::OK if success
//! \sa DumpCrcInFile
//! \sa CompareCrcFiles
//!
//------------------------------------------------------------------------------
RC CrcComparison::Crc01XNotifier2Xml(const char *GpuDevID,
                                     const char *FileName,
                                     void *paCrcSettings,
                                     bool CreateNewFile,
                                     bool PrintAllCrcs)
{
    RC rc;
    FILE *pFp;

    UINT32 CrcAddr;
    UINT32 DisplayClass;

    EvoCRCSettings *pCrcSettings= (EvoCRCSettings *)paCrcSettings;

    Display *pDisplay = pCrcSettings->pCrcBuffer->GetGpuDev()->GetDisplay();
    MASSERT(pDisplay);
    DisplayClass = pDisplay->GetClass();

    UINT32* pCrcBuffer = (pCrcSettings->pCrcBuffer != NULL &&
                pCrcSettings->pCrcBuffer->GetMemHandle() != 0) ?
               (UINT32 *)pCrcSettings->pCrcBuffer->GetAddress() : NULL;

    if(pCrcBuffer == NULL)
        return RC::SOFTWARE_ERROR;

    // If CreateNewFile = TRUE OR if file doesnt exist then create New File with header
    // else append current <NOTIFIER> to the file
    if (CreateNewFile || !CrcComparison::FileExists(FileName))
    {
        CHECK_RC(AddHeaderToCrcXml(GpuDevID, FileName));
        pFp = fopen(FileName, "r+");
        rc = fseek(pFp, (UINT32)0,SEEK_END);
    }
    else
    {
        pFp = fopen(FileName, "r+");
        rc = fseek(pFp, (UINT32)(strlen("</TAG>")-(2*strlen("</TAG>"))-2) ,SEEK_END);
    }

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "File operation : fseek FAIL, so not able to append new data to file\n");
        return rc;
    }

    // Update the file with CRC Notifier's Global Data
    UINT32 Status;
    UINT32 CrcCount;
    bool OverRun;
    bool DsiOverflow;
    bool CompOverflow;
    bool PriOverflow;
    bool SecOverflow;
    bool epc;
    bool WidePipeCrc = false;

    fprintf(pFp, "<NOTIFIER>\n");

    // Controling Channel
    if (pCrcSettings->ControlChannel == EvoCRCSettings::CORE)
    {
        fprintf(pFp, "\t<CONTROLLINGCHANNEL>CORE</CONTROLLINGCHANNEL>\n");
    }
    else if (pCrcSettings->ControlChannel == EvoCRCSettings::BASE)
    {
        fprintf(pFp, "\t<CONTROLLINGCHANNEL>BASE</CONTROLLINGCHANNEL>\n");
    }
    else if (pCrcSettings->ControlChannel == EvoCRCSettings::OVERLAY)
    {
        fprintf(pFp, "\t<CONTROLLINGCHANNEL>OVERLAY</CONTROLLINGCHANNEL>\n");
    }

    (pCrcSettings->CrcPrimaryOutput == EvoCRCSettings::CRC_RG) ?
            fprintf(pFp, "\t<PRIMARY_CRC_OUTPUT>CRC_RG</PRIMARY_CRC_OUTPUT>\n") :
            fprintf(pFp, "\t<PRIMARY_CRC_OUTPUT>CRC_OR_SF</PRIMARY_CRC_OUTPUT>\n");

    (pCrcSettings->CrcSecondaryOutput == EvoCRCSettings::CRC_RG) ?
            fprintf(pFp, "\t<SECONDARY_CRC_OUTPUT>CRC_RG</SECONDARY_CRC_OUTPUT>\n") :
            fprintf(pFp, "\t<SECONDARY_CRC_OUTPUT>CRC_OR_SF</SECONDARY_CRC_OUTPUT>\n");

    Status              = MEM_RD32(pCrcBuffer);
    CrcCount            = REF_VAL(LW507D_NOTIFIER_CRC_1_STATUS_0_COUNT, Status);
    OverRun             = REF_VAL(LW507D_NOTIFIER_CRC_1_STATUS_0_OVERRUN, Status);
    DsiOverflow         = REF_VAL(LW507D_NOTIFIER_CRC_1_STATUS_0_DSI_OVERFLOW, Status);
    CompOverflow        = REF_VAL(LW507D_NOTIFIER_CRC_1_STATUS_0_COMPOSITOR_OVERFLOW, Status);
    PriOverflow         = REF_VAL(LW507D_NOTIFIER_CRC_1_STATUS_0_PRIMARY_OUTPUT_OVERFLOW, Status);
    SecOverflow         = REF_VAL(LW507D_NOTIFIER_CRC_1_STATUS_0_SECONDARY_OUTPUT_OVERFLOW, Status);
    epc                 = REF_VAL(LW507D_NOTIFIER_CRC_1_STATUS_0_EXPECT_BUFFER_COLLAPSE, Status);

    if(DisplayClass == LW9070_DISPLAY ||
       DisplayClass == LW9170_DISPLAY ||
       DisplayClass == LW9270_DISPLAY ||
       DisplayClass == LW9470_DISPLAY ||
       DisplayClass == LW9570_DISPLAY ||
       DisplayClass == LW9770_DISPLAY ||
       DisplayClass == LW9870_DISPLAY)
    {
        WidePipeCrc = REF_VAL(LW907D_NOTIFIER_CRC_1_STATUS_0_WIDE_PIPE_CRC, Status);
    }

    fprintf(pFp, "\t<COUNT>%d</COUNT>\n", CrcCount);
    fprintf(pFp, "\t<OVERRUN>%d</OVERRUN>\n", OverRun);
    fprintf(pFp, "\t<DSI_OVERFLOW>%d</DSI_OVERFLOW>\n", DsiOverflow);
    fprintf(pFp, "\t<COMPOSITOR_OVERFLOW>%d</COMPOSITOR_OVERFLOW>\n", CompOverflow);
    fprintf(pFp, "\t<PRIMARY_OVERFLOW>%d</PRIMARY_OVERFLOW>\n", PriOverflow);
    fprintf(pFp, "\t<SECONDARY_OVERFLOW>%d</SECONDARY_OVERFLOW>\n", SecOverflow);
    fprintf(pFp, "\t<EXPECTED_BUFFER_COLLAPSE>%d</EXPECTED_BUFFER_COLLAPSE>\n", epc);

    if(DisplayClass == LW9070_DISPLAY ||
       DisplayClass == LW9170_DISPLAY ||
       DisplayClass == LW9270_DISPLAY ||
       DisplayClass == LW9470_DISPLAY ||
       DisplayClass == LW9570_DISPLAY ||
       DisplayClass == LW9770_DISPLAY ||
       DisplayClass == LW9870_DISPLAY)
    {
        fprintf(pFp, "\t<WIDE_PIPE_CRC>%d</WIDE_PIPE_CRC>\n", WidePipeCrc);
    }

    CrcAddr =  LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_2;

    UINT32 NumExpectedCRCs = (PrintAllCrcs)?
                              CrcCount : pCrcSettings->NumExpectedCRCs;
    // Dump CRC data for current Notifier
    for (UINT32 i = 0; i < NumExpectedCRCs; i++)
    {

        UINT32 CrcEntry;
        UINT32 Time;
        UINT32 Tag;
        UINT32 TimeStampMode;
        UINT32 HwFlipLockMode;
        UINT32 PresentIntervalMet;
        UINT32 CompCrc;
        UINT32 PriCrc;
        UINT32 SecCrc;

        fprintf(pFp, "\t<CRC id=\"%d\">\n", i+1);
        CrcEntry             = MEM_RD32(pCrcBuffer + CrcAddr);
        Time                 = REF_VAL(LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_2_AUDIT_TIMESTAMP, CrcEntry);
        Tag                  = REF_VAL(LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_2_TAG, CrcEntry);
        TimeStampMode        = REF_VAL(LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_2_TIMESTAMP_MODE, CrcEntry);
        HwFlipLockMode       = REF_VAL(LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_2_HW_FLIP_LOCK_MODE, CrcEntry);
        PresentIntervalMet   = REF_VAL(LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_2_PRESENT_INTERVAL_MET, CrcEntry);

        CompCrc              = MEM_RD32(pCrcBuffer + CrcAddr +
                                        (LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_3 - LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_2));
        PriCrc               = MEM_RD32(pCrcBuffer + CrcAddr +
                                        (LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_4 - LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_2));
        SecCrc               = MEM_RD32(pCrcBuffer + CrcAddr +
                                        (LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_5 - LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_2));

        fprintf(pFp, "\t\t<TIME>%d</TIME>\n", Time);
        fprintf(pFp, "\t\t<TAG>%d</TAG>\n", Tag);
        fprintf(pFp, "\t\t<TIMESTAMP_MODE>%d</TIMESTAMP_MODE>\n", TimeStampMode);
        fprintf(pFp, "\t\t<HW_FLIPLOCK_MODE>%d</HW_FLIPLOCK_MODE>\n", HwFlipLockMode);
        fprintf(pFp, "\t\t<PRESENT_INTERVAL_MET>%d</PRESENT_INTERVAL_MET>\n", PresentIntervalMet);
        fprintf(pFp, "\t\t<COMPOSITOR_CRC>%u</COMPOSITOR_CRC>\n", CompCrc);
        fprintf(pFp, "\t\t<PRIMARY_CRC>%u</PRIMARY_CRC>\n", PriCrc);
        fprintf(pFp, "\t\t<SECONDARY_CRC>%u</SECONDARY_CRC>\n", SecCrc);
        fprintf(pFp, "\t</CRC>\n");

        CrcAddr += (LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_5 - LW507D_NOTIFIER_CRC_1_CRC_ENTRY0_2)  + 1;
    }
    fprintf(pFp, "</NOTIFIER>\n");
    fprintf(pFp, "</TAG>\n");

    fclose(pFp);

    return rc;
}

//!
//! Crc03XNotifier2Xml : Function which actually Dump DISP03X CRC notifier to XML file
//! Arguments :
//! GpuDevID : GpuID of Lwrrently Bound Device
//! FileName : FileName where user of the API wants to Dump the data. If File
//!            already exists it will just append the new Notifier's data else
//!            create the file
//! pCrcSettings : LwDispCRCSettings variable that stores Crc buffer and notifier details
//! CreateNewFile : TRUE = Create A New Crc XML File
//!                 FALSE = Append to existing XML file
//! Return RC::OK if success
//! \sa DumpCrcInFile
//! \sa CompareCrcFiles
//!
//------------------------------------------------------------------------------
RC CrcComparison::Crc03XNotifier2Xml(const char *GpuDevID,
                                     const char *FileName,
                                     void *paCrcSettings,
                                     bool CreateNewFile,
                                     bool PrintAllCrcs)
{
    RC rc;
    FILE *pFp;

    LwDispCRCSettings *pCrcSettings= (LwDispCRCSettings *)paCrcSettings;

    UINT32* pCrcBuffer = (pCrcSettings->pCrcBuffer != NULL &&
                pCrcSettings->pCrcBuffer->GetMemHandle() != 0) ?
               (UINT32 *)pCrcSettings->pCrcBuffer->GetAddress() : NULL;

    if(pCrcBuffer == NULL)
        return RC::SOFTWARE_ERROR;

    // If CreateNewFile = TRUE OR if file doesnt exist then create New File with header
    // else append current <NOTIFIER> to the file
    if (CreateNewFile || !CrcComparison::FileExists(FileName))
    {
        CHECK_RC(AddHeaderToCrcXml(GpuDevID, FileName));
        pFp = fopen(FileName, "r+");
        rc = fseek(pFp, (UINT32)0,SEEK_END);
    }
    else
    {
        pFp = fopen(FileName, "r+");
        rc = fseek(pFp, (UINT32)(strlen("</TAG>")-(2*strlen("</TAG>"))-2) ,SEEK_END);
    }

    if (rc != OK)
    {
        Printf(Tee::PriHigh, "File operation : fseek FAIL, so not able to append new data to file\n");
        return rc;
    }

    //// Update the file with CRC Notifier's Global Data

    fprintf(pFp, "<NOTIFIER>\n");

    enum CRC_OUTPUT_TYPE
    {
        CRC_OUTPUT_TYPE_PRIMARY = 0,
        CRC_OUTPUT_TYPE_SECONDARY = 1,
        CRC_OUTPUT_TYPE_NONE = 2
    };

    for (UINT32 i= CRC_OUTPUT_TYPE_PRIMARY; i < CRC_OUTPUT_TYPE_NONE; i++)
    {
        LwDispCRCSettings::CRC_OUTPUT crcOutputType = (i == CRC_OUTPUT_TYPE_PRIMARY) ? pCrcSettings->CrcPrimaryOutput :
                                                        pCrcSettings->CrcSecondaryOutput;
        string crcOutput = "";
        switch(crcOutputType)
        {
            case LwDispCRCSettings::CRC_SF: crcOutput += "CRC_SF"; break;
            case LwDispCRCSettings::CRC_SOR0: crcOutput += "CRC_SOR0"; break;
            case LwDispCRCSettings::CRC_SOR1: crcOutput += "CRC_SOR1"; break;
            case LwDispCRCSettings::CRC_SOR2: crcOutput += "CRC_SOR2"; break;
            case LwDispCRCSettings::CRC_SOR3: crcOutput += "CRC_SOR3"; break;
            case LwDispCRCSettings::CRC_SOR4: crcOutput += "CRC_SOR4"; break;
            case LwDispCRCSettings::CRC_SOR5: crcOutput += "CRC_SOR5"; break;
            case LwDispCRCSettings::CRC_SOR6: crcOutput += "CRC_SOR6"; break;
            case LwDispCRCSettings::CRC_SOR7: crcOutput += "CRC_SOR7"; break;
            case LwDispCRCSettings::CRC_PIOR0: crcOutput += "CRC_PIOR0"; break;
            case LwDispCRCSettings::CRC_PIOR1: crcOutput += "CRC_PIOR1"; break;
            case LwDispCRCSettings::CRC_PIOR2: crcOutput += "CRC_PIOR2"; break;
            case LwDispCRCSettings::CRC_PIOR3: crcOutput += "CRC_PIOR3"; break;
            case LwDispCRCSettings::CRC_WBOR0: crcOutput += "CRC_WBOR0"; break;
            case LwDispCRCSettings::CRC_WBOR1: crcOutput += "CRC_WBOR1"; break;
            case LwDispCRCSettings::CRC_WBOR2: crcOutput += "CRC_WBOR2"; break;
            case LwDispCRCSettings::CRC_WBOR3: crcOutput += "CRC_WBOR3"; break;
            case LwDispCRCSettings::CRC_WBOR4: crcOutput += "CRC_WBOR4"; break;
            case LwDispCRCSettings::CRC_WBOR5: crcOutput += "CRC_WBOR5"; break;
            case LwDispCRCSettings::CRC_WBOR6: crcOutput += "CRC_WBOR6"; break;
            case LwDispCRCSettings::CRC_WBOR7: crcOutput += "CRC_WBOR7"; break;
            case LwDispCRCSettings::CRC_DSI0: crcOutput += "CRC_DSI0"; break;
            case LwDispCRCSettings::CRC_DSI1: crcOutput += "CRC_DSI1"; break;
            case LwDispCRCSettings::CRC_DSI2: crcOutput += "CRC_DSI2"; break;
            case LwDispCRCSettings::CRC_DSI3: crcOutput += "CRC_DSI3"; break;
            default:
                crcOutput += "NONE";
        }

        string strCrcOutput = "";
        if (i == CRC_OUTPUT_TYPE_PRIMARY)
        {
            strCrcOutput += "\t<PRIMARY_CRC_OUTPUT>";
            strCrcOutput +=  crcOutput;
            strCrcOutput +="</PRIMARY_CRC_OUTPUT>\n";
            fprintf(pFp, strCrcOutput.c_str());
        }
        else if (i == CRC_OUTPUT_TYPE_SECONDARY)
        {
            strCrcOutput += "\t<SECONDARY_CRC_OUTPUT>";
            strCrcOutput +=  crcOutput;
            strCrcOutput +="</SECONDARY_CRC_OUTPUT>\n";
            fprintf(pFp, strCrcOutput.c_str());
        }
    }

    UINT32 Status           = MEM_RD32(pCrcBuffer);
    UINT32 CrcCount         = REF_VAL(LWC37D_NOTIFIER_CRC_STATUS_0_COUNT, Status);
    bool OverRun            = REF_VAL(LWC37D_NOTIFIER_CRC_STATUS_0_OVERRUN, Status);
    bool feOverflow         = REF_VAL(LWC37D_NOTIFIER_CRC_STATUS_0_FE_OVERFLOW, Status);
    bool CompOverflow       = REF_VAL(LWC37D_NOTIFIER_CRC_STATUS_0_COMPOSITOR_OVERFLOW, Status);
    bool PriOverflow        = REF_VAL(LWC37D_NOTIFIER_CRC_STATUS_0_PRIMARY_OUTPUT_OVERFLOW, Status);
    bool SecOverflow        = REF_VAL(LWC37D_NOTIFIER_CRC_STATUS_0_SECONDARY_OUTPUT_OVERFLOW, Status);
    bool RgOverflow         = REF_VAL(LWC37D_NOTIFIER_CRC_STATUS_0_RG_OVERFLOW, Status);
    bool epc                = REF_VAL(LWC37D_NOTIFIER_CRC_STATUS_0_EXPECT_BUFFER_COLLAPSE, Status);
    bool colorDepthAgnostic =  REF_VAL(LWC37D_NOTIFIER_CRC_STATUS_0_COLOR_DEPTH_AGNOSTIC, Status);

    fprintf(pFp, "\t<COUNT>%d</COUNT>\n", CrcCount);
    fprintf(pFp, "\t<OVERRUN>%d</OVERRUN>\n", OverRun);
    fprintf(pFp, "\t<FE_OVERFLOW>%d</FE_OVERFLOW>\n", feOverflow);
    fprintf(pFp, "\t<COMPOSITOR_OVERFLOW>%d</COMPOSITOR_OVERFLOW>\n", CompOverflow);
    fprintf(pFp, "\t<RG_OVERFLOW>%d</RG_OVERFLOW>\n", RgOverflow);
    fprintf(pFp, "\t<PRIMARY_OVERFLOW>%d</PRIMARY_OVERFLOW>\n", PriOverflow);
    fprintf(pFp, "\t<SECONDARY_OVERFLOW>%d</SECONDARY_OVERFLOW>\n", SecOverflow);
    fprintf(pFp, "\t<EXPECTED_BUFFER_COLLAPSE>%d</EXPECTED_BUFFER_COLLAPSE>\n", epc);
    fprintf(pFp, "\t<COLOR_DEPTH_AGNOSTIC>%d</COLOR_DEPTH_AGNOSTIC>\n", colorDepthAgnostic);

    UINT32 crcCount = DRF_VAL(C37D, _NOTIFIER_CRC, _STATUS_0_COUNT, MEM_RD32(pCrcBuffer));
    UINT32 crcAddr =  LWC37D_NOTIFIER_CRC_ENTRY_BASE;

    UINT32 NumExpectedCRCs = (PrintAllCrcs)?
                              crcCount : pCrcSettings->NumExpectedCRCs;
    // Dump CRC data for current Notifier
    for (UINT32 i = 0; i < NumExpectedCRCs; i++)
    {

        fprintf(pFp, "\t<CRC id=\"%d\">\n", i+1);

        UINT32 crcSubEntry0             = MEM_RD32(pCrcBuffer + crcAddr);
        UINT32 Tag                  = REF_VAL(LWC37D_NOTIFIER_CRC_ENTRY_SUB_ENTRY_0_TAG, crcSubEntry0);
        UINT32 PresentIntervalMet   = REF_VAL(LWC37D_NOTIFIER_CRC_ENTRY_SUB_ENTRY_0_PRESENT_INTERVAL_MET, crcSubEntry0);

        UINT32 CrcSubEntry1         = MEM_RD32(pCrcBuffer + crcAddr + LWC37D_NOTIFIER_CRC_ENTRY_SUB_ENTRY_1(0));
        UINT32 TimeStampMode        = REF_VAL(LWC37D_NOTIFIER_CRC_ENTRY_SUB_ENTRY_1_TIMESTAMP_MODE, CrcSubEntry1);
        UINT32 Stereo               = REF_VAL(LWC37D_NOTIFIER_CRC_ENTRY_SUB_ENTRY_1_STEREO_MODE, CrcSubEntry1);
        UINT32 Interlaced           = REF_VAL(LWC37D_NOTIFIER_CRC_ENTRY_SUB_ENTRY_1_INTERLACED_MODE, CrcSubEntry1);
        UINT32 Dither               = REF_VAL(LWC37D_NOTIFIER_CRC_ENTRY_SUB_ENTRY_1_DITHER, CrcSubEntry1);
        UINT32 Sli                  = REF_VAL(LWC37D_NOTIFIER_CRC_ENTRY_SUB_ENTRY_1_SLI, CrcSubEntry1);

        UINT32 CompCrc = MEM_RD32(pCrcBuffer + crcAddr + LWC37D_NOTIFIER_CRC_ENTRY_SUB_ENTRY_3(0) - LWC37D_NOTIFIER_CRC_ENTRY_BASE);
        UINT32 RGCrc   = MEM_RD32(pCrcBuffer + crcAddr + LWC37D_NOTIFIER_CRC_ENTRY_SUB_ENTRY_4(0) - LWC37D_NOTIFIER_CRC_ENTRY_BASE);
        UINT32 PriCrc  = MEM_RD32(pCrcBuffer + crcAddr + LWC37D_NOTIFIER_CRC_ENTRY_SUB_ENTRY_5(0) - LWC37D_NOTIFIER_CRC_ENTRY_BASE);
        UINT32 SecCrc  = MEM_RD32(pCrcBuffer + crcAddr + LWC37D_NOTIFIER_CRC_ENTRY_SUB_ENTRY_6(0) - LWC37D_NOTIFIER_CRC_ENTRY_BASE);

        fprintf(pFp, "\t\t<TAG>%d</TAG>\n", Tag);
        fprintf(pFp, "\t\t<TIMESTAMP_MODE>%d</TIMESTAMP_MODE>\n", TimeStampMode);
        fprintf(pFp, "\t\t<STEREO>%d</STEREO>\n", Stereo);
        fprintf(pFp, "\t\t<INTERLACED>%d</INTERLACED>\n", Interlaced);
        fprintf(pFp, "\t\t<DITHER>%d</DITHER>\n", Dither);
        fprintf(pFp, "\t\t<SLI>%d</SLI>\n", Sli);
        fprintf(pFp, "\t\t<PRESENT_INTERVAL_MET>%d</PRESENT_INTERVAL_MET>\n", PresentIntervalMet);
        fprintf(pFp, "\t\t<COMPOSITOR_CRC>%u</COMPOSITOR_CRC>\n", CompCrc);
        fprintf(pFp, "\t\t<RG_CRC>%u</RG_CRC>\n", RGCrc);
        fprintf(pFp, "\t\t<PRIMARY_CRC>%u</PRIMARY_CRC>\n", PriCrc);
        fprintf(pFp, "\t\t<SECONDARY_CRC>%u</SECONDARY_CRC>\n", SecCrc);
        fprintf(pFp, "\t</CRC>\n");

        crcAddr += (LWC37D_NOTIFIER_CRC_ENTRY_SUB_ENTRY_7(0) - LWC37D_NOTIFIER_CRC_ENTRY_BASE + 1);
    }

    fprintf(pFp, "</NOTIFIER>\n");
    fprintf(pFp, "</TAG>\n");

    fclose(pFp);

    return rc;
}

//!
//! AddHeaderToCrcXml : Function To add Header in CRC XML file
//! \sa DumpCrcInFile
//! \sa CompareCrcFiles
//-----------------------------------------------------------------------------
RC CrcComparison::AddHeaderToCrcXml(const char *GpuDevID, const char *FileName)
{
    RC rc;
    FILE *pFp;

    pFp = fopen(FileName, "w+");
    if (pFp == NULL)
    {
        Printf(Tee::PriHigh,"AddHeaderToCrcXml : Failed to open File %s w+ mode\n", FileName);
        return RC::CANNOT_OPEN_FILE;
    }

    // Set XML Version
    fprintf(pFp, "<?xml version=\"1.0\"?>\n");

    // Starting Tag with attribute as Filename
    fprintf(pFp, "<TAG FileName = \"%s\">\n", FileName);

    // Actual Header, which contain
    // 1 : FILETYPE : for crc, it should be CRC
    // 2 : PLATFORM : where CRC captured
    // 3 : CHIP     : Gpu on which CRC got captured
    fprintf(pFp, "<HEADER>\n");
    fprintf(pFp, "\t<FILETYPE>CRC</FILETYPE>\n");
    fprintf(pFp, "\t<PLATFORM>");
    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        fprintf(pFp, "HW");
    }
    else if (Platform::GetSimulationMode() == Platform::RTL)
    {
        fprintf(pFp, "RTL");
    }
    else
    {
        fprintf(pFp, "FMODEL");
    }
    fprintf(pFp, "</PLATFORM>\n");
    fprintf(pFp, "\t<CHIP>");
    fprintf(pFp, "%s",GpuDevID);
    fprintf(pFp, "</CHIP>\n");
    // Just a message to warn not to edit this file manually
    fprintf(pFp, "\t<MESSAGE>");
    fprintf(pFp, "Do Not Edit, This is Auto Generated File");
    fprintf(pFp, "</MESSAGE>\n");
    fprintf(pFp, "</HEADER>\n");

    fclose(pFp);

    return rc;
}

// Upper level call to take diff of two crc file
//------------------------------------------------------------------------------
RC CrcComparison::CompareCrcFiles(const char *FileName1,
                                  const char *FileName2,
                                  const char *OutputFile,
                                  VerifyCrcTree *pCrcFlag)
{
    RC rc;
    XmlWrapper XmlWrapper1;
    XmlWrapper XmlWrapper2;
    XmlNode *pFileRoot1;
    XmlNode *pFileRoot2;

    // Create XML tree of required files
    CHECK_RC(XmlWrapper1.ParseXmlFileToTree(FileName1, &pFileRoot1));
    CHECK_RC(XmlWrapper2.ParseXmlFileToTree(FileName2, &pFileRoot2));
    // Compare the File trees
    CHECK_RC(DiffCrcXmlTree(pFileRoot1, pFileRoot2, OutputFile, pCrcFlag));

    return rc;
}

//!
//! Actuall call to take diff of two XML tree
//! This Call lwrrently work for comapring the CRC on all the platform for
//! progressive as well as interlace raster with a exception when comparing CRC
//! while interlace raster between FMODEL and RTL platform, planning to have it
//! in next version.
//!
//------------------------------------------------------------------------------
RC CrcComparison::DiffCrcXmlTree(XmlNode *pRootNode1,
                                 XmlNode *pRootNode2,
                                 const char *OutputFile,
                                 VerifyCrcTree *pCrcFlag)
{
    RC rc;
    FILE *pFp = NULL;
    string FileName1;
    string FileName2;
    char buffer[200];
    UINT32 IterationCount = 1;
    XmlAttributeMap::iterator map_iter1;
    XmlAttributeMap::iterator map_iter2;
    UINT32 NotifierCount = 0;
    bool bCrcFlagSpecified = true;

    // Iterating child node of starting <TAG>
    XmlNodeList::iterator child_iter1;
    XmlNodeList::iterator child_iter2;

    // Open Output file to capture diff which occur while CRC file comparison, if any
    if (OutputFile != NULL)
    {
        pFp = fopen(OutputFile, "w+");

        if (pFp == NULL)
        {
            Printf(Tee::PriHigh,"DiffCrcXmlTree::Failed to Open Outputfile %s\n", OutputFile);
            return RC::SOFTWARE_ERROR;
        }
    }
    // Check if File has valid starting point
    if ((pRootNode1->Name() != "TAG") || (pRootNode2->Name() != "TAG"))
    {
        sprintf(buffer, "TAG not found \n");
        Printf(Tee::PriHigh, "%s", buffer);
        DUMP2FILE(pFp, buffer);
        CHECK_RC_CLEANUP(RC::BAD_FORMAT_SPECIFICAION);
    }

    // Store the file name , which help later while reporting error
    map_iter1 = pRootNode1->AttributeMap()->find("FileName");
    map_iter2 = pRootNode2->AttributeMap()->find("FileName");
    if ((map_iter1 == pRootNode1->AttributeMap()->end()) ||
        (map_iter2 == pRootNode2->AttributeMap()->end()))
    {
        sprintf(buffer, " FILE FORMAT not valid : FileName missing\n");
        Printf(Tee::PriHigh, "%s", buffer);
        DUMP2FILE(pFp, buffer);
        CHECK_RC_CLEANUP(RC::BAD_FORMAT_SPECIFICAION);
    }

    FileName1 = ((*map_iter1).second);
    FileName2 = ((*map_iter2).second);

    // Check if file have any field
    if (pRootNode1->Children()->begin() == pRootNode1->Children()->end())
    {
        sprintf(buffer, "%s have no CRC Notifier \n", FileName1.c_str());
        Printf(Tee::PriHigh, "%s", buffer);
        DUMP2FILE(pFp, buffer);
        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
    }
    if (pRootNode2->Children()->begin() == pRootNode2->Children()->end())
    {
        sprintf(buffer, "%s have no CRC Notifier \n", FileName2.c_str());
        Printf(Tee::PriHigh, "%s", buffer);
        DUMP2FILE(pFp, buffer);
        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
    }

    if (pCrcFlag == NULL)
    {
        pCrcFlag = new VerifyCrcTree();
        bCrcFlagSpecified = false;
    }

    for (child_iter1 = pRootNode1->Children()->begin(),
         child_iter2 = pRootNode2->Children()->begin();
         ((child_iter1 != pRootNode1->Children()->end())&&
          (child_iter2 != pRootNode2->Children()->end()));
         ++child_iter1, ++child_iter2, ++IterationCount)
    {
        XmlNode *pChildNode1 = *child_iter1;
        XmlNode *pChildNode2 = *child_iter2;

        MASSERT(pChildNode1->Name() == pChildNode2->Name());

        // Verify Header of File
        if (IterationCount == 1)
        {
            // Checking just one file entry as we already ensure that both files should have
            // same fields through MASSERT
            if (pChildNode1->Name() != "HEADER")
            {
                sprintf(buffer, "%s HEADER is missing \n", FileName1.c_str());
                Printf(Tee::PriHigh, "%s", buffer);
                DUMP2FILE(pFp, buffer);
                CHECK_RC_CLEANUP(RC::BAD_FORMAT_SPECIFICAION);
            }

            XmlNodeList::iterator header_iter1;
            XmlNodeList::iterator header_iter2;

            MASSERT(pChildNode1->Children()->size() == pChildNode2->Children()->size());
            UINT32 HeaderFields = 0;
            for (header_iter1 = pChildNode1->Children()->begin(),
                 header_iter2 = pChildNode2->Children()->begin();
                 HeaderFields < pChildNode1->Children()->size();
                 ++header_iter1, ++header_iter2, ++HeaderFields)
            {
                XmlNode *pHeaderChild1 = *header_iter1;
                XmlNode *pHeaderChild2 = *header_iter2;

                MASSERT (pHeaderChild1->Name() == pHeaderChild2->Name());

                if (((pHeaderChild1->Name() == "CHIP") && (pCrcFlag->crcHeaderField.bSkipChip)) ||
                    ((pHeaderChild1->Name() == "PLATFORM") && (pCrcFlag->crcHeaderField.bSkipPlatform)))
                {
                    continue;
                }

                if ((pHeaderChild1->Name() == "FILETYPE") ||
                    (pHeaderChild1->Name() == "CHIP"))
                {
                     if (pHeaderChild1->Body() != pHeaderChild2->Body())
                     {
                         sprintf(buffer, "%s of file HEADER Mismatches \n", pHeaderChild1->Name().c_str());
                         Printf(Tee::PriHigh, "%s", buffer);
                         DUMP2FILE(pFp, buffer);
                         CHECK_RC_CLEANUP(RC::BAD_FORMAT_SPECIFICAION);
                     }
                }
                else
                {
                    if (pHeaderChild1->Body() != pHeaderChild2->Body())
                    {
                        sprintf(buffer, "%s of file Mismatches \n", pHeaderChild1->Name().c_str());
                        Printf(Tee::PriHigh,"%s", buffer);
                        DUMP2FILE(pFp, buffer);
                    }
                }
            }
        }
        // Verify and compare Notifier Field of Files
        else
        {
            if (pChildNode1->Name() != "NOTIFIER")
            {
                sprintf(buffer, "UNIDENTIFIED Tag %s\n", pChildNode1->Name().c_str());
                Printf(Tee::PriHigh, "%s", buffer);
                DUMP2FILE(pFp, buffer);
                CHECK_RC_CLEANUP(RC::BAD_FORMAT_SPECIFICAION);
            }

            ++NotifierCount;

            XmlNodeList::iterator notifier_iter1;
            XmlNodeList::iterator notifier_iter2;

            notifier_iter1 = pChildNode1->Children()->begin();
            notifier_iter2 = pChildNode2->Children()->begin();

            while ((notifier_iter1 != pChildNode1->Children()->end()) &&
                   (notifier_iter2 != pChildNode2->Children()->end()))
            {
                XmlNode *pNotifierChild1 = *notifier_iter1;
                XmlNode *pNotifierChild2 = *notifier_iter2;
                UINT32 IncNotifierChild1 = 0;
                UINT32 IncNotifierChild2 = 0;

                MASSERT(pNotifierChild1->Name() == pNotifierChild2->Name());

                if (((pNotifierChild1->Name() == "CONTROLLINGCHANNEL") && (pCrcFlag->crcNotifierField.bSkipControllingChannel)) ||
                    ((pNotifierChild1->Name() == "PRIMARY_CRC_OUTPUT") && (pCrcFlag->crcNotifierField.bSkipPrimaryCrcOutput)) ||
                    ((pNotifierChild1->Name() == "SECONDARY_CRC_OUTPUT") && (pCrcFlag->crcNotifierField.bSkipSecondaryCrcOutput)) ||
                    ((pNotifierChild1->Name() == "EXPECTED_BUFFER_COLLAPSE") && (pCrcFlag->crcNotifierField.bSkipExpectedBufferCollapse)) ||
                    ((pNotifierChild1->Name() == "WIDE_PIPE_CRC") && (pCrcFlag->crcNotifierField.bSkipWidePipeCrc)))
                {
                    notifier_iter1++;
                    notifier_iter2++;
                    continue;
                }

                // Check the CRC Fields
                if (pNotifierChild1->Name() == "CRC")
                {

                    XmlNodeList::iterator crc_iter1;
                    XmlNodeList::iterator crc_iter2;
                    XmlAttributeMap::iterator crc_attrib1;
                    XmlAttributeMap::iterator crc_attrib2;

                    crc_attrib1 = (pNotifierChild1->AttributeMap())->find("id");
                    crc_attrib2 = (pNotifierChild2->AttributeMap())->find("id");

                    if ((crc_attrib1 == pNotifierChild1->AttributeMap()->end()) ||
                        (crc_attrib2 == pNotifierChild2->AttributeMap()->end()))
                    {
                        sprintf(buffer, " CRC TAG FORMAT not valid : CRC's id missing\n");
                        Printf(Tee::PriHigh, "%s", buffer);
                        DUMP2FILE(pFp, buffer);
                        CHECK_RC_CLEANUP(RC::BAD_FORMAT_SPECIFICAION);
                    }

                    string CrcCount1 = ((*crc_attrib1).second);
                    string CrcCount2 = ((*crc_attrib2).second);

                    MASSERT(pNotifierChild1->Children()->size() == pNotifierChild2->Children()->size());
                    bool bCrcOptional1;
                    bool bCrcOptional2;

                    CHECK_RC(IsCrcOptional(pNotifierChild1, &bCrcOptional1, &(pCrcFlag->crcNotifierField.crcField)));
                    CHECK_RC(IsCrcOptional(pNotifierChild2, &bCrcOptional2, &(pCrcFlag->crcNotifierField.crcField)));

                    IncNotifierChild1 = bCrcOptional1 ? 1 : 0;
                    IncNotifierChild2 = bCrcOptional2 ? 1 : 0;

                    if (bCrcOptional1 || bCrcOptional2)
                    {
                        notifier_iter1 += IncNotifierChild1;
                        notifier_iter2 += IncNotifierChild2;
                        continue;
                    }

                    UINT32 CrcFields = 0;
                    for (crc_iter1 = pNotifierChild1->Children()->begin(),
                         crc_iter2 = pNotifierChild2->Children()->begin();
                         CrcFields < pNotifierChild1->Children()->size();
                         ++crc_iter1, ++crc_iter2, ++CrcFields)
                    {
                        XmlNode *pCrcChild1 = *crc_iter1;
                        XmlNode *pCrcChild2 = *crc_iter2;

                        MASSERT(pCrcChild1->Name() == pCrcChild2->Name());

                        if (((pCrcChild1->Name() == "TIMESTAMP_MODE") && (pCrcFlag->crcNotifierField.crcField.bSkipTimestampMode)) ||
                            ((pCrcChild1->Name() == "HW_FLIPLOCK_MODE") && (pCrcFlag->crcNotifierField.crcField.bSkipHwFliplockMode)) ||
                            ((pCrcChild1->Name() == "COMPOSITOR_CRC") && (pCrcFlag->crcNotifierField.crcField.bSkipCompositorCRC)) ||
                            ((pCrcChild1->Name() == "SECONDARY_CRC") && (pCrcFlag->crcNotifierField.bSkipSecondaryCrcOutput)) ||
                            (pCrcChild1->Name() == "TIME"))
                        {
                            continue;
                        }

                        if (pCrcChild1->Body() != pCrcChild2->Body())
                        {
                            if (pCrcChild1->Name() == "TAG" && pCrcFlag->crcNotifierField.bIgnoreTagNumber)
                            {
                                continue;
                            }
                            sprintf(buffer, "Notifier %d : %s of CRC %s of File %s Mismatches with CRC %s of File %s \n",
                                            NotifierCount, pCrcChild1->Name().c_str(), CrcCount1.c_str(),
                                            FileName1.c_str(), CrcCount2.c_str(), FileName2.c_str());
                            Printf(Tee::PriHigh, "%s", buffer);
                            DUMP2FILE(pFp, buffer);
                            CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
                        }
                    }
                }
                //
                // Check other global info of CRC notifier.
                // If any of OVERRUN, DSI_OVERFLOW, COMPOSITOR_OVERFLOW, PRIMARY_OVERFLOW
                // SECONDARY_OVERFLOW or EXPECTED_BUFFER_COLLAPSE is non-zero, return error.
                //
                else
                {

                    if ((pNotifierChild1->Name() == "OVERRUN") ||
                        (pNotifierChild1->Name() == "DSI_OVERFLOW") ||
                        (pNotifierChild1->Name() == "COMPOSITOR_OVERFLOW") ||
                        (pNotifierChild1->Name() == "PRIMARY_OVERFLOW") ||
                        (pNotifierChild1->Name() == "SECONDARY_OVERFLOW") ||
                        (pNotifierChild1->Name() == "EXPECTED_BUFFER_COLLAPSE"))
                    {
                        string FileName;

                        if (pNotifierChild1->Body() == "1")
                        {
                            FileName = FileName1;
                        }
                        else if (pNotifierChild2->Body() == "1")
                        {
                            FileName = FileName2;
                        }
                        if ((pNotifierChild1->Body() == "1") || (pNotifierChild2->Body() == "1"))
                        {
                            // Return Error in case of any kind of overflow or overrun unless asked not to
                            if (!(pCrcFlag->crcNotifierField.bIgnorePrimaryOverflow &&
                                pNotifierChild1->Name() == "PRIMARY_OVERFLOW"))
                            {
                                sprintf(buffer, "Error :  %s = 1 of Notifier %d for CRC file %s \n",
                                       pNotifierChild1->Name().c_str(), NotifierCount, FileName.c_str());
                                Printf(Tee::PriHigh, "%s", buffer);
                                DUMP2FILE(pFp, buffer);
                                CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
                            }
                        }
                    }
                    else if ((pNotifierChild1->Name() == "CONTROLLINGCHANNEL") ||
                             (pNotifierChild1->Name() == "WIDE_PIPE_CRC") ||
                             (pNotifierChild1->Name() == "PRIMARY_CRC_OUTPUT") ||
                             (pNotifierChild1->Name() == "SECONDARY_CRC_OUTPUT")
                             )
                    {
                        if (pNotifierChild1->Body() != pNotifierChild2->Body())
                        {
                            sprintf(buffer, "%s Field Mismatches of Notifier %d\n",
                                                  pNotifierChild1->Name().c_str(), NotifierCount);
                            Printf(Tee::PriHigh, "%s", buffer);
                            DUMP2FILE(pFp, buffer);
                            CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
                        }
                    }
                    else if (pNotifierChild1->Body() != pNotifierChild2->Body())
                    {
                        sprintf(buffer, "%s field mismatches of Notifier %d\n",
                                pNotifierChild1->Name().c_str(), NotifierCount);
                        Printf(Tee::PriHigh, "%s", buffer);
                        DUMP2FILE(pFp, buffer);
                    }

                }
                notifier_iter1++;
                notifier_iter2++;
            }

            // check if this Notifier still have valid CRC field uncompared
            if (notifier_iter1 != pChildNode1->Children()->end() ||
                notifier_iter2 != pChildNode2->Children()->end())
            {
                string FileName = (notifier_iter1 != pChildNode1->Children()->end()) ? FileName1 : FileName2;
                XmlNodeList::iterator iterator = (notifier_iter1 != pChildNode1->Children()->end()) ?
                                                  notifier_iter1 : notifier_iter2;
                XmlNode *pNotifier = (notifier_iter1 != pChildNode1->Children()->end()) ? pChildNode1 : pChildNode2;

                XmlNode *pNotifierChild = *iterator;
                bool IsOptional;

                while (iterator != pNotifier->Children()->end())
                {
                    if (pNotifierChild->Name() != "CRC")
                    {
                        sprintf(buffer, "Error : Notifier %d of File %s having Invalid Tag \n",
                                                NotifierCount, FileName.c_str());
                        Printf(Tee::PriHigh, "%s", buffer);
                        DUMP2FILE(pFp, buffer);
                        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
                    }
                    CHECK_RC(IsCrcOptional(pNotifierChild, &IsOptional,&(pCrcFlag->crcNotifierField.crcField)));
                    if (!IsOptional)
                    {
                        sprintf(buffer, "Error : Notifier %d of File %s Still having Valid CRC to compare \n",
                                                NotifierCount, FileName.c_str());
                        Printf(Tee::PriHigh, "%s", buffer);
                        DUMP2FILE(pFp, buffer);
                        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);

                    }
                    iterator++;
                }
            }
        }
    }

    if ((child_iter1 == pRootNode1->Children()->end()) && (child_iter2 != pRootNode2->Children()->end()))
    {
        sprintf(buffer, "File %s contains less Notifier then File %s\n", FileName1.c_str(), FileName2.c_str());
        Printf(Tee::PriHigh, "%s", buffer);
        DUMP2FILE(pFp, buffer);
        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
    }
    else if ((child_iter1 != pRootNode1->Children()->end()) && (child_iter2 == pRootNode2->Children()->end()))
    {
        sprintf(buffer, "File %s contains less Notifier then File %s\n", FileName2.c_str(), FileName1.c_str());
        Printf(Tee::PriHigh, "%s", buffer);
        DUMP2FILE(pFp, buffer);
        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
    }

Cleanup:

    if (!bCrcFlagSpecified)
    {
        delete pCrcFlag;
    }

    if (pFp)
    {
        fclose(pFp);
        pFp = NULL;
    }

    return rc;
}

//!
//! \brief IsCrcOptional : Check if this CRC is optional through its
//!                        PRESENT_INTERVAL_MET field.
//!
//! crcIterator[in]  : CRC stucture Interator to traverse all member of structure
//! bIsOptional[out] : "true" if PRESENT_INTERVAL_MET = 1 and user has not said to skip this, else "flase"
//!
//! \return RC::DATA_MISMATCH if PRESENT_INTERVAL_MET not found else return OK.
//!

RC CrcComparison::IsCrcOptional(XmlNode *pCrcNode, bool *bIsOptional, VerifCrc *pCrcField )
{
    RC rc;

    XmlNodeList::iterator crc_iter;
    XmlNode *pMetPresentInterval = NULL;

    for(crc_iter = pCrcNode->Children()->begin();
        crc_iter != pCrcNode->Children()->end(); crc_iter++)
    {
        if ((*crc_iter)->Name() == "PRESENT_INTERVAL_MET")
        {
            pMetPresentInterval = *crc_iter;
            break;
        }
    }

    // Error checking
    if (pMetPresentInterval == NULL)
    {
        Printf(Tee::PriHigh," IsCrcOptional :: INVALID FIELD\n");
        return RC::DATA_MISMATCH;
    }

    // Has the user asked to explicitly compare
    if(pCrcField && pCrcField->bCompEvenIfPresentIntervalMet)
    {
        // As we need to ignore Present Interval met flag, this crc is not optional
        *bIsOptional = false ;
    }
    else
    {
        *bIsOptional = (pMetPresentInterval->Body() == "1")? true : false;
    }

    return rc;
}

