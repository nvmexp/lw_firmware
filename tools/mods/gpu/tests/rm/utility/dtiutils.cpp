/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  dtiutils.cpp
 * @brief Display Test Infrastructure support functions
 */

#include "dtiutils.h"
extern "C"
{
#include "modeset.h"
}

#ifndef __LWTIMING_H__
#include "lwtiming.h"
#endif
#ifndef INCLUDED_SURF2D_H
#include "gpu/utility/surf2d.h"
#endif
#ifndef INCLUDED_EDID_H
#include "core/include/modsedid.h"
#endif
#ifndef INCLUDED_MODESET_UTILS_H
#include "gpu/display/modeset_utils.h"
#endif
#ifndef INCLUDED_GPUDEV_H
#include "gpu/include/gpudev.h"
#endif
#ifndef INCLUDED_RAWCNSL_H
#include "core/include/rawcnsl.h"
#endif
#ifndef INCLUDED_CNSLMGR_H
#include "core/include/cnslmgr.h"
#endif
#include "ctrl/ctrl5070.h"  // for LW5070_CTRL_CMD_GET_RG_UNDERFLOW_PROP
#include "class/cl5070.h" // LW50_DISPLAY
#include "class/cl8270.h" // G82_DISPLAY
#include "class/cl8870.h" // G94_DISPLAY
#include "class/cl8370.h" // GT200_DISPLAY
#include "class/cl8570.h" // GT214_DISPLAY
#include "class/cl9070.h" // LW9070_DISPLAY
#include "class/cl9170.h" // LW9170_DISPLAY
#include "class/cl9270.h" // LW9270_DISPLAY
#include "class/cl9470.h" // LW9470_DISPLAY
#include "class/cl9570.h" // LW9570_DISPLAY
#include "class/cl9770.h" // LW9770_DISPLAY
#include "class/cl9870.h" // LW9870_DISPLAY
#include "class/clc570.h" // LWC570_DISPLAY
#include "class/clc370.h" // LWC370_DISPLAY
#include "class/clc670.h" // LWC670_DISPLAY
#include "class/clc770.h" // LWC770_DISPLAY
#include "class/clc870.h" // LWC870_DISPLAY

#include <cmath>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <time.h>
#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    #include "gpu/display/win32disp.h"
    #include "Windows.h"
    #include "lwapi.h"
#endif

#include "core/include/memcheck.h"

using namespace DTIUtils;

//! \brief manualVerification
//! verifies manually by prompting user whether the display looks ok
//------------------------------------------------------------------
RC DTIUtils::VerifUtils::manualVerification(Display* pDisplay, DisplayID SetmodeDisplay, Surface2D *Surf2Print, string PromptMessage, Display::Mode resolution, UINT32 timeoutMS, UINT32 sleepInterval, UINT32 subdeviceInstance)
{
    RC          rc;

    // For WBOR the manualVerification is simply asking the tester to go look at
    // the written out image of the wrbk surface after the automatic test
    LW0073_CTRL_SPECIFIC_OR_GET_INFO_PARAMS orGetInfoParams = {0};
    orGetInfoParams.displayId = SetmodeDisplay;
    CHECK_RC(pDisplay->RmControl(
                LW0073_CTRL_CMD_SPECIFIC_OR_GET_INFO,
                &orGetInfoParams,
                sizeof(orGetInfoParams)
            ));

    if (orGetInfoParams.type == LW0073_CTRL_SPECIFIC_OR_TYPE_WBOR)
    {
        Printf(Tee::PriHigh, "\n*****************************************************\n");
        Printf(Tee::PriHigh, "\nWBOR does not have display so does not support\n");
        Printf(Tee::PriHigh, "\nmanualVerification mode, instead run autoVerif\n");
        Printf(Tee::PriHigh, "\nand look at the output image (TestName_WRBK_res.png)\n");
        Printf(Tee::PriHigh, "\n*****************************************************\n");
        MASSERT(!"Manual Verification for WRBK not supported");
    }

    CHECK_RC(showPrompt(SetmodeDisplay, Surf2Print, PromptMessage, resolution, subdeviceInstance));
    CHECK_RC(acceptKey(timeoutMS, sleepInterval));

    return rc;
}

//! \brief showPrompt
//! This shows the prompt horizontally centered on the screen
//------------------------------------------------------------------
RC DTIUtils::VerifUtils::showPrompt(DisplayID SetmodeDisplay, Surface2D *Surf2Print, string PromptMessage, Display::Mode resolution, UINT32 subdeviceInstance)
{
    RC rc;
    UINT32 FontWidth=8;
    UINT32 FontHeight=8;
    bool IsMappedByUs = false;
    UINT32 ScaleX = (UINT32)(resolution.width / 400);
    UINT32 ScaleY = (UINT32)(resolution.height / 400);
    UINT32 Scale = (ScaleX > ScaleY) ? ScaleY : ScaleX;
    if (Scale < 1) Scale = 1;

    if (!(Surf2Print && Surf2Print->GetMemHandle() != 0))
    {
        Printf(Tee::PriHigh, "%s => Surf2Print is NULL or is not allocated",__FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    UINT32 requiredWidth = FontWidth * Scale * (UINT32)PromptMessage.size();
    UINT32 requiredHeight = FontHeight * Scale;

    UINT32 xPos = (resolution.width - requiredWidth) / 2;
    UINT32 yPos = (resolution.height - requiredHeight) / 2;

    // Map the surface if it is not mappped.
    if (!Surf2Print->GetAddress())
    {
        // Map only the region in center where we want to print message.
        // This is required since mapping entire image takes lots of memory
        // thus causing RC error LWRM_INSUFFICIENT_RESOURCES
        if( (rc = Surf2Print->MapRect(xPos, yPos, requiredWidth, requiredHeight , subdeviceInstance)) != OK)
        {
            Printf(Tee::PriHigh, "%s DTIUtils::VerifUtils::showPrompt => Surface2D MapRect() Failed rc = %d", __FUNCTION__, (UINT32)rc);
            Printf(Tee::PriHigh, "\n *** Manual Prompt wont be shown *** ");
            return rc;
        }
        IsMappedByUs = true;
    }

    CHECK_RC(GpuUtility::PutChars
    (
        xPos,
        yPos,
        PromptMessage,
        Scale,
        0xFFFFFF,
        0,
        false,
        Surf2Print
    ));

    // Unmap Before leaving.
    // Unmap only if the Surface was Mapped by us.
    // If end user has mapped surface then dont unmap it.
    if (IsMappedByUs && Surf2Print->GetAddress())
    {
        Surf2Print->Unmap();
    }

    return rc;
}

//======================================================
// Accept Keyboard Input
// returns OK if 'Y'
// returns RC::SOFTWARE_ERROR on timeout or pressing 'N'
//======================================================
RC DTIUtils::VerifUtils::acceptKey(UINT32 timeoutMS, UINT32 sleepInterval)
{
    RC rc;
    bool Found=false;
    UINT32 timeOutExpired = 0;
    Console::VirtualKey Answer = Console::VKC_LOWER_N;

    CHECK_RC(Utility::FlushInputBuffer());

    ConsoleManager::ConsoleContext consoleCtx;
    Console *pConsole = consoleCtx.AcquireRealConsole(true);

    do
    {
        if(!pConsole->KeyboardHit())
        {
            Printf(Tee::PriHigh, "\n Please press 'y' or 'n'. \t Timer = %d", (timeoutMS - timeOutExpired));

            Tasker::Sleep(sleepInterval);

            // Timeout mechanism
            timeOutExpired += sleepInterval;

            if (timeOutExpired > timeoutMS)
            {
                Printf(Tee::PriHigh, "\n %s => User didnt responded to the prompt. Returning RC::SOFTWARE_ERROR \n",__FUNCTION__);
                rc = RC::SOFTWARE_ERROR;
                break;
            }

            continue;
        }

        Answer = pConsole->GetKey();

        switch(Answer)
        {
        case Console::VKC_CAPITAL_Y:
        case Console::VKC_LOWER_Y:
            Printf(Tee::PriHigh, "\n User responded 'Y' / 'y' \n");
            Found=true;
            break;
        case Console::VKC_CAPITAL_N:
        case Console::VKC_LOWER_N:
            Printf(Tee::PriHigh, "\n User responded 'N' / 'n' \n");
            Found=true;
            rc  = RC::SOFTWARE_ERROR;
            break;
        default:
            Printf(Tee::PriHigh, "Please press only 'y' or 'n'."
                " Incorrect response, Please Try again \n");
            // retry with reset timeout
            timeOutExpired = 0;
        }

    }while (!Found);

    return rc;
}

//! automaticallyVerfying
//! automatically verfies the output by comparing CRCs
//------------------------------------------------------------------------------
RC DTIUtils::VerifUtils::autoVerification(Display   *pDisplay,
                                    GpuDevice       *pGpuDev,
                                    DisplayID        SetmodeDisplay,
                                    UINT32           width,
                                    UINT32           height,
                                    UINT32           depth,
                                    string           goldenFolderPath,
                                    string           CrcFileName,
                                    Surface2D       *pCoreImage,
                                    VerifyCrcTree   *pCrcCompFlag)
{
    RC rc;
    CrcComparison crcCompObj;
    UINT32 compositorCrc, primaryCrc, secondaryCrc;
    bool bCrcCompFlagAlloced = false;
    vector<string> searchPath;
    EvoDisplay * pEvoDisp = nullptr;

    searchPath.push_back (".");
    Utility::AppendElwSearchPaths(&searchPath,"LD_LIBRARY_PATH");
    Utility::AppendElwSearchPaths(&searchPath,"MODS_RUNSPACE");

    // For WBOR the CRC generation process is different
    // Find out if this is a wrbk device
    bool bIsWbor = false;

    LW0073_CTRL_SPECIFIC_OR_GET_INFO_PARAMS orGetInfoParams = {0};
    orGetInfoParams.displayId = SetmodeDisplay;
    CHECK_RC(pDisplay->RmControl(
                LW0073_CTRL_CMD_SPECIFIC_OR_GET_INFO,
                &orGetInfoParams,
                sizeof(orGetInfoParams)
            ));

    if (orGetInfoParams.type == LW0073_CTRL_SPECIFIC_OR_TYPE_WBOR)
    {
        bIsWbor  = true;
        pEvoDisp = pDisplay->GetEvoDisplay();
        MASSERT(pEvoDisp);
    }

    primaryCrc = compositorCrc = secondaryCrc = 0;

    EvoCRCSettings crcSettings;
    crcSettings.ControlChannel = EvoCRCSettings::CORE;
    crcSettings.MemLocation = Memory::Coherent;

    if (!pCrcCompFlag)
    {
        pCrcCompFlag = new VerifyCrcTree();
        pCrcCompFlag->crcHeaderField.bSkipChip = pCrcCompFlag->crcHeaderField.bSkipPlatform = true;
        bCrcCompFlagAlloced = true;
    }

    // WBOR needs extra CRC settings
    if (bIsWbor)
    {
        crcSettings.CaptureAllCRCs = true;
        crcSettings.PollCRCCompletion = false;
        //
        // Clearing the CRC notifier before detaching WBOR results in extra CRCs during detach
        // which triggers overflow warning for the first frame on next attach
        // According to arch we should ignore this status bit
        //
        pCrcCompFlag->crcNotifierField.bIgnorePrimaryOverflow = true;

        // WBOR will report PI met for its frames, we do not want to skip checking CRC
        Printf(Tee::PriHigh, "Setting bCompEvenIfPresentIntervalMet to true for WBOR\n");
        pCrcCompFlag->crcNotifierField.crcField.bCompEvenIfPresentIntervalMet = true;
    }

    CHECK_RC(pDisplay->SetupCrcBuffer(SetmodeDisplay,
                                      &crcSettings));

    if (bIsWbor)
    {
        Surface2D *pSurface;
        UINT32     head;
        std::ostringstream oss;

        CHECK_RC(pEvoDisp->GetHeadByDisplayID(SetmodeDisplay, &head));

        oss << CrcFileName << "_Head" << head << ".png";

        // Start CRC
        CHECK_RC(pDisplay->StartRunningCrc(SetmodeDisplay, &crcSettings));

        // Wait for core update complete

        // Loadv to promote the CRC setting to Active
        UINT32 idx;
        CHECK_RC(pEvoDisp->GetWritebackFrame(head, false, &idx));
        CHECK_RC(pEvoDisp->PollWritebackFrame(head, idx, true));

        // Save the surface
        // This lwrrently only support single A8R8G8B8 surface
        CHECK_RC(pEvoDisp->GetWritebackSurface(SetmodeDisplay, &pSurface, 0));
        CHECK_RC(pSurface->WritePng(oss.str().c_str()));

        // Stop running the CRC
        CHECK_RC(pDisplay->StopRunningCrc(SetmodeDisplay, &crcSettings));

        // Generate another loadv to generate the CRC flush out to the notifier
        // Here WBOR will read the crc from the prev loadv and will indicate
        // that we no longer require CRC (so 1 CRC will be written)
        CHECK_RC(pEvoDisp->GetWritebackFrame(head, false, &idx));
        CHECK_RC(pEvoDisp->PollWritebackFrame(head, idx, true));

        // Poll for CRC completion
        CHECK_RC(pEvoDisp->PollCrcCompletion(&crcSettings));
    }
    else
    {
        CHECK_RC(pDisplay->GetCrcValues(SetmodeDisplay,
                                            &crcSettings,
                                            &compositorCrc,
                                            &primaryCrc,
                                            &secondaryCrc));
    }
    CHECK_RC(crcCompObj.DumpCrcToXml(pGpuDev,
                                     CrcFileName.c_str(),
                                     &crcSettings,
                                     true));

    if( !CrcComparison::FileExists(CrcFileName.c_str(), &searchPath))
    {
        Printf(Tee::PriHigh, "\n ****************************ERROR******************************");
        Printf(Tee::PriHigh, "\n %s => CRC Current Run File [%s] was not created. Returning RC:: FILE_DOES_NOT_EXIST \n",
            __FUNCTION__, CrcFileName.c_str());
        Printf(Tee::PriHigh, "\n **********************************************************************");
        return RC::FILE_DOES_NOT_EXIST;
    }

    string logFileName = CrcFileName + string(".log");
    string goldenFileName = string(goldenFolderPath + CrcFileName);

    if( !CrcComparison::FileExists(goldenFileName.c_str(), &searchPath))
    {
        Printf(Tee::PriHigh, "\n ************** ERROR::CRC GOLDEN FILE DOES NOT EXIST **************");
        Printf(Tee::PriHigh, "\n %s => CRC Golden File [%s] does not exists. Returning RC:: FILE_DOES_NOT_EXIST \n",
            __FUNCTION__,goldenFileName.c_str());
        Printf(Tee::PriHigh, "\n =>PLEASE SUBMIT GOLDEN FILE FOR THIS CONFIG TO PERFORCE PATH");
        Printf(Tee::PriHigh, "\n **********************************************************************");
        return RC::FILE_DOES_NOT_EXIST;
    }

    // API to take diff between 2 XML crc files
    rc = crcCompObj.CompareCrcFiles(CrcFileName.c_str(),
                                    goldenFileName.c_str(),
                                    logFileName.c_str(),
                                    pCrcCompFlag);
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "\n %s => CRC Compare failed for Golden File [%s]. RC = %d \n",
            __FUNCTION__,goldenFileName.c_str(), (UINT32)rc);
    }

    if (bCrcCompFlagAlloced)
    {
        delete pCrcCompFlag;
    }

    return rc;
}

