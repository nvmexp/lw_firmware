/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
//      disp_crc.cpp - DispTest class - CRC related methods
//      Copyright (c) 1999-2005 by LWPU Corporation.
//      All rights reserved.
//
//      Originally part of "disp_test.cpp" - broken out 27 October 2005
//
//      Written by: Matt Craighead, Larry Coffey, et al
//      Date:       29 July 2004
//
//      Routines in this module:
//      CrcInterruptEventHandler        Helper function for logging display interrupts needed during initialization of the CRC Manager (in "CrcInitialize")
//      CrcInitialize                   Initialize the CRC Manager
//      CrcSetOwner                     Set the string to be used in the owner field of the CRC output files
//      CrcSetCheckTolerance            Set the tolerance when checking non-FModel generated lead.crc/gold.crc
//      CrcSetFModelCheckTolerance      Set the tolerance when checking FModel generated lead.crc/gold.crc
//      CrcAddVgaHead                   Turn on CRC Capturing for OR of or_type and or_number on Head which is in VGA mode
//      CrcAssignHead                   Capture the current logical and physical heads
//      CrcAddHead                      Attach the given notifier to act as the CRC notifier for the specified head
//      CrcNoCheckHead                  Set the ModeEnable flag to false for the head of interest. This make the crc checker skip the crc entries corresponding to this head
//      CrcAddFcodeHead                 Attach the given notifier to act as the CRC notifier for the specified head
//      CrcAddNotifier                  Add the notifier to the list of notifiers for which the test will generate CRCs
//      CrcAddSemaphore                 Add the semaphore to the list of semaphores for which the test will generate CRCs
//      CrcAddInterrupt                 Add the interrupt to the list of interrupts for which the test will generate CRCs
//      CrcAddInterruptAndCount         Add the interrupt to the list of interrupts for which the test will generate CRCs
//      CrcSetStartTime                 Read the AUDIT_TIMESTAMP register and store the current time as the test start time
//      SetLegacyCrc                    This functions does a write to CRA9 to send a SetLegacyCrc method to DSI
//      Crlwpdate                       Scan the CRC notifiers and semaphores for any change that should be logged as a CRC event; update the CRC event lists as appropriate
//      CrlwpdateInterrupt              Check if an interrupt that oclwred should be logged and update the CRC interrupt event list as appropriate
//      CrcWriteHeadList                Helper function to write out a comma-separated list of the head numbers
//      CrcWriteEventFiles              Write the contents of the CRC event lists to the CRC output files
//      CrcWriteHeadEventFile           Write the CRC output file for the head notifier events
//      CrcWriteNotifierEventFile       Write the CRC output file for the notifier events
//      CrcWriteSemaphoreEventFile      Write the CRC output file for the semaphore events
//      CrcWriteInterruptEventFile      Write the CRC output file for the interrupt events
//      CrcCleanup                      Deallocate memory used for CRC data structures
//      ModifyTestName                  Override test name
//      ModifySubtestName               Override subtest name
//      AppendSubtestName               Append to the subtest name
//
#include "Lwcm.h"
#include "lwcmrsvd.h"

#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "gpu/include/dispchan.h"
#include "core/include/color.h"
#include "core/include/utility.h"
#include "core/include/imagefil.h"
#include "random.h"
#include "lwos.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include <math.h>
#include <list>
#include <map>
#include <errno.h>

//#include "gpu/js/dispt_sr.h"
#include "core/utility/errloggr.h"

#include "lw50/dev_disp.h"

#include "class/cl507c.h"
#include "class/cl507d.h"
#include "class/cl507e.h"
#include "class/cl8270.h"
#include "class/cl827e.h"
#include "class/cl8370.h"
#include "class/cl857d.h"
#include "class/cl9070.h"
#include "class/cl907d.h"
#include "class/cl9170.h"
#include "class/cl917d.h"
#include "class/cl9270.h"
#include "class/cl927d.h"
#include "class/cl9470.h"
#include "class/cl9570.h"
#include "class/cl9770.h"
#include "class/cl9870.h"
#define DISPTEST_PRIVATE
#include "core/include/disp_test.h"

#ifdef USE_DTB_DISPLAY_CLASSES
#include "gpu/tests/disptest/display_01xx.h"
#include "gpu/tests/disptest/display_02xx.h"
#include "disptest/display_02xx_dtb.h"
#include "disptest/display_021x_dtb.h"
#include "disptest/display_022x_dtb.h"
#include "disptest/display_024x_dtb.h"
#include "disptest/display_025x_dtb.h"
#include "disptest/display_027x_dtb.h"
#include "disptest/display_028x_dtb.h"
#else
#include "gpu/tests/disptest/display_01xx.h"
#include "gpu/tests/disptest/display_02xx.h"
#include "gpu/tests/disptest/display_021x.h"
#include "gpu/tests/disptest/display_022x.h"
#include "gpu/tests/disptest/display_024x.h"
#include "gpu/tests/disptest/display_025x.h"
#include "gpu/tests/disptest/display_027x.h"
#include "gpu/tests/disptest/display_028x.h"
#endif

#include "gpu/include/vgacrc.h"
#include "core/include/cmdline.h"
#include "core/include/xp.h"

namespace DispTest
{
    // No local ("static") routines needed

    // No access to DispTest member data needed
}

/****************************************************************************************
 * DispTest::CrcInterruptEventHandler
 *
 *  Arguments Description:
 *  - address: address of interrupt register being cleared
 *  - value: data being written to clear the register
 *  - data: unused, kept for legacy purposes
 *
 *  Functional Description:
 *  - Helper function for logging display interrupts needed during initialization of the CRC Manager
 *
 *  Notes:
 *  - This function must be global because the Windows driver build depends on it
 ***************************************************************************************/
bool CrcInterruptEventHandler(UINT32 address, UINT32 value, void *data)
{
    DispTest::CrlwpdateInterrupt(address, value);
    return false;
}

/****************************************************************************************
 * DispTest::CrcInitialize
 *
 *  Arguments Description:
 *  - TestName:
 *  - SubTestName:
 *  - Owner:
 *
 *  Functional Description:
 *  - Initialize the CRC Manager
 ***************************************************************************************/