//! IsSupportedProtocol
//! Check if the protocol for port corresponding to the displayID can support
//! any of the protocols in the comma separated list of protocols passed by the user
//------------------------------------------------------------------------------
bool DTIUtils::VerifUtils::IsSupportedProtocol(Display *pDisplay,
                                               DisplayID displayID,
                                               string desiredProtocolList)
{
    vector <string> desiredProtocol;

    // Colwert desiredProtocolString to upper case to allow user to use either lower or upper case or a mix when specifying the protocol test must use.
    std::transform(desiredProtocolList.begin(), desiredProtocolList.end(), desiredProtocolList.begin(), upper);

    //Check if the comma seperated protocol cmdline passed by the user is valid protocol string
    if (!IsValidProtocolString(desiredProtocolList))
    {
        return false;
    }

    string lwrProtocol = " ";
    if (pDisplay->GetProtocolForDisplayId(displayID, lwrProtocol) != OK)
    {
        Printf(Tee::PriHigh, " Error in getting Protocol for DisplayId = 0x%08X\n",(UINT32)displayID);
        return false;
    }

    desiredProtocol = DTIUtils::VerifUtils::Tokenize(desiredProtocolList, ",");

    for (UINT32 i = 0; i < desiredProtocol.size(); i++)
    {
        //
        // Check if the protocol for the current display device (lwrProtocol)
        // matches the desired (desiredProtocol[i]). If they match, we have found a
        // displayId we can use. But even if they don't match on
        // string comparion it's possible the current displayId is usable.
        // Checks for such cases inside the if below.
        //
        if (desiredProtocol[i].compare(lwrProtocol) != 0)
        {

            if (
                 // If desired protocol is TMDS_A and the connector is dual link, we can use it
                 ( (desiredProtocol[i].compare("SINGLE_TMDS_A") == 0) &&
                   (lwrProtocol.compare("DUAL_TMDS") == 0) ) ||
                 // If desired protocol is SINGLE_TMDS then any of SINGLE_TMDS_(A|B) or DUAL_TMDS works
                 ( (desiredProtocol[i].compare("SINGLE_TMDS") == 0) &&
                   ( (lwrProtocol.compare("SINGLE_TMDS_A") == 0) ||
                     (lwrProtocol.compare("SINGLE_TMDS_B") == 0) ||
                     (lwrProtocol.compare("DUAL_TMDS") == 0) ) ) ||
                 // If desired protocol is DP then any of DP_(A|B) works
                 ( (desiredProtocol[i].compare("DP") == 0) &&
                   ( (lwrProtocol.compare("DP_A") == 0) ||
                     (lwrProtocol.compare("DP_B") == 0) ) ) ||
                 // If desired protocol is DSI then DSI works
                 ((desiredProtocol[i].compare("DSI") == 0) &&
                     (lwrProtocol.compare("DSI") == 0)) ||
                 // If desired protocol is EDP then any of DP_(A|B) works
                 ( (desiredProtocol[i].compare("EDP") == 0) &&
                   ( (lwrProtocol.compare("DP_A") == 0) ||
                     (lwrProtocol.compare("DP_B") == 0) ) ) )
            {
                Printf(Tee::PriHigh, " Using displayId = 0x%08X since %s can be used on port that supports %s\n", (UINT32)displayID, desiredProtocol[i].c_str(), lwrProtocol.c_str());
            }
            else
            {
                Printf(Tee::PriHigh, " Skipping displayId = 0x%08X since %s can NOT be used on port that supports %s\n",(UINT32)displayID, desiredProtocol[i].c_str(), lwrProtocol.c_str());
                continue;
            }
        }
        else
        {
            Printf(Tee::PriHigh, " Using displayId = 0x%08X which is an exact match for %s\n", (UINT32)displayID, desiredProtocol[i].c_str());
        }
        return true;
    }
    return false;
}

//------------------------------------------------------------------------------
//! IsModeSupportedOnMonitor
//! Check if the Mode required is supported on displayID passed.
//! This function checks the edid resolutions to check if given mode is supported on that monitor
//------------------------------------------------------------------------------
RC DTIUtils::VerifUtils::IsModeSupportedOnMonitor(Display          *pDisplay,
                                                  Display::Mode     mode,
                                                  DisplayID         dispId,
                                                  bool             *pModeSupportedOnMonitor,
                                                  bool              bGetResFromCPL)
{
    RC rc;
    vector<Display::Mode>         ListedRes;

    rc = DTIUtils::EDIDUtils::GetListedModes(pDisplay,
                                  dispId,
                                  &ListedRes,
                                  bGetResFromCPL);
    if (rc != OK)
    {
        Printf(Tee::PriHigh, "ERROR: %s: Failed to GetListedResolutions() for DispId = %X with RC=%d \n",
                            __FUNCTION__,(UINT32)dispId,(UINT32)rc);

        return rc;
    }

    *pModeSupportedOnMonitor = false;
    for(UINT32 i = 0; i < (UINT32)ListedRes.size(); i++)
    {
        if(ListedRes[i].width       == mode.width &&
           ListedRes[i].height      == mode.height &&
           ListedRes[i].refreshRate == mode.refreshRate
          )
        {
            *pModeSupportedOnMonitor = true;
            break;
        }
    }

    if (!(*pModeSupportedOnMonitor) && ((mode.width > 4095) || (mode.height > 4095)))
    {
        Printf(Tee::PriHigh,
               "NOTE: %s: On some SKUs, support for 4K resolution is enabled only on the debug version"
               " of the driver and mods (bugs 997511 & 1005532).\n",
               __FUNCTION__);
    }

    return rc;
}

//! \brief upper
//! This wrapper function colwerts given character to upper case
int DTIUtils::VerifUtils::upper(int c)
{
    if (c >=97 && c <= 122)
        return c - 32;
    else
        return c;
}

//! \brief Tokenizefunction
//!
//! In this function we divide the line in to parts depending on the
//! delimiter value, if we don't passe any delimiter value
//! it takes whitespae as default.
//!
vector<string> DTIUtils::VerifUtils::Tokenize(const string &str,
                                   const string &delimiters)
{
    vector<string> tokens;

    // Skip delimiters at beginning.
    string::size_type lastPos = str.find_first_not_of(delimiters, 0);

    // Find first "non-delimiter".
    string::size_type pos     = str.find_first_of(delimiters, lastPos);

    while (string::npos != pos || string::npos != lastPos)
    {
        // Found a token, add it to the vector.
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        // Skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);
        // Find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }

    return tokens;
}

//! Check if the comma seperated protocol cmdline passed by the user is valid protocol string
bool DTIUtils::VerifUtils::IsValidProtocolString(string desiredProtocol)
{
    // Make entire protocol cmdline string as UCASE
    std::transform(desiredProtocol.begin(), desiredProtocol.end(), desiredProtocol.begin(), upper);

    // split the string to comma seperated vector<string>
    vector<string> reqProtocol;
    reqProtocol= Tokenize (desiredProtocol,",");

    // loop to check if each protocol passsed is valid
    vector<string> AllProtocols;
    AllProtocols.push_back("CRT");
    AllProtocols.push_back("TV");
    AllProtocols.push_back("LVDS_LWSTOM");
    AllProtocols.push_back("SINGLE_TMDS"); //HACK: added this for end user to specify SINGLE_TMDS instead of SINGLE_TMDS_A,SINGLE_TMDS_B
    AllProtocols.push_back("SINGLE_TMDS_A");
    AllProtocols.push_back("SINGLE_TMDS_B");
    AllProtocols.push_back("DUAL_TMDS");
    AllProtocols.push_back("EDP");
    AllProtocols.push_back("DP");       //HACK: added DP for end user to specify DP instead of DP_A,DP_B
    AllProtocols.push_back("DP_A");
    AllProtocols.push_back("DP_B");
    AllProtocols.push_back("EXT_TMDS_ENC");
    AllProtocols.push_back("WRBK");
    AllProtocols.push_back("DSI");

    for (UINT32 count=0; count < reqProtocol.size(); count++)
    {
        vector<string>::iterator result;
        result = find(AllProtocols.begin(), AllProtocols.end(), reqProtocol[count]);

        if(result == AllProtocols.end())
        {
            Printf(Tee::PriHigh, "\nDesired Protocol string contains Invalid protocol => %s ",reqProtocol[count].c_str());
            return false;
        }
    }

    return true;
}

RC  DTIUtils::VerifUtils::checkDispUnderflow(Display *pDisplay, UINT32 numHeads, UINT32 *pUnderflowHead)
{
    RC rc;
    bool displayUnderflowed = false;

    for(UINT32 head = 0; head < numHeads; head++)
    {
        // misc info:
        LW5070_CTRL_CMD_GET_RG_UNDERFLOW_PROP_PARAMS UnderFlowParam;
        rc = DTIUtils::VerifUtils::GetUnderflowState(pDisplay,
            head,
            &UnderFlowParam);

        if (OK != rc)
        {
            Printf(Tee::PriHigh, "%s : Failed to get RG underflow state on head = %d. RC = %d \n",
                __FUNCTION__, head, (UINT32)rc);
            return rc;
        }

        if (UnderFlowParam.underflowParams.underflow)
        {
            displayUnderflowed = true;
            *pUnderflowHead = head;
        }
    }

    if(displayUnderflowed)
        return RC::LWRM_ERROR;
    return rc;
}

//! GetUnderflowState
//!     Get RG underflow state for head specified
RC  DTIUtils::VerifUtils::GetUnderflowState(Display *pDisplay,
                                            UINT32 head,
                                            LW5070_CTRL_CMD_GET_RG_UNDERFLOW_PROP_PARAMS *pGetUnderflowParams)
{
    RC rc;

    memset(pGetUnderflowParams, 0, sizeof(LW5070_CTRL_CMD_GET_RG_UNDERFLOW_PROP_PARAMS));
    pGetUnderflowParams->base.subdeviceIndex  = pDisplay->GetMasterSubdevice();

    memset(&pGetUnderflowParams->underflowParams, 0, sizeof(pGetUnderflowParams->underflowParams));
    pGetUnderflowParams->underflowParams.head = head;

    if (OK != (rc = pDisplay->RmControl5070(LW5070_CTRL_CMD_GET_RG_UNDERFLOW_PROP,
        pGetUnderflowParams, sizeof(LW5070_CTRL_CMD_GET_RG_UNDERFLOW_PROP_PARAMS))))
    {
        Printf(Tee::PriNormal, "%s : Failed to get RG underflow params. RC = %d \n",
            __FUNCTION__, (UINT32)rc);
    }

    return rc;
}

//! SetUnderflowState
//!     Set RG underflow state on head specified
RC  DTIUtils::VerifUtils::SetUnderflowState(Display *pDisplay,
                                            UINT32 head,
                                            UINT32 enable,
                                            UINT32 clearUnderflow,
                                            UINT32 mode)
{
    RC rc;
    LW5070_CTRL_CMD_SET_RG_UNDERFLOW_PROP_PARAMS setUnderFlowParam;
    memset(&setUnderFlowParam, 0, sizeof(setUnderFlowParam));
    setUnderFlowParam.base.subdeviceIndex  = pDisplay->GetMasterSubdevice();

    memset(&setUnderFlowParam.underflowParams, 0, sizeof(setUnderFlowParam.underflowParams));
    setUnderFlowParam.underflowParams.head       = head;
    setUnderFlowParam.underflowParams.enable     = enable;
    setUnderFlowParam.underflowParams.underflow  = clearUnderflow;
    setUnderFlowParam.underflowParams.mode       = mode;

    if (OK != (rc = pDisplay->RmControl5070(LW5070_CTRL_CMD_SET_RG_UNDERFLOW_PROP,
        &setUnderFlowParam, sizeof(LW5070_CTRL_CMD_SET_RG_UNDERFLOW_PROP_PARAMS))))
    {
        Printf(Tee::PriNormal,
            "%s Set Underflow State failed with  RC = %d \n",
            __FUNCTION__, (UINT32)rc);
        return rc;
    }

    return rc;
}

//! \brief Search all paths in 'searchpath' and try to open the file 'fname'.  Upon
//! success, return a newly allocated ifstream.  On failure, return NULL.

ifstream * Mislwtils:: alloc_ifstream (vector<string> & searchpath, const string & fname)
{
    ifstream *pOpenfile= NULL;
    for (vector<string>::const_iterator it = searchpath.begin(); it != searchpath.end(); ++it)
    {

        const string & dirname = *it;
        string fullpath = "";

        if (dirname != "")
        {
            fullpath += dirname;
            fullpath += "/";
        }

        fullpath += fname;

        pOpenfile = new ifstream (fullpath.c_str());

        if (pOpenfile->is_open())
        {
            break;
        }
        delete pOpenfile;
        pOpenfile = NULL;
    }
    return pOpenfile;
}

RC DTIUtils::Mislwtils::GetHeadRoutingMask(Display *pDisplay,
                                           LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS *pHeadRoutingMap)
{
    RC rc;
    rc = pDisplay->RmControl(LW0073_CTRL_CMD_SYSTEM_GET_HEAD_ROUTING_MAP,
                         pHeadRoutingMap,
                         sizeof (LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS));

    if (rc != OK)
    {
        Printf(Tee::PriNormal,
               "%s Head Routing Map failed: RC =0x%d\n",__FUNCTION__, (UINT32)rc);
        return rc;
    }

    if (pHeadRoutingMap->displayMask == 0)
    {
        Printf(Tee::PriLow, "%s : head routing map: unsupported \n",__FUNCTION__);
    }
    else
    {
        Printf(Tee::PriLow, "%s :Head routing map: 0x%x\n",
               __FUNCTION__, (UINT32)pHeadRoutingMap->headRoutingMap);
    }
    return rc;
}

RC DTIUtils::Mislwtils::GetHeadFromRoutingMask(DisplayID dispayId,
                          LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS HeadMapParams,
                          UINT32 *pHead)
{
    UINT32 disp = 0;
    UINT32 i = 0;

    for (i = 0; i < 32; ++i)
    {
        if (HeadMapParams.displayMask & (1 << i))
        {
            if (((UINT32)dispayId) == (1U << i))
            {
                // Found the display.
                *pHead = (HeadMapParams.headRoutingMap >> (4*disp)) & 0x0F;
                Printf(Tee::PriDebug, "GetHead : Got Head : %d  for Display ID : 0x%x ",
                       (*pHead), (UINT32)dispayId);

                return OK;
            }
            ++disp;
        }
    }

    // We should never reach this code.
    return RC::SOFTWARE_ERROR;
}

//!
//! \brief Colwert disp class to the equivalent string
//! \param displayClass          [in]    display Class
//! \param displayClassStr       [out]   display Class Str
//!
//! \return  if the operation completes successfully, the return value is RC::OK.
//!          if the opration fails return value in non RM::OK. returns RC error in errors.h.
RC DTIUtils::Mislwtils:: ColwertDisplayClassToString(UINT32 displayClass,
                                                     string &displayClassStr)
{
    RC rc;

    switch (displayClass)
    {
        case LWC870_DISPLAY:
            displayClassStr = "LWC870_DISPLAY";
            break;
        case LWC770_DISPLAY:
            displayClassStr = "LWC770_DISPLAY";
            break;        
        case LWC670_DISPLAY:
            displayClassStr = "LWC670_DISPLAY";
            break;
        case LWC570_DISPLAY:
            displayClassStr = "LWC570_DISPLAY";
            break;
        case LWC370_DISPLAY:
            displayClassStr = "LWC370_DISPLAY";
            break;
        case LW9870_DISPLAY:
            displayClassStr = "LW9870_DISPLAY";
            break;
        case LW9770_DISPLAY:
            displayClassStr = "LW9770_DISPLAY";
            break;
        case LW9570_DISPLAY:
            displayClassStr = "LW9570_DISPLAY";
            break;
        case LW9470_DISPLAY:
            displayClassStr = "LW9470_DISPLAY";
            break;
        case LW9270_DISPLAY:
            displayClassStr = "LW9270_DISPLAY";
            break;
        case LW9170_DISPLAY:
            displayClassStr = "LW9170_DISPLAY";
            break;
        case LW9070_DISPLAY:
            displayClassStr = "LW9070_DISPLAY";
            break;
        case GT214_DISPLAY:
            displayClassStr = "GT214_DISPLAY";
            break;
        case G94_DISPLAY:
            displayClassStr = "G94_DISPLAY";
            break;
        case GT200_DISPLAY:
            displayClassStr = "GT200_DISPLAY";
            break;
        case G82_DISPLAY:
            displayClassStr = "G82_DISPLAY";
            break;
        case LW50_DISPLAY:
            displayClassStr = "LW50_DISPLAY";
            break;        
    }

    return rc;
}

//! CompareSurfaces
//! surf2Compare  = Surface to be compared
//! goldenSurface = Surface2D Golden Object
//! resultMode    = Set First LSB bit as 1  for printing on stdout
//!                 Set Second LSB bit as 1 for printing to LOG File
RC DTIUtils::VerifUtils::CompareSurfaces(Surface2D    *surf2Compare,
                   Surface2D    *goldenSurface,
                   UINT32        resultMode)
{
    RC rc;
    bool IsOrigSurfMappedByUs = false;
    bool IsGoldenSurfMappedByUs = false;

    MASSERT(surf2Compare);
    MASSERT(goldenSurface);

    if (!(surf2Compare && surf2Compare->GetMemHandle() != 0))
    {
        Printf(Tee::PriHigh, "%s => surf2Compare is NULL or is not allocated",__FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    if (!(goldenSurface && goldenSurface->GetMemHandle() != 0))
    {
        Printf(Tee::PriHigh, "%s => goldenSurface is NULL or is not allocated",__FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    // Map the surface if it is not mappped.
    if (!surf2Compare->GetAddress())
    {
        if ((rc = surf2Compare->Map(0)) != OK)
        {
            Printf(Tee::PriHigh, "%s => surf2Compare Map() Failed rc = %d", __FUNCTION__, (UINT32)rc);
            Printf(Tee::PriHigh, "\n *** Aborting Surface Comparison *** ");
            return rc;
        }
        IsOrigSurfMappedByUs = true;
    }

    // Map the surface if it is not mappped.
    if (!goldenSurface->GetAddress())
    {
        if ((rc = goldenSurface->Map(0)) != OK)
        {
            Printf(Tee::PriHigh, "%s => goldenSurface Map() Failed rc = %d",__FUNCTION__,(UINT32)rc);
            Printf(Tee::PriHigh, "\n *** Aborting Surface Comparison *** ");
            return rc;
        }
        IsGoldenSurfMappedByUs = true;
    }

    if (surf2Compare->GetWidth() != goldenSurface->GetWidth())
    {
        Printf(Tee::PriHigh, "GetWidth Mismatch: Surf2Compare:(0x%X); Golden: (0x%X)\n",surf2Compare->GetWidth(), goldenSurface->GetWidth());
        rc = RC::GOLDEN_VALUE_MISCOMPARE;
    }

    if (surf2Compare->GetHeight() != goldenSurface->GetHeight())
    {
        Printf(Tee::PriHigh, "GetHeight Mismatch: Surf2Compare:(0x%X); Golden: (0x%X)\n",surf2Compare->GetHeight(), goldenSurface->GetHeight());
        rc = RC::GOLDEN_VALUE_MISCOMPARE;
    }

    if (surf2Compare->GetDepth() != goldenSurface->GetDepth())
    {
        Printf(Tee::PriHigh, "GetDepth Mismatch: Surf2Compare:(0x%X); Golden: (0x%X)\n",surf2Compare->GetDepth(), goldenSurface->GetDepth());
        rc = RC::GOLDEN_VALUE_MISCOMPARE;
    }

    if (surf2Compare->GetBitsPerPixel() != goldenSurface->GetBitsPerPixel())
    {
        Printf(Tee::PriHigh, "GetBitsPerPixel Mismatch: Surf2Compare:(0x%X); Golden: (0x%X)\n",surf2Compare->GetBitsPerPixel(), goldenSurface->GetBitsPerPixel());
        rc = RC::GOLDEN_VALUE_MISCOMPARE;
    }

    if (surf2Compare->GetPitch() != goldenSurface->GetPitch())
    {
        Printf(Tee::PriHigh, "GetPitch Mismatch: Surf2Compare:(0x%X); Golden: (0x%X)\n",surf2Compare->GetPitch(), goldenSurface->GetPitch());
        rc = RC::GOLDEN_VALUE_MISCOMPARE;
    }

    if (surf2Compare->GetPartStride() != goldenSurface->GetPartStride())
    {
        Printf(Tee::PriHigh, "GetPartStride Mismatch: Surf2Compare:(0x%X); Golden: (0x%X)\n",surf2Compare->GetPartStride(), goldenSurface->GetPartStride());
        rc = RC::GOLDEN_VALUE_MISCOMPARE;
    }

    if (surf2Compare->GetBlockHeight() != goldenSurface->GetBlockHeight())
    {
        Printf(Tee::PriHigh, "GetBlockHeight Mismatch: Surf2Compare:(0x%X); Golden: (0x%X)\n",surf2Compare->GetBlockHeight(), goldenSurface->GetBlockHeight());
        rc = RC::GOLDEN_VALUE_MISCOMPARE;
    }

    if (surf2Compare->GetLayout() != goldenSurface->GetLayout())
    {
        Printf(Tee::PriHigh, "GetLayout Mismatch: Surf2Compare:(0x%X); Golden: (0x%X)\n",surf2Compare->GetLayout(), goldenSurface->GetLayout());
        rc = RC::GOLDEN_VALUE_MISCOMPARE;
    }

    if (surf2Compare->GetColorFormat() != goldenSurface->GetColorFormat())
    {
        Printf(Tee::PriHigh, "GetColorFormat Mismatch: Surf2Compare:(0x%X); Golden: (0x%X)\n",surf2Compare->GetColorFormat(), goldenSurface->GetColorFormat());
        rc = RC::GOLDEN_VALUE_MISCOMPARE;
    }

    if (surf2Compare->GetAAMode() != goldenSurface->GetAAMode())
    {
        Printf(Tee::PriHigh, "GetAAMode Mismatch: Surf2Compare:(0x%X); Golden: (0x%X)\n",surf2Compare->GetAAMode(), goldenSurface->GetAAMode());
        rc = RC::GOLDEN_VALUE_MISCOMPARE;
    }

    // Test each pixel Value
    // Loop through every pixel of the two surfaces and verify they match
    UINT32 pixel_a;
    UINT32 pixel_b;

    for (UINT32 y = 0; y < surf2Compare->GetHeight() && y < goldenSurface->GetHeight(); ++y)
    {
        for (UINT32 x = 0; x < surf2Compare->GetWidth() && x < goldenSurface->GetWidth(); ++x)
        {
            pixel_a = surf2Compare->ReadPixel(x, y);
            pixel_b = goldenSurface->ReadPixel(x, y);

            if (pixel_a != pixel_b)
            {
                if (resultMode & 1)
                {
                    Printf(Tee::PriHigh, "Pixel(%d,%d) Mismatch: Surf2Compare:(%u); Golden: (%u)\n", x, y, pixel_a, pixel_b);
                }

                if (resultMode & 2)
                {
                    //TODO: Write To LOG File Logic goes here
                }
                rc = RC::GOLDEN_VALUE_MISCOMPARE;
            }
        }
    }

    // Unmap Before leaving.
    // Unmap only if the Surface was Mapped by us.
    // If end user has mapped surface then dont unmap it.
    if (IsOrigSurfMappedByUs && surf2Compare->GetAddress())
    {
        surf2Compare->Unmap();
    }

    if (IsGoldenSurfMappedByUs && goldenSurface->GetAddress())
    {
        goldenSurface->Unmap();
    }

    return rc;
}

RC DTIUtils::VerifUtils::CreateGoldenAndCompare(GpuDevice       *pOwningDev,
                          Surface2D    *surf2Compare,
                          Surface2D    *GoldenSurface,
                          UINT32        ImageWidth,
                          UINT32        ImageHeight,
                          UINT32        ImageDepth,
                          string        ImageName,
                          UINT32        color)
{
    RC rc;

    MASSERT(surf2Compare);
    MASSERT(GoldenSurface);

    if (GoldenSurface->GetMemHandle() == 0)
    {
        GoldenSurface->SetWidth(ImageWidth);
        GoldenSurface->SetHeight(ImageHeight);
        GoldenSurface->SetDepth(ImageDepth);
        GoldenSurface->SetColorFormat(ColorUtils::ColorDepthToFormat(ImageDepth));
        GoldenSurface->SetDisplayable(true);
        GoldenSurface->SetPhysContig(true);
        GoldenSurface->SetLocation(Memory::Fb);
        GoldenSurface->SetAddressModel(Memory::Paging);
        GoldenSurface->SetType(0);              //LWOS32_TYPE_IMAGE = 0
        CHECK_RC(GoldenSurface->Alloc(pOwningDev));
    }

    if (GoldenSurface->GetMemHandle() != 0)
    {
        if(ImageName != "")
            GoldenSurface->ReadPng(ImageName.c_str(), 0);
        if(color != 0)
            GoldenSurface->Fill(color, 0);
    }

    return DTIUtils::VerifUtils::CompareSurfaces(surf2Compare,GoldenSurface,3);
}

//! \brief colwerts a char to hex
//------------------------------------------------------------------------------
int DTIUtils::EDIDUtils::gethex(char ch1)
{
    if (ch1 >= 'A' && ch1 <= 'F')
        return ch1 - 'A' + 10;
    if (ch1 >= 'a' && ch1 <= 'f')
        return ch1 - 'a' + 10;
    return ch1 - '0';
}

//! \brief this function gets Edid buf from given file
//!
//------------------------------------------------------------------------------
RC DTIUtils::EDIDUtils::GetEdidBufFromFile(string EdidFileName,UINT08 *&edidData, UINT32 *edidSize)
{
    RC rc;
    string edidbuf= "" ,tmpEdidBuf= "";
    vector<string> searchPath;
    searchPath.push_back (".");
    Utility::AppendElwSearchPaths(&searchPath,"LD_LIBRARY_PATH");
    Utility::AppendElwSearchPaths(&searchPath,"MODS_RUNSPACE");
    ifstream *pEdidFile = Mislwtils::alloc_ifstream(searchPath, EdidFileName);
    if (!pEdidFile)
    {
        Printf(Tee::PriHigh, "Cannot open EDID file %s\n"
               "The EDID file is expected to be in the edids subdirectory, which is normally populated with edids\n"
               " from //sw/mods/rmtest/edids/... did you sync this directory before building MODS?\n",
               EdidFileName.c_str());
        rc = RC::CANNOT_OPEN_FILE;
        delete pEdidFile;
        return rc;
    }

    pEdidFile->seekg (0, ios::beg);

    while (getline(*pEdidFile, tmpEdidBuf))
    {
        edidbuf += tmpEdidBuf;
    }

    //replace the EDID charecters with the ""
    FindNReplace(edidbuf," ", "");
    FindNReplace(edidbuf,"\t", "");
    FindNReplace(edidbuf,"\n", "");
    FindNReplace(edidbuf,"\r", "");

    edidData  = new UINT08[ (edidbuf.size() + 1) / 2];

    Printf(Tee::PriDebug," \n Edid Data Starts \n ");
    UINT32 i= 0,j = 0;
    for (i = 0,j = 0; i < edidbuf.size() ; i += 2)
    {
        edidData[j] = (UINT08)(gethex(edidbuf[i])* 16 + gethex(edidbuf[i + 1]));
        Printf(Tee::PriDebug,"0x%x,",edidData[j]);
        j++;
    }

    Printf(Tee::PriDebug," \n Edid data Completed \n");

    //fill the param Edid Size
    *edidSize = j;

    pEdidFile->close();
    delete pEdidFile;
    pEdidFile = NULL;

    return rc;
}

//! \brief this function replaces a given character from a file
//!
//------------------------------------------------------------------------------
void DTIUtils::EDIDUtils::FindNReplace(string &content,string findstr, string repstr)
{
    size_t j = 0;
    while (true)
    {
        j = content.find(findstr,j);
        if (j == string::npos)
        {
            break;
        }
        content.replace(j,findstr.size(),repstr);
    }
    return;
}

//!
//! \brief It attaches fake displays from the given list of vbios displays
//!        for the given protocol and sets the given EDID on the faked displayID.
//! \param pDisplay [in]         Display pointer
//! \param dispProtocol [in]     fake only if the display supports this protoocl.
//! \param vbiosDisplays[in]     vector of DisplayIDs which need to be faked.
//! \param pFakeDispIds[out]     vector of faked DisplayIDs
//! \param Edidfilename[in, optional]    EDID to fake the displays with.
//! \return     if the operation completes succesfully, the return value is RC::OK
//!             and outputs the list faked displays in pFakeDispIds.
//!
RC DTIUtils::EDIDUtils::FakeDisplay
(
    Display *pDisplay,
    string dispProtocol,
    DisplayIDs vbiosDisplays,
    DisplayIDs *pFakeDispIds,
    string Edidfilename
)
{
    RC rc = RC::SOFTWARE_ERROR;

    if (vbiosDisplays.size() == 0)
    {
        Printf(Tee::PriHigh, "\nNo displays to fake\n");
        return RC::BAD_PARAMETER;
    }

    for(UINT32 loopCount = 0; loopCount < vbiosDisplays.size(); loopCount++)
    {
        if (DTIUtils::VerifUtils::IsSupportedProtocol(pDisplay, vbiosDisplays[loopCount], dispProtocol))
        {
            CHECK_RC(DTIUtils::EDIDUtils::SetLwstomEdid(pDisplay, vbiosDisplays[loopCount], false));
            pFakeDispIds->push_back(vbiosDisplays[loopCount]);
        }
    }

    return rc;
}

//! \brief SetLwstomEdid
//!
//! In this function when
//! bSetEdid = When true; we set EDID for given displayID as per Edidfilename
//!            else when false; we attach fake device as per Edidfilename
//! Edidfilename = Filename of edid file to use for faking this displayId
//!
RC DTIUtils::EDIDUtils::SetLwstomEdid(Display *pDisplay,
                                      DisplayID displayID,
                                      bool bSetEdid,
                                      string Edidfilename)
{
    UINT08 *rawEdid = NULL;
    UINT32 edidSize = 0;
    RC rc = OK;

    // If Edidfilename is not provided then select the default EDID
    if(Edidfilename.compare("") == 0)
    {
        Display::DisplayType thisDispType;
        CHECK_RC(pDisplay->GetDisplayType(displayID, &thisDispType));

        switch(thisDispType)
        {
            case Display::CRT:
            {
                Edidfilename = "edids/CRT_A_VIEWSONIC_G70FM.txt";
                break;
            }
            case Display::DFP:
            {
                Edidfilename = "edids/DFP_D_NEC_LCD1970NX.txt";
                break;
            }
            default:
            {
                Printf(Tee::PriHigh, "Unknown type 0x%x for display 0x%x\n",
                                    thisDispType,
                                    (UINT32)(displayID));
                MASSERT(0);
                return RC::ILWALID_DISPLAY_TYPE;
            }
        }
    }

    CHECK_RC(DTIUtils::EDIDUtils::GetEdidBufFromFile(Edidfilename, rawEdid, &edidSize));

    if(bSetEdid)
    {
        CHECK_RC(pDisplay->SetEdid(displayID, rawEdid, edidSize));
        Printf(Tee::PriHigh, "Setting EDID for DisplayID 0x%X\n", (UINT32)displayID);
    }
    else
    {
        CHECK_RC(pDisplay->AttachFakeDevice(displayID,
                                            rawEdid,
                                            edidSize));
        Printf(Tee::PriHigh, "Faking DisplayID 0x%X\n", (UINT32)displayID);
    }

    if(rawEdid)
    {
        delete[] rawEdid;
        rawEdid = NULL;
    }
    return rc;
}

//! \brief GetSupportedRes
//! This method returns lwrrently passed resolution if it is ListedResolution in EDID.
//! Else returns vector of all resolutions >= lwrrently passed resolution and end user can select any resolution from it.
//------------------------------------------------------------------------------
RC DTIUtils::EDIDUtils::GetSupportedRes(Display        *pDisplay,
                                        DisplayID       displayID,
                                        Display::Mode            requiredRes,
                                        vector<Display::Mode>   *pSupportedRes)
{
    RC rc;

    UINT32 ResCount;
    Edid *pEdid = pDisplay->GetEdidMember();

    if (pEdid->IsListedResolution(displayID, requiredRes.width, requiredRes.height, requiredRes.refreshRate))
    {
        pSupportedRes->push_back(requiredRes);

    }
    else
    {
        CHECK_RC(pEdid->GetNumListedResolutions(displayID,
                                                &ResCount));

        UINT32 * Widths  = new UINT32[ResCount];
        UINT32 * Heights = new UINT32[ResCount];
        UINT32 * RefreshRates = new UINT32[ResCount];

        rc = pEdid->GetListedResolutions(displayID,
                                         Widths,
                                         Heights,
                                         RefreshRates);
        if(rc != OK)
        {
            Printf(Tee::PriDebug, "Failed to read EDID while getting Display "
                "Device's  listed resolutions \n");

            delete [] Widths;
            delete [] Heights;
            delete [] RefreshRates;
            return rc;
        }

        // add the resolutions greater then
        for(UINT32 count = 0;  count < ResCount; count++)
        {
            if((Widths[count] >= requiredRes.width) && (Heights[count] >= requiredRes.height))
            {
                pSupportedRes->push_back(Display::Mode(Widths[count], Heights[count], 32, RefreshRates[count]));
            }
        }

        // clean resources used
        delete [] Widths;
        delete [] Heights;
        delete [] RefreshRates;
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief GetListedModes
//! This method returns vector of all Modes listed in EDID.
//! returns RC =OK on succesful operation
//!         else returns appropriate RC ERROR
//------------------------------------------------------------------------------
RC DTIUtils::EDIDUtils::GetListedModes(Display                       *pDisplay,
                                       DisplayID                      displayID,
                                       vector<Display::Mode>         *pListedModes,
                                       bool                           bGetResFromCPL)
{
    RC       rc;

    if( pDisplay->GetDisplayClassFamily() == Display::WIN32DISP &&
        bGetResFromCPL )
    {
        rc = DTIUtils::EDIDUtils::GetListedModesFromCPL(pDisplay,
                                                      displayID,
                                                      pListedModes);
        return rc;
    }

    Edid    *pEdid = pDisplay->GetEdidMember();

    // Ilwalidate EDID so we re read it using new modeset library
    pEdid->Ilwalidate(displayID);
    const Edid::ListedRes  *pListedResolutions =  pEdid->GetEdidListedResolutionsList(displayID);

    if(pListedResolutions == NULL)
    {
        Printf(Tee::PriError, "Failed to read EDID while getting Display "
                "Device's  listed resolutions \n");
        return RC::ILWALID_EDID;
    }

    Edid::ListedRes::const_iterator pLwrRes    =  pListedResolutions->begin();

    pListedModes->clear();
    for ( ; pLwrRes != pListedResolutions->end(); pLwrRes++)
    {
        Display::Mode lwrrMode((UINT32)pLwrRes->width,
                               (UINT32)pLwrRes->height,
                               32,
                               (UINT32)pLwrRes->refresh);
        pListedModes->push_back(lwrrMode);
    }

    return rc;
}

// GetListedModesFromCPL()
// Calls into win32API to find the list of valid of modes for a given displayID
// It populate modeList with those modes. This is needed since MODS Edid library returns very few modes
RC DTIUtils::EDIDUtils::GetListedModesFromCPL(Display *pDisplay,DisplayID dispId, vector<Display::Mode> *pModeList)
{
    RC rc;
#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    bool                        topologyChanged      = false;
    bool                        ignorePreDispIDState = true;
    bool                        found                = false;
    UINT32                      lwrPathCount         = 0;
    UINT32                      thisEnum             = 0;
    UINT32                      outputId             = 0;
    DisplayIDs                  lwrrScanningDisplays;
    DisplayIDs                  getResForDispId(1,dispId);
    vector<UINT32>              heads;
    vector<Display::Mode>       modes;
    vector<Display::Mode>::iterator it;
    vector<Display::Mode>       listedModes;
    Display::Mode               smallestMode;
    INT32                       smallestModeIndex = -1;
    UINT64                      smallResFetchRate = (UINT64)-1;
    UINT64                      lwrrResFetchRate;

    LwAPI_Status                lwApiStatus;
    LwDisplayHandle             hLwDisplay;
    LwAPI_ShortString           displayName;
    LW_DISPLAYCONFIG_PATH_INFO *pLwrPathInfo         = NULL;

    heads.push_back(Display::ILWALID_HEAD);

    // Let's get the smallest resolution, so the setmode done is fast on emulation.
    // We can't hardcode resolution here, else Setmode would fail if EDID doesn't have that mode.
    rc = DTIUtils::EDIDUtils::GetListedModes(pDisplay,
                                     dispId,
                                     &listedModes,
                                     false);
    if (rc != OK)
    {
        Printf(Tee::PriHigh, "ERROR: %s: Failed to GetListedModes(0x%X). RC=%d.\n",
                            __FUNCTION__, (UINT32)dispId, (UINT32)rc);
        return rc;
    }

    memset(&smallestMode, 0, sizeof(smallestMode));

    for (UINT32 i = 0; i < (UINT32)listedModes.size(); i++)
    {
        // filter out all non 32 bit modes.
        if (32 != listedModes[i].depth)
        {
            continue;
        }

        lwrrResFetchRate = (UINT64)listedModes[i].width * listedModes[i].height *
                             listedModes[i].refreshRate * listedModes[i].depth;

        if (smallResFetchRate > lwrrResFetchRate)
        {
            smallResFetchRate = lwrrResFetchRate;
            smallestModeIndex = i;
        }
    }

    if (-1 != smallestModeIndex)
    {
        smallestMode = listedModes[smallestModeIndex];
    }

    modes.push_back(smallestMode);

    // Initialize LwApi
    CHECK_RC_CLEANUP(pDisplay->GetWin32Display()->InitializeLwApi());
    pDisplay->GetScanningOutDisplays(&lwrrScanningDisplays);
    if(! pDisplay->IsDispAvailInList(getResForDispId, lwrrScanningDisplays))
    {
        ignorePreDispIDState = pDisplay->GetWin32Display()->GetIgnorePrevModesetDispID();

        CHECK_RC_CLEANUP(pDisplay->GetWin32Display()->AllocateAndGetDisplayConfig(&lwrPathCount, (void **)&pLwrPathInfo));
        pDisplay->GetWin32Display()->SetIgnorePrevModesetDispID(true);
        CHECK_RC_CLEANUP(pDisplay->SetMode(getResForDispId,
                                   heads,
                                   modes,
                                   false,
                                   Display::STANDARD));
        topologyChanged = true;
    }

    // Let's first get the display handle corresponding to this displayMask
    while ((lwApiStatus = LwAPI_EnumLwidiaDisplayHandle(thisEnum, &hLwDisplay)) == LWAPI_OK)
    {
        if (LwAPI_GetAssociatedDisplayOutputId(hLwDisplay, &outputId)!= LWAPI_OK)
        {
            Printf(Tee::PriHigh,
                   "\n %s : LwAPI_GetAssociatedDisplayOutputId() failed \n",
                   __FUNCTION__);
            CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
        }

        if (outputId != (UINT32)dispId)
        {
            thisEnum++;
            continue;
        }

        // Get the display name for this handle
        if (LWAPI_OK !=  LwAPI_GetAssociatedLwidiaDisplayName(hLwDisplay, displayName))
        {
            Printf(Tee::PriHigh,
                   "\n %s : LwAPI_GetAssociatedLwidiaDisplayName() failed \n",
                   __FUNCTION__);
            CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
        }

        // Let's call the win32 API to get the list of valid modes
        char const *pDisplayName = displayName;
        UINT32      count        = 0;
        DEVMODE     dispMode     = {0};
        dispMode.dmSize = sizeof(DEVMODE);

        pModeList->clear();
        while (EnumDisplaySettingsEx(pDisplayName, count, &dispMode, 0))
        {
            Display::Mode mode;
            mode.width        = (unsigned)dispMode.dmPelsWidth;
            mode.height       = (unsigned)dispMode.dmPelsHeight;
            mode.depth        = (unsigned)dispMode.dmBitsPerPel;
            mode.refreshRate  = (unsigned)dispMode.dmDisplayFrequency;

            // Insert only unique modes
            if(!std::count(pModeList->begin(), pModeList->end(), mode))
                pModeList->push_back(mode);
            count++;
        }

        found = true;
        break;
    }

Cleanup:
    // Let's Restore display state
    if(topologyChanged)
    {
        lwApiStatus = LwAPI_DISP_SetDisplayConfig(lwrPathCount, pLwrPathInfo, 0);
        if (LWAPI_OK != lwApiStatus)
        {
            Printf(Tee::PriHigh,
                   "\n %s : LwAPI_DISP_SetDisplayConfig() failed \n",
                   __FUNCTION__);
            CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
        }
    }

    pDisplay->GetWin32Display()->SetIgnorePrevModesetDispID(ignorePreDispIDState);

    //Deallocate all the local object memory
    Win32Display::DeAllocateDisplayConfig(lwrPathCount, (void **)&pLwrPathInfo);

    if(!found)
        return RC::NO_DISPLAY_DEVICE_CONNECTED;
#endif
    return rc;
}

//!
//! \brief Checks if the EDID is valid. Needed since @ emulation and @ fmodel
//! we need to check if EDID is available for disp id. If not then test can SetLwstomEdid.
//!
//! \param pDisplay         [in]    Display pointer
//! \param dispId           [in]    DisplayID
//!
//! \return  if the EDID is valid then returns true else false.
bool DTIUtils::EDIDUtils::IsValidEdid (Display *pDisplay, DisplayID dispId)
{
    RC rc;
    UINT32       rawEdidSize    = 0;
    Edid        *pEdid          = pDisplay->GetEdidMember();
    UINT08      *pRawEdid       = NULL;

    // Ilwalidate EDID so we read it again and
    // not used cached one from the modsedid library
    pEdid->Ilwalidate(dispId);

    // First we need to get access to the raw EDID
    // to check if EDID is present
    rc = pEdid->GetRawEdid(dispId, NULL, &rawEdidSize);
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "%s() : Failed reading EDID for disp id 0x%X \n",
            __FUNCTION__, (UINT32)dispId);
        return false;
    }

    pRawEdid = (UINT08 *)new UINT08[rawEdidSize];
    memset(pRawEdid, 0, rawEdidSize);

    // This internally calls private method ReadEdid()
    // which does all validation for EDID so no need to do it explicitly
    rc = pEdid->GetRawEdid(dispId, pRawEdid, &rawEdidSize);
    delete [] pRawEdid;
    pRawEdid = NULL;
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "%s() : Failed to GetRawEdid for disp id 0x%X \n",
            __FUNCTION__, (UINT32)dispId);
        return false;
    }

    return true;
}

//!
//! \brief Checks if the EDID has listed resolutions
//!
//! \param pDisplay         [in]    Display pointer
//! \param dispId           [in]    DisplayID
//!
//! \return  if the EDID is valid then returns true else false.
bool DTIUtils::EDIDUtils::EdidHasResolutions(Display *pDisplay, DisplayID dispId)
{
    Edid        *pEdid          = pDisplay->GetEdidMember();

    // Let's check if the EDID has any resolutions in it or not.
    const Edid::ListedRes  *pListedResolutions =  pEdid->GetEdidListedResolutionsList(dispId);
    if (!pListedResolutions)
    {
        Printf(Tee::PriDebug, "%s(): Coudnt get resolutions from EDID for display id 0x%X.",
            __FUNCTION__, (UINT32)dispId);
        return false;
    }

    if (!pListedResolutions->size())
    {
        Printf(Tee::PriDebug, "%s(): EDID for display id 0x%X has no modes defined in it.",
            __FUNCTION__, (UINT32)dispId);
        return false;
    }

    return true;
}

//! function CreateDispIdType1ExtBlock()
//!     This function creates EDID Extension block using Display ID v1.3 spec.
//!     This Extension block is needed to create EDID with modes 65k x 65k.
//! params pDisplay               [in] : Pointer to display class.
//! params referenceDispId        [in] : Pass the Fake disp Id to be used as reference for generating EDID.
//!     Note: We assume the dispId passed has the edid "referenceEdidFileName_DFP" faked already.
//!      Not doing any re-faking here so as to avoid additional modesets [to speed up for emu].
//! params mode                   [in] : Mode for which extension block structure is to be created.
//! params dispIdExtBlockType1    [in] : Display ID v1.3 Type 1 extension block.
RC  DTIUtils::EDIDUtils::CreateDispIdType1ExtBlock(Display *pDisplay,
            DisplayID               referenceDispId,
            Display::Mode           &mode,
            DispIdExtBlockType1     &dispIdExtBlockType1,
            LWT_TIMING              *pLwtTiming)
{
    RC          rc;
    DisplayID   fakedDispId;
    DisplayIDs  detectedDisplays;
    UINT32 flag = 0;
    LWT_TIMING timing = {0};
    UINT32 hblank = 0;
    UINT32 vblank = 0;
    UINT32 pclk10KHzMinus1;

    // Step 1) Let's check if the passed params are valid.
    if(Utility::CountBits(referenceDispId) != 1)
    {
        Printf(Tee::PriHigh, "ERROR: %s: Only one display can be selected.\n",
            __FUNCTION__);
        CHECK_RC_CLEANUP(RC::ILWALID_DISPLAY_MASK);
    }

    CHECK_RC_CLEANUP(pDisplay->GetDetected(&detectedDisplays));
    if (!pDisplay->IsDispAvailInList(referenceDispId, detectedDisplays))
    {
        Printf(Tee::PriHigh, "WARNING: %s: The display must be a subset of the detected displays.\n",
            __FUNCTION__);
        CHECK_RC_CLEANUP(RC::ILWALID_DISPLAY_MASK);
    }
    fakedDispId     = referenceDispId;

    if (pLwtTiming)
    {
        timing = *pLwtTiming;
    }
    else
    {
        // Get Timings.
        // As per marketing all modes are to be tested @ CVT_RB.
        memset(&timing, 0 , sizeof(LWT_TIMING));
        rc = ModesetUtils::CallwlateLWTTiming(pDisplay,
                    fakedDispId,
                    mode.width,
                    mode.height,
                    mode.refreshRate,
                    Display::CVT_RB,
                    &timing,
                    flag);

        if (rc != OK)
        {
            Printf(Tee::PriHigh, "ERROR: %s: CallwlateLWTTiming() failed to get custom timing. RC = %d\n",
                __FUNCTION__, (UINT32)rc);
            CHECK_RC_CLEANUP(rc);
        }

        Printf(Tee::PriHigh, "\n\n----------------- Resolution = -----------------------\n");
        Printf(Tee::PriHigh, "   Width        = %u\n", mode.width);
        Printf(Tee::PriHigh, "   Height       = %u\n", mode.height);
        Printf(Tee::PriHigh, "   Refresh Rate = %u\n", mode.refreshRate);
        Printf(Tee::PriHigh, "   Timing Type  = Display::CVT_RB\n");

        ModesetUtils::PrintLwtTiming(&timing, Tee::PriHigh);
    }

    hblank = timing.HTotal - timing.HVisible;
    vblank = timing.VTotal - timing.VVisible;

    // Bytes 0,1,2      : Pclk / 10000. 0.01 - 167,772.16 [000000h - FFFFFFh]
    pclk10KHzMinus1 = timing.pclk - 1;
    dispIdExtBlockType1.PClk10KhzLow    = (UINT08) (pclk10KHzMinus1 & 0xFF);
    dispIdExtBlockType1.PClk10KhzMid    = (UINT08) ((pclk10KHzMinus1 >> 8) & 0xFF);
    dispIdExtBlockType1.PClk10KhzHigh   = (UINT08) ((pclk10KHzMinus1 >> 16) & 0xFF);

    // bit  7   : PREFERRED 'Detailed' Timing selector. : Not setting this as preferred timing.
    // bits 6-5 : 3D Stereo Support : no stereo
    // bit  4   : Interface Frame Scanning Type : Progressive Scan Frame
    // bits 3-0 : Aspect Ratio : 16:9
    dispIdExtBlockType1.TimingFlags = (UINT08) 0x40;

    // Byte 4,5         : Horizontal Active Image Pixels. 1 - 65,536 Pixels [0000h - FFFFh]
    dispIdExtBlockType1.HActiveLow = (UINT08) ((timing.HVisible - 1) & 0xFF);
    dispIdExtBlockType1.HActiveHigh = (UINT08) (((timing.HVisible - 1) >> 8) & 0xFF);

    // Byte 6,7         : Horizontal Blank Pixels. 1 - 65,536 Pixels [0000h - FFFFh]
    dispIdExtBlockType1.HBlankLow = (UINT08) ((hblank - 1) & 0xFF);
    dispIdExtBlockType1.HBlankHigh = (UINT08) (((hblank - 1) >> 8) & 0xFF);

    // Byte 8,9         :
    // High bits 14 - 8 : HFrontPorchHigh
    // Bit 15           : Horizontal Sync Polarity. [0 = -ve; 1= +ve]
    dispIdExtBlockType1.HFrontPorchLow = (UINT08) ((timing.HFrontPorch  - 1) & 0xFF);
    dispIdExtBlockType1.HFrontPorchHighAndPolarity = (UINT08) (((timing.HFrontPorch  - 1) >> 8) & 0xFF);

    // Setting +ve HSync Polarity
    dispIdExtBlockType1.HFrontPorchHighAndPolarity |= 0x80;

    // Byte 10,11       : Horizontal Sync Width: 1 - 65,536 Pixels [0000h - FFFFh]
    dispIdExtBlockType1.HSyncWidthLow = (UINT08) ((timing.HSyncWidth - 1) & 0xFF);
    dispIdExtBlockType1.HSyncWidthHigh = (UINT08) (((timing.HSyncWidth - 1) >> 8) & 0xFF);

    // Byte 12,13       : Vertical Active Image Pixels. 1 - 65,536 Pixels [0000h - FFFFh]
    dispIdExtBlockType1.VActiveLow = (UINT08) ((timing.VVisible - 1) & 0xFF);
    dispIdExtBlockType1.VActiveHigh = (UINT08) (((timing.VVisible - 1) >> 8) & 0xFF);

    // Byte 14,15       : Vertical Blank Pixels. 1 - 65,536 Pixels [0000h - FFFFh]
    dispIdExtBlockType1.VBlankLow = (UINT08) ((vblank - 1) & 0xFF);
    dispIdExtBlockType1.VBlankHigh = (UINT08) (((vblank - 1) >> 8) & 0xFF);

    // Byte 16,17       :
    // Bits 14 - 8      : VFrontPorchHigh
    // Bit 15           : Vertical Sync Polarity. [0 = -ve; 1= +ve]
    dispIdExtBlockType1.VFrontPorchLow = (UINT08) ((timing.VFrontPorch - 1) & 0xFF);
    dispIdExtBlockType1.VFrontPorchHighAndPolarity = (UINT08) (((timing.VFrontPorch - 1) >> 8) & 0xFF);

    // Setting -ve VSync Polarity
    dispIdExtBlockType1.VFrontPorchHighAndPolarity &= 0x7F;

    // Byte 18,19       : Vertical Sync Width: 1 - 65,536 Pixels [0000h - FFFFh]
    dispIdExtBlockType1.VSyncWidthLow = (UINT08) ((timing.VSyncWidth - 1) & 0xFF);
    dispIdExtBlockType1.VSyncWidthHigh = (UINT08) (((timing.VSyncWidth - 1) >> 8) & 0xFF);

Cleanup:

    return rc;
}

//! function CreateDTDExtBlock()
//!     This function creates EDID DTD Extension block.
//!     This Extension block is needed to create EDID with modes upto 4k x 4k.
//! params pDisplay               [in] : Pointer to display class.
//! params referenceDispId        [in] : Pass the Fake disp Id to be used as reference for generating EDID.
//!     Note: We assume the dispId passed has the edid "referenceEdidFileName_DFP" faked already.
//!      Not doing any re-faking here so as to avoid additional modesets [to speed up for emu].
//! params mode                   [in] : Mode for which extension block structure is to be created.
//! params dtdExtBlock            [out]: EDID DTD extension block.
//! params pLwtTiming             [in] : LWT Timing, to be added to the DTD block
RC  DTIUtils::EDIDUtils::CreateDTDExtBlock(Display *pDisplay,
            DisplayID             referenceDispId,
            Display::Mode         &mode,
            DTDBlock              &dtdExtBlock,
            LWT_TIMING            *pLwtTiming)
{
    RC          rc;
    DisplayID   fakedDispId;
    DisplayIDs  detectedDisplays;
    UINT32 flag = 0;
    LWT_TIMING timing = {0};
    UINT32 hblank = 0;
    UINT32 vblank = 0;
    UINT32 hVisibleInMM = 0;
    UINT32 vVisibleInMM = 0;
    float  mmPerInch        = 2.54f * 10.0f;
    UINT32 pixelsPerInch    = 96;

    // Step 1) Let's check if the passed params are valid.
    if(Utility::CountBits(referenceDispId) != 1)
    {
        Printf(Tee::PriHigh, "ERROR: %s: Only one display can be selected.\n",
            __FUNCTION__);
        CHECK_RC_CLEANUP(RC::ILWALID_DISPLAY_MASK);
    }

    CHECK_RC_CLEANUP(pDisplay->GetDetected(&detectedDisplays));
    if (!pDisplay->IsDispAvailInList(referenceDispId, detectedDisplays))
    {
        Printf(Tee::PriHigh, "WARNING: %s: The display must be a subset of the detected displays.\n",
            __FUNCTION__);
        CHECK_RC_CLEANUP(RC::ILWALID_DISPLAY_MASK);
    }
    fakedDispId     = referenceDispId;

    if (pLwtTiming)
    {
        timing = *pLwtTiming;
    }
    else
    {
        // Get Timings.
        // As per marketing all modes are to be tested @ CVT_RB.
        memset(&timing, 0 , sizeof(LWT_TIMING));
        rc = ModesetUtils::CallwlateLWTTiming(pDisplay,
                    fakedDispId,
                    mode.width,
                    mode.height,
                    mode.refreshRate,
                    Display::CVT_RB,
                    &timing,
                    flag);

        if (rc != OK)
        {
            Printf(Tee::PriHigh, "ERROR: %s: CallwlateLWTTiming() failed to get custom timing. RC = %d\n",
                __FUNCTION__, (UINT32)rc);
            CHECK_RC_CLEANUP(rc);
        }

        Printf(Tee::PriHigh, "\n\n----------------- Resolution = -----------------------\n");
        Printf(Tee::PriHigh, "   Width        = %u\n", mode.width);
        Printf(Tee::PriHigh, "   Height       = %u\n", mode.height);
        Printf(Tee::PriHigh, "   Refresh Rate = %u\n", mode.refreshRate);
        Printf(Tee::PriHigh, "   Timing Type  = Display::CVT_RB\n");

        ModesetUtils::PrintLwtTiming(&timing, Tee::PriHigh);
    }

    hblank = timing.HTotal - timing.HVisible;
    vblank = timing.VTotal - timing.VVisible;

    // Add to DTD blocks.
    // byte 0,1: Storing the pixel clock.
    dtdExtBlock.PClk10KhzLow       = (UINT08) (timing.pclk & 0xFF);
    dtdExtBlock.PClk10KhzHigh      = (UINT08) ((timing.pclk >> 8 ) & 0xFF);

    // byte 2: Horizontal Addressable Video in pixels --- contains lower 8 bits
    dtdExtBlock.HActiveLow         = (UINT08) (timing.HVisible & 0xFF);

    // byte 3: Horizontal Blanking in pixels --- contains lower 8 bits
    dtdExtBlock.HBlankLow          = (UINT08) (hblank & 0xFF);

    // byte 4: Horizontal Addressable Video in pixels - stored in Upper Nibble: contains upper 4 bits
    //          Horizontal Blanking in pixels          - stored in Lower Nibble: contains upper 4 bits
    dtdExtBlock.HActiveHighAndHBlankHigh   = (UINT08) (((timing.HVisible & 0xF00) >> 4) |
        (hblank & 0xF00) >> 8);

    // byte 5: Vertical Addressable Video in lines --- contains lower 8 bits
    dtdExtBlock.VActiveLow         = (UINT08) (timing.VVisible & 0xFF);

    // byte 6: Vertical Blanking in lines --- contains lower 8 bits
    dtdExtBlock.VBlankLow          = (UINT08) (vblank & 0xFF);

    // byte 7: Vertical Addressable Video in lines -- stored in Upper Nibble: contains upper 4 bits
    //          Vertical Blanking in lines --- stored in Lower Nibble: contains upper 4 bits
    dtdExtBlock.VActiveHighAndVBlankHigh = (UINT08) (((timing.VVisible & 0xF00) >> 4) |
        (vblank & 0xF00) >> 8);

    // byte 8: Horizontal Front Porch in pixels --- contains lower 8 bits
    dtdExtBlock.HFrontPorchLow     = (UINT08) (timing.HFrontPorch & 0xFF);

    // byte 9: Horizontal Sync Pulse Width in pixels --- contains lower 8 bits
    dtdExtBlock.HSyncWidthLow      = (UINT08) (timing.HSyncWidth & 0xFF);

    //byte 10: Vertical Front Porch in Lines --- stored in Upper Nibble: contains lower 4 bits
    //          Vertical Sync Pulse Width in Lines --- stored in Lower Nibble: contains lower 4 bits
    dtdExtBlock.VFrontPorchHighAndVSyncWidthHigh   = (UINT08) (((timing.VFrontPorch & 0xF) << 4) |
        (timing.VSyncWidth & 0xF));

    // byte 11: n n _ _ _ _ _ _ Horizontal Front Porch in pixels      --- contains upper 2 bits
    //           _ _ n n _ _ _ _ Horizontal Sync Pulse Width in Pixels --- contains upper 2 bits
    //           _ _ _ _ n n _ _ Vertical Front Porch in lines         --- contains upper 2 bits
    //           _ _ _ _ _ _ n n Vertical Sync Pulse Width in lines    --- contains upper 2 bits
    dtdExtBlock.FrontPorchSyncWidthHV = (UINT08) ( ((timing.HFrontPorch & 0x300) >> 2) |
        ((timing.HSyncWidth  & 0x300) >> 4) |
        ((timing.VFrontPorch & 0x30)  >> 2) |
        ((timing.VSyncWidth  & 0x30)  >> 4));

    // size in mm = pixels * mmPerInch / pixelsPerInch;  //assuming 96 DPI monitor
    // byte 12: Horizontal Addressable Video Image Size in mm --- contains lower 8 bits
    hVisibleInMM                      =  (UINT32)((float)(timing.HVisible) * mmPerInch / pixelsPerInch);
    dtdExtBlock.HVisibleInMMLow       = (UINT08) (hVisibleInMM & 0xFF);

    // byte 13: Vertical Addressable Video Image Size in mm --- contains lower 8 bits
    vVisibleInMM                      =  (UINT32)((float)(timing.VVisible) * mmPerInch / pixelsPerInch);
    dtdExtBlock.VVisibleInMMLow       = (UINT08) (vVisibleInMM & 0xFF);

    // byte 14: Horizontal Addressable Video Image Size in mm --- stored in Upper Nibble: contains upper 4 bits
    //           Vertical Addressable Video Image Size in mm --- stored in Lower Nibble: contains upper 4 bits
    dtdExtBlock.HVActiveHigh          = (UINT08) (((hVisibleInMM & 0xF00) >> 4) |
        ((vVisibleInMM & 0xF00) >> 8));

    // byte 15: Horizontal Border in pixels
    dtdExtBlock.HBorder               = (UINT08) (timing.HBorder & 0xFF);

    // byte 16: Vertical Border in pixels
    dtdExtBlock.VBorder               = (UINT08) (timing.VBorder & 0xFF);

    // byte 17:  TODO: Add byte 17 adter consulting with hw on appropriate value for it.
    // Bit 7, Desc: Signal Interface Type. 0 = Non interlaced.
    // Bit 6 5 0, Desc: Stereo Viewing Support
    // Bit 6 5 0, Value 0 0 x, Desc: Normal Display - No Stereo. The value of bit 0 is "don't care"
    // Bit 4 3 2 1, Desc: Analog Sync Signal Definition
    // Bit 4 3 2 1, Value 1 1 _ _, Desc: Digital Separate Sync
    dtdExtBlock.TimingFlags           = (UINT08) 0x18;

Cleanup:

    return rc;
}

//! function CreateEdidFileWithModes()
//!     This function creates EDID file with the modes provided in argument populated in DTD blocks.
//! params pDisplay               [in] : Pointer to display class.
//! params referenceDispId        [in] : Pass the Fake disp Id to be uised as reference for generating Edid.
//!     Note: We assume the dispId passed has the edid "referenceEdidFileName_DFP" faked already.
//!     Not doing any re-faking here so as to avoid additional modesets [to speed up for emu].
//! params modeSettings           [in] : Modes needed for this config.
//! params referenceEdidFileName_DFP [in] : EDID file to be used as base EDID file. Note: This file should be EDID ver1.4.
//! params newEdidFileName        [in] : Filename of new EDID file to be generated.
//! params pErrorStr              [in] : Char buffer where error string is to be passed back.
RC  DTIUtils::EDIDUtils::CreateEdidFileWithModes(Display  *pDisplay,
                                     DisplayID             referenceDispId,
                                     vector<Display::Mode>  &modeSettings,
                                     string                referenceEdidFileName_DFP,
                                     string                newEdidFileName,
                                     char                 *pErrorStr)
{
    RC          rc;

    string      edidFileName   = "";
    UINT32      index           = 0;
    DisplayID   fakedDispId;
    UINT08      *rawEdid = NULL;
    UINT32      edidSize = 0;
    FILE        *fpEDIDFile = NULL;
    UINT32      dtdBlockStart[] = {54,72,90,108};
    UINT32      dtdBlockIndex = 0;
    UINT32      dispIdExtnBlockIndex = 0;
    DisplayIDs  detectedDisplays;
    UINT08      runningSum  = 0;
    UINT08      checksum    = 0;
    char        buffer[32];

    if (!pErrorStr)
    {
        Printf(Tee::PriError, "%s: Error str pointer cannot be NULL.\n",
            __FUNCTION__);
        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
    }

    // Step 1) Let's check if the passed params are valid.
    if(Utility::CountBits(referenceDispId) != 1)
    {
        Printf(Tee::PriHigh, "ERROR: %s: Only one display can be selected.\n",
            __FUNCTION__);
        CHECK_RC_CLEANUP(RC::ILWALID_DISPLAY_MASK);
    }

    CHECK_RC_CLEANUP(pDisplay->GetDetected(&detectedDisplays));
    if (!pDisplay->IsDispAvailInList(referenceDispId, detectedDisplays))
    {
        Printf(Tee::PriHigh, "WARNING: %s: The display must be a subset of the detected displays.\n",
            __FUNCTION__);
        CHECK_RC_CLEANUP(RC::ILWALID_DISPLAY_MASK);
    }
    fakedDispId     = referenceDispId;

    // TODO: Add support for more then 4 heads.
    if (modeSettings.size() > 4)
    {
        sprintf(pErrorStr,"ERROR: %s: AutoGenerate edid is NOT supported for more then 4 heads due to 4 DTD limitation.\n",
            __FUNCTION__);
        Printf(Tee::PriHigh, "%s", pErrorStr);
        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
    }


    if (newEdidFileName == "")
    {
        sprintf(pErrorStr,"ERROR: %s: New EDID Filename ptr cannot be NULL.\n",
            __FUNCTION__);
        Printf(Tee::PriHigh, "%s", pErrorStr);
        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
    }

    edidFileName   += string("edids/");
    edidFileName   += DTIUtils::Mislwtils::trim(referenceEdidFileName_DFP);

    // Step 2) create new edid filename.
    fpEDIDFile = fopen((newEdidFileName).c_str(), "w");
    if (fpEDIDFile == NULL)
    {
        sprintf(pErrorStr,"ERROR: %s: Failed to open File %s in 'w' mode\n",
            __FUNCTION__, newEdidFileName.c_str());
        Printf(Tee::PriHigh, "%s", pErrorStr);
        CHECK_RC_CLEANUP(RC::CANNOT_OPEN_FILE);
    }

    // Let's get this EDID into the buffer as we need to manipulate it later.
    rc = DTIUtils::EDIDUtils::GetEdidBufFromFile(edidFileName, rawEdid, &edidSize);
    if(rc != OK)
    {
        sprintf(pErrorStr, "ERROR: %s: GetEdidBufFromFile(%s) failed with RC =%d.",
            __FUNCTION__, edidFileName.c_str(), (UINT32)rc);
        Printf(Tee::PriHigh, "%s", pErrorStr);
        CHECK_RC_CLEANUP(rc);
    }

    // Step 3) Set the Manufacturer ID = "LWD" and Product Code/Serial num (Bytes 8~15) to 0xFF to avoid
    // any special monitor treatment in DD side.
    rawEdid[8] = 0x3A;
    rawEdid[9] = 0xC4;
    for (index = 10; index < 16; index++)
    {
        rawEdid[index] = 0xFF;
    }

    // Also reset all DTD blocks as DTD blocks might have Monitor identifier
    // which causes issue due to special monitor treating by DD side [Bug 1062179.]
    for (index = 0; index < 4; index++)
    {
        for (UINT32 dtdByte = 0; dtdByte < 18; dtdByte++)
        {
            rawEdid[dtdBlockStart[index] + dtdByte] = 0;
        }
    }

    // Step 4) Create EDID file: Callwlate custom timings that suits
    // the requested mode settings and update EDID buffer.
    for (index = 0; index < modeSettings.size(); index++)
    {
        UINT32 flag = 0;
        LWT_TIMING timing = {0};
        DispIdExtBlockType1 dispIdExtBlockType1 = {0};
        DTDBlock dtdExtBlock = {0};
        UINT32 destOffset = 0;

        const UINT16 BASE_EDID_BLOCK_SIZE = 128;
        const UINT16 DISP_ID_BLOCK_START = 8;

        // Get Timings.
        // As per marketing all modes are to be tested @ CVT_RB.
        // TODO: Add support for testing other display timings [other then CVT_RB] in separate CL.
        memset(&timing, 0 , sizeof(LWT_TIMING));
        rc = ModesetUtils::CallwlateLWTTiming(pDisplay,
            fakedDispId,
            modeSettings[index].width,
            modeSettings[index].height,
            modeSettings[index].refreshRate,
            Display::CVT_RB,
            &timing,
            flag);

        if (rc != OK)
        {
            sprintf(pErrorStr, "ERROR: %s: GetTiming() failed to get custom timing. RC = %d\n",
                __FUNCTION__, (UINT32)rc);
            Printf(Tee::PriHigh, "%s", pErrorStr);
            CHECK_RC_CLEANUP(rc);
        }

        // Add the logic here to insert to the EDID DTD blocks.
        Printf(Tee::PriHigh, "\n\n----------------- Resolution = %d -----------------------\n", index);
        Printf(Tee::PriHigh, "   Width        = %u\n", modeSettings[index].width);
        Printf(Tee::PriHigh, "   Height       = %u\n", modeSettings[index].height);
        Printf(Tee::PriHigh, "   Refresh Rate = %u\n", modeSettings[index].refreshRate);
        Printf(Tee::PriHigh, "   Timing Type  = Display::CVT_RB\n");

        ModesetUtils::PrintLwtTiming(&timing, Tee::PriHigh);

        //
        // The original DTD Extension Block does not support modes with HTotal
        // or VTotal larger than 4K, or modes with pclk larger than 0xFFFF.  If
        // we have one of these modes, we need to use a DisplayID Type 1
        // Extension Block.  Also, the base block has room for only four DTD
        // blocks, so if we have already filled these blocks, we have to add
        // any additional modes to the DisplayID Extension Block.
        //
        if ((timing.HTotal > 4095 || timing.VTotal > 4095) ||
            (timing.pclk > 0xFFFF) ||
            dtdBlockIndex >= 4)
        {
            if (dispIdExtnBlockIndex >= 4)
            {
                sprintf(pErrorStr,"\nERROR: %s: AutoGenerateEdid supports adding only 8 custom modes, with limitation of only 4 modes larger than 4kx4k.\n",
                    __FUNCTION__);
                Printf(Tee::PriHigh, "%s", pErrorStr);
                CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
            }

            rc = CreateDispIdType1ExtBlock(pDisplay,
                    fakedDispId,
                    modeSettings[index],
                    dispIdExtBlockType1,
                    &timing);
            if (rc != OK)
            {
                Printf(Tee::PriHigh, "ERROR: %s: Failed to CreateDispIdType1ExtBlock(0x%X) for %d x %d, %d bit, %d Hz. RC=%d.\n",
                                    __FUNCTION__, (UINT32)fakedDispId,
                                    modeSettings[index].width, modeSettings[index].height,
                                    modeSettings[index].depth, modeSettings[index].refreshRate,
                                    (UINT32)rc);
                return rc;
            }

            destOffset = BASE_EDID_BLOCK_SIZE + DISP_ID_BLOCK_START + dispIdExtnBlockIndex * sizeof(dispIdExtBlockType1);
            memcpy(rawEdid + destOffset, &dispIdExtBlockType1, sizeof(dispIdExtBlockType1));

            // Let's increments Display Id Extn Block index, so we add next block to new block.
            dispIdExtnBlockIndex++;
        }
        else
        {
            rc = CreateDTDExtBlock(pDisplay,
                    fakedDispId,
                    modeSettings[index],
                    dtdExtBlock,
                    &timing);

            if (rc != OK)
            {
                Printf(Tee::PriHigh, "ERROR: %s: Failed to CreateDTDExtBlock(0x%X) for %d x %d, %d bit, %d Hz. RC=%d.\n",
                                    __FUNCTION__, (UINT32)fakedDispId,
                                    modeSettings[index].width, modeSettings[index].height,
                                    modeSettings[index].depth, modeSettings[index].refreshRate,
                                    (UINT32)rc);
                return rc;
            }
            UINT32 DTDStartOffset = dtdBlockStart[dtdBlockIndex];
            memcpy(rawEdid + DTDStartOffset, (char *)&dtdExtBlock, sizeof(dtdExtBlock));

            // Let's increments dtd index, so we add next block to new DTD block
            dtdBlockIndex++;
        }
    }

    // Step 5) write the edid to the file.
    runningSum  = 0;
    checksum    = 0;

    for (UINT32 edidByte = 0; edidByte < edidSize; edidByte++)
    {
        // Let's compute the checksum here. Every 128 byte is checksum.
        if ((edidByte + 1) % 128 == 0)
        {
            checksum = ((UINT32)(0x100) - runningSum) % 0x100;

            sprintf(pErrorStr,  "INFO: %s: EDID[%d] => Previous Checksum = (0x%X). New Checksum = (0x%X).\n",
                __FUNCTION__, (UINT32)edidByte, (UINT32)rawEdid[edidByte], (UINT32)checksum);
            Printf(Tee::PriHigh, "%s", pErrorStr);

            rawEdid[edidByte] = checksum;
            runningSum = 0;
        }
        else
        {
            runningSum = (UINT08)(((UINT32)runningSum + rawEdid[edidByte]) % 0x100);
        }

        if (edidByte % 16 == 0 && edidByte)
        {
            DTIUtils::ReportUtils::write2File(fpEDIDFile, string("\n"));
        }

        sprintf(buffer, "%02X ", (UINT32) rawEdid[edidByte]);
        rc = DTIUtils::ReportUtils::write2File(fpEDIDFile, string(buffer));
        if(rc != OK)
        {
            sprintf(pErrorStr,  "ERROR: %s: New EDID write2File failed while writing byte (0x%X) with RC =%d.\n",
                __FUNCTION__, (UINT32)edidByte, (UINT32)rc);
            Printf(Tee::PriHigh, "%s", pErrorStr);
            CHECK_RC_CLEANUP(rc);
        }
    }

Cleanup:
    if(rawEdid)
    {
        delete[] rawEdid;
        rawEdid = NULL;
    }

    if (fpEDIDFile)
    {
        fclose(fpEDIDFile);
        fpEDIDFile = NULL ;
    }

    return rc;
}

DTIUtils::ImageUtils::ImageUtils()
{
    ImageWidth = 0;
    ImageHeight = 0;
    ImageName = "";
    format = IMAGE_NONE;
}

DTIUtils::ImageUtils::ImageUtils(UINT32 width,UINT32 height)
{
    this->ImageWidth = width;
    this->ImageHeight = height;
    ImageUtils::UpdateImageArray(this);
}

string DTIUtils::ImageUtils::GetImageName()
{
    return ImageName;
}

UINT32 DTIUtils::ImageUtils::GetImageWidth()
{
    return ImageWidth;
}

UINT32 DTIUtils::ImageUtils::GetImageHeight()
{
    return ImageHeight;
}

DTIUtils::ImageUtils DTIUtils::ImageUtils::SelectImage(UINT32 width,UINT32 height)
{
    DTIUtils::ImageUtils imgArr;
    imgArr.ImageWidth = width;
    imgArr.ImageHeight = height;
    UpdateImageArray(&imgArr);
    return imgArr;
}

DTIUtils::ImageUtils DTIUtils::ImageUtils::SelectImage(ImageUtils *reqImg)
{
    DTIUtils::ImageUtils imgArr;
    imgArr.ImageWidth = reqImg->ImageWidth;
    imgArr.ImageHeight = reqImg->ImageHeight;
    imgArr.format = reqImg->format;
    UpdateImageArray(&imgArr);
    return imgArr;
}

void DTIUtils::ImageUtils::UpdateImageArray(ImageUtils *imgArr)
{
    char FileName[256];
    UINT32 width = imgArr->ImageWidth;
    UINT32 height = imgArr->ImageHeight;
    vector<string> PathDirectories;
    PathDirectories.push_back(".");
    Utility::AppendElwSearchPaths(&PathDirectories, "LD_LIBRARY_PATH");
    Utility::AppendElwSearchPaths(&PathDirectories, "MODS_RUNSPACE");
    
    switch (imgArr->format)
    {
        case IMAGE_HDR:
            sprintf(FileName,"dispinfradata/images/image%dx%d.hdr",width,height);
            break;
        case IMAGE_RAW:
            sprintf(FileName,"dispinfradata/images/image%dx%d.raw",width,height);
            break;
        case IMAGE_PNG:
        default:
            sprintf(FileName,"dispinfradata/images/image%dx%d.png",width,height);
    }

    string FileNameFromPath = Utility::FindFile(FileName, PathDirectories);

    if (FileNameFromPath.size() == 0)
    {
        switch (imgArr->format)
        {
            case IMAGE_HDR:
                Printf(Tee::PriHigh," \nWARNING: No Image of name %dx%d exist exist."
                                    "Selecting 800x600.hdr image.",width,height);
                sprintf(FileName, "dispinfradata/images/image800x600.hdr");
                width = 800;
                height = 600;
                break;
            case IMAGE_RAW:
                sprintf(FileName,"dispinfradata/images/image%dx%d.raw",width,height);
                break;
            case IMAGE_PNG:
            default:
                Printf(Tee::PriHigh," \nWARNING: No Image of size %dx%d exist."
                                    " Selecting 640x480 image.",width,height);
                sprintf(FileName, "dispinfradata/images/image640x480.PNG");
                width = 640;
                height = 480;
        }
    }
    imgArr->ImageWidth  = width;
    imgArr->ImageHeight = height;
    imgArr->ImageName   = string(FileName);
}

std::string Mislwtils::trim_right(const std::string &source , const std::string& t)
{
    std::string str = source;
    string::size_type pos = str.find_last_not_of(t);

    if(pos != string::npos)
        return str.erase(str.find_last_not_of(t) + 1);
    else
        return str.erase(0, str.length() - 1);
}

std::string Mislwtils::trim_left( const std::string& source, const std::string& t)
{
    std::string str = source;
    string::size_type pos = str.find_first_not_of(t);;

    if(pos != string::npos)
        return str.erase(0, str.find_first_not_of(t));
    else
        return str.erase(0, str.length() - 1);
}

std::string Mislwtils::trim(const std::string& source, const std::string& t)
{
    std::string str = source;
    return trim_left( trim_right( str, t) , t );
}

//! \brief GetLogBase2 function.
//!
//! Returns the log to the base 2 of the given number
//!
//! \param nNum Number whose logarithm is to be computed
//! \return Base 2 Logarithm of the given number
//------------------------------------------------------------------------------
UINT32 Mislwtils::getLogBase2(UINT32 nNum)
{
    return (UINT32)ceil(log((double)nNum)/log(2.0));
}

RC DTIUtils::ReportUtils::write2File(FILE *fp, const string & data)
{
    RC rc;

    fprintf(fp,"%s", data.c_str());
    fflush(fp);
    return rc;
}

RC DTIUtils::ReportUtils::CreateCSVReportHeader(string &headerString)
{
    RC rc;
    headerString = "CombinationDetails,TimeStamp,HashKey,Protocol,DisplayId,Width,Height,Depth,RefreshRate,ChannelID,Head,ColorFormat,IMP_Status,SetMode_Status,Comment";

    return rc;
}

RC DTIUtils::ReportUtils::CreateCSVReportString(Display *pDisplay, vector<SetImageConfig> setImageConfig, ColorUtils::Format colorFormat,
                        bool bIMPpassed, bool bSetModeStatus, const string &comment, string &reportString)
{
    RC  rc;
    string lwrProtocol = "";
    char lwrrReportString[255];

    char lwrrTime[30] = "";
    GetLwrrTime("%Y-%m-%d-%H-%M-%S", lwrrTime, sizeof lwrrTime);

    for(UINT32 count = 0; count < setImageConfig.size(); count++)
    {
        //1 Get The protocol for this disp ID
        if (pDisplay->GetProtocolForDisplayId(setImageConfig[count].displayId, lwrProtocol) != OK)
        {
            Printf(Tee::PriHigh, "%s :Error in getting Protocol for DisplayId = 0x%08X\n",__FUNCTION__, (UINT32)setImageConfig[count].displayId);
            return RC::SOFTWARE_ERROR;
        }

        //2 Create Hash string [Protocol-DispID-width-Height-depth-RefreshRate]
        char HashKey[255];
        sprintf(HashKey,"%s-0x%08X-%d-%d-%d-%d",lwrProtocol.c_str(),
                                        (UINT32)setImageConfig[count].displayId,
                                        setImageConfig[count].mode.width,
                                        setImageConfig[count].mode.height,
                                        setImageConfig[count].mode.depth,
                                        setImageConfig[count].mode.refreshRate);

        //3 create Report String
        sprintf(lwrrReportString,",%s,%s,%s,0x%08X,%d,%d,%d,%d,%d,%d,%s,%s,%s,%s\n",lwrrTime,
                                        HashKey,
                                        lwrProtocol.c_str(),
                                        (UINT32)setImageConfig[count].displayId,
                                        setImageConfig[count].mode.width,
                                        setImageConfig[count].mode.height,
                                        setImageConfig[count].mode.depth,
                                        setImageConfig[count].mode.refreshRate,
                                        setImageConfig[count].channelId,
                                        setImageConfig[count].head,
                                        ColorUtils::FormatToString(colorFormat).c_str(),
                                        bIMPpassed ? "PASS" : "FAIL",
                                        bSetModeStatus ? "PASS" : "FAIL",
                                        comment.c_str());
        reportString += lwrrReportString;
    }

    return rc;
}

//------------------------------------------------------------------------------
//! GetLwrrTime
//! This method returns the current time formatted as per the format string passsed in timeFormat argument.
//------------------------------------------------------------------------------
RC DTIUtils::ReportUtils::GetLwrrTime(const char *timeFormat,char *lwrrTime, UINT32 lwrrTimeSize)
{
    RC rc;
    time_t now;
    struct tm *d;

    time(&now);
    d = localtime(&now);
    strftime(lwrrTime, lwrrTimeSize, timeFormat, d);
    return rc;
}