RC DispTest::CrcInitialize(string TestName, string SubTestName, string Owner)
{
    RC rc;

    /* Set the Crc Manager Initialized flag to True*/
    LwrrentDevice()->CrcIsInitialized = true;

    unsigned int dev_index = LwrrentDevice()->GetDeviceIndex();

    string file_prefix;
    char sIndex[8];

    sprintf(sIndex, "%d", dev_index);

    if (dev_index) {
        file_prefix = string(DISPTEST_CRC_DEFAULT_FILENAME_PREFIX) + sIndex;
    } else {
        file_prefix = string(DISPTEST_CRC_DEFAULT_FILENAME_PREFIX);
    }

    // initialize global crc parameters
    LwrrentDevice()->pCrcInfo                       = new struct crc_info;
    LwrrentDevice()->pCrcInfo->p_test_name          = new string(TestName);
    LwrrentDevice()->pCrcInfo->p_subtest_name       = new string(SubTestName);
    LwrrentDevice()->pCrcInfo->p_owner              = new string(Owner);
    LwrrentDevice()->pCrcInfo->tolerance            = DISPTEST_CRC_DEFAULT_TOLERANCE;
    LwrrentDevice()->pCrcInfo->tolerance_fmodel     = DISPTEST_CRC_DEFAULT_TOLERANCE_FMODEL;
    LwrrentDevice()->pCrcInfo->p_head_filename      = new string(file_prefix + DISPTEST_CRC_DEFAULT_HEAD_FILENAME_SUFFIX);
    LwrrentDevice()->pCrcInfo->p_notifier_filename  = new string(file_prefix + DISPTEST_CRC_DEFAULT_NOTIFIER_FILENAME_SUFFIX);
    LwrrentDevice()->pCrcInfo->p_semaphore_filename = new string(file_prefix + DISPTEST_CRC_DEFAULT_SEMAPHORE_FILENAME_SUFFIX);
    LwrrentDevice()->pCrcInfo->p_interrupt_filename = new string(file_prefix + DISPTEST_CRC_DEFAULT_INTERRUPT_FILENAME_SUFFIX);
    // initialize crc lists
    LwrrentDevice()->pCrcHeadList      = new list<struct crc_head_info*>();
    LwrrentDevice()->pCrcNotifierList  = new list<struct crc_notifier_info*>();
    LwrrentDevice()->pCrcSemaphoreList = new list<struct crc_semaphore_info*>();
    LwrrentDevice()->pCrcInterruptList = new list<struct crc_interrupt_info*>();
    // initialize crc lookup maps
    LwrrentDevice()->pCrcHeadMap      = new map< UINT32, struct crc_head_info* >();
    LwrrentDevice()->pCrcNotifierMap  = new map< LwRm::Handle, map<UINT32, struct crc_notifier_info*> * >();
    LwrrentDevice()->pCrcSemaphoreMap = new map< LwRm::Handle, map<UINT32, struct crc_semaphore_info*> * >();
    LwrrentDevice()->pCrcInterruptMap = new map< UINT32, struct crc_interrupt_info* >();
    // initialize crc event lists
    LwrrentDevice()->pCrcHeadEventList      = new list<struct crc_head_event*>();
    LwrrentDevice()->pCrcNotifierEventList  = new list<struct crc_notifier_event*>();
    LwrrentDevice()->pCrcSemaphoreEventList = new list<struct crc_semaphore_event*>();
    LwrrentDevice()->pCrcInterruptEventList = new list<struct crc_interrupt_event*>();
    // initialize crc object event list lookup maps
    LwrrentDevice()->pCrcHeadEventMap      = new map< struct crc_head_info*,      list<struct crc_head_event*> * >();
    LwrrentDevice()->pCrcNotifierEventMap  = new map< struct crc_notifier_info*,  list<struct crc_notifier_event*> * >();
    LwrrentDevice()->pCrcSemaphoreEventMap = new map< struct crc_semaphore_info*, list<struct crc_semaphore_event*> * >();
    LwrrentDevice()->pCrcInterruptEventMap = new map< struct crc_interrupt_info*, list<struct crc_interrupt_event*> * >();
    // initialize the display interrupt event logger
    GpuPtr pGpu;
    pGpu->RegisterDispIntrHook(&CrcInterruptEventHandler, NULL);

    //
    // allocate the display class if it isn't already
    UINT32 index = LwrrentDevice()->GetDeviceIndex();
    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        // TODO: when there are multiple displays we will need to do better than this
        // in terms of making sure we have allocated a display class in the right place

        // read the class register, which is alway in the same place for all classes
        UINT32 dispClass = DispTest::GetBoundGpuSubdevice()->RegRd32(LW_PDISP_CLASSES(0));

        Display * my_display = NULL;

        // we don't have a display class yet
        #ifdef USE_DTB_DISPLAY_CLASSES
        if(REF_VAL(LW_PDISP_CLASSES_CLASS_ID, dispClass) == 0x907D)
        {
            my_display = new Display_02xx_dtb(index);
            // In case that this is not initialized in DispTest::Initialize()
            // (Not called?) We setup the display class here.
            if (!LwrrentDisplayClass())
              LwrrentDevice()->unInitializedClass = LW9070_DISPLAY;
        }
        else if(REF_VAL(LW_PDISP_CLASSES_CLASS_ID, dispClass) == 0x917D)
        {
            my_display = new Display_021x_dtb(index);
            // In case that this is not initialized in DispTest::Initialize()
            // (Not called?) We setup the display class here.
            if (!LwrrentDisplayClass())
              LwrrentDevice()->unInitializedClass = LW9170_DISPLAY;
        }
        else if(REF_VAL(LW_PDISP_CLASSES_CLASS_ID, dispClass) == 0x927D)
        {
            my_display = new Display_022x_dtb(index);
            // In case that this is not initialized in DispTest::Initialize()
            // (Not called?) We setup the display class here.
            if (!LwrrentDisplayClass())
              LwrrentDevice()->unInitializedClass = LW9270_DISPLAY;
        }
        else if(REF_VAL(LW_PDISP_CLASSES_CLASS_ID, dispClass) == 0x947D)
        {
            my_display = new Display_024x_dtb(index);
            // In case that this is not initialized in DispTest::Initialize()
            // (Not called?) We setup the display class here.
            if (!LwrrentDisplayClass())
              LwrrentDevice()->unInitializedClass = LW9470_DISPLAY;
        }
        else if(REF_VAL(LW_PDISP_CLASSES_CLASS_ID, dispClass) == 0x957D)
        {
            my_display = new Display_025x_dtb(index);
            // In case that this is not initialized in DispTest::Initialize()
            // (Not called?) We setup the display class here.
            if (!LwrrentDisplayClass())
              LwrrentDevice()->unInitializedClass = LW9570_DISPLAY;
        }
        else if(REF_VAL(LW_PDISP_CLASSES_CLASS_ID, dispClass) == 0x977D)
        {
            my_display = new Display_027x_dtb(index);
            // In case that this is not initialized in DispTest::Initialize()
            // (Not called?) We setup the display class here.
            if (!LwrrentDisplayClass())
              LwrrentDevice()->unInitializedClass = LW9770_DISPLAY;
        }
        else if(REF_VAL(LW_PDISP_CLASSES_CLASS_ID, dispClass) == 0x987D)
        {
            my_display = new Display_028x_dtb(index);
            // In case that this is not initialized in DispTest::Initialize()
            // (Not called?) We setup the display class here.
            if (!LwrrentDisplayClass())
              LwrrentDevice()->unInitializedClass = LW9870_DISPLAY;
        }
        #else
        // otherwise, we need to figure out the right one to allocate

        // if the class is pre-907d (class020x), use the 01xx c++ class
        if(REF_VAL(LW_PDISP_CLASSES_CLASS_ID, dispClass) < 0x907D)
        {
            my_display = new Display_01xx(index);
            // In case that this is not initialized in DispTest::Initialize()
            // (Not called?) We setup the display class here.
            if (!LwrrentDisplayClass())
              LwrrentDevice()->unInitializedClass = LW50_DISPLAY;
        }
        else if(REF_VAL(LW_PDISP_CLASSES_CLASS_ID, dispClass) == 0x907D)
        {
            my_display = new Display_02xx(index);
            // In case that this is not initialized in DispTest::Initialize()
            // (Not called?) We setup the display class here.
            if (!LwrrentDisplayClass())
              LwrrentDevice()->unInitializedClass = LW9070_DISPLAY;
        }
        else if(REF_VAL(LW_PDISP_CLASSES_CLASS_ID, dispClass) == 0x917D)
        {
            my_display = new Display_021x(index);
            // In case that this is not initialized in DispTest::Initialize()
            // (Not called?) We setup the display class here.
            if (!LwrrentDisplayClass())
              LwrrentDevice()->unInitializedClass = LW9170_DISPLAY;
        }
        else if(REF_VAL(LW_PDISP_CLASSES_CLASS_ID, dispClass) == 0x927D)
        {
            my_display = new Display_022x(index);
            // In case that this is not initialized in DispTest::Initialize()
            // (Not called?) We setup the display class here.
            if (!LwrrentDisplayClass())
              LwrrentDevice()->unInitializedClass = LW9270_DISPLAY;
        }
        else if(REF_VAL(LW_PDISP_CLASSES_CLASS_ID, dispClass) == 0x947D)
        {
            my_display = new Display_024x(index);
            // In case that this is not initialized in DispTest::Initialize()
            // (Not called?) We setup the display class here.
            if (!LwrrentDisplayClass())
              LwrrentDevice()->unInitializedClass = LW9470_DISPLAY;
        }
        else if(REF_VAL(LW_PDISP_CLASSES_CLASS_ID, dispClass) == 0x957D)
        {
            my_display = new Display_025x(index);
            // In case that this is not initialized in DispTest::Initialize()
            // (Not called?) We setup the display class here.
            if (!LwrrentDisplayClass())
              LwrrentDevice()->unInitializedClass = LW9570_DISPLAY;
        }
        else if(REF_VAL(LW_PDISP_CLASSES_CLASS_ID, dispClass) == 0x977D)
        {
            my_display = new Display_027x(index);
            // In case that this is not initialized in DispTest::Initialize()
            // (Not called?) We setup the display class here.
            if (!LwrrentDisplayClass())
              LwrrentDevice()->unInitializedClass = LW9770_DISPLAY;
        }
        else if(REF_VAL(LW_PDISP_CLASSES_CLASS_ID, dispClass) == 0x987D)
        {
            my_display = new Display_028x(index);
            // In case that this is not initialized in DispTest::Initialize()
            // (Not called?) We setup the display class here.
            if (!LwrrentDisplayClass())
              LwrrentDevice()->unInitializedClass = LW9870_DISPLAY;
        }
        /*
        else if(REF_VAL(LW_PDISP_DSI_CLASS_SW_CLASS_ID, dispClass) == ???)
        {
            m_display_devices.push_back(new Display_03xx(index));
        }
        .
        .
        .
        */
        #endif
        else
        {
            Printf(Tee::PriHigh, "DispTest::Display - Unrecognized Display Class. Not Initializing.\n");
        }

        if(my_display)
        {
            rc = my_display->Initialize();
            if(rc != OK)
            {
                Printf(Tee::PriHigh, "ERROR: Failed to initialize DTB Display class\n");
                delete(my_display);
                return(rc);
            }
            m_display_devices.push_back(my_display);
        }
    }

    // set the crc start time
    DispTest::CrcSetStartTime();
    DispTest::LwstomInitialize();
    return OK;
}

/****************************************************************************************
 * DispTest::CrcSetOwner
 *
 *  Arguments Description:
 *  - Owner: the name of the test owner
 *
 *  Functional Description:
 *  - Set the string to be used in the owner field of the CRC output files.
 ***************************************************************************************/
RC DispTest::CrcSetOwner(string Owner)
{
    *(LwrrentDevice()->pCrcInfo->p_owner) = Owner;
    return OK;
}

/****************************************************************************************
 * DispTest::CrcSetCheckTolerance
 *
 *  Arguments Description:
 *  - Tolerance: allowable time difference in microseconds (uS)
 *
 *  Functional Description:
 *  - Set the tolerance when checking non-FModel generated lead.crc/gold.crc.
 ***************************************************************************************/
RC DispTest::CrcSetCheckTolerance(UINT32 Tolerance)
{
    LwrrentDevice()->pCrcInfo->tolerance = Tolerance;
    return OK;
}

/****************************************************************************************
 * DispTest::CrcSetFModelCheckTolerance
 *
 *  Arguments Description:
 *  - Tolerance: allowable time difference in microseconds (uS)
 *
 *  Functional Description:
 *  - Set the tolerance when checking FModel generated lead.crc/gold.crc.
 ***************************************************************************************/
RC DispTest::CrcSetFModelCheckTolerance(UINT32 Tolerance)
{
    LwrrentDevice()->pCrcInfo->tolerance_fmodel = Tolerance;
    return OK;
}

/****************************************************************************************
 * DispTest::CrcAddVgaHead
 *
 *  Arguments Description:
 *  - Head:             head the notifier is associated with
 *  - or_type:          "dac", "sor" or "pior"
 *  - or_number:          for "dac"": 0,1 or 2
 *                        for "sor": 0 or 1
 *                        for "pior": 0,1 or 2
 *  - or_is_primary: The OR specified is the primary OR used
 *
 *  Functional Description:
 *  - Turn on CRC Capturing for OR of or_type and or_number on Head which is in VGA mode
 ***************************************************************************************/
RC DispTest::CrcAddVgaHead(UINT32       Head,
                        const char*     or_type,
                        UINT32          or_number,
                        bool            or_is_primary)
{
    struct crc_head_info *info;

    // check that head number is valid
    if ((Head != 0) && (Head != 1) && (Head != 2) && (Head != 3))
    {
        Printf(Tee::PriHigh,"Attempt to Attach VGA CRC Handler for Invalid Head %u (should be Head 0 or Head 1 or Head 2 or Head 3)\n", Head);
        return RC::BAD_PARAMETER;
    }

    if(strcmp(or_type,"dac") != 0
            && strcmp(or_type,"sor") != 0
            && strcmp(or_type,"pior")!= 0
      )
    {
        Printf(Tee::PriHigh,"Attempt to Specify an invalid OR %s%u\n", or_type,or_number);
        return RC::BAD_PARAMETER;
    }

    // Find out if this head has been registered already
    if (LwrrentDevice()->pCrcHeadMap->find(Head) != LwrrentDevice()->pCrcHeadMap->end())
    {
        info = (*LwrrentDevice()->pCrcHeadMap)[Head];
        if (or_is_primary == info->is_active) {
            // If we are trying to register a primary OR but we have already done that, flag it.
            Printf(Tee::PriHigh,"Attempt to Specify a Primary OR for the VGA CRC Handler for Head %u when One Already Exists\n", Head);
            return RC::BAD_PARAMETER;
        } else if (or_is_primary ^ info->sec_is_active){
            // If we are trying to register a secondary OR but we have already done that, flag it.
            Printf(Tee::PriHigh,"Attempt to Specify a Secondary OR for the VGA CRC Handler for Head %u when One Already Exists\n", Head);
            return RC::BAD_PARAMETER;
        } else {
            // We are setting up the remaining available OR spot
            // Leave the fields as they were is we are changing the other OR settings
            info->is_active        = or_is_primary | info->is_active;
            if (or_is_primary)
            {
                info->pOr_type  = new string(or_type);
            }
            info->or_number        = or_is_primary? or_number: info->or_number;
            info->sec_is_active    = !or_is_primary | info->sec_is_active;
            if (!or_is_primary)
            {
                info->pSec_or_type  = new string(or_type);
            }
            info->sec_or_number    = !or_is_primary? or_number: info->sec_or_number;

        }
    }
    else
    {
        // create new info structure for head
        info = new struct crc_head_info;
        info->head             = Head;
        info->channel_handle   = 0; // Just leave this as Null since there is no channel handle when in VGA mode
        info->ctx_dma_handle   = 0; // Just leave this as Null since there is no Crc Notifier handle when in VGA mode
        info->offset           = 0; /* crc notifier must start at beginning of context dma */
        info->tolerance        = 128; // Hard-Coded, no magic here, just piked a nubmer (DKilani)
        info->VgaMode          = true;
        info->FcodeMode        = false;
        info->FcodeBaseAdd     = 0;
        info->pFcodeTarget     = NULL;
        info->ModeEnabled      = false;
        info->pHeadModeList    = new list <string*>;
        info->IsFirstSnooze    = true;
        info->is_active        = or_is_primary;
        if (or_is_primary)
        {
            info->pOr_type  = new string(or_type);
        }
        info->or_number        = or_is_primary? or_number:0;
        info->crc_overflow     = false;
        info->sec_is_active    = !or_is_primary;
        if (!or_is_primary)
        {
            info->pSec_or_type = new string(or_type);
        }
        info->sec_or_number    = !or_is_primary? or_number:0;
        info->sec_crc_overflow = false;

        // update the crc lists and maps
        LwrrentDevice()->pCrcHeadList->push_back(info);
        (*LwrrentDevice()->pCrcHeadMap)[info->head] = info;

        // initialize the crc event list for this head
        list<struct crc_head_event*> *event_list = new list<struct crc_head_event*>();
        (*LwrrentDevice()->pCrcHeadEventMap)[info] = event_list;
    }
    return OK;
}

/****************************************************************************************
 * DispTest::AssignHead
 *
 *  Arguments Description:
 *  - physical_head_a:             head the notifier is associated with
 *  - physical_head_b:             head the notifier is associated with
 *  - physical_head_c:             head the notifier is associated with
 *  - physical_head_d:             head the notifier is associated with
 *  Functional Description:
 ***************************************************************************************/
RC DispTest::AssignHead(UINT32 physical_head_a,
                        UINT32 physical_head_b,
                        UINT32 physical_head_c,
                        UINT32 physical_head_d)
{
    if (!LwrrentDisplayClass()) return OK;
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        MASSERT(!"***ERROR: DispTest::AssignHead - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->AssignHead(physical_head_a, physical_head_b, physical_head_c, physical_head_d));
}
/****************************************************************************************
 * DispTest::CrcAddHead
 *
 *  Arguments Description:
 *  - Head:             head the notifier is associated with
 *  - CtxDmaHandle:     notifier context dma handle
 *  - ChannelHandle:    handle to channel associated with notifier
 *
 *  Functional Description:
 *  - Attach the given notifier to act as the CRC notifier for the specified head.
 *    Note that only one CRC notifier per head is allowed.
 *    An error oclwrs on an attempt to assign a CRC notifier to a head that already has
 *    a CRC notifier.
 ***************************************************************************************/
RC DispTest::CrcAddHead(UINT32       Head,
                        LwRm::Handle CtxDmaHandle,
                        LwRm::Handle ChannelHandle)
{
    if (!LwrrentDisplayClass()) return OK;
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        MASSERT(!"***ERROR: DispTest::CrcAddHead - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->CrcAddHead(Head, CtxDmaHandle, ChannelHandle));
}

/****************************************************************************************
 * DispTest::CrcNoCheckHead
 *
 *  Arguments Description:
 *  - Head:             head the notifier is associated with
 *
 *  Functional Description:
 *  - Set the ModeEnable flag to false for the head of interest. This make the crc checker skip the crc entries corresponding to this head.
 *
 ***************************************************************************************/
RC DispTest::CrcNoCheckHead(UINT32       Head)
{
    // check that head number is valid
    if ((Head != 0) && (Head != 1) && (Head != 2) && (Head != 3))
    {
        Printf(Tee::PriHigh,"Attempt to Control an Invalid Head %u (should be Head 0 or Head 1 or Head 2 or Head 3)\n", Head);
        return RC::BAD_PARAMETER;
    }

    // check that a notifier for the head doesn't already exist
    if (LwrrentDevice()->pCrcHeadMap->find(Head) == LwrrentDevice()->pCrcHeadMap->end())
    {
        Printf(Tee::PriHigh,"Attempt to Control Head %u when it is not configured\n", Head);
        return RC::BAD_PARAMETER;
    }

    struct crc_head_info *info = (*LwrrentDevice()->pCrcHeadMap)[Head];
    info->ModeEnabled    = false;

    return OK;
}

/****************************************************************************************
 * DispTest::CrcAddFcodeHead
 *
 *  Arguments Description:
 *  - Head:             Head the notifier is associated with
 *  - pBaseAddress:     Base Address for the Crc Ctx Dma
 *  - Target
 *
 *  Functional Description:
 *  - Attach the given notifier to act as the CRC notifier for the specified head.
 *    Note that only one CRC notifier per head is allowed.
 *    An error oclwrs on an attempt to assign a CRC notifier to a head that already has
 *    a CRC notifier.
 ***************************************************************************************/
RC DispTest::CrcAddFcodeHead(UINT32        Head,
                             PHYSADDR *    pBaseAddress,
                             const char *  Target)
{
    if (!LwrrentDisplayClass()) return OK;
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::CrcAddFcodeHead - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->CrcAddFcodeHead(Head, pBaseAddress, Target));
}

/****************************************************************************************
 * DispTest::CrcAddNotifier
 *
 *  Arguments Description:
 *  - Tag:              identifier assigned by the test
 *  - CtxDmaHandle:     notifier context dma handle
 *  - Offset:           start of notifier memory within context dma
 *  - NotifierType:     type of the notifier ("core_completion", "core_capabilities", "base", "overlay", or "crc")
 *                      (so the CRC manager can lookup the done bit)
 *  - ChannelHandle:    handle to channel associated with notifier
 *  - Tolerance:        allowable time difference in microseconds (uS)
 *                      when comparing outputs of different runs
 *  - RefHead
 *
 *  Functional Description:
 *  - Add the notifier to the list of notifiers for which the test will generate CRCs.
 *    Note that each notifier can be used at most once per test.
 *    An error oclwrs on an attempt to reuse a notifier.
 ***************************************************************************************/
RC DispTest::CrcAddNotifier(UINT32       Tag,
                            LwRm::Handle CtxDmaHandle,
                            UINT32       Offset,
                            string       NotifierType,
                            LwRm::Handle ChannelHandle,
                            UINT32       Tolerance,
                            bool         RefHead)
{
    if (!LwrrentDisplayClass()) return OK;
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::CrcAddNotifier - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->CrcAddNotifier(Tag,
                                                    CtxDmaHandle,
                                                    Offset,
                                                    NotifierType,
                                                    ChannelHandle,
                                                    Tolerance,
                                                    RefHead)
        );
}

/****************************************************************************************
 * DispTest::CrcAddSemaphore
 *
 *  Arguments Description:
 *  - Tag:              identifier assigned by the test
 *  - CtxDmaHandle:     semaphore context dma handle
    - Offset:           start of semaphore memory within context dma
 *  - PollVal:          value on which to poll
 *  - ChannelHandle:    handle to channel associated with semaphore
 *  - Tolerance:        allowable time difference in microseconds (uS)
 *                      when comparing outputs of different runs
 *  - RefHead:          dump each notifier entry with exact logic head assosiated
 *
 *  Functional Description:
 *  - Add the semaphore to the list of semaphores for which the test will generate CRCs.
 *    Note that each offset in a semaphore can be used at most once per test.
 *    An error oclwrs on an attempt to reuse an offset within a semaphore.
 ***************************************************************************************/
RC DispTest::CrcAddSemaphore(UINT32       Tag,
                             LwRm::Handle CtxDmaHandle,
                             UINT32       Offset,
                             UINT32       PollVal,
                             LwRm::Handle ChannelHandle,
                             UINT32       Tolerance,
                             bool         RefHead)
{
    // check that crc info for this offset within the semaphore doesn't already exist
    if (LwrrentDevice()->pCrcSemaphoreMap->find(CtxDmaHandle) != LwrrentDevice()->pCrcSemaphoreMap->end())
    {
        if (((*(LwrrentDevice()->pCrcSemaphoreMap->find(CtxDmaHandle))).second)->find(Offset) !=
            ((*(LwrrentDevice()->pCrcSemaphoreMap->find(CtxDmaHandle))).second)->end())
        {
            Printf(Tee::PriHigh,"Attempt to Redefine CRC Semaphore (Handle: %08x  Offset: %u)\n", CtxDmaHandle, Offset);
            return RC::BAD_PARAMETER;
        }
    }

    // create new info structure for semaphore
    struct crc_semaphore_info *info = new struct crc_semaphore_info;
    info->tag             = Tag;
    info->channel_handle  = ChannelHandle;
    info->ctx_dma_handle  = CtxDmaHandle;
    info->status_pos      = 0;               /* status should be at the start of semaphore memory */
                                             /* (at offset from the start of context dma memory   */
    info->bitmask         = 0xFFFFFFFF;      /* default the bitmask so that we poll for the */
                                             /* exact 32-bit value at the specified offset  */
    info->poll_value      = PollVal;
    info->tolerance       = Tolerance;
    info->is_active       = true;
    info->is_ref_head     = RefHead;

    // Check that the Offset provided is within the admissible range
    if (Offset > 1023 )
    {
        // output error message and return
        Printf(Tee::PriHigh,"[Error] : Offset out of Range for Semaphore %d \n", Tag);
        delete info;
        return RC::BAD_PARAMETER;
    }
    info->offset     = Offset;

    // update the crc lists
    LwrrentDevice()->pCrcSemaphoreList->push_back(info);

    // update the crc maps
    if (LwrrentDevice()->pCrcSemaphoreMap->find(CtxDmaHandle) != LwrrentDevice()->pCrcSemaphoreMap->end()) {
        // entry for semaphore exists, create entry for offset
        (*((*(LwrrentDevice()->pCrcSemaphoreMap->find(CtxDmaHandle))).second))[info->offset] = info;
    }
    else
    {
        // create entries for both semaphore and offset
        map<UINT32, struct crc_semaphore_info*> *offset_map =
            new map<UINT32, struct crc_semaphore_info*>();
        (*offset_map)[info->offset] = info;
        (*LwrrentDevice()->pCrcSemaphoreMap)[info->ctx_dma_handle] = offset_map;
    }

    // initialize the crc event list for this semaphore offset
    list<struct crc_semaphore_event*> *event_list = new list<struct crc_semaphore_event*>();
    (*LwrrentDevice()->pCrcSemaphoreEventMap)[info] = event_list;
    return OK;
}

/****************************************************************************************
 * DispTest::CrcAddInterrupt
 *
 *  Arguments Description:
 *  - Tag:              identifier assigned by the test
 *  - Name:             interrupt identifying name (from the ref manuals)
 *                      (see CrcAddInterruptAndCount below for list of
 *                       interrupt names)
 *  - Tolerance:        allowable time difference in microseconds (uS)
 *                      when comparing outputs of different runs
 *
 *  Functional Description:
 *  - Add the interrupt to the list of interrupts for which the test will generate CRCs.
 ***************************************************************************************/
RC DispTest::CrcAddInterrupt(UINT32 Tag, string Name, UINT32 Tolerance)
{
    return CrcAddInterruptAndCount(Tag, Name, Tolerance, 0);
}

/****************************************************************************************
 * DispTest::CrcAddInterrupt
 *
 *  Arguments Description:
 *  - Tag:              identifier assigned by the test
 *  - Name:             interrupt identifying name (from the ref manuals)
 *    Current supported interrupt names are:
 *      "chn_0_awaken"
 *      "chn_1_awaken"
 *      "chn_2_awaken"
 *      "chn_3_awaken"
 *      "chn_4_awaken"
 *      "chn_5_awaken"
 *      "chn_6_awaken"
 *      "chn_0_exception"
 *      "chn_1_exception"
 *      "chn_2_exception"
 *      "chn_3_exception"
 *      "chn_4_exception"
 *      "chn_5_exception"
 *      "chn_6_exception"
 *      "chn_7_exception"
 *      "chn_8_exception"
 *      "head_0_vblank"
 *      "head_1_vblank"
 *      "supervisor1"
 *      "supervisor2"
 *      "supervisor3"
 *      "vbios_release"
 *      "vbios_attention"
 *  - Tolerance:        allowable time difference in microseconds (uS)
 *                      when comparing outputs of different runs
 *  - ExpectedCount:    Number of expected interrupts of this type
 *
 *  Functional Description:
 *  - Add the interrupt to the list of interrupts for which the test will generate CRCs.
 ***************************************************************************************/
RC DispTest::CrcAddInterruptAndCount(UINT32 Tag, string Name, UINT32 Tolerance, UINT32 ExpectedCount)
{
    if (!LwrrentDisplayClass()) return OK;
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::CrcAddInterruptAndCount - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->CrcAddInterrupt(Tag, Name, Tolerance, ExpectedCount));
}

/****************************************************************************************
 * DispTest::CrcSetStartTime
 *
 *  Functional Description:
 *  - Read the AUDIT_TIMESTAMP register and store the current time as the test start time.
 ***************************************************************************************/
void DispTest::CrcSetStartTime()
{
    if (!LwrrentDisplayClass()) return;
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        MASSERT(!"***ERROR: DispTest::CrcSetStartTime - Current device class not allocated.");
    }

    ///and forward the call to the class
    m_display_devices[index]->CrcSetStartTime();
}

/****************************************************************************************
 * DispTest::SetLegacyCrc
 *
 *  Arguments Description:
 *  - en:
 *  - head:
 *
 *  Functional Description:
 *  - This functions does a write to CRA9 to send a SetLegacyCrc method to DSI
 ***************************************************************************************/
RC DispTest::SetLegacyCrc(UINT32 en, UINT32 head)
{
        if (!LwrrentDisplayClass()) return OK;
        ///check that this device has a Display class
        UINT32 index = LwrrentDevice()->GetDeviceIndex();

        if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
        {
            MASSERT(!"***ERROR: DispTest::Crlwpdate - Current device class not allocated.");
        }

        ///and forward the call to the class
        return(m_display_devices[index]->SetLegacyCrc(en, head));
}

/****************************************************************************************
 * DispTest::Crlwpdate
 *
 *  Functional Description:
 *  - Scan the CRC notifiers and semaphores for any change that should be logged as a
 *    CRC event.  Update the CRC event lists as appropriate.
 ***************************************************************************************/
void DispTest::Crlwpdate()
{
    if (!LwrrentDisplayClass()) return;
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        MASSERT(!"***ERROR: DispTest::Crlwpdate - Current device class not allocated.");
    }

    ///and forward the call to the class
    m_display_devices[index]->Crlwpdate();
}

/****************************************************************************************
 * DispTest::CrlwpdateInterrupt
 *
 *  Arguments Description:
 *    address: the address of the register to check for in the interrupt table
 *    value: the value of the interrupt, as read by the handler
 *           this should include exactly the bits the handler intends to clear
 *
 *  Functional Description:
 *  - Check if an interrupt that oclwred should be logged and update the CRC interrupt
 *    event list as appropriate.
 ***************************************************************************************/
void DispTest::CrlwpdateInterrupt(UINT32 address, UINT32 value)
{
    if (!LwrrentDisplayClass()) return;
    ///check whether DispTest already called CrcCleanup
    if(LwrrentDevice()->pCrcInterruptList == NULL)
        return;

    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        MASSERT(!"***ERROR: DispTest::CrlwpdateInterrupt - Current device class not allocated.");
    }

    ///and forward the call to the class
    m_display_devices[index]->CrlwpdateInterrupt(address, value);
}

/****************************************************************************************
 * DispTest::CrcWriteHeadList
 *
 *  Arguments Description:
 *  - fp:
 *
 *  Functional Description:
 *  - Helper function to write out a comma-separated list of the head numbers.
 ***************************************************************************************/
void DispTest::CrcWriteHeadList(FILE *fp)
{
    if (!LwrrentDisplayClass()) return;
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        MASSERT(!"***ERROR: DispTest::CrcWriteHeadList - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->CrcWriteHeadList(fp));
}

/****************************************************************************************
 * DispTest::CrcWriteEventFiles
 *
 *  Functional Description:
 *  - Write the contents of the CRC event lists to the CRC output files.
 ***************************************************************************************/
RC DispTest::CrcWriteEventFiles()
{
    // write each crc file
    RC rc_head      = CrcWriteHeadEventFile();
    RC rc_notifier  = CrcWriteNotifierEventFile();
    RC rc_semaphore = CrcWriteSemaphoreEventFile();
    RC rc_interrupt = CrcWriteInterruptEventFile();
    RC rc_lwstom    = WriteLwstomEventFiles();

    // Using the errorlogger to processing the interrupt data
//     RC rc_intr_handle = CrcInterruptProcessing();

//     if( rc_intr_handle != OK ) {
//         Printf(Tee::PriHigh,"Error in interrupt Processing!\n");
//         return rc_intr_handle;
//     }
//     ErrorLogger::TestCompleted();

    // check return codes
    RC rc = OK;
    if      (rc_head != OK)      { rc = rc_head; }
    else if (rc_notifier != OK)  { rc = rc_notifier; }
    else if (rc_semaphore != OK) { rc = rc_semaphore; }
    else if (rc_interrupt != OK) { rc = rc_interrupt; }
    else if (rc_lwstom != OK)    { rc = rc_lwstom; }

    // print error status
    if (rc != OK)
    {
        Printf(Tee::PriHigh,"Error Writing CRC Files\n");
        return RC::BAD_PARAMETER;
    }
    return rc;
}

// Instead of writing out the interrupt log file for post processing,
// we're now directly letting the errorlogger to handle the content
// comparison
RC
DispTest::CrcInterruptProcessing() {
    string file_name;
    RC  rc = OK;
    const vector<string>& argv_list = CommandLine::Argv();
    bool  update_intr_file = false;

    // Construct the golden file name
    // Note: It is assumed that the golden file will be located in the same
    // directory as the script file or the sub-directory if it is a sub test.
    file_name = CommandLine::ScriptFilePath();
    file_name += *(LwrrentDevice()->pCrcInfo->p_subtest_name);
    file_name += "/test.int";
    const char* fname = file_name.c_str();
    bool  fexist = Xp::DoesFileExist( file_name );

    // MODS does not have a machanism to support '-Crlwpdate' options yet.
    // So we manually implemented here for interrupt file only.
    for( vector<string>::const_iterator iter = argv_list.begin();
         iter != argv_list.end(); ++iter ) {
        if( !(iter->c_str()) || (strlen( iter->c_str() )<5) )
            continue;

        if( !strncmp( iter->c_str(), "-crlwpdate", 5 ) ) {
            update_intr_file = true;
            break;
        }
    }

    if( ! ErrorLogger::HasErrors() ) {
        if( ! fexist )
            return OK;
        else if( update_intr_file ) { // test.int should be removed
            P4Action( "delete", fname );

            // Also delete this file in case the next run happens before
            // 'p4 submit'
            Xp::EraseFile( file_name );
            return OK;
        }
    }

    if( update_intr_file ) {        // the interrupt file needs to update
        bool p4_edited = false;

        if( fexist ) {
            if( P4Action( "edit", fname ) )
                Printf(Tee::PriNormal,"Error: p4 edit %s failed\n", fname);
            else
                p4_edited = true;
        }

        rc = ErrorLogger::WriteFile( fname );

        if( (rc == OK) && !p4_edited )
            // for update, if the file does not exist, we need add one
            P4Action( "add", fname );
    }
    else                        // Just processing the interrupt file
        ErrorLogger::IgnoreErrorsForThisTest();
//         rc = ErrorLogger::CompareErrorsWithFile( fname,
//             ErrorLogger::Exact );

    return rc;
}

/****************************************************************************************
 * DispTest::CrcWriteHeadEventFile
 *
 *  Functional Description:
 *  - Write the CRC output file for the head notifier events.
 ***************************************************************************************/
RC DispTest::CrcWriteHeadEventFile()
{
    if (!LwrrentDisplayClass()) return OK;
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        MASSERT(!"***ERROR: DispTest::CrcWriteHeadEventFile - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->CrcWriteHeadEventFile());
}

/****************************************************************************************
 * DispTest::CrcWriteNotifierEventFile
 *
 *  Functional Description:
 *  - Write the CRC output file for the notifier events.
 ***************************************************************************************/
RC DispTest::CrcWriteNotifierEventFile()
{
    if (!LwrrentDisplayClass()) return OK;
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        MASSERT(!"***ERROR: DispTest::CrcWriteNotifierEventFile - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->CrcWriteNotifierEventFile());
}

/****************************************************************************************
 * DispTest::CrcWriteSemaphoreEventFile
 *
 *  Functional Description:
 *  - Write the CRC output file for the semaphore events.
 ***************************************************************************************/
RC DispTest::CrcWriteSemaphoreEventFile()
{
    if (!LwrrentDisplayClass()) return OK;
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        MASSERT(!"***ERROR: DispTest::CrcWriteSemaphoreEventFile - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->CrcWriteSemaphoreEventFile());
}

/****************************************************************************************
 * DispTest::CrcWriteInterruptEventFile
 *
 *  Functional Description:
 *  - Write the CRC output file for the interrupt events.
 ***************************************************************************************/
RC DispTest::CrcWriteInterruptEventFile()
{
    if (!LwrrentDisplayClass()) return OK;
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        MASSERT(!"***ERROR: DispTest::CrcWriteInterruptEventFile - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->CrcWriteInterruptEventFile());
}

/****************************************************************************************
 * DispTest::CrcCountInterrupts
 *
 *  Functional Description:
 *  - count the number of interrupt events for named interrupt
 ***************************************************************************************/
RC DispTest::GetCrcInterruptCount(string InterruptName, UINT32 *count)
{
    // parse data for each interrupt event
    list<struct crc_interrupt_event*>::iterator e_iter;
    *count = 0;
    for (e_iter = LwrrentDevice()->pCrcInterruptEventList->begin();
         e_iter != LwrrentDevice()->pCrcInterruptEventList->end();
         e_iter++)
    {
        // get the associated interrupt
        struct crc_interrupt_info *info = (*e_iter)->p_info;
        // update count if found
        if (InterruptName == info->p_name->c_str()) {
          *count = (*e_iter)->count + 1;
        }
    }

    return OK;
}

/****************************************************************************************
 * DispTest::CrcCleanup
 *
 *  Functional Description:
 *  - Deallocate memory used for CRC data structures
 ***************************************************************************************/
void DispTest::CrcCleanup()
{
    /* cleanup global crc info */
    delete (LwrrentDevice()->pCrcInfo->p_test_name);
    delete (LwrrentDevice()->pCrcInfo->p_subtest_name);
    delete (LwrrentDevice()->pCrcInfo->p_owner);
    delete (LwrrentDevice()->pCrcInfo->p_head_filename);
    delete (LwrrentDevice()->pCrcInfo->p_notifier_filename);
    delete (LwrrentDevice()->pCrcInfo->p_semaphore_filename);
    delete (LwrrentDevice()->pCrcInfo->p_interrupt_filename);
    delete LwrrentDevice()->pCrcInfo;
    /* cleanup crc head notifiers */
    list <struct crc_head_info*>::iterator h_iter;
    for (h_iter = LwrrentDevice()->pCrcHeadList->begin(); h_iter != LwrrentDevice()->pCrcHeadList->end(); h_iter++)
    {
        struct crc_head_info *h_info = *h_iter;
        delete h_info->pOr_type;
        delete h_info->pSec_or_type;
        delete h_info->pFcodeTarget;
        delete h_info->pHeadModeList;
        delete h_info;
    }
    delete LwrrentDevice()->pCrcHeadList;
    delete LwrrentDevice()->pCrcHeadMap;
    /* cleanup crc notifiers */
    list <struct crc_notifier_info*>::iterator n_iter;
    for (n_iter = LwrrentDevice()->pCrcNotifierList->begin(); n_iter != LwrrentDevice()->pCrcNotifierList->end(); n_iter++)
    {
        struct crc_notifier_info *n_info = *n_iter;
        delete (n_info->p_notifier_type);
        delete n_info;
    }
    delete LwrrentDevice()->pCrcNotifierList;
    map< LwRm::Handle, map<UINT32, struct crc_notifier_info*> * >::iterator n_map_iter;
    for (n_map_iter = LwrrentDevice()->pCrcNotifierMap->begin();
         n_map_iter != LwrrentDevice()->pCrcNotifierMap->end();
         n_map_iter++)
    {
        map<UINT32, struct crc_notifier_info*> *n_map = n_map_iter->second;
        delete n_map;
    }
    delete LwrrentDevice()->pCrcNotifierMap;
    /* cleanup crc semaphores */
    list <struct crc_semaphore_info*>::iterator s_iter;
    for (s_iter = LwrrentDevice()->pCrcSemaphoreList->begin(); s_iter != LwrrentDevice()->pCrcSemaphoreList->end(); s_iter++)
    {
        struct crc_semaphore_info *s_info = *s_iter;
        delete s_info;
    }
    delete LwrrentDevice()->pCrcSemaphoreList;
    map< LwRm::Handle, map<UINT32, struct crc_semaphore_info*> * >::iterator s_map_iter;
    for (s_map_iter = LwrrentDevice()->pCrcSemaphoreMap->begin();
         s_map_iter != LwrrentDevice()->pCrcSemaphoreMap->end();
         s_map_iter++)
    {
        map<UINT32, struct crc_semaphore_info*> *s_map = s_map_iter->second;
        delete s_map;
    }
    delete LwrrentDevice()->pCrcSemaphoreMap;
    /* cleanup crc interrupts */
    list <struct crc_interrupt_info*>::iterator i_iter;
    for (i_iter = LwrrentDevice()->pCrcInterruptList->begin(); i_iter != LwrrentDevice()->pCrcInterruptList->end(); i_iter++)
    {
        struct crc_interrupt_info *i_info = *i_iter;
        delete (i_info->p_name);
        delete i_info;
    }
    delete LwrrentDevice()->pCrcInterruptList;
    LwrrentDevice()->pCrcInterruptList = NULL;
    delete LwrrentDevice()->pCrcInterruptMap;
    /* cleanup crc head notifier events */
    list<struct crc_head_event*>::iterator he_iter;
    for (he_iter = LwrrentDevice()->pCrcHeadEventList->begin(); he_iter != LwrrentDevice()->pCrcHeadEventList->end(); he_iter++)
    {
        struct crc_head_event *he = *he_iter;
        delete he;
    }
    delete LwrrentDevice()->pCrcHeadEventList;
    map< struct crc_head_info*, list<struct crc_head_event*> * >::iterator he_map_iter;
    for (he_map_iter = LwrrentDevice()->pCrcHeadEventMap->begin();
         he_map_iter != LwrrentDevice()->pCrcHeadEventMap->end();
         he_map_iter++)
    {
        list<struct crc_head_event*> *he_list = he_map_iter->second;
        delete he_list;
    }
    delete LwrrentDevice()->pCrcHeadEventMap;
    /* cleanup crc notifier events */
    list<struct crc_notifier_event*>::iterator ne_iter;
    for (ne_iter = LwrrentDevice()->pCrcNotifierEventList->begin(); ne_iter != LwrrentDevice()->pCrcNotifierEventList->end(); ne_iter++)
    {
        struct crc_notifier_event *ne = *ne_iter;
        delete [] (ne->p_words);
        delete ne;
    }
    delete LwrrentDevice()->pCrcNotifierEventList;
    map< struct crc_notifier_info*, list<struct crc_notifier_event*> * >::iterator ne_map_iter;
    for (ne_map_iter = LwrrentDevice()->pCrcNotifierEventMap->begin();
         ne_map_iter != LwrrentDevice()->pCrcNotifierEventMap->end();
         ne_map_iter++)
    {
        list<struct crc_notifier_event*> *ne_list = ne_map_iter->second;
        delete ne_list;
    }
    delete LwrrentDevice()->pCrcNotifierEventMap;
    /* cleanup crc semaphore events */
    list<struct crc_semaphore_event*>::iterator se_iter;
    for (se_iter = LwrrentDevice()->pCrcSemaphoreEventList->begin(); se_iter != LwrrentDevice()->pCrcSemaphoreEventList->end(); se_iter++)
    {
        struct crc_semaphore_event *se = *se_iter;
        delete se;
    }
    delete LwrrentDevice()->pCrcSemaphoreEventList;
    map< struct crc_semaphore_info*, list<struct crc_semaphore_event*> * >::iterator se_map_iter;
    for (se_map_iter = LwrrentDevice()->pCrcSemaphoreEventMap->begin();
         se_map_iter != LwrrentDevice()->pCrcSemaphoreEventMap->end();
         se_map_iter++)
    {
        list<struct crc_semaphore_event*> *se_list = se_map_iter->second;
        delete se_list;
    }
    delete LwrrentDevice()->pCrcSemaphoreEventMap;
    /* cleanup crc interrupt events */
    list<struct crc_interrupt_event*>::iterator ie_iter;
    for (ie_iter = LwrrentDevice()->pCrcInterruptEventList->begin(); ie_iter != LwrrentDevice()->pCrcInterruptEventList->end(); ie_iter++)
    {
        struct crc_interrupt_event *ie = *ie_iter;
        delete ie;
    }
    delete LwrrentDevice()->pCrcInterruptEventList;
    map< struct crc_interrupt_info*, list<struct crc_interrupt_event*> * >::iterator ie_map_iter;
    for (ie_map_iter = LwrrentDevice()->pCrcInterruptEventMap->begin();
         ie_map_iter != LwrrentDevice()->pCrcInterruptEventMap->end();
         ie_map_iter++)
    {
        list<struct crc_interrupt_event*> *ie_list = ie_map_iter->second;
        delete ie_list;
    }
    delete LwrrentDevice()->pCrcInterruptEventMap;

    DispTest::LwstomCleanup();
}

/****************************************************************************************
 * DispTest::ModifyTestName
 *
 *  Arguments Description:
 *  - sTestName: the name of the test
 *
 *  Functional Description:
 *  - Set the string to be used in the test name field of the CRC output files.
 ***************************************************************************************/
RC DispTest::ModifyTestName(string sTestName)
{
    *(LwrrentDevice()->pCrcInfo->p_test_name) = sTestName;
    return OK;
}

/****************************************************************************************
 * DispTest::ModifySubtestName
 *
 *  Arguments Description:
 *  - sTestName: the name of the subtest
 *
 *  Functional Description:
 *  - Set the string to be used in the subtest name field of the CRC output files.
 ***************************************************************************************/
RC DispTest::ModifySubtestName(string sSubtestName)
{
    *(LwrrentDevice()->pCrcInfo->p_subtest_name) = sSubtestName;
    return OK;
}

/****************************************************************************************
 * DispTest::AppendSubtestName
 *
 *  Arguments Description:
 *  - sSubtestNamePostfix: appendment to the name of the subtest
 *
 *  Functional Description:
 *  - Set the string to be used to append the subtest name field of the CRC output files.
 ***************************************************************************************/
RC DispTest::AppendSubtestName(string sSubtestNamePostfix)
{
    *(LwrrentDevice()->pCrcInfo->p_subtest_name) =
        *(LwrrentDevice()->pCrcInfo->p_subtest_name)
        + sSubtestNamePostfix;
    return OK;
}

