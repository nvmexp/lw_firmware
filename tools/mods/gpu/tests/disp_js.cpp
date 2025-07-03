/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
//      disp_js.cpp - DispTest class - JavaScript interface and related methods
//      Copyright (c) 1999-2016 by LWPU Corporation.
//      All rights reserved.
//
//      Originally part of "disp_test.cpp" - broken out 28 October 2005
//
//      Written by: Matt Craighead, Larry Coffey, et al
//      Date:       29 July 2004
//
//      Routines in this module:
//      <JavaScript interface routines>
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
#include <vector>
#include <errno.h>

#ifdef USE_DTB_DISPLAY_CLASSES
#include "gpu/js/dispt_sr.h"
#else
#include "gpu/js/dispt_sr.h"
#endif

#include "core/utility/errloggr.h"

#include "class/cl507d.h"
#include "ctrl/ctrl5070.h"
#include "ctrl/ctrl0080.h"
#define DISPTEST_PRIVATE
#include "core/include/disp_test.h"
#include "gpu/include/vgacrc.h"

#ifdef USE_DTB_DISPLAY_CLASSES
#include "gpu/js_gpusb.h"
#else
#include "gpu/js_gpusb.h"
#endif

namespace DispTest
{
    // No local ("static") routines needed

    // DispTest member data
    extern std::vector<DispTestDeviceInfo*> m_DeviceInfo;
    extern std::map<DispTestDeviceInfo*, int> m_DeviceIndex;
    extern bool debug_messages;
}

////////////////////////////////////////////////////////////////////////////////////////////
//
// JavaScript class: DispTest
//
//  Methods:
//      constructor     Allocates resources for the new DispTestInfo class
//      finalize        Frees resources allocated in the constructor
//
typedef DispTest::DispTestDeviceInfo DispTestInfo;

#define JS_CLASS_HAS_PRIVATE(className) \
   static JSClass className##Class =                                \
   {                                                                \
      #className,                                                   \
      JSCLASS_HAS_PRIVATE,                                          \
      JS_PropertyStub,                                              \
      JS_PropertyStub,                                              \
      JS_PropertyStub,                                              \
      JS_PropertyStub,                                              \
      JS_EnumerateStub,                                             \
      JS_ResolveStub,                                               \
      JS_ColwertStub,                                               \
      C_##className##_finalize                                      \
   };                                                               \

//  Method definition: DispTest::constructor
C_(DispTest_constructor)
{
    JavaScriptPtr pJavaScript;

    UINT32 MyIndex;

    if(NumArguments != 1)
    {
        JS_ReportError(pContext, "usage: new DispTest");
        return JS_FALSE;
    }

    if (OK != pJavaScript->FromJsval(pArguments[0], &MyIndex)) {
        JS_ReportError(pContext, "usage: new DispTest");
        return JS_FALSE;
    }

    DispTest::DispTestDeviceInfo *pClass = new DispTest::DispTestDeviceInfo();
    DispTest::m_DeviceIndex[pClass] = MyIndex;
    pClass->m_MyDeviceIndex = MyIndex;
    DispTest::m_DeviceInfo.push_back(pClass);

    MASSERT(pClass);

    return JS_SetPrivate(pContext, pObject, pClass);
}

//  Method definition: DispTest::finalize
static void C_DispTest_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    void *pvClass = JS_GetPrivate(cx, obj);
    if(pvClass)
    {
        DispTest::DispTestDeviceInfo * pClass = (DispTest::DispTestDeviceInfo *)pvClass;
        delete pClass;
    }
}

JS_CLASS_HAS_PRIVATE(DispTest);

//  Class definition: Disptest
static SObject DispTest_Object
(
    "DispTest",
    DispTestClass,
    0,
    0,
    "Disp Test.",
    C_DispTest_constructor
);

////////////////////////////////////////////////////////////////////////////////////////////
//
//  Javascript Interface Definitions - DispTest object, properties and methods.
//
//      Type    Name                                        Return
//      ----    ----                                        ------
//      STest   DispTest_SetDebugMessageDisplay             RC | JS_FALSE
//      STest   DispTest_Initialize                         RC | JS_FALSE
//      STest   DispTest_CrcInitialize                      RC | JS_FALSE
//      SMethod DispTest_Setup                              JS_TRUE | JS_FALSE
//      STest   DispTest_Release                            RC | JS_FALSE
//      STest   DispTest_SetLwrrentDevice                   RC | JS_FALSE
//      STest   DispTest_BindDevice                         RC | JS_FALSE
//      STest   DispTest_SwitchToChannel                    RC | JS_FALSE
//      STest   DispTest_IdleChannelControl                 RC | JS_FALSE
//      STest   DispTest_SetChnStatePollNotif               RC | JS_FALSE
//      STest   DispTest_SetChnStatePollNotifBlocking       RC | JS_FALSE
//      STest   DispTest_GetChnStatePollNotifRMPolling      RC | JS_FALSE
//      STest   DispTest_GetInternalTVRasterTimings         RC | JS_FALSE
//      STest   DispTest_EnqMethod                          RC | JS_FALSE
//      STest   DispTest_EnqUpdateAndMode                   RC | JS_FALSE
//      STest   DispTest_EnqCoreUpdateAndMode               RC | JS_FALSE
//      STest   DispTest_EnqMethodMulti                     RC | JS_FALSE
//      STest   DispTest_EnqMethodNonInc                    RC | JS_FALSE
//      STest   DispTest_EnqMethodNonIncMulti               RC | JS_FALSE
//      STest   DispTest_EnqMethodNop                       RC | JS_FALSE
//      STest   DispTest_EnqMethodJump                      RC | JS_FALSE
//      STest   DispTest_EnqMethodSetSubdevice              RC | JS_FALSE
//      STest   DispTest_EnqMethodOpcode                    RC | JS_FALSE
//      STest   DispTest_SetAutoFlush                       RC | JS_FALSE
//      STest   DispTest_Flush                              RC | JS_FALSE
//      STest   DispTest_BindContextDma                     RC | JS_FALSE
//      SMethod DispTest_CreateNotifierContextDma           JS_TRUE | JS_FALSE
//      SMethod DispTest_CreateSemaphoreContextDma          JS_TRUE | JS_FALSE
//      SMethod DispTest_CreateContextDma                   JS_TRUE | JS_FALSE
//      STest   DispTest_DeleteContextDma                   RC | JS_FALSE
//      STest   DispTest_SetContextDmaDebugRegs             RC | JS_FALSE
//      STest   DispTest_WriteVal32                         RC | JS_FALSE
//      SMethod DispTest_ReadVal32                          JS_TRUE | JS_FALSE
//      STest   DispTest_PollHWValue                        RC | JS_FALSE
//      STest   DispTest_PollHWGreaterEqualValue            RC | JS_FALSE
//      STest   DispTest_PollHWLessEqualValue               RC | JS_FALSE
//      STest   DispTest_PollHWNotValue                     RC | JS_FALSE
//      STest   DispTest_PollIORegValue                     RC | JS_FALSE
//      STest   DispTest_PollRegValue                       RC | JS_FALSE
//      STest   DispTest_PollRegNotValue                    RC | JS_FALSE
//      STest   DispTest_PollRegLessValue                   RC | JS_FALSE
//      STest   DispTest_PollRegGreaterValue                RC | JS_FALSE
//      STest   DispTest_Poll2RegsGreaterValue              RC | JS_FALSE
//      STest   DispTest_PollRegLessEqualValue              RC | JS_FALSE
//      STest   DispTest_PollRegGreaterEqualValue           RC | JS_FALSE
//      STest   DispTest_PollValue                          RC | JS_FALSE
//      STest   DispTest_PollValueAtAddr                    RC | JS_FALSE
//      STest   DispTest_PollDone                           RC | JS_FALSE
//      STest   DispTest_CrcSetOwner                        RC | JS_FALSE
//      STest   DispTest_ModifyTestName                     RC | JS_FALSE
//      STest   DispTest_ModifySubtestName                  RC | JS_FALSE
//      STest   DispTest_AppendSubtestName                   RC | JS_FALSE
//      STest   DispTest_CrcSetCheckTolerance               RC | JS_FALSE
//      STest   DispTest_CrcSetFModelCheckTolerance         RC | JS_FALSE
//      STest   DispTest_VgaCrlwpdate                       RC | JS_FALSE
//      STest   DispTest_AssignHead                         RC | JS_FALSE
//      STest   DispTest_CrcAddHead                         RC | JS_FALSE
//      STest   DispTest_CrcAddVgaHead                      RC | JS_FALSE
//      STest   DispTest_CrcAddFcodeHead                    RC | JS_FALSE
//      STest   DispTest_CrcNoCheckHead                     RC | JS_FALSE
//      STest   DispTest_CrcAddNotifier                     RC | JS_FALSE
//      STest   DispTest_CrcAddSemaphore                    RC | JS_FALSE
//      STest   DispTest_CrcAddInterrupt                    RC | JS_FALSE
//      STest   DispTest_CrcAddInterruptAndCount            RC | JS_FALSE
//      STest   DispTest_CrcAddNotifierRefHeads             RC | JS_FALSE
//      STest   DispTest_CrcAddSemaphoreRefHeads            RC | JS_FALSE
//      STest   DispTest_CrcAddInterruptRefHeads            RC | JS_FALSE
//      STest   DispTest_CrcSetStartTime                    RC | JS_FALSE
//      STest   DispTest_CrcWriteEventFiles                 RC | JS_FALSE
//      STest   DispTest_Cleanup                            RC | JS_FALSE
//      SMethod DispTest_ReadCrtc                           JS_TRUE | JS_FALSE
//      STest   DispTest_WriteCrtc                          RC | JS_FALSE
//      STest   DispTest_IsoInitVGA                         RC | JS_FALSE
//      STest   DispTest_VGASetClocks                       RC | JS_FALSE
//      STest   DispTest_IsoProgramVPLL                     RC | JS_FALSE
//      STest   DispTest_IsoInitLiteVGA                     RC | JS_FALSE
//      STest   DispTest_IsoShutDowlwGA                     RC | JS_FALSE
//      STest   DispTest_IsoDumpImages                      RC | JS_FALSE
//      STest   DispTest_IsoIndexedRegWrVGA                 RC | JS_FALSE
//      SMethod DispTest_IsoIndexedRegRdVGA                 JS_TRUE | JS_FALSE
//      STest   DispTest_FillVGABackdoor                    RC | JS_FALSE
//      STest   DispTest_SaveVGABackdoor                    RC | JS_FALSE
//      STest   DispTest_BackdoorVgaInit                    RC | JS_FALSE
//      STest   DispTest_LowPower                           RC | JS_FALSE
//      STest   DispTest_LowerPower                         RC | JS_FALSE
//      STest   DispTest_LowestPower                        RC | JS_FALSE
//      STest   DispTest_BlockLevelClockGating              RC | JS_FALSE
//      STest   DispTest_SlowLwclk                          RC | JS_FALSE
//      STest   DispTest_BackdoorVgaRelease                 RC | JS_FALSE
//      STest   DispTest_BackdoorVgaWr08                    RC | JS_FALSE
//      SMethod DispTest_BackdoorVgaRd08                    JS_TRUE | JS_FALSE
//      STest   DispTest_BackdoorVgaWr16                    RC | JS_FALSE
//      SMethod DispTest_BackdoorVgaRd16                    JS_TRUE | JS_FALSE
//      STest   DispTest_BackdoorVgaWr32                    RC | JS_FALSE
//      SMethod DispTest_BackdoorVgaRd32                    JS_TRUE | JS_FALSE
//      STest   DispTest_SetSkipDsiSupervisor2Event         RC | JS_FALSE
//      STest   DispTest_SetPioChannelTimeout               RC | JS_FALSE
//      STest   DispTest_SetSupervisorRestartMode           RC | JS_FALSE
//      STest   DispTest_GetSupervisorRestartMode           RC | JS_FALSE
//      STest   DispTest_SetExceptionRestartMode            RC | JS_FALSE
//      STest   DispTest_GetExceptionRestartMode            RC | JS_FALSE
//      STest   DispTest_SetSkipFreeCount                   RC | JS_FALSE
//      STest   DispTest_SetSwapHeads                       RC | JS_FALSE
//      STest   DispTest_SetUseHead                         RC | JS_FALSE
//      STest   DispTest_SetVPLLRef                         RC | JS_FALSE
//      STest   DispTest_SetVPLLArchType                    RC | JS_FALSE
//      STest   DispTest_SetSorSequencerStartPoint          RC | JS_FALSE
//      STest   DispTest_GetSorSequencerStartPoint          RC | JS_FALSE
//      STest   DispTest_SetPiorSequencerStartPoint         RC | JS_FALSE
//      STest   DispTest_GetPiorSequencerStartPoint         RC | JS_FALSE
//      STest   DispTest_ControlSetSorOpMode                RC | JS_FALSE
//      STest   DispTest_ControlSetPiorOpMode               RC | JS_FALSE
//      STest   DispTest_ControlSetDacConfig                RC | JS_FALSE
//      STest   DispTest_ControlSetErrorMask                RC | JS_FALSE
//      STest   DispTest_ControlSetSorPwmMode               RC | JS_FALSE
//      STest   DispTest_ControlGetDacPwrMode               RC | JS_FALSE
//      STest   DispTest_ControlSetDacPwrMode               RC | JS_FALSE
//      STest   ControlSetRgUnderflowProp                   RC | JS_FALSE
//      STest   ControlSetSemaAcqDelay                      RC | JS_FALSE
//      STest   DispTest_ControlGetSorOpMode                RC | JS_FALSE
//      STest   DispTest_ControlSetVbiosAttentionEvent      RC | JS_FALSE
//      STest   DispTest_ControlSetRgFliplockProp           RC | JS_FALSE
//      STest   DispTest_ControlGetPrevModeSwitchFlags      RC | JS_FALSE
//      STest   DispTest_ControlGetDacLoad                  RC | JS_FALSE
//      STest   DispTest_ControlSetDacLoad                  RC | JS_FALSE
//      STest   DispTest_ControlGetOverlayFlipCount         RC | JS_FALSE
//      STest   DispTest_ControlSetOverlayFlipCount         RC | JS_FALSE
//      STest   DispTest_ControlSetDmiElv                   RC | JS_FALSE
//      STest   DispTest_ControlPrepForResumeFromUnfrUpd    RC | JS_FALSE
//      STest   DispTest_ControlSetUnfrUpdEvent             RC | JS_FALSE
//      STest   StartErrorLogging                           RC | JS_FALSE
//      STest   StopErrorLoggingAndCheck                    JS_TRUE | JS_FALSE
//      STest   InstallErrorLoggerFilter                    JS_TRUE | JS_FALSE
//      STest   DispTest_CollectLwDpsCrcs                   RC | JS_FALSE
//      STest   DispTest_CollectLwstomEvent                 RC | JS_FALSE
//      STest   DispTest_SetHDMIEnable                      RC | JS_FALSE
//      STest   DispTest_GetHDMICapableDisplayIdForSor      RC | JS_FALSE
//      STest   DispTest_SetInterruptHandlerName            RC | JS_FALSE
//      STest   DispTest_SetHDMISinkCaps                    RC | JS_FALSE
//
//
C_(DispTest_SetDebugMessageDisplay);
static STest DispTest_SetDebugMessageDisplay
(
    DispTest_Object,
    "SetDebugMessageDisplay",
    C_DispTest_SetDebugMessageDisplay,
    0,
    "Turn on/off verbose debug messages"
);

C_(DispTest_Initialize);
static STest DispTest_Initialize
(
    DispTest_Object,
    "Initialize",
    C_DispTest_Initialize,
    0,
    "Initialize a Display Engine"
);

C_(DispTest_CrcInitialize);
static STest DispTest_CrcInitialize
(
    DispTest_Object,
    "CrcInitialize",
    C_DispTest_CrcInitialize,
    0,
    "Initialize a Crc Manager"
);

C_(DispTest_StoreBin2FB);
static SMethod DispTest_StoreBin2FB
(
    DispTest_Object,
    "StoreBin2FB",
    C_DispTest_StoreBin2FB,
    4,
    "Write BIN to DFB"
);

C_(DispTest_LoadFB2Bin);
static SMethod DispTest_LoadFB2Bin
(
    DispTest_Object,
    "LoadFB2Bin",
    C_DispTest_LoadFB2Bin,
    4,
    "Read DFB and store data in file"
);

C_(DispTest_AllocDispMemory);
static SMethod DispTest_AllocDispMemory
(
    DispTest_Object,
    "AllocDispMemory",
    C_DispTest_AllocDispMemory,
    1,
    "Allocate memory for Display"
);

C_(DispTest_GetUcodePhyAddr);
static SMethod DispTest_GetUcodePhyAddr
(
    DispTest_Object,
    "GetUcodePhyAddr",
    C_DispTest_GetUcodePhyAddr,
    0,
    "Get physical address for Ucode"
);

C_(DispTest_BootupFalcon);
static SMethod DispTest_BootupFalcon
(
    DispTest_Object,
    "BootupFalcon",
    C_DispTest_BootupFalcon,
    3,
    "Write Regs for Falcon bootup"
);

C_(DispTest_BootupAFalcon);
static SMethod DispTest_BootupAFalcon
(
    DispTest_Object,
    "BootupAFalcon",
    C_DispTest_BootupAFalcon,
    3,
    "Write Regs for Audio Falcon bootup"
);

C_(DispTest_ConfigZPW);
static SMethod DispTest_ConfigZPW
(
    DispTest_Object,
    "ConfigZPW",
    C_DispTest_ConfigZPW,
    4,
    "Config ZPW params"
);

C_(DispTest_Setup);
static SMethod DispTest_Setup
(
    DispTest_Object,
    "Setup",
    C_DispTest_Setup,
    0,
    "Setup a Display Channel"
);

C_(DispTest_Release);
static STest DispTest_Release
(
    DispTest_Object,
    "Release",
    C_DispTest_Release,
    0,
    "Release a Channel"
);

C_(DispTest_BindDevice);
static STest DispTest_SetLwrrentDevice
(
    DispTest_Object,
    "SetLwrrentDevice",
    C_DispTest_BindDevice,
    1,
    "Switch to the given device"
);
static STest DispTest_BindDevice
(
    DispTest_Object,
    "BindDevice",
    C_DispTest_BindDevice,
    1,
    "Switch to the given device"
);

C_(DispTest_SwitchToChannel);
static STest DispTest_SwitchToChannel
(
    DispTest_Object,
    "SwitchToChannel",
    C_DispTest_SwitchToChannel,
    1,
    "Switch to the given display channel"
);

C_(DispTest_IdleChannelControl);
static STest DispTest_IdleChannelControl
(
    DispTest_Object,
    "IdleChannelControl",
    C_DispTest_IdleChannelControl,
    4,
    "wait for channel idle state"
);

C_(DispTest_SetChnStatePollNotif);
static STest DispTest_SetChnStatePollNotif
(
    DispTest_Object,
    "SetChnStatePollNotif",
    C_DispTest_SetChnStatePollNotif,
    5,
    "Set Channel State for Poll Notifier"
);

C_(DispTest_SetChnStatePollNotifBlocking);
static STest DispTest_SetChnStatePollNotifBlocking
(
    DispTest_Object,
    "SetChnStatePollNotifBlocking",
    C_DispTest_SetChnStatePollNotifBlocking,
    3,
    "Set Channel State Poll Notifier Blocking"
);

C_(DispTest_GetChnStatePollNotifRMPolling);
static STest DispTest_GetChnStatePollNotifRMPolling
(
    DispTest_Object,
    "GetChnStatePollNotifRMPolling",
    C_DispTest_GetChnStatePollNotifRMPolling,
    3,
    "Set Channel State Poll Notifier Blocking"
);

C_(DispTest_GetInternalTVRasterTimings);
static STest DispTest_GetInternalTVRasterTimings
(
    DispTest_Object,
    "GetInternalTVRasterTimings",
    C_DispTest_GetInternalTVRasterTimings,
    2,
    "Get TV Raster Timings from RM"
);

C_(DispTest_GetRGStatus);
static STest DispTest_GetRGStatus
(
    DispTest_Object,
    "GetRGStatus",
    C_DispTest_GetRGStatus,
    3,
    "Get RG Status from RM"
);

C_(DispTest_EnqMethod);
static STest DispTest_EnqMethod
(
    DispTest_Object,
    "EnqMethod",
    C_DispTest_EnqMethod,
    0,
    "Enqueue Method and Data into Channel PusbBuffer"
);

C_(DispTest_EnqUpdateAndMode);
static STest DispTest_EnqUpdateAndMode
(
    DispTest_Object,
    "EnqUpdateAndMode",
    C_DispTest_EnqUpdateAndMode,
    0,
    "Enqueue Update into the PB and Set the Content Mode corresponding to the Update"
);

C_(DispTest_EnqCoreUpdateAndMode);
static STest DispTest_EnqCoreUpdateAndMode
(
    DispTest_Object,
    "EnqCoreUpdateAndMode",
    C_DispTest_EnqCoreUpdateAndMode,
    0,
    "Enqueue Core Update into the PB and Set the Content Mode corresponding to the Update"
);

C_(DispTest_EnqCrcMode);
static STest DispTest_EnqCrcMode
(
    DispTest_Object,
    "EnqCrcMode",
    C_DispTest_EnqCrcMode,
    0,
    "Enqueue just a crc mode string for a given head but do not actually enqueue an update."
);

C_(DispTest_EnqMethodMulti);
static STest DispTest_EnqMethodMulti
(
    DispTest_Object,
    "EnqMethodMulti",
    C_DispTest_EnqMethodMulti,
    0,
    "Enqueue Method and Multiple Data into Channel PusbBuffer"
);

C_(DispTest_EnqMethodNonInc);
static STest DispTest_EnqMethodNonInc
(
    DispTest_Object,
    "EnqMethodNonInc",
    C_DispTest_EnqMethodNonInc,
    0,
    "Enqueue Method and Data into Channel PusbBuffer"
);

C_(DispTest_EnqMethodNonIncMulti);
static STest DispTest_EnqMethodNonIncMulti
(
    DispTest_Object,
    "EnqMethodNonIncMulti",
    C_DispTest_EnqMethodNonIncMulti,
    0,
    "Enqueue Method and Multiple Data into Channel PusbBuffer"
);

C_(DispTest_EnqMethodNop);
static STest DispTest_EnqMethodNop
(
    DispTest_Object,
    "EnqMethodNop",
    C_DispTest_EnqMethodNop,
    0,
    "Enqueue Nop Method into Channel PusbBuffer"
);

C_(DispTest_EnqMethodJump);
static STest DispTest_EnqMethodJump
(
    DispTest_Object,
    "EnqMethodJump",
    C_DispTest_EnqMethodJump,
    0,
    "Enqueue Jump Method into Channel PusbBuffer"
);

C_(DispTest_EnqMethodSetSubdevice);
static STest DispTest_EnqMethodSetSubdevice
(
    DispTest_Object,
    "EnqMethodSetSubdevice",
    C_DispTest_EnqMethodSetSubdevice,
    0,
    "Enqueue SetSubdevice Method into Channel PusbBuffer"
);

C_(DispTest_EnqMethodOpcode);
static STest DispTest_EnqMethodOpcode
(
    DispTest_Object,
    "EnqMethodOpcode",
    C_DispTest_EnqMethodOpcode,
    0,
    "Enqueue Channel PusbBuffer a method with the specified OPCODE"
);

C_(DispTest_SetAutoFlush);
static STest DispTest_SetAutoFlush
(
    DispTest_Object,
    "SetAutoFlush",
    C_DispTest_SetAutoFlush,
    0,
    "Control AutoFlush behavior for Channel PushBuffer"
);

C_(DispTest_Flush);
static STest DispTest_Flush
(
    DispTest_Object,
    "Flush",
    C_DispTest_Flush,
    0,
    "Flush Channel and Update PushBuffer Put Pointer"
);

C_(DispTest_BindContextDma);
static STest DispTest_BindContextDma
(
    DispTest_Object,
    "BindContextDma",
    C_DispTest_BindContextDma,
    0,
    "Bind a context DMA to a channel"
);

C_(DispTest_CreateNotifierContextDma);
static SMethod DispTest_CreateNotifierContextDma
(
    DispTest_Object,
    "CreateNotifierContextDma",
    C_DispTest_CreateNotifierContextDma,
    0,
    "Allocate Memory and Create a Notifier Ctx DMA"
);

C_(DispTest_CreateSemaphoreContextDma);
static SMethod DispTest_CreateSemaphoreContextDma
(
    DispTest_Object,
    "CreateSemaphoreContextDma",
    C_DispTest_CreateSemaphoreContextDma,
    0,
    "Allocate Memory and Create a Semaphore Ctx DMA"
);

C_(DispTest_CreateContextDma);
static SMethod DispTest_CreateContextDma
(
    DispTest_Object,
    "CreateContextDma",
    C_DispTest_CreateContextDma,
    0,
    "Allocate Memory and Create a Ctx DMA"
);

C_(DispTest_DeleteContextDma);
static STest DispTest_DeleteContextDma
(
    DispTest_Object,
    "DeleteContextDma",
    C_DispTest_DeleteContextDma,
    0,
    "Release Context DMA Handle and Free Allocated Memory"
);

C_(DispTest_SetContextDmaDebugRegs);
static STest DispTest_SetContextDmaDebugRegs
(
    DispTest_Object,
    "SetContextDmaDebugRegs",
    C_DispTest_SetContextDmaDebugRegs,
    0,
    "Write the Context Dma Params to the Debug Regs"
);

C_(DispTest_WriteVal32);
static STest DispTest_WriteVal32
(
    DispTest_Object,
    "WriteVal32",
    C_DispTest_WriteVal32,
    0,
    "Write Value to Memory"
);

C_(DispTest_ReadVal32);
static SMethod DispTest_ReadVal32
(
    DispTest_Object,
    "ReadVal32",
    C_DispTest_ReadVal32,
    0,
    "Read Value from Memory"
);

C_(DispTest_PollHWValue);
static STest DispTest_PollHWValue
(
    DispTest_Object,
    "PollHWValue",
    C_DispTest_PollHWValue,
    0,
    "Poll on given Address for a HWValue"
);

C_(DispTest_PollHWGreaterEqualValue);
static STest DispTest_PollHWGreaterEqualValue
(
    DispTest_Object,
    "PollHWGreaterEqualValue",
    C_DispTest_PollHWGreaterEqualValue,
    0,
    "Poll on given Address for a HWValue greater than given value"
);

C_(DispTest_PollHWLessEqualValue);
static STest DispTest_PollHWLessEqualValue
(
    DispTest_Object,
    "PollHWLessEqualValue",
    C_DispTest_PollHWLessEqualValue,
    0,
    "Poll on given Address for a HWValue greater than given value"
);

C_(DispTest_PollHWNotValue);
static STest DispTest_PollHWNotValue
(
    DispTest_Object,
    "PollHWNotValue",
    C_DispTest_PollHWNotValue,
    0,
    "Poll on given Address for a HWValue"
);

C_(DispTest_PollIORegValue);
static STest DispTest_PollIORegValue
(
    DispTest_Object,
    "PollIORegValue",
    C_DispTest_PollIORegValue,
    0,
    "Poll on given Address for a IORegValue"
);

C_(DispTest_PollRegValue);
static STest DispTest_PollRegValue
(
    DispTest_Object,
    "PollRegValue",
    C_DispTest_PollRegValue,
    0,
    "Poll on given Address for a RegValue"
);

C_(DispTest_PollRegNotValue);
static STest DispTest_PollRegNotValue
(
    DispTest_Object,
    "PollRegNotValue",
    C_DispTest_PollRegNotValue,
    0,
    "Poll on given Address for a RegValue"
);

C_(DispTest_PollRegLessValue);
static STest DispTest_PollRegLessValue
(
    DispTest_Object,
    "PollRegLessValue",
    C_DispTest_PollRegLessValue,
    0,
    "Poll on given Address for a RegValue less than given value"
);

C_(DispTest_PollRegGreaterValue);
static STest DispTest_PollRegGreaterValue
(
    DispTest_Object,
    "PollRegGreaterValue",
    C_DispTest_PollRegGreaterValue,
    0,
    "Poll on given Address for a RegValue greater than given value"
);

C_(DispTest_Poll2RegsGreaterValue);
static STest DispTest_Poll2RegsGreaterValue
(
    DispTest_Object,
    "Poll2RegsGreaterValue",
    C_DispTest_Poll2RegsGreaterValue,
    0,
    "Poll on two given Addresses for RegValues greater than given values"
);

C_(DispTest_PollRegLessEqualValue);
static STest DispTest_PollRegLessEqualValue
(
    DispTest_Object,
    "PollRegLessEqualValue",
    C_DispTest_PollRegLessEqualValue,
    0,
    "Poll on given Address for a RegValue less than given value"
);

C_(DispTest_PollRegGreaterEqualValue);
static STest DispTest_PollRegGreaterEqualValue
(
    DispTest_Object,
    "PollRegGreaterEqualValue",
    C_DispTest_PollRegGreaterEqualValue,
    0,
    "Poll on given Address for a RegValue greater than given value"
);

C_(DispTest_PollValue);
static STest DispTest_PollValue
(
    DispTest_Object,
    "PollValue",
    C_DispTest_PollValue,
    0,
    "Poll on given Address for a Value"
);

C_(DispTest_PollGreaterEqualValue);
static STest DispTest_PollGreaterEqualValue
(
    DispTest_Object,
    "PollGreaterEqualValue",
    C_DispTest_PollGreaterEqualValue,
    0,
    "Poll on given Address for a Value"
);

C_(DispTest_PollValueAtAddr);
static STest DispTest_PollValueAtAddr
(
    DispTest_Object,
    "PollValueAtAddr",
    C_DispTest_PollValueAtAddr,
    0,
    "Poll on given Address for a Value"
);

C_(DispTest_PollScanlineGreaterEqualValue);
static STest DispTest_PollScanlineGreaterEqualValue
(
    DispTest_Object,
    "PollScanlineGreaterEqualValue",
    C_DispTest_PollScanlineGreaterEqualValue,
    3,
    "Poll on Scanline for a Value"
);

C_(DispTest_PollScanlineLessValue);
static STest DispTest_PollScanlineLessValue
(
    DispTest_Object,
    "PollScanlineLessValue",
    C_DispTest_PollScanlineLessValue,
    3,
    "Poll on Scanline for a Value"
);

C_(DispTest_PollDone);
static STest DispTest_PollDone
(
    DispTest_Object,
    "PollDone",
    C_DispTest_PollDone,
    0,
    "Poll on given Address for Done Bit"
);

C_(DispTest_PollRGScanLocked);
static STest DispTest_PollRGScanLocked
(
    DispTest_Object,
    "PollRGScanLocked",
    C_DispTest_PollRGScanLocked,
    2,
    "Poll for RG Scan Lock"
);

C_(DispTest_PollRGFlipLocked);
static STest DispTest_PollRGFlipLocked
(
    DispTest_Object,
    "PollRGFlipLocked",
    C_DispTest_PollRGFlipLocked,
    2,
    "Poll for RG Flip Lock"
);

C_(DispTest_PollRGScanUnlocked);
static STest DispTest_PollRGScanUnlocked
(
    DispTest_Object,
    "PollRGScanUnlocked",
    C_DispTest_PollRGScanUnlocked,
    2,
    "Poll for RG Scan Not Locked"
);

C_(DispTest_PollRGFlipUnlocked);
static STest DispTest_PollRGFlipUnlocked
(
    DispTest_Object,
    "PollRGFlipUnlocked",
    C_DispTest_PollRGFlipUnlocked,
    2,
    "Poll for RG Flip Not Locked"
);

C_(DispTest_CrcSetOwner);
static STest DispTest_CrcSetOwner
(
    DispTest_Object,
    "CrcSetOwner",
    C_DispTest_CrcSetOwner,
    0,
    "Set the Owner field for CRC output files"
);

C_(DispTest_ModifyTestName);
static STest DispTest_ModifyTestName
(
    DispTest_Object,
    "ModifyTestName",
    C_DispTest_ModifyTestName,
    0,
    "Set the test name for CRC output files"
);

C_(DispTest_ModifySubtestName);
static STest DispTest_ModifySubtestName
(
    DispTest_Object,
    "ModifySubtestName",
    C_DispTest_ModifySubtestName,
    0,
    "Set the subtest name for CRC output files"
);

C_(DispTest_AppendSubtestName);
static STest DispTest_AppendSubtestName
(
    DispTest_Object,
    "AppendSubtestName",
    C_DispTest_AppendSubtestName,
    0,
    "Append to the subtest name for CRC output files"
);

C_(DispTest_CrcSetCheckTolerance);
static STest DispTest_CrcSetCheckTolerance
(
    DispTest_Object,
    "CrcSetCheckTolerance",
    C_DispTest_CrcSetCheckTolerance,
    0,
    "Set the lead.crc/gold.crc tolerance for non-FModel runs"
);

C_(DispTest_CrcSetFModelCheckTolerance);
static STest DispTest_CrcSetFModelCheckTolerance
(
    DispTest_Object,
    "CrcSetFModelCheckTolerance",
    C_DispTest_CrcSetFModelCheckTolerance,
    0,
    "Set the lead.crc/gold.crc tolerance for FModel runs"
);

C_(DispTest_VgaCrlwpdate);
static STest DispTest_VgaCrlwpdate
(
    DispTest_Object,
    "VgaCrlwpdate",
    C_DispTest_VgaCrlwpdate,
    0,
    "Capture VGA Crcs"
);

C_(DispTest_AssignHead);
static STest DispTest_AssignHead
(
    DispTest_Object,
    "AssignHead",
    C_DispTest_AssignHead,
    0,
    "Capture Physical Head to Logical Head"
);

C_(DispTest_CrcAddHead);
static STest DispTest_CrcAddHead
(
    DispTest_Object,
    "CrcAddHead",
    C_DispTest_CrcAddHead,
    0,
    "Add CRC notifier for a specific head"
);

C_(DispTest_CrcAddVgaHead);
static STest DispTest_CrcAddVgaHead
(
    DispTest_Object,
    "CrcAddVgaHead",
    C_DispTest_CrcAddVgaHead,
    0,
    "Add Vga CRC handler for a specific head"
);

C_(DispTest_CrcAddFcodeHead);
static STest DispTest_CrcAddFcodeHead
(
    DispTest_Object,
    "CrcAddFcodeHead",
    C_DispTest_CrcAddFcodeHead,
    0,
    "Add Fcode CRC handler for a specific head"
);

C_(DispTest_CrcNoCheckHead);
static STest DispTest_CrcNoCheckHead
(
    DispTest_Object,
    "CrcNoCheckHead",
    C_DispTest_CrcNoCheckHead,
    0,
    "Disable Crc Checking for Head"
);

C_(DispTest_CrcAddNotifier);
static STest DispTest_CrcAddNotifier
(
    DispTest_Object,
    "CrcAddNotifier",
    C_DispTest_CrcAddNotifier,
    0,
    "Add notifier to the set for which to generate CRCs"
);

C_(DispTest_CrcAddSemaphore);
static STest DispTest_CrcAddSemaphore
(
    DispTest_Object,
    "CrcAddSemaphore",
    C_DispTest_CrcAddSemaphore,
    0,
    "Add semaphore to the set for which to generate CRCs"
);

C_(DispTest_CrcAddInterrupt);
static STest DispTest_CrcAddInterrupt
(
    DispTest_Object,
    "CrcAddInterrupt",
    C_DispTest_CrcAddInterrupt,
    0,
    "Add interrupt to the set for which to generate CRCs"
);

C_(DispTest_CrcAddInterruptAndCount);
static STest DispTest_CrcAddInterruptAndCount
(
    DispTest_Object,
    "CrcAddInterruptAndCount",
    C_DispTest_CrcAddInterruptAndCount,
    0,
    "Add interrupt to the set for which to generate CRCs"
);

C_(DispTest_CrcAddNotifierRefHeads);
static STest DispTest_CrcAddNotifierRefHeads
(
    DispTest_Object,
    "CrcAddNotifierRefHeads",
    C_DispTest_CrcAddNotifierRefHeads,
    0,
    "Add notifier to the set for which to generate CRCs"
);

C_(DispTest_CrcAddSemaphoreRefHeads);
static STest DispTest_CrcAddSemaphoreRefHeads
(
    DispTest_Object,
    "CrcAddSemaphoreRefHeads",
    C_DispTest_CrcAddSemaphoreRefHeads,
    0,
    "Add semaphore to the set for which to generate CRCs"
);

C_(DispTest_CrcAddInterruptRefHeads);
static STest DispTest_CrcAddInterruptRefHeads
(
    DispTest_Object,
    "CrcAddInterruptRefHeads",
    C_DispTest_CrcAddInterruptRefHeads,
    0,
    "Add interrupt to the set for which to generate CRCs"
);

C_(DispTest_CollectLwstomEvent);
static STest DispTest_CollectLwstomEvent
(
    DispTest_Object,
    "CollectLwstomEvent",
    C_DispTest_CollectLwstomEvent,
    2,
    "Collect strings for later output to the specified custom file"
);

C_(DispTest_CollectLwDpsCrcs);
static STest DispTest_CollectLwDpsCrcs
(
    DispTest_Object,
    "CollectLwDpsCrcs",
    C_DispTest_CollectLwDpsCrcs,
    1,
    "Collect LwDps strip CRCs for the specified head"
);

C_(DispTest_CrcSetStartTime);
static STest DispTest_CrcSetStartTime
(
    DispTest_Object,
    "CrcSetStartTime",
    C_DispTest_CrcSetStartTime,
    0,
    "Set the CRC start time to be the current time"
);

C_(DispTest_CrcWriteEventFiles);
static STest DispTest_CrcWriteEventFiles
(
    DispTest_Object,
    "CrcWriteEventFiles",
    C_DispTest_CrcWriteEventFiles,
    0,
    "Write the event CRC output files"
);

C_(DispTest_Cleanup);
static STest DispTest_Cleanup
(
    DispTest_Object,
    "Cleanup",
    C_DispTest_Cleanup,
    0,
    "Cleanup the Test resources"
);

C_(DispTest_ReadCrtc);
static SMethod DispTest_ReadCrtc
(
    DispTest_Object,
    "ReadCrtc",
    C_DispTest_ReadCrtc,
    0,
    "Read from a Crtc Register"
);

C_(DispTest_WriteCrtc);
static STest DispTest_WriteCrtc
(
    DispTest_Object,
    "WriteCrtc",
    C_DispTest_WriteCrtc,
    0,
    "Write to a Crtc Register"
);

C_(DispTest_IsoInitVGA);
static STest DispTest_IsoInitVGA
(
    DispTest_Object,
    "IsoInitVGA",
    C_DispTest_IsoInitVGA,
    0,
    "Initialize VGA"
);

C_(DispTest_VGASetClocks);
static STest DispTest_VGASetClocks
(
    DispTest_Object,
    "VGASetClocks",
    C_DispTest_VGASetClocks,
    0,
    "Set the VGA clocks"
);

C_(DispTest_IsoProgramVPLL);
static STest DispTest_IsoProgramVPLL
(
    DispTest_Object,
    "IsoProgramVPLL",
    C_DispTest_IsoProgramVPLL,
    0,
    "Programs the VPLL coeff"
);

C_(DispTest_IsoInitLiteVGA);
static STest DispTest_IsoInitLiteVGA
(
    DispTest_Object,
    "IsoInitLiteVGA",
    C_DispTest_IsoInitLiteVGA,
    0,
    "Lite VGA init"
);

C_(DispTest_IsoShutDowlwGA);
static STest DispTest_IsoShutDowlwGA
(
    DispTest_Object,
    "IsoShutDowlwGA",
    C_DispTest_IsoShutDowlwGA,
    0,
    "Shut down VGA"
);

C_(DispTest_IsoDumpImages);
static STest DispTest_IsoDumpImages
(
    DispTest_Object,
    "IsoDumpImages",
    C_DispTest_IsoDumpImages,
    0,
    "Saves RTL output into .tga files"
);

C_(DispTest_IsoIndexedRegWrVGA);
static STest DispTest_IsoIndexedRegWrVGA
(
    DispTest_Object,
    "IsoIndexedRegWrVGA",
    C_DispTest_IsoIndexedRegWrVGA,
    0,
    "write indirectly to I/O port"
);

C_(DispTest_IsoIndexedRegRdVGA);
static SMethod DispTest_IsoIndexedRegRdVGA
(
    DispTest_Object,
    "IsoIndexedRegRdVGA",
    C_DispTest_IsoIndexedRegRdVGA,
    0,
    "write indirectly to I/O port"
);

C_(DispTest_FillVGABackdoor);
static STest DispTest_FillVGABackdoor
(
    DispTest_Object,
    "FillVGABackdoor",
    C_DispTest_FillVGABackdoor,
    3,
    "fill VGA with backdoor content from file"
);

C_(DispTest_SaveVGABackdoor);
static STest DispTest_SaveVGABackdoor
(
    DispTest_Object,
    "SaveVGABackdoor",
    C_DispTest_SaveVGABackdoor,
    3,
    "save VGA backdoor content to file"
);

C_(DispTest_BackdoorVgaInit);
static STest DispTest_BackdoorVgaInit
(
    DispTest_Object,
    "BackdoorVgaInit",
    C_DispTest_BackdoorVgaInit,
    0,
    "initialize FB memory pointer for VGA space"
);

C_(DispTest_LowPower);
static STest DispTest_LowPower
(
    DispTest_Object,
    "LowPower",
    C_DispTest_LowPower,
    0,
    "enable power management, conservative savings mode"
);

C_(DispTest_LowerPower);
static STest DispTest_LowerPower
(
    DispTest_Object,
    "LowerPower",
    C_DispTest_LowerPower,
    0,
    "enable power management, modest savings mode"
);

C_(DispTest_LowestPower);
static STest DispTest_LowestPower
(
    DispTest_Object,
    "LowestPower",
    C_DispTest_LowestPower,
    0,
    "enable power management, aggressive savings mode"
);

C_(DispTest_BlockLevelClockGating);
static STest DispTest_BlockLevelClockGating
(
    DispTest_Object,
    "BlockLevelClockGating",
    C_DispTest_BlockLevelClockGating,
    0,
    "enable block level clock gating"
);

C_(DispTest_SlowLwclk);
static STest DispTest_SlowLwclk
(
    DispTest_Object,
    "SlowLwclk",
    C_DispTest_SlowLwclk,
    0,
    "enable lwclk slowdown"
);

C_(DispTest_BackdoorVgaRelease);
static STest DispTest_BackdoorVgaRelease
(
    DispTest_Object,
    "BackdoorVgaRelease",
    C_DispTest_BackdoorVgaRelease,
    0,
    "release FB memory pointer for VGA space"
);

C_(DispTest_BackdoorVgaWr08);
static STest DispTest_BackdoorVgaWr08
(
    DispTest_Object,
    "BackdoorVgaWr08",
    C_DispTest_BackdoorVgaWr08,
    2,
    "write 8 bits to VGA space at offset"
);

C_(DispTest_BackdoorVgaRd08);
static SMethod DispTest_BackdoorVgaRd08
(
    DispTest_Object,
    "BackdoorVgaRd08",
    C_DispTest_BackdoorVgaRd08,
    1,
    "read 8 bits from VGA space at offset"
);

C_(DispTest_BackdoorVgaWr16);
static STest DispTest_BackdoorVgaWr16
(
    DispTest_Object,
    "BackdoorVgaWr16",
    C_DispTest_BackdoorVgaWr16,
    2,
    "write 16 bits to VGA space at offset"
);

C_(DispTest_BackdoorVgaRd16);
static SMethod DispTest_BackdoorVgaRd16
(
    DispTest_Object,
    "BackdoorVgaRd16",
    C_DispTest_BackdoorVgaRd16,
    1,
    "read 16 bits from VGA space at offset"
);

C_(DispTest_BackdoorVgaWr32);
static STest DispTest_BackdoorVgaWr32
(
    DispTest_Object,
    "BackdoorVgaWr32",
    C_DispTest_BackdoorVgaWr32,
    2,
    "write 32 bits to VGA space at offset"
);

C_(DispTest_BackdoorVgaRd32);
static SMethod DispTest_BackdoorVgaRd32
(
    DispTest_Object,
    "BackdoorVgaRd32",
    C_DispTest_BackdoorVgaRd32,
    1,
    "read 32 bits from VGA space at offset"
);

C_(DispTest_SetSkipDsiSupervisor2Event);
static STest DispTest_SetSkipDsiSupervisor2Event
(
    DispTest_Object,
    "SetSkipDsiSupervisor2Event",
    C_DispTest_SetSkipDsiSupervisor2Event,
    1,
    "register a SkipDsiSupervisor2Event with the RM"
);

C_(DispTest_SetPioChannelTimeout);
static STest DispTest_SetPioChannelTimeout
(
    DispTest_Object,
    "SetPioChannelTimeout",
    C_DispTest_SetPioChannelTimeout,
    2,
    "Set the Pio Channel Timout"
);

C_(DispTest_SetSupervisorRestartMode);
static STest DispTest_SetSupervisorRestartMode
(
    DispTest_Object,
    "SetSupervisorRestartMode",
    C_DispTest_SetSupervisorRestartMode,
    2,
    "Indicate change of supervisor interrupt handling to RM"
);

C_(DispTest_GetSupervisorRestartMode);
static STest DispTest_GetSupervisorRestartMode
(
    DispTest_Object,
    "GetSupervisorRestartMode",
    C_DispTest_GetSupervisorRestartMode,
    2,
    "Retrieve supervisor interrupt handling"
);

C_(DispTest_SetExceptionRestartMode);
static STest DispTest_SetExceptionRestartMode
(
    DispTest_Object,
    "SetExceptionRestartMode",
    C_DispTest_SetExceptionRestartMode,
    2,
    "Indicate means of exception handling to RM"
);

C_(DispTest_GetExceptionRestartMode);
static STest DispTest_GetExceptionRestartMode
(
    DispTest_Object,
    "GetExceptionRestartMode",
    C_DispTest_GetExceptionRestartMode,
    3,
    "Retrieve means of exception handling"
);

C_(DispTest_SetSkipFreeCount);
static STest DispTest_SetSkipFreeCount
(
    DispTest_Object,
    "SetSkipFreeCount",
    C_DispTest_SetSkipFreeCount,
    2,
    "skip checking of free count for PIO channels"
);

C_(DispTest_SetSwapHeads);
static STest DispTest_SetSwapHeads
(
    DispTest_Object,
    "SetSwapHeads",
    C_DispTest_SetSwapHeads,
    1,
    "skip checking of free count for PIO channels"
);

C_(DispTest_SetUseHead);
static STest DispTest_SetUseHead
(
    DispTest_Object,
    "SetUseHead",
    C_DispTest_SetUseHead,
    2,
    "skip checking of free count for PIO channels"
);

C_(DispTest_SetVPLLRef);
static STest DispTest_SetVPLLRef
(
    DispTest_Object,
    "SetVPLLRef",
    C_DispTest_SetVPLLRef,
    3,
    "selects VPLL input reference"
);

C_(DispTest_SetVPLLArchType);
static STest DispTest_SetVPLLArchType
(
    DispTest_Object,
    "SetVPLLArchType",
    C_DispTest_SetVPLLArchType,
    2,
    "selects VPLL Arch Type"
);

C_(DispTest_SetSorSequencerStartPoint);
static STest DispTest_SetSorSequencerStartPoint
(
    DispTest_Object,
    "SetSorSequencerStartPoint",
    C_DispTest_SetSorSequencerStartPoint,
    18,
    "specify alternate start points in SOR sequencer"
);

C_(DispTest_GetSorSequencerStartPoint);
static STest DispTest_GetSorSequencerStartPoint
(
    DispTest_Object,
    "GetSorSequencerStartPoint",
    C_DispTest_GetSorSequencerStartPoint,
    4,
    "retrieve the alternate start points in SOR sequencer"
);

C_(DispTest_SetPiorSequencerStartPoint);
static STest DispTest_SetPiorSequencerStartPoint
(
    DispTest_Object,
    "SetPiorSequencerStartPoint",
    C_DispTest_SetPiorSequencerStartPoint,
    18,
    "specify alternate start points in PIOR sequencer"
);

C_(DispTest_GetPiorSequencerStartPoint);
static STest DispTest_GetPiorSequencerStartPoint
(
    DispTest_Object,
    "GetPiorSequencerStartPoint",
    C_DispTest_GetPiorSequencerStartPoint,
    4,
    "retrieve the alternate start points in PIOR sequencer"
);

C_(DispTest_ControlSetActiveSubdevice);
static STest DispTest_ControlSetActiveSubdevice
(
    DispTest_Object,
    "ControlSetActiveSubdevice",
    C_DispTest_ControlSetActiveSubdevice,
    1,
    "Set active subdevice index for display control"
);

C_(DispTest_ControlSetSorOpMode);
static STest DispTest_ControlSetSorOpMode
(
    DispTest_Object,
    "ControlSetSorOpMode",
    C_DispTest_ControlSetSorOpMode,
    17,
    "Set SOR Op Mode"
);

C_(DispTest_ControlSetPiorOpMode);
static STest DispTest_ControlSetPiorOpMode
(
    DispTest_Object,
    "ControlSetPiorOpMode",
    C_DispTest_ControlSetPiorOpMode,
    10,
    "Set PIOR Op Mode"
);

C_(DispTest_ControlSetUseTestPiorSettings);
static STest DispTest_ControlSetUseTestPiorSettings
(
    DispTest_Object,
    "ControlSetUseTestPiorSettings",
    C_DispTest_ControlSetUseTestPiorSettings,
    1,
    "Use Test Pior Settings"
);

C_(DispTest_ControlGetPiorOpMode);
static STest DispTest_ControlGetPiorOpMode
(
    DispTest_Object,
    "ControlGetPiorOpMode",
    C_DispTest_ControlGetPiorOpMode,
    10,
    "Get PIOR Op Mode"
);

C_(DispTest_ControlSetSfBlank);
static STest DispTest_ControlSetSfBlank
(
    DispTest_Object,
    "ControlSetSfBlank",
    C_DispTest_ControlSetSfBlank,
    4,
    "Set SF Blank"
);

C_(DispTest_ControlSetDacConfig);
static STest DispTest_ControlSetDacConfig
(
    DispTest_Object,
    "ControlSetDacConfig",
    C_DispTest_ControlSetDacConfig,
    10,
    "Set Dac Config"
);

C_(DispTest_ControlSetErrorMask);
static STest DispTest_ControlSetErrorMask
(
    DispTest_Object,
    "ControlSetErrorMask",
    C_DispTest_ControlSetErrorMask,
    4,
    "Set Error Mask"
);

C_(DispTest_ControlSetSorPwmMode);
static STest DispTest_ControlSetSorPwmMode
(
    DispTest_Object,
    "ControlSetSorPwmMode",
    C_DispTest_ControlSetSorPwmMode,
    7,
    "Set SOR Pwr Mode"
);

C_(DispTest_ControlGetDacPwrMode);
static STest DispTest_ControlGetDacPwrMode
(
    DispTest_Object,
    "ControlGetDacPwrMode",
    C_DispTest_ControlGetDacPwrMode,
    2,
    "Get DAC Pwr Mode"
);

C_(DispTest_ControlSetDacPwrMode);
static STest DispTest_ControlSetDacPwrMode
(
    DispTest_Object,
    "ControlSetDacPwrMode",
    C_DispTest_ControlSetDacPwrMode,
    10,
    "Set DAC Pwr Mode"
);

C_(DispTest_ControlSetRgUnderflowProp);
static STest ControlSetRgUnderflowProp
(
    DispTest_Object,
    "ControlSetRgUnderflowProp",
    C_DispTest_ControlSetRgUnderflowProp,
    4,
    "Set RG Underflow Prop"
);

C_(DispTest_ControlSetSemaAcqDelay);
static STest ControlSetSemaAcqDelay
(
    DispTest_Object,
    "ControlSetSemaAcqDelay",
    C_DispTest_ControlSetSemaAcqDelay,
    1,
    "Set Semaphore Acquire Delay"
);

C_(DispTest_ControlGetSorOpMode);
static STest DispTest_ControlGetSorOpMode
(
    DispTest_Object,
    "ControlGetSorOpMode",
    C_DispTest_ControlGetSorOpMode,
    18,
    "Retrieves the current SOR Op Mode settings"
);

C_(DispTest_ControlSetVbiosAttentionEvent);
static STest DispTest_ControlSetVbiosAttentionEvent
(
    DispTest_Object,
    "ControlSetVbiosAttentionEvent",
    C_DispTest_ControlSetVbiosAttentionEvent,
    1,
    "Set call back function used by RM in event of VBIOS_ATTENTION_EVENT interrupt been fired and cleared."
);

C_(DispTest_ControlSetRgFliplockProp);
static STest DispTest_ControlSetRgFliplockProp
(
    DispTest_Object,
    "ControlSetRgFliplockProp",
    C_DispTest_ControlSetRgFliplockProp,
    3,
    "Set RG Fliplock Prop"
);

C_(DispTest_ControlGetPrevModeSwitchFlags);
static STest DispTest_ControlGetPrevModeSwitchFlags
(
    DispTest_Object,
    "ControlGetPrevModeSwitchFlags",
    C_DispTest_ControlGetPrevModeSwitchFlags,
    1,
    "Get previous modeswitch flags"
);

C_(DispTest_ControlGetDacLoad);
static STest DispTest_ControlGetDacLoad
(
    DispTest_Object,
    "ControlGetDacLoad",
    C_DispTest_ControlGetDacLoad,
    2,
    "Get DAC load"
);

C_(DispTest_ControlSetDacLoad);
static STest DispTest_ControlSetDacLoad
(
    DispTest_Object,
    "ControlSetDacLoad",
    C_DispTest_ControlSetDacLoad,
    7,
    "Set DAC load"
);

C_(DispTest_ControlGetOverlayFlipCount);
static STest DispTest_ControlGetOverlayFlipCount
(
    DispTest_Object,
    "ControlGetOverlayFlipCount",
    C_DispTest_ControlGetOverlayFlipCount,
    2,
    "Get Overlay FlipCount"
);

C_(DispTest_ControlSetOverlayFlipCount);
static STest DispTest_ControlSetOverlayFlipCount
(
    DispTest_Object,
    "ControlSetOverlayFlipCount",
    C_DispTest_ControlSetOverlayFlipCount,
    2,
    "Set Overlay FlipCount"
);

C_(DispTest_ControlSetDmiElv);
static STest DispTest_ControlSetDmiElv
(
    DispTest_Object,
    "ControlSetDmiElv",
    C_DispTest_ControlSetDmiElv,
    3,
    "Set DMI Early Load V"
);

C_(DispTest_ControlPrepForResumeFromUnfrUpd);
static STest DispTest_ControlPrepForResumeFromUnfrUpd
(
    DispTest_Object,
    "ControlPrepForResumeFromUnfrUpd",
    C_DispTest_ControlPrepForResumeFromUnfrUpd,
    0,
    "Wait for RM to signal that it is ok to proceed."
);

C_(DispTest_ControlSetUnfrUpdEvent);
static STest DispTest_ControlSetUnfrUpdEvent
(
    DispTest_Object,
    "ControlSetUnfrUpdEvent",
    C_DispTest_ControlSetUnfrUpdEvent,
    1,
    "Set call back function used by RM if an unfriendly vbios grab is detected."
);

C_(DispTest_GetHDMICapableDisplayIdForSor);
static STest DispTest_GetHDMICapableDisplayIdForSor
(
    DispTest_Object,
    "GetHDMICapableDisplayIdForSor",
    C_DispTest_GetHDMICapableDisplayIdForSor,
    2,
    "gets the displayId of a HDMI capable device for a given sor if available"
);

C_(DispTest_SetInterruptHandlerName);
static STest DispTest_SetInterruptHandlerName
(
    DispTest_Object,
    "SetInterruptHandlerName",
    C_DispTest_SetInterruptHandlerName,
    2,
    "Set custom interrupt handler function name"
);

C_(DispTest_SetHDMIEnable);
static STest DispTest_SetHDMIEnable
(
    DispTest_Object,
    "SetHDMIEnable",
    C_DispTest_SetHDMIEnable,
    2,
    "Enables/Disables HDMI on a given displayId"
);

C_(DispTest_StartErrorLogging);
static STest StartErrorLogging
(
    DispTest_Object,
    "StartErrorLogging",
    C_DispTest_StartErrorLogging,
    1,
    "Start Error Logging"
);

C_(DispTest_StopErrorLoggingAndCheck);
static STest StopErrorLoggingAndCheck
(
    DispTest_Object,
    "StopErrorLoggingAndCheck",
    C_DispTest_StopErrorLoggingAndCheck,
    1,
    "Stop Error Logging And Check"
);

C_(DispTest_InstallErrorLoggerFilter);
static STest InstallErrorLoggerFilter
(
    DispTest_Object,
    "InstallErrorLoggerFilter",
    C_DispTest_InstallErrorLoggerFilter,
    1,
    "Install a ErrorLogger Filter"
);

C_(DispTest_GetBaseChannelScanline);
static SMethod GetBaseChannelScanline
(
    DispTest_Object,
    "GetBaseChannelScanline",
    C_DispTest_GetBaseChannelScanline,
    1,
    "Get Base Channel Scanline"
);

C_(DispTest_GetChannelPut);
static SMethod GetChannelPut
(
    DispTest_Object,
    "GetChannelPut",
    C_DispTest_GetChannelPut,
    1,
    "Get Channel Put Pointer"
);

C_(DispTest_SetChannelPut);
static STest SetChannelPut
(
    DispTest_Object,
    "SetChannelPut",
    C_DispTest_SetChannelPut,
    2,
    "Set Channel Put Pointer"
);

C_(DispTest_GetChannelGet);
static SMethod GetChannelGet
(
    DispTest_Object,
    "GetChannelGet",
    C_DispTest_GetChannelGet,
    1,
    "Get Channel Get Pointer"
);

C_(DispTest_SetChannelGet);
static STest SetChannelGet
(
    DispTest_Object,
    "SetChannelGet",
    C_DispTest_SetChannelGet,
    2,
    "Set Channel Get Pointer"
);

C_(DispTest_GetCrcInterruptCount);
static SMethod GetCrcInterruptCount
(
    DispTest_Object,
    "GetCrcInterruptCount",
    C_DispTest_GetCrcInterruptCount,
    1,
    "Get Interrupt Count"
);

C_(DispTest_GetPtimer);
static SMethod GetPtimer
(
    DispTest_Object,
    "GetPtimer",
    C_DispTest_GetPtimer,
    2,
    "Get Ptimer value"
);

C_(DispTest_PTimerCallwlator);
static SMethod PTimerCallwlator
(
    DispTest_Object,
    "PTimerCallwlator",
    C_DispTest_PTimerCallwlator,
    2,
    "Callwlate Ptimer value"
);

C_(DispTest_GetBoundGpuSubdevice);
static STest DispTest_GetBoundGpuSubdevice
(
    DispTest_Object,
    "GetBoundGpuSubdevice",
    C_DispTest_GetBoundGpuSubdevice,
    0,
    "Get the bound GpuSubdevice for the DispTest"
);

P_(DispTest_Get_gpu);

static SProperty DispTest_gpu
(
  DispTest_Object,
  "gpu",
  0,
  0,
  DispTest_Get_gpu,
  0,//DispTest_Set_gpu,
  0,
  "Get the current (bound) GpuSubdevice"
);

C_(DispTest_SetupCarveoutFb);
static SMethod DispTest_SetupCarveoutFb
(
    DispTest_Object,
    "SetupCarveoutFb",
    C_DispTest_SetupCarveoutFb,
    1,
    "Carve out a chunk of system memory as the FB for 0FB GPUs"
);

C_(DispTest_CleanupCarveoutFb);
static SMethod DispTest_CleanupCarveoutFb
(
    DispTest_Object,
    "CleanupCarveoutFb",
    C_DispTest_CleanupCarveoutFb,
    1,
    "Clean up previously allocated system memory carved out as the FB"
);

C_(DispTest_GetPinsetCount);
static SMethod DispTest_GetPinsetCount
(
    DispTest_Object,
    "GetPinsetCount",
    C_DispTest_GetPinsetCount,
    0,
    "Get number of pinsets."
);

C_(DispTest_GetScanLockPin);
static SMethod DispTest_GetScanLockPin
(
    DispTest_Object,
    "GetScanLockPin",
    C_DispTest_GetScanLockPin,
    1,
    "Get the scan-lock pin of the specified pinset."
);

C_(DispTest_GetFlipLockPin);
static SMethod DispTest_GetFlipLockPin
(
    DispTest_Object,
    "GetFlipLockPin",
    C_DispTest_GetFlipLockPin,
    1,
    "Get the flip-lock pin of the specified pinset."
);

C_(DispTest_GetStereoPin);
static SMethod DispTest_GetStereoPin
(
    DispTest_Object,
    "GetStereoPin",
    C_DispTest_GetStereoPin,
    1,
    "Get the stereo pin."
);

C_(DispTest_GetExtSyncPin);
static SMethod DispTest_GetExtSyncPin
(
    DispTest_Object,
    "GetExtSyncPin",
    C_DispTest_GetExtSyncPin,
    1,
    "Get the external sync pin."
);

C_(DispTest_GetDsiForceBits);
static SMethod DispTest_GetDsiForceBits
(
   DispTest_Object,
   "GetDsiForceBits",
   C_DispTest_GetDsiForceBits,
   2,
   "Read the DsiForceBits for a given head."
);

C_(DispTest_SetDsiForceBits);
static STest DispTest_SetDsiForceBits
(
   DispTest_Object,
   "SetDsiForceBits",
   C_DispTest_SetDsiForceBits,
   9,
   "Write the DsiForceBits for a given head."
);

C_(DispTest_SetSPPLLSpreadPercentage);
static STest DispTest_SetSPPLLSpreadPercentage
(
   DispTest_Object,
   "SetSPPLLSpreadPercentage",
   C_DispTest_SetSPPLLSpreadPercentage,
   0,
   "Set the SPPLL Spread Percentage via an integer [0 - 100000]."
);

C_(DispTest_SetMscg);
static STest DispTest_SetMscg
(
   DispTest_Object,
   "SetMscg",
   C_DispTest_SetMscg,
   0,
   "Enable or disable the MSCG."
);

C_(DispTest_SetForceDpTimeslot);
static STest DispTest_SetForceDpTimeslot
(
   DispTest_Object,
   "SetForceDpTimeslot",
   C_DispTest_SetForceDpTimeslot,
   0,
   "Forces the DP multistream timeslot via an integer [0 - 63]. (0 = don't force)"
);

C_(DispTest_DisableAutoSetDisplayId);
static STest DispTest_DisableAutoSetDisplayId
(
   DispTest_Object,
   "DisableAutoSetDisplayId",
   C_DispTest_DisableAutoSetDisplayId,
   0,
   "Disable code that automatically enques SetDisplayID methods based on OrSetControl data."
);

C_(DispTest_LookupDisplayId);
static SMethod DispTest_LookupDisplayId
(
   DispTest_Object,
   "LookupDisplayId",
   C_DispTest_LookupDisplayId,
   0,
   "Look up the displayID corresponding to a particular or configuration, specified by ortype, ornum, protocol"
);

C_(DispTest_DpCtrl);
static STest DispTest_DpCtrl
(
   DispTest_Object,
   "DpCtrl",
   C_DispTest_DpCtrl,
   12,
   "Do DP link training"
);

C_(DispTest_SetHDMISinkCaps);
static STest DispTest_SetHDMISinkCaps
(
    DispTest_Object,
    "SetHDMISinkCaps",
    C_DispTest_SetHDMISinkCaps,
    2,
    "sets HDMI capabilities of a displayId for greater than 340 MHz TMDS clock and greater than 340 MHz scrambling"
);

////////////////////////////////////////////////////////////////////////////////////////////
//
//  Javascript Interface Implementation - Interface methods and helper functions
//
/*
 * Helper function to output status before a call to a DispTest function.
 */
void PrintCallStatus(const char *msg)
{
    if (DispTest::debug_messages) {
        Printf(Tee::PriNormal, "%s\n", msg);
    }
}

/*
 * Helper function to output status after a call to a DispTest function.
 */
void PrintReturnStatus(const char *msg, RC rc)
{
    if ( OK == rc ) {
        if (DispTest::debug_messages) {
            Printf(Tee::PriNormal, "$$$ %s PASS $$$\n", msg);
        }
    } else {
        Printf(Tee::PriHigh, "%s *** FAIL ***\n", msg);
    }
}

/*
 * Helper function to output status and return value after a call to a DispTest function.
 */
void PrintReturnStatus(const char *msg, RC rc, UINT32 Value)
{
    if ( OK == rc ) {
        if (DispTest::debug_messages) {
            Printf(Tee::PriNormal, "$$$ %s PASS $$$\n$$$ Returned: %08x\n", msg, Value);
        }
    } else {
        Printf(Tee::PriHigh, "%s *** FAIL ***\n", msg);
    }
}

/****************************************************************************************
 * DispTest::SetDebugMessageDisplay
 *
 *  Arguments Description:
 *  - Verbose : true iff verbose messages are desired
 *
 *  Functional Description:
 *  - Turn on/off verbose debug messages during test exelwtion
 ****************************************************************************************/
C_(DispTest_SetDebugMessageDisplay)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    bool Verbose;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Verbose)))
    {
        JS_ReportError(pContext, "Usage: DispTest.SetDebugMessageDisplay(Verbose)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.SetDebugMessageDisplay(%d)", (int)(Verbose));
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::SetDebugMessageDisplay(Verbose);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_Initialize)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // parse arguments
    JavaScriptPtr pJavaScript;
    string TestName;
    string SubTestName;

    JSObject* DTArray;

    if (NumArguments >= 2) {
        if ( (OK != pJavaScript->FromJsval(pArguments[0], &TestName)) ||
             (OK != pJavaScript->FromJsval(pArguments[1], &SubTestName)))
        {
            JS_ReportError(pContext, "Usage: DispTest.Initialize(\"TestName\", \"SubTestName\")");
            return JS_FALSE;
        }
    }

    if (NumArguments == 3) {
        if ( OK != pJavaScript->FromJsval(pArguments[3], &DTArray) )
        {
            JS_ReportError(pContext, "Usage: DispTest.Initialize(\"TestName\", \"SubTestName\")");
            return JS_FALSE;
        }
    }
    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.Initialize(%s, %s)", TestName.c_str(), SubTestName.c_str());
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::Initialize(TestName, SubTestName);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_CrcInitialize)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    string TestName;
    string SubTestName;
    string Owner;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &TestName)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &SubTestName)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Owner)))
    {
        JS_ReportError(pContext, "Usage: DispTest.CrcInitialize(\"TestName\", \"SubTestName\", \"Owner\")");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.CrcInitialize(%s, %s, %s)", TestName.c_str(), SubTestName.c_str(), Owner.c_str());
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::CrcInitialize(TestName, SubTestName, Owner);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_StoreBin2FB)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);

    DISPTEST_SET_LWRRENT_DEVICE;
    JavaScriptPtr pJavaScript;
    string filename;

    UINT64 SysAddr;
    UINT64 Offset;
    UINT32 fset;

    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &filename)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &SysAddr)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Offset)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &fset)))
    {
        JS_ReportError(pContext, "Usage: DispTest.StoreBin2FB(\"fileName\", \"SysAddr\",\"Offset\", \"fset\")"); return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "\nDispTest.StoreBin2FB(%s, %llu, %llu, %u)\n", filename.c_str(), SysAddr, Offset, fset);
    PrintCallStatus(msg);

    // call the function
    DispTest::StoreBin2FB(filename, SysAddr, Offset, fset);

    // output completion status

    // store return value
    return JS_TRUE;
}

C_(DispTest_LoadFB2Bin)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);

    DISPTEST_SET_LWRRENT_DEVICE;
    JavaScriptPtr pJavaScript;
    string filename;

    UINT64 SysAddr;
    UINT64 Offset;
    INT32  length;

    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &filename)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &SysAddr)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Offset)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &length)))
    {
        JS_ReportError(pContext, "Usage: DispTest.LoadFB2Bin(\"fileName\", \"SysAddr\",\"Offset\",\"length\")");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "\nDispTest.LoadFB2Bin(%s, %llu, %llu, %u)\n", filename.c_str(), SysAddr, Offset, length);
    PrintCallStatus(msg);

    // call the function
    DispTest::LoadFB2Bin(filename, SysAddr, Offset, length);

    // output completion status

    // store return value
    return JS_TRUE;
}

C_(DispTest_BootupFalcon)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);

    DISPTEST_SET_LWRRENT_DEVICE;
    JavaScriptPtr pJavaScript;

    UINT64 bootVec;
    UINT64 bl_dmem_offset;
    string Falcon;

    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &bootVec)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &bl_dmem_offset)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Falcon))
    )
    {
        JS_ReportError(pContext, "Usage: DispTest.BootupFalcon( \"bootVec\",\"bl_dmem_offset\",\"Falcon\")");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "\nDDispTest.BootupFalcon(%llu,%llu,%s)\n", bootVec,bl_dmem_offset,Falcon.c_str());
    PrintCallStatus(msg);

    // call the function
    *pReturlwalue = INT_TO_JSVAL(DispTest::BootupFalcon(bootVec,bl_dmem_offset,Falcon));

    // output completion status

    // store return value
    return JS_TRUE;
}

C_(DispTest_BootupAFalcon)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);

    DISPTEST_SET_LWRRENT_DEVICE;
    JavaScriptPtr pJavaScript;

    UINT32 hMem;
    UINT64 bootVec;
    UINT64 bl_dmem_offset;

    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &hMem)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &bootVec)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &bl_dmem_offset)))
    {
        JS_ReportError(pContext, "Usage: DispTest.BootupAFalcon( \"hMem\",\"bootVec\",\"bl_dmem_offset\")");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "\nDDispTest.BootupAFalcon(%u,%llu,%llu)\n", hMem,bootVec,bl_dmem_offset);
    PrintCallStatus(msg);

    // call the function
    *pReturlwalue = INT_TO_JSVAL(DispTest::BootupAFalcon(hMem,bootVec,bl_dmem_offset));

    // output completion status

    // store return value
    return JS_TRUE;
}

C_(DispTest_ConfigZPW)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);

    DISPTEST_SET_LWRRENT_DEVICE;
    JavaScriptPtr pJavaScript;

    bool zpw_enable;
    bool zpw_ctx_mode;
    bool zpw_fast_mode;
    string ucode_dir;

    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &zpw_enable)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &zpw_ctx_mode)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &zpw_fast_mode))||
        (OK != pJavaScript->FromJsval(pArguments[3], &ucode_dir)))
    {
        JS_ReportError(pContext, "Usage: DispTest.ConfigZPW( \"zpw_enable\",\"zpw_ctx_mode\",\"zpw_fast_mode\",\"ucode_dir\")");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "\nDDispTest.ConfigZPW(%u,%u,%u,%s)\n", zpw_enable, zpw_ctx_mode, zpw_fast_mode, ucode_dir.c_str());
    PrintCallStatus(msg);

    // call the function
    *pReturlwalue = INT_TO_JSVAL(DispTest::ConfigZPW(zpw_enable,zpw_ctx_mode,zpw_fast_mode, ucode_dir));

    // output completion status

    // store return value
    return JS_TRUE;
}

C_(DispTest_AllocDispMemory)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);

    DISPTEST_SET_LWRRENT_DEVICE;
    JavaScriptPtr pJavaScript;
    UINT64 Size;
    UINT64 hMem;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Size)))
    {
        JS_ReportError(pContext, "Usage: DispTest.AllocDispMemory(\"Size\")");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "\nDispTest.AllocDispMemory(%llx)\n",Size);
    PrintCallStatus(msg);

    // call the function
    hMem = DispTest::AllocDispMemory(Size);

    // Set the return value.
    *pReturlwalue = JSVAL_NULL;
    if (OK != pJavaScript->ToJsval(hMem, pReturlwalue))
    {
       JS_ReportError(pContext, "Error oclwrred in DispTest::AllocDispMemory (...)");
       *pReturlwalue = JSVAL_NULL;
       return JS_FALSE;
    }

    return JS_TRUE;
}

C_(DispTest_GetUcodePhyAddr)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);

    DISPTEST_SET_LWRRENT_DEVICE;
    JavaScriptPtr pJavaScript;
    UINT64 hMem;

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: DispTest.GetUcodePhyAddr()");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "\nDispTest.GetUcodePhyAddr()\n");
    PrintCallStatus(msg);

    // call the function
    hMem = DispTest::GetUcodePhyAddr();

    // Set the return value.
    *pReturlwalue = JSVAL_NULL;
    if (OK != pJavaScript->ToJsval(hMem, pReturlwalue))
    {
       JS_ReportError(pContext, "Error oclwrred in DispTest::GetUcodePhyAddr (...)");
       *pReturlwalue = JSVAL_NULL;
       return JS_FALSE;
    }

    return JS_TRUE;
}

C_(DispTest_Setup)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    string ChannelName;
    string PbLocation;
    UINT32 Head;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ChannelName)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &PbLocation)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Head)))
    {
        JS_ReportError(pContext, "Usage: ChannelHandle = DispTest.Setup(\"ChannelName\", \"PbLocation\", Head)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "\nDispTest.Setup(%s, %s, %u)\n", ChannelName.c_str(), PbLocation.c_str(), Head);
    PrintCallStatus(msg);

    // call the function
    LwRm::Handle RtnChannelHandle;
    RC rc = DispTest::Setup(ChannelName, PbLocation, Head, &RtnChannelHandle);

    // output completion status
    PrintReturnStatus(msg, rc, RtnChannelHandle);

    // store return value
    if (OK != pJavaScript->ToJsval(RtnChannelHandle, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in DispTest::Setup(\"ChannelName\", \"PbLocation\", Head)");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(DispTest_Release)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle ChannelHandle;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ChannelHandle)))
    {
        JS_ReportError(pContext, "Usage: DispTest.Release(ChannelHandle)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.Release(%08x)", ChannelHandle);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::Release(ChannelHandle);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_BindDevice)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    UINT32 DevIndex;

    static char usage[] = "Usage: DispTest.BindDevice(DevIndex)";

    if ( (NumArguments != 1) ||
         (OK != pJavaScript->FromJsval(pArguments[0], &DevIndex)) ) {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    DispTest::BindDevice(DevIndex);

    RETURN_RC(OK);
}

C_(DispTest_SwitchToChannel)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    JavaScriptPtr pJs;
    LwRm::Handle hCh;

    if( NumArguments != 1 ||
       (OK != pJs->FromJsval(pArguments[0], &hCh)))
    {
        JS_ReportError(pContext, "Usage: DispTest.SwitchToChannel(hDisplayChannel)");
        return JS_FALSE;
    }

    RETURN_RC(DispTest::SwitchToChannel(hCh));
}

C_(DispTest_GetHDMICapableDisplayIdForSor)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;
    JavaScriptPtr pJs;
    JSObject *RetArray;
    UINT32  sorIndex;
    UINT32  displayId = 0;

    if( NumArguments != 2 ||
       (OK != pJs->FromJsval(pArguments[0], &sorIndex)) ||
       (OK != pJs->FromJsval(pArguments[1], &RetArray)) )
    {
        JS_ReportError(pContext, "Usage: DispTest.GetHDMICapableDisplayIdForSor(sorIndex, RetValues=Array(displayId))");
        return JS_FALSE;
    }

    RC rc = DispTest::GetHDMICapableDisplayIdForSor(sorIndex, &displayId);

    if (OK != pJs->SetElement(RetArray, 0, displayId ))
    {
        JS_ReportError(pContext, "Usage: DispTest.GetHDMICapableDisplayIdForSor(sorIndex, RetValues=Array(displayId))");
        return JS_FALSE;
    }

    RETURN_RC(rc);
}

C_(DispTest_SetInterruptHandlerName)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    string funcname;
    UINT32 intrHandleSrc;

    if ((NumArguments != 2) ||
        (OK !=  pJavaScript->FromJsval(pArguments[0], &intrHandleSrc)) ||
        (OK !=  pJavaScript->FromJsval(pArguments[1], &funcname)))
    {
        JS_ReportError(pContext, "Usage: DispTest.SetInterruptHandlerName(UINT32 intrHandleSrc, const string& funcname)");
        return JS_FALSE;
    }

    RETURN_RC(DispTest::SetInterruptHandlerName(intrHandleSrc, funcname));
}

C_(DispTest_SetHDMIEnable)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;
    JavaScriptPtr pJs;
    UINT32  en32;
    bool  enable;
    UINT32  displayId = 0;

    if( NumArguments != 2 ||
       (OK != pJs->FromJsval(pArguments[0], &displayId)) ||
       (OK != pJs->FromJsval(pArguments[1], &en32)) )
    {
        JS_ReportError(pContext, "Usage: DispTest.SetHDMIEnable(displayId, enable)");
        return JS_FALSE;
    }

    enable = !!(en32);

    RETURN_RC(DispTest::SetHDMIEnable(displayId, enable));

}

C_(DispTest_IdleChannelControl)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle ChannelHandle = 0;
    UINT32 DesiredChannelStateMask = 0;
    UINT32 Accelerator = 0;
    UINT32 Timeout = 0;

    string ChannelStateStr;

    JsArray jsDataArray;

    map<string,UINT32> ChannelStateMap;

    ChannelStateMap["STATE_IDLE"] = LW5070_CTRL_IDLE_CHANNEL_STATE_IDLE;
    ChannelStateMap["STATE_WRTIDLE"] = LW5070_CTRL_IDLE_CHANNEL_STATE_WRTIDLE;
    ChannelStateMap["STATE_QUIESCENT1"] = LW5070_CTRL_IDLE_CHANNEL_STATE_QUIESCENT1;
    ChannelStateMap["STATE_QUIESCENT2"] = LW5070_CTRL_IDLE_CHANNEL_STATE_QUIESCENT2;
#ifdef LW_VERIF_FEATURES
    ChannelStateMap["STATE_EMPTY"] = LW5070_CTRL_IDLE_CHANNEL_STATE_EMPTY;
    ChannelStateMap["STATE_FLUSHED"] = LW5070_CTRL_IDLE_CHANNEL_STATE_FLUSHED;
    ChannelStateMap["STATE_BUSY"] = LW5070_CTRL_IDLE_CHANNEL_STATE_BUSY;
#endif

    static char usage[] = "Usage: DispTest.IdleChannelControl(UINT32 ChannelHandle, string DesiredChannelStateArray[])";

    if ((OK != pJavaScript->FromJsval(pArguments[0], &ChannelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &jsDataArray))) {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    if ( (NumArguments > 2) &&
         (OK != pJavaScript->FromJsval(pArguments[2], &Accelerator)) ) {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    if ( (NumArguments == 4) &&
         (OK != pJavaScript->FromJsval(pArguments[3], &Timeout)) ) {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    for (unsigned int i = 0; i < jsDataArray.size(); i++) {
        if (OK != pJavaScript->FromJsval(jsDataArray[i], &ChannelStateStr)) {
            JS_ReportError(pContext, "data element bad value.");
            return JS_FALSE;
        }

        if (ChannelStateMap.find(ChannelStateStr) == ChannelStateMap.end()) {
            RETURN_RC(RC::BAD_PARAMETER);
        }

        DesiredChannelStateMask |= ChannelStateMap[ChannelStateStr];
    }

    // call the function
    RC rc = DispTest::IdleChannelControl(ChannelHandle,
                                         DesiredChannelStateMask,
                                         Accelerator,
                                         Timeout);

    RETURN_RC(rc);
}

C_(DispTest_SetChnStatePollNotif)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle ChannelHandle = 0;
    string ChannelStateStr;
    LwRm::Handle hNotifierCtxDma = 0;
    UINT32 Offset = 0;

    UINT32 NotifChannelState = 0;

    string ChannelName;
    UINT32 ChannelInstance = 0;

    map<string,UINT32> ChannelStateMap;

    ChannelStateMap["STATE_IDLE"]          = LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_CHN_STATE_IDLE;
    ChannelStateMap["STATE_WRTIDLE"]       = LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_CHN_STATE_WRTIDLE;
    ChannelStateMap["STATE_EMPTY"]         = LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_CHN_STATE_EMPTY;
    ChannelStateMap["STATE_FLUSHED"]       = LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_CHN_STATE_FLUSHED;
    ChannelStateMap["STATE_BUSY"]          = LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_CHN_STATE_BUSY;
    ChannelStateMap["STATE_DEALLOC"]       = LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_CHN_STATE_DEALLOC;
    ChannelStateMap["STATE_DEALLOC_LIMBO"] = LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_CHN_STATE_DEALLOC_LIMBO;
    ChannelStateMap["STATE_LIMBO1"]        = LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_CHN_STATE_LIMBO1;
    ChannelStateMap["STATE_LIMBO2"]        = LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_CHN_STATE_LIMBO2;
    ChannelStateMap["STATE_FCODEINIT"]     = LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_CHN_STATE_FCODEINIT;
    ChannelStateMap["STATE_FCODE"]         = LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_CHN_STATE_FCODE;
    ChannelStateMap["STATE_VBIOSINIT"]     = LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_CHN_STATE_VBIOSINIT;
    ChannelStateMap["STATE_VBIOSOPER"]     = LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_CHN_STATE_VBIOSOPER;
    ChannelStateMap["STATE_UNCONNECTED"]   = LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_CHN_STATE_UNCONNECTED;
    ChannelStateMap["STATE_INITIALIZE"]    = LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_CHN_STATE_INITIALIZE;
    ChannelStateMap["STATE_SHUTDOWN1"]     = LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_CHN_STATE_SHUTDOWN1;
    ChannelStateMap["STATE_SHUTDOWN2"]     = LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_CHN_STATE_SHUTDOWN2;

    static char usage[] = "Usage: DispTest.SetChnStatePollNotif(UINT32 ChannelHandle, string ChannelStateStr, UINT32 hNotifierCtxDma, UINT32 Offset)";
/*
    if (NumArguments != 4) {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
*/
    if (NumArguments == 4) {

        if ((OK != pJavaScript->FromJsval(pArguments[0], &ChannelHandle)) ||
            (OK != pJavaScript->FromJsval(pArguments[1], &ChannelStateStr)) ||
            (OK != pJavaScript->FromJsval(pArguments[2], &hNotifierCtxDma)) ||
            (OK != pJavaScript->FromJsval(pArguments[3], &Offset))    ) {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }

        if (ChannelStateMap.find(ChannelStateStr) == ChannelStateMap.end()) {
            RETURN_RC(RC::BAD_PARAMETER);
        }

        NotifChannelState = ChannelStateMap[ChannelStateStr];

        // call the function
        RC rc = DispTest::SetChnStatePollNotif(ChannelHandle,
                NotifChannelState,
                hNotifierCtxDma,
                Offset);
        RETURN_RC(rc);
    }
    else if (NumArguments == 5)
    {
        if ((OK != pJavaScript->FromJsval(pArguments[0], &ChannelName)) ||
            (OK != pJavaScript->FromJsval(pArguments[1], &ChannelInstance)) ||
            (OK != pJavaScript->FromJsval(pArguments[2], &ChannelStateStr)) ||
            (OK != pJavaScript->FromJsval(pArguments[3], &hNotifierCtxDma)) ||
            (OK != pJavaScript->FromJsval(pArguments[4], &Offset))    ) {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }

        if (ChannelStateMap.find(ChannelStateStr) == ChannelStateMap.end()) {
            RETURN_RC(RC::BAD_PARAMETER);
        }

        NotifChannelState = ChannelStateMap[ChannelStateStr];

        // call the function
        RC rc = DispTest::SetChnStatePollNotif(ChannelName,
                ChannelInstance,
                NotifChannelState,
                hNotifierCtxDma,
                Offset);

        RETURN_RC(rc);
    }
    else
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    //RETURN_RC(rc);
}

C_(DispTest_SetChnStatePollNotifBlocking)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle hNotifierCtxDma = 0;
    UINT32 value = 0;
    UINT32 offset = 0;

    static char usage[] = "Usage: DispTest.SetChnStatePollNotifBlocking(UINT32 hNotifierCtxDma, UINT32 offset, UINT32 value)";

    if (NumArguments != 3) {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    if ((OK != pJavaScript->FromJsval(pArguments[0], &hNotifierCtxDma)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &offset)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &value))    ) {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

   // call the function
    RC rc = DispTest::SetChnStatePollNotifBlocking(hNotifierCtxDma, offset, value);

    RETURN_RC(rc);
}

C_(DispTest_GetChnStatePollNotifRMPolling)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle hNotifierCtxDma = 0;
    FLOAT64 timeoutMs = 0;
    UINT32 offset = 0;

    static char usage[] = "Usage: DispTest.GetChnStatePollNotifRMPolling(UINT32 hNotifierCtxDma, UINT32 offset, FLOAT64 timeoutMs)";

    if (NumArguments != 3) {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    if ((OK != pJavaScript->FromJsval(pArguments[0], &hNotifierCtxDma)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &offset)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &timeoutMs))    ) {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

   // call the function
    RC rc = DispTest::GetChnStatePollNotifRMPolling(hNotifierCtxDma, offset, timeoutMs);

    RETURN_RC(rc);
}

C_(DispTest_GetInternalTVRasterTimings)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    JSObject *RetArray;;

    UINT32 Protocol;

    UINT32 PClkFreqKHz;     // [OUT]
    UINT32 HActive;         // [OUT]
    UINT32 VActive;         // [OUT]
    UINT32 Width;           // [OUT]
    UINT32 Height;          // [OUT]
    UINT32 SyncEndX;        // [OUT]
    UINT32 SyncEndY;        // [OUT]
    UINT32 BlankEndX;       // [OUT]
    UINT32 BlankEndY;       // [OUT]
    UINT32 BlankStartX;     // [OUT]
    UINT32 BlankStartY;     // [OUT]
    UINT32 Blank2EndY;      // [OUT]
    UINT32 Blank2StartY;    // [OUT]

    static char errTxt[]  = "Usage: DispTest.GetInternalTVRasterTimings(Protocol, RetValues = Array(PClkFreqKHz, HActive, VActive, Width, Height, SyncEndX, SyncEndY, BlankEndX, BlankEndY, BlankStartX, BlankStartY, Blank2EndY, Blank2StartY) )";

    map<UINT32,UINT32> ProtocolMap;
#define MAP_PROTOCOL_ENTRY(rm_protocol,class_protocol) \
    ProtocolMap[LW507D_DAC_SET_CONTROL_PROTOCOL_##class_protocol] = LW5070_CTRL_CMD_GET_INTERNAL_TV_RASTER_TIMINGS_PROTOCOL_##rm_protocol;

    MAP_PROTOCOL_ENTRY(NTSC_M,CPST_NTSC_M);
    MAP_PROTOCOL_ENTRY(NTSC_J,CPST_NTSC_J);
    MAP_PROTOCOL_ENTRY(PAL_BDGHI,CPST_PAL_BDGHI);
    MAP_PROTOCOL_ENTRY(PAL_M,CPST_PAL_M);
    MAP_PROTOCOL_ENTRY(PAL_N,CPST_PAL_N);
    MAP_PROTOCOL_ENTRY(PAL_CN,CPST_PAL_CN);
    MAP_PROTOCOL_ENTRY(NTSC_M,COMP_NTSC_M);
    MAP_PROTOCOL_ENTRY(NTSC_J,COMP_NTSC_J);
    MAP_PROTOCOL_ENTRY(PAL_BDGHI,COMP_PAL_BDGHI);
    MAP_PROTOCOL_ENTRY(PAL_M,COMP_PAL_M);
    MAP_PROTOCOL_ENTRY(PAL_N,COMP_PAL_N);
    MAP_PROTOCOL_ENTRY(PAL_CN,COMP_PAL_CN);
    MAP_PROTOCOL_ENTRY(480P_60,COMP_480P_60);
    MAP_PROTOCOL_ENTRY(576P_50,COMP_576P_50);
    MAP_PROTOCOL_ENTRY(720P_50,COMP_720P_50);
    MAP_PROTOCOL_ENTRY(720P_60,COMP_720P_60);
    MAP_PROTOCOL_ENTRY(1080I_50,COMP_1080I_50);
    MAP_PROTOCOL_ENTRY(1080I_60,COMP_1080I_60);

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Protocol)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &RetArray)))
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    Printf(Tee::PriNormal, "PROTOCOL_MAP: %x -> %x\n",
           Protocol, ProtocolMap[Protocol]);

    RC rc =  DispTest::GetInternalTVRasterTimings(ProtocolMap[Protocol],
                                                  &PClkFreqKHz,
                                                  &HActive,
                                                  &VActive,
                                                  &Width,
                                                  &Height,
                                                  &SyncEndX,
                                                  &SyncEndY,
                                                  &BlankEndX,
                                                  &BlankEndY,
                                                  &BlankStartX,
                                                  &BlankStartY,
                                                  &Blank2EndY,
                                                  &Blank2StartY     );

    if ((OK != pJavaScript->SetElement(RetArray, 0, PClkFreqKHz )) ||
        (OK != pJavaScript->SetElement(RetArray, 1, HActive )) ||
        (OK != pJavaScript->SetElement(RetArray, 2, VActive )) ||
        (OK != pJavaScript->SetElement(RetArray, 3, Width )) ||
        (OK != pJavaScript->SetElement(RetArray, 4, Height )) ||
        (OK != pJavaScript->SetElement(RetArray, 5, SyncEndX )) ||
        (OK != pJavaScript->SetElement(RetArray, 6, SyncEndY )) ||
        (OK != pJavaScript->SetElement(RetArray, 7, BlankEndX )) ||
        (OK != pJavaScript->SetElement(RetArray, 8, BlankEndY )) ||
        (OK != pJavaScript->SetElement(RetArray, 9, BlankStartX )) ||
        (OK != pJavaScript->SetElement(RetArray, 10, BlankStartY )) ||
        (OK != pJavaScript->SetElement(RetArray, 11, Blank2EndY )) ||
        (OK != pJavaScript->SetElement(RetArray, 12, Blank2StartY )) )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(rc);
}

C_(DispTest_GetRGStatus)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    JSObject *RetArray;;

    UINT32 head;

    UINT32 scanLocked;     // [OUT]
    UINT32 flipLocked;     // [OUT]

    static char errTxt[]  = "Usage: DispTest.GetRGStatus(head, RetValues = Array(scanLocked, flipLocked) )";

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &head)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &RetArray)))
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RC rc =  DispTest::GetRGStatus(head,
                                   &scanLocked,
                                   &flipLocked     );

    if ((OK != pJavaScript->SetElement(RetArray, 0, scanLocked )) ||
        (OK != pJavaScript->SetElement(RetArray, 1, flipLocked )) )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(rc);
}

C_(DispTest_EnqMethod)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle ChannelHandle;
    UINT32 Method;
    UINT32 Data;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ChannelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Method)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Data)))
    {
        JS_ReportError(pContext, "Usage: DispTest.EnqMethod(ChannelHandle, Method, Data)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.EnqMethod(%08x, %08x, %08x)", ChannelHandle, Method, Data);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::EnqMethod(ChannelHandle, Method, Data);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_EnqUpdateAndMode)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle ChannelHandle;
    UINT32 Method;
    UINT32 Data;
    string Mode;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ChannelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Method)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Data)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &Mode)))
    {
        JS_ReportError(pContext, "Usage: DispTest.EnqUpdateAndMode(ChannelHandle, Method, Data, Mode)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.EnqUpdateAndMode(%08x, %08x, %08x, %s)", ChannelHandle, Method, Data, Mode.c_str());
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::EnqUpdateAndMode(ChannelHandle, Method, Data, Mode.c_str());

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_EnqCoreUpdateAndMode)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle ChannelHandle;
    UINT32 Method;
    UINT32 Data;
    string head_a_mode;
    string head_b_mode;
    string head_c_mode;
    string head_d_mode;
    if( (NumArguments < 4)
    ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ChannelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Method)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Data)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &head_a_mode))
    )
    {
        JS_ReportError(pContext, "Usage: DispTest.EnqCoreUpdateAndModeMethod(ChannelHandle, Method, Data, HeadAMode, [HeadBMode, ...])");
        return JS_FALSE;
    }

    if((NumArguments < 5) || (OK != pJavaScript->FromJsval(pArguments[4], &head_b_mode)))
        head_b_mode = head_a_mode;
    if((NumArguments < 6) || (OK != pJavaScript->FromJsval(pArguments[5], &head_c_mode)))
        head_c_mode = head_b_mode;
    if((NumArguments < 7) || (OK != pJavaScript->FromJsval(pArguments[6], &head_d_mode)))
        head_d_mode = head_c_mode;

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.EnqCoreUpdateAndModeMethod(%08x, %08x, %08x, %s, %s, %s, %s)", ChannelHandle, Method, Data,  head_a_mode.c_str(), head_b_mode.c_str(), head_c_mode.c_str(), head_d_mode.c_str());
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::EnqCoreUpdateAndMode(ChannelHandle, Method, Data, head_a_mode.c_str(), head_b_mode.c_str(), head_c_mode.c_str(), head_d_mode.c_str());

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_EnqCrcMode)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 head_num;
    string head_mode;
    if( (NumArguments != 2)
        || (OK != pJavaScript->FromJsval(pArguments[0], &head_num))
        || (OK != pJavaScript->FromJsval(pArguments[1], &head_mode))
    )
    {
        JS_ReportError(pContext, "Usage: DispTest.EnqCrcMode(head_num, head_mode)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.EnqCrcMode(%d, %s)", head_num, head_mode.c_str());
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::EnqCrcMode(head_num, head_mode.c_str());

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_EnqMethodMulti)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle ChannelHandle;
    UINT32 Method;
    UINT32 Count;
    JsArray jsDataArray;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ChannelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Method)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Count)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &jsDataArray)))
    {
        JS_ReportError(pContext, "Usage: DispTest.EnqMethod(ChannelHandle, Method, Count, DataPtr)");
        return JS_FALSE;
    }

    vector<UINT32> Data(jsDataArray.size());
    UINT32 dataTemp;

    for (UINT32 i=0; i < jsDataArray.size(); i++)
    {
        if (OK != pJavaScript->FromJsval((jsDataArray[i]), &dataTemp))
        {
            JS_ReportError(pContext, "data element bad value.");
            return JS_FALSE;
        }
        Data[i] = dataTemp;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.EnqMethodMulti(%08x, %08x, %08x, %08x)", ChannelHandle, Method, Count, Data[0]);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::EnqMethodMulti(ChannelHandle, Method, Count, &Data[0]);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_EnqMethodNonInc)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle ChannelHandle;
    UINT32 Method;
    UINT32 Data;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ChannelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Method)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Data)))
    {
        JS_ReportError(pContext, "Usage: DispTest.EnqMethodNonInc(ChannelHandle, Method, Data)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.EnqMethodNonInc(%08x, %08x, %08x)", ChannelHandle, Method, Data);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::EnqMethodNonInc(ChannelHandle, Method, Data);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_EnqMethodNonIncMulti)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle ChannelHandle;
    UINT32 Method;
    UINT32 Count;
    JsArray jsDataArray;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ChannelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Method)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Count)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &jsDataArray)))
    {
        JS_ReportError(pContext, "Usage: DispTest.EnqMethodNonInc(ChannelHandle, Method, Count, DataPtr)");
        return JS_FALSE;
    }

    vector<UINT32> Data(jsDataArray.size());
    UINT32 dataTemp;

    for (UINT32 i=0; i < jsDataArray.size(); i++)
    {
        if (OK != pJavaScript->FromJsval((jsDataArray[i]), &dataTemp))
        {
            JS_ReportError(pContext, "data element bad value.");
            return JS_FALSE;
        }
        Data[i] = dataTemp;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.EnqMethodNonInc(%08x, %08x, %08x, %08x)", ChannelHandle, Method, Count, Data[0]);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::EnqMethodNonIncMulti(ChannelHandle, Method, Count, &Data[0]);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_EnqMethodNop)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle ChannelHandle;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ChannelHandle)))
    {
        JS_ReportError(pContext, "Usage: DispTest.EnqMethodNop(ChannelHandle)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.EnqMethodNop(%08x)", ChannelHandle);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::EnqMethodNop(ChannelHandle);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_EnqMethodJump)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle ChannelHandle;
    UINT32 Offset;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ChannelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Offset)))
    {
        JS_ReportError(pContext, "Usage: DispTest.EnqMethodJump(ChannelHandle, Offset)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.EnqMethodJump(%08x, %08x)", ChannelHandle, Offset);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::EnqMethodJump(ChannelHandle, Offset);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_EnqMethodSetSubdevice)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle ChannelHandle;
    UINT32 Subdevice;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ChannelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Subdevice)))
    {
        JS_ReportError(pContext, "Usage: DispTest.EnqMethodSetSubdevice(ChannelHandle, Subdevice)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.EnqMethodSetSubdevice(%08x, %08x)", ChannelHandle, Subdevice);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::EnqMethodSetSubdevice(ChannelHandle, Subdevice);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_EnqMethodOpcode)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle ChannelHandle;
    UINT32 Opcode;
    UINT32 Method;
    UINT32 Count;
    JsArray jsDataArray;
    if ((NumArguments != 5) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ChannelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Opcode)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Method)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &Count)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &jsDataArray)))
    {
        JS_ReportError(pContext, "Usage: DispTest.EnqMethod(ChannelHandle, Opcode, Method, Count, DataPtr)");
        return JS_FALSE;
    }

    vector<UINT32> Data(jsDataArray.size());
    UINT32 dataTemp;

    for (UINT32 i=0; i < jsDataArray.size(); i++)
    {
        if (OK != pJavaScript->FromJsval((jsDataArray[i]), &dataTemp))
        {
            JS_ReportError(pContext, "data element bad value.");
            return JS_FALSE;
        }
        Data[i] = dataTemp;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.EnqMethodOpcode(%08x, %08x, %08x, %08x)", ChannelHandle, Method, Count, Data[0]);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::EnqMethodOpcode(ChannelHandle, Opcode, Method, Count, &Data[0]);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_SetAutoFlush)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle ChannelHandle;
    bool AutoFlushEnable;
    UINT32 AutoFlushThreshold;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ChannelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &AutoFlushEnable)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &AutoFlushThreshold)))
    {
        JS_ReportError(pContext, "Usage: DispTest.SetAutoFlush(ChannelHandle, AutoFlushEnable, AutoFlushThreshold)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.SetAutoFlush(%08x, %d, %u)", ChannelHandle, (int)(AutoFlushEnable), AutoFlushThreshold);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::SetAutoFlush(ChannelHandle, AutoFlushEnable, AutoFlushThreshold);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_Flush)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle ChannelHandle;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ChannelHandle)))
    {
        JS_ReportError(pContext, "Usage: DispTest.Flush(ChannelHandle)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.Flush(%08x)", ChannelHandle);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::Flush(ChannelHandle);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_BindContextDma)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle hChannel;
    LwRm::Handle hCtxDma;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &hChannel)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &hCtxDma)))
    {
        JS_ReportError(pContext, "Usage: DispTest.BindContextDma(hChannel, hCtxDma)");
        return JS_FALSE;
    }

    LwRmPtr pLwRm;
    RETURN_RC(pLwRm->BindContextDma(hChannel, hCtxDma));
}

C_(DispTest_CreateNotifierContextDma)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle ChannelHandle;
    string MemoryLocation;
    bool bindCtx = true;
    if (NumArguments == 2)
    {
        if((OK != pJavaScript->FromJsval(pArguments[0], &ChannelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &MemoryLocation)))
        {
            JS_ReportError(pContext, "Usage: Handle = DispTest.CreateNotifierContextDma(ChannelHandle, \"MemoryLocation\", [bindCtx])");
            return JS_FALSE;
        }
    }
    else if (NumArguments == 3)
    {
        if((OK != pJavaScript->FromJsval(pArguments[0], &ChannelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &MemoryLocation)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &bindCtx)))
        {
            JS_ReportError(pContext, "Usage: Handle = DispTest.CreateNotifierContextDma(ChannelHandle, \"MemoryLocation\", [bindCtx])");
            return JS_FALSE;
        }
    }
    else
    {
            JS_ReportError(pContext, "Usage: Handle = DispTest.CreateNotifierContextDma(ChannelHandle, \"MemoryLocation\", [bindCtx])");
            return JS_FALSE;
    }

       /*
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ChannelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &MemoryLocation)))
    {
        JS_ReportError(pContext, "Usage: Handle = DispTest.CreateNotifierContextDma(ChannelHandle, \"MemoryLocation\")");
        return JS_FALSE;
    }
*/
    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.CreateNotifierContextDma(%08x, %s)", ChannelHandle, MemoryLocation.c_str());
    PrintCallStatus(msg);

    // call the function
    LwRm::Handle RtnHandle;
    RC rc = DispTest::CreateNotifierContextDma(ChannelHandle, MemoryLocation, &RtnHandle, bindCtx);

    // output completion status
    PrintReturnStatus(msg, rc, RtnHandle);

    // store return value
    if (OK != pJavaScript->ToJsval(RtnHandle, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in DispTest::CreateNotifierContextDma(ChannelHandle, \"MemoryLocation\")");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(DispTest_CreateSemaphoreContextDma)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle ChannelHandle;
    string MemoryLocation;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ChannelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &MemoryLocation)))
    {
        JS_ReportError(pContext, "Usage: Handle = DispTest.CreateSemaphoreContextDma(ChannelHandle, \"MemoryLocation\")");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.CreateSemaphoreContextDma(%08x, %s)", ChannelHandle, MemoryLocation.c_str());
    PrintCallStatus(msg);

    // call the function
    LwRm::Handle RtnHandle;
    RC rc = DispTest::CreateSemaphoreContextDma(ChannelHandle, MemoryLocation, &RtnHandle);

    // output completion status
    PrintReturnStatus(msg, rc, RtnHandle);

    // store return value
    if (OK != pJavaScript->ToJsval(RtnHandle, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in DispTest::CreateSemaphoreContextDma(ChannelHandle, \"MemoryLocation\")");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(DispTest_CreateContextDma)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle ChannelHandle;
    UINT32 Flags;
    UINT32 Size;
    string MemoryLocation;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &ChannelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Flags)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Size)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &MemoryLocation)))
    {
        JS_ReportError(pContext, "Usage: Handle = DispTest.CreateContextDma(ChannelHandle, Flags, Size, \"MemoryLocation\")");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.CreateContextDma(%08x, %08x, %u, %s)", ChannelHandle, Flags, Size, MemoryLocation.c_str());
    PrintCallStatus(msg);

    // call the function
    LwRm::Handle RtnHandle;
    RC rc = DispTest::CreateContextDma(ChannelHandle, Flags, Size, MemoryLocation, &RtnHandle);

    // output completion status
    PrintReturnStatus(msg, rc, RtnHandle);

    // store return value
    if (OK != pJavaScript->ToJsval(RtnHandle, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in DispTest::CreateContextDma(ChannelHandle, Flags, Size, \"MemoryLocation\")");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(DispTest_DeleteContextDma)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle Handle;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Handle)))
    {
        JS_ReportError(pContext, "Usage: DispTest.DeleteContextDma(Handle)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.DeleteContextDma(%08x)", Handle);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::DeleteContextDma(Handle);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_SetContextDmaDebugRegs)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle Handle;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Handle)))
    {
        JS_ReportError(pContext, "Usage: DispTest.SetContextDmaDebugRegs(Handle)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.SetContextDmaDebugRegs(%08x)", Handle);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::SetContextDmaDebugRegs(Handle);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_WriteVal32)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle Handle;
    UINT32 Offset;
    UINT32 Value;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Handle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Offset)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Value)))
    {
        JS_ReportError(pContext, "Usage: DispTest.WriteVal32(Handle, Offset, Value)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.WriteVal32(%08x, %u, %08x)", Handle, Offset, Value);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::WriteVal32(Handle, Offset, Value);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_ReadVal32)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle Handle;
    UINT32 Offset;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Handle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Offset)))
    {
        JS_ReportError(pContext, "Usage: Value = DispTest.ReadVal32(Handle, Offset)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.ReadVal32(%08x, %u)", Handle, Offset);
    PrintCallStatus(msg);

    // call the function
    UINT32 Rtlwalue;
    RC rc = DispTest::ReadVal32(Handle, Offset, &Rtlwalue);

    // output completion status
    PrintReturnStatus(msg, rc, Rtlwalue);

    // store return value
    if ((rc != OK) || (OK != pJavaScript->ToJsval(Rtlwalue, pReturlwalue)))
    {
        JS_ReportError(pContext, "Error oclwrred in DispTest::ReadVal32(Handle, Offset)");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(DispTest_PollHWValue)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    string name;
    UINT32 index;
    UINT32 Value;
    UINT32 Mask;
    UINT32 Size;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 6) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &name)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &index)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Value)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &Mask)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &Size)) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollHWValue(SignalName, index, Value, Mask, Size, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollHWValue(%s, %u, %u, %u, %u, %f)", name.c_str(), index, Value, Mask, Size, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollHWValue(name.c_str(), index, Value, Mask, Size, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_PollHWGreaterEqualValue)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    string name;
    UINT32 index;
    UINT32 Value;
    UINT32 Mask;
    UINT32 Size;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 6) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &name)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &index)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Value)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &Mask)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &Size)) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollHWGreaterEqualValue(SignalName, index, Value, Mask, Size, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollHWGreaterEqualValue(%s, %u, %u, %u, %u, %f)", name.c_str(), index, Value, Mask, Size, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollHWGreaterEqualValue(name.c_str(), index, Value, Mask, Size, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_PollHWLessEqualValue)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    string name;
    UINT32 index;
    UINT32 Value;
    UINT32 Mask;
    UINT32 Size;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 6) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &name)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &index)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Value)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &Mask)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &Size)) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollHWLessEqualValue(SignalName, index, Value, Mask, Size, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollHWLessEqualValue(%s, %u, %u, %u, %u, %f)", name.c_str(), index, Value, Mask, Size, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollHWLessEqualValue(name.c_str(), index, Value, Mask, Size, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_PollHWNotValue)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    string name;
    UINT32 index;
    UINT32 Value;
    UINT32 Mask;
    UINT32 Size;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 6) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &name)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &index)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Value)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &Mask)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &Size)) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollHWNotValue(SignalName, index, Value, Mask, Size, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollHWNotValue(%s, %u, %u, %u, %u, %f)", name.c_str(), index, Value, Mask, Size, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollHWNotValue(name.c_str(), index, Value, Mask, Size, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_PollIORegValue)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Address;
    UINT32 Value;
    UINT32 Mask;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Address)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Value)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Mask)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollIORegValue(Address, Value, Mask, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollIORegValue(%u, %08x, %u, %f)", Address, Value, Mask, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollIORegValue((UINT16)Address, (UINT16)Value, (UINT16)Mask, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_PollRegValue)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Address;
    UINT32 Value;
    UINT32 Mask;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Address)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Value)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Mask)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollRegValue(Address, Value, Mask, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollRegValue(%u, %08x, %u, %f)", Address, Value, Mask, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollRegValue(Address, Value, Mask, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_PollRegNotValue)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Address;
    UINT32 Value;
    UINT32 Mask;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Address)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Value)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Mask)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollRegNotValue(Address, Value, Mask, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollRegNotValue(%u, %08x, %u, %f)", Address, Value, Mask, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollRegNotValue(Address, Value, Mask, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_PollRegLessValue)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Address;
    UINT32 Value;
    UINT32 Mask;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Address)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Value)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Mask)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollRegLessValue(Address, Value, Mask, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollRegLessValue(%u, %08x, %u, %f)", Address, Value, Mask, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollRegLessValue(Address, Value, Mask, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_PollRegGreaterValue)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Address;
    UINT32 Value;
    UINT32 Mask;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Address)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Value)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Mask)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollRegGreaterValue(Address, Value, Mask, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollRegGreaterValue(%u, %08x, %u, %f)", Address, Value, Mask, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollRegGreaterValue(Address, Value, Mask, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_Poll2RegsGreaterValue)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Address;
    UINT32 Value;
    UINT32 Mask;
    UINT32 Address2;
    UINT32 Value2;
    UINT32 Mask2;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 7) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Address)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Value)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Mask)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &Address2)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &Value2)) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &Mask2)) ||
        (OK != pJavaScript->FromJsval(pArguments[6], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.Poll2RegsGreaterValue(Address, Value, Mask, Address2, Value2, Mask2,TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.Poll2RegsGreaterValue(%u, %08x, %u, %u, %08x, %u, %f)", Address, Value, Mask, Address2, Value2, Mask2, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::Poll2RegsGreaterValue(Address, Value, Mask, Address2, Value2, Mask2,TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_PollRegLessEqualValue)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Address;
    UINT32 Value;
    UINT32 Mask;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Address)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Value)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Mask)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollRegLessEqualValue(Address, Value, Mask, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollRegLessEqualValue(%u, %08x, %u, %f)", Address, Value, Mask, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollRegLessEqualValue(Address, Value, Mask, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_PollRegGreaterEqualValue)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Address;
    UINT32 Value;
    UINT32 Mask;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Address)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Value)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Mask)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollRegGreaterEqualValue(Address, Value, Mask, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollRegGreaterEqualValue(%u, %08x, %u, %f)", Address, Value, Mask, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollRegGreaterEqualValue(Address, Value, Mask, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_PollValue)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle Handle;
    UINT32 Offset;
    UINT32 Value;
    UINT32 Mask;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 5) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Handle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Offset)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Value)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &Mask)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollValue(Handle, Offset, Value, Mask, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollValue(%08x, %u, %08x, %u, %f)", Handle, Offset, Value, Mask, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollValue(Handle, Offset, Value, Mask, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_PollGreaterEqualValue)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle Handle;
    UINT32 Offset;
    UINT32 Value;
    UINT32 Mask;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 5) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Handle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Offset)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Value)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &Mask)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollGreaterEqualValue(Handle, Offset, Value, Mask, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollGreaterEqualValue(%08x, %u, %08x, %u, %f)", Handle, Offset, Value, Mask, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollGreaterEqualValue(Handle, Offset, Value, Mask, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_PollValueAtAddr)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    FLOAT64 Address;
    UINT32 Offset;
    UINT32 Value;
    UINT32 Mask;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 5) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Address)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Offset)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Value)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &Mask)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollValueAtAddr(Address, Offset, Value, Mask, TimeoutMs)");
        return JS_FALSE;
    }

    PHYSADDR addr64 = (PHYSADDR)Address;

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollValueAtAddr(%llu, %u, %08x, %u, %f)", addr64, Offset, Value, Mask, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollValueAtAddr(addr64, Offset, Value, Mask, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_PollScanlineGreaterEqualValue)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle Handle;
    UINT32 Value;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Handle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Value)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollScanlineGreaterEqualValue(Handle, Value, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollScanlineGreaterEqualValue(%08x, %u, %f)", Handle, Value,  TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollScanlineGreaterEqualValue(Handle, Value, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_PollScanlineLessValue)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle Handle;
    UINT32 Value;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Handle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Value)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollScanlineLessValue(Handle, Value, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollScanlineLessValue(%08x, %u, %f)", Handle, Value,  TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollScanlineLessValue(Handle, Value, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_PollDone)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle Handle;
    UINT32 Offset;
    UINT32 Bit;
    bool Value;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 5) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Handle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Offset)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Bit)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &Value)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollDone(Handle, Offset, Bit, Value, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollDone(%08x, %u, %u, %u, %f)", Handle, Offset, Bit, Value, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollDone(Handle, Offset, Bit, Value, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_PollRGScanLocked)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Head;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Head)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollRGScanLocked(Head, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollRGScanLocked(%u, %f)", Head, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollRGScanLocked(Head, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_PollRGFlipLocked)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Head;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Head)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollRGFlipLocked(Head, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollRGFlipLocked(%u, %f)", Head, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollRGFlipLocked(Head, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_PollRGScanUnlocked)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Head;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Head)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollRGScanUnlocked(Head, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollRGScanUnlocked(%u, %f)", Head, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollRGScanUnlocked(Head, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_PollRGFlipUnlocked)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Head;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Head)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.PollRGFlipUnlocked(Head, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.PollRGFlipUnlocked(%u, %f)", Head, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::PollRGFlipUnlocked(Head, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_CrcSetOwner)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    string Owner;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Owner)))
    {
        JS_ReportError(pContext, "Usage: DispTest.CrcSetOwner(\"Owner\")");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.CrcSetOwner(%s)", Owner.c_str());
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::CrcSetOwner(Owner);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_ModifyTestName)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    string Testname;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Testname)))
    {
        JS_ReportError(pContext, "Usage: DispTest.ModifyTestName(\"Testname\")");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.ModifyTestName(%s)", Testname.c_str());
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::ModifyTestName(Testname);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_ModifySubtestName)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    string Subtestname;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Subtestname)))
    {
        JS_ReportError(pContext, "Usage: DispTest.ModifySubtestName(\"Subtestname\")");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.ModifySubtestName(%s)", Subtestname.c_str());
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::ModifySubtestName(Subtestname);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_AppendSubtestName)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    string Subtestname;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Subtestname)))
    {
        JS_ReportError(pContext, "Usage: DispTest.AppendSubtestName(\"Subtestname\")");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.AppendSubtestName(%s)", Subtestname.c_str());
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::AppendSubtestName(Subtestname);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_CrcSetCheckTolerance)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Tolerance;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Tolerance)))
    {
        JS_ReportError(pContext, "Usage: DispTest.CrcSetCheckTolerance(Tolerance)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.CrcSetCheckTolerance(%u)", Tolerance);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::CrcSetCheckTolerance(Tolerance);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_CrcSetFModelCheckTolerance)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Tolerance;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Tolerance)))
    {
        JS_ReportError(pContext, "Usage: DispTest.CrcSetFModelCheckTolerance(Tolerance)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.CrcSetFModelCheckTolerance(%u)", Tolerance);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::CrcSetFModelCheckTolerance(Tolerance);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_VgaCrlwpdate)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: DispTest.VgaCrlwpdate()");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.VgaCrlwpdate()");
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::VgaCrlwpdate();

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_AssignHead)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 physical_head_a;
    UINT32 physical_head_b;
    UINT32 physical_head_c;
    UINT32 physical_head_d;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &physical_head_a)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &physical_head_b)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &physical_head_c)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &physical_head_d)))
    {
        JS_ReportError(pContext, "Usage: DispTest.AssignHead(physical_head_a, physical_head_b, physical_head_c, physical_head_d)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.AssignHead(%u, %u, %u, %u)", physical_head_a, physical_head_b, physical_head_c, physical_head_d);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::AssignHead(physical_head_a, physical_head_b, physical_head_c, physical_head_d);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_CrcAddHead)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Head;
    LwRm::Handle CtxDmaHandle;
    LwRm::Handle ChannelHandle;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Head)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &CtxDmaHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &ChannelHandle)))
    {
        JS_ReportError(pContext, "Usage: DispTest.CrcAddHead(Head, CtxDmaHandle, ChannelHandle)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.CrcAddHead(%u, %08x, %08x)", Head, CtxDmaHandle, ChannelHandle);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::CrcAddHead(Head, CtxDmaHandle, ChannelHandle);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_CrcAddVgaHead)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Head;
    string OrType;
    UINT32 OrNumber;
    bool OrIsPrimary;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Head)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &OrType)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &OrNumber)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &OrIsPrimary)))
    {
        JS_ReportError(pContext, "Usage: DispTest.CrcAddVgaHead(Head, or_type, or_number, primary_or)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.CrcAddVgaHead(%u, %s, %u, %u)", Head, OrType.c_str(), OrNumber, OrIsPrimary);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::CrcAddVgaHead(Head, OrType.c_str(), OrNumber, OrIsPrimary);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_CrcAddFcodeHead)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Head;
    FLOAT64 BaseAddr;
    string Target;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Head)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &BaseAddr)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Target)))
    {
        JS_ReportError(pContext, "Usage: DispTest.CrcAddFcodeHead(Head, BaseAdr, Target)");
        return JS_FALSE;
    }

    PHYSADDR addr64 = (PHYSADDR)BaseAddr;

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.CrcAddFcodeHead(%u, %llu, %s)", Head, addr64, Target.c_str());
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::CrcAddFcodeHead(Head, &addr64, Target.c_str());

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_CrcNoCheckHead)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Head;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Head)))
    {
        JS_ReportError(pContext, "Usage: DispTest.CrcNoCheckHead(Head)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.CrcNoCheckHead(%u)", Head);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::CrcNoCheckHead(Head);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_CrcAddNotifier)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Tag;
    LwRm::Handle CtxDmaHandle;
    UINT32 Offset;
    string NotifierType;
    LwRm::Handle ChannelHandle;
    UINT32 Tolerance;
    if ((NumArguments != 6) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Tag)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &CtxDmaHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Offset)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &NotifierType)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &ChannelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &Tolerance)))
    {
        JS_ReportError(pContext, "Usage: DispTest.CrcAddNotifier(Tag, CtxDmaHandle, Offset, \"NotifierType\", ChannelHandle, Tolerance)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.CrcAddNotifier(%u, %08x, %u, %s, %08x, %u)", Tag, CtxDmaHandle, Offset, NotifierType.c_str(), ChannelHandle, Tolerance);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::CrcAddNotifier(Tag, CtxDmaHandle, Offset, NotifierType, ChannelHandle, Tolerance);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_CrcAddSemaphore)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Tag;
    LwRm::Handle CtxDmaHandle;
    UINT32 Offset;
    UINT32 PollVal;
    LwRm::Handle ChannelHandle;
    UINT32 Tolerance;
    if ((NumArguments != 6) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Tag)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &CtxDmaHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Offset)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &PollVal)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &ChannelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &Tolerance)))
    {
        JS_ReportError(pContext, "Usage: DispTest.CrcAddSemaphore(Tag, CtxDmaHandle, Offset, PollVal, ChannelHandle, Tolerance)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.CrcAddSemaphore(%u, %08x, %u, %u, %08x, %u)", Tag, CtxDmaHandle, Offset, PollVal, ChannelHandle, Tolerance);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::CrcAddSemaphore(Tag, CtxDmaHandle, Offset, PollVal, ChannelHandle, Tolerance);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_CrcAddInterrupt)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Tag;
    string Name;
    UINT32 Tolerance;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Tag)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Name)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Tolerance)))
    {
        JS_ReportError(pContext, "Usage: DispTest.CrcAddInterruptAndCount(Tag, \"Name\", Tolerance)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.CrcAddInterrupt(%u, %s, %u)", Tag, Name.c_str(), Tolerance);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::CrcAddInterrupt(Tag, Name, Tolerance);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_CrcAddInterruptAndCount)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Tag;
    string Name;
    UINT32 Tolerance;
    UINT32 ExpectedCount;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Tag)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Name)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Tolerance)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &ExpectedCount)))
    {
        JS_ReportError(pContext, "Usage: DispTest.CrcAddInterruptAndCount(Tag, \"Name\", Tolerance, ExpectedCount)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.CrcAddInterruptAndCount(%u, %s, %u, %u)", Tag, Name.c_str(), Tolerance, ExpectedCount);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::CrcAddInterruptAndCount(Tag, Name, Tolerance, ExpectedCount);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_CrcAddNotifierRefHeads)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Tag;
    LwRm::Handle CtxDmaHandle;
    UINT32 Offset;
    string NotifierType;
    LwRm::Handle ChannelHandle;
    UINT32 Tolerance;
    if ((NumArguments != 6) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Tag)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &CtxDmaHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Offset)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &NotifierType)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &ChannelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &Tolerance)))
    {
        JS_ReportError(pContext, "Usage: DispTest.CrcAddNotifier(Tag, CtxDmaHandle, Offset, \"NotifierType\", ChannelHandle, Tolerance)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.CrcAddNotifierRefHeads(%u, %08x, %u, %s, %08x, %u)", Tag, CtxDmaHandle, Offset, NotifierType.c_str(), ChannelHandle, Tolerance);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::CrcAddNotifier(Tag, CtxDmaHandle, Offset, NotifierType, ChannelHandle, Tolerance, true);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);

}

C_(DispTest_CrcAddSemaphoreRefHeads)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Tag;
    LwRm::Handle CtxDmaHandle;
    UINT32 Offset;
    UINT32 PollVal;
    LwRm::Handle ChannelHandle;
    UINT32 Tolerance;
    if ((NumArguments != 6) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Tag)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &CtxDmaHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Offset)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &PollVal)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &ChannelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &Tolerance)))
    {
        JS_ReportError(pContext, "Usage: DispTest.CrcAddSemaphore(Tag, CtxDmaHandle, Offset, PollVal, ChannelHandle, Tolerance)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.CrcAddSemaphoreRefHeads(%u, %08x, %u, %u, %08x, %u)", Tag, CtxDmaHandle, Offset, PollVal, ChannelHandle, Tolerance);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::CrcAddSemaphore(Tag, CtxDmaHandle, Offset, PollVal, ChannelHandle, Tolerance, true);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_CrcAddInterruptRefHeads)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    Printf(Tee::PriNormal, "ERROR: CrcAddInterruptRefHeads is deprecated.  Please use CrcAddInterrupt or CrcAddInterruptAndCount instead.\n");
    RETURN_RC(RC::UNSUPPORTED_FUNCTION);
}

C_(DispTest_CollectLwDpsCrcs)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Head;
    if ( (NumArguments != 1) ||
         (OK != pJavaScript->FromJsval(pArguments[0], &Head)) )
    {
        JS_ReportError(pContext, "Usage: DispTest.CollectLwDpsCrcs(Head)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.CollectLwDpsCrcs(%u)", Head);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::CollectLwDpsCrcs(Head);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_CollectLwstomEvent)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    string FileName;
    string LwstomString;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &FileName)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &LwstomString)))
    {
        JS_ReportError(pContext, "Usage: DispTest.CollectLwstomEvent(\"filename\", \"string\")");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.CollectLwstomEvent(%s, %s)", FileName.c_str(), LwstomString.c_str());
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::CollectLwstomEvent(FileName, LwstomString);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_CrcSetStartTime)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: DispTest.CrcSetStartTime()");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.CrcSetStartTime()");
    PrintCallStatus(msg);

    // call the function
    DispTest::CrcSetStartTime();
    RC rc = OK;

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_CrcWriteEventFiles)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: DispTest.CrcWriteEventFiles()");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.CrcWriteEventFiles()");
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::CrcWriteEventFiles();

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_Cleanup)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: DispTest.Cleanup()");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.Cleanup()");
    PrintCallStatus(msg);

    RC rc = OK;

    // call the function
    DispTest::Cleanup();
    DispTest::CrcCleanup();

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_ReadCrtc)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Index;
    UINT32 Head;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Index)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Head)))
    {
        JS_ReportError(pContext, "Usage: DispTest.ReadCrtc(index, head)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.ReadCrtc(%u, %u)", Index, Head);
    PrintCallStatus(msg);

    UINT32 Rtlwalue;

    // call the function
    RC rc = DispTest::ReadCrtc(Index,Head, &Rtlwalue);

    // output completion status
    PrintReturnStatus(msg, rc, Rtlwalue);

    // store return value
    if (OK != pJavaScript->ToJsval(Rtlwalue, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in DispTest::ReadCrtc(Index, Head)");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(DispTest_WriteCrtc)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Index;
    UINT32 Data;
    UINT32 Head;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Index)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Data)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Head)))
    {
        JS_ReportError(pContext, "Usage: DispTest.WriteCrtc(index, data, head)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE,"DispTest.WriteCrtc(%u, %u, %u)", Index, Data, Head);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::WriteCrtc(Index,Data,Head);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_IsoInitVGA)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 head;
    UINT32 dac_index;
    UINT32 raster_width;
    UINT32 raster_height;
    UINT32 is_rvga;
    UINT32 en_legacy_res;
    UINT32 set_active_rstr;
    UINT32 set_pclk;
    UINT32 pclk_freq;
    UINT32 sim_turbo;
    UINT32 emu_run;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 12) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &head)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &dac_index)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &raster_width)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &raster_height)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &is_rvga)) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &en_legacy_res)) ||
        (OK != pJavaScript->FromJsval(pArguments[6], &set_active_rstr)) ||
        (OK != pJavaScript->FromJsval(pArguments[7], &set_pclk)) ||
        (OK != pJavaScript->FromJsval(pArguments[8], &pclk_freq)) ||
        (OK != pJavaScript->FromJsval(pArguments[9], &sim_turbo)) ||
        (OK != pJavaScript->FromJsval(pArguments[10], &emu_run)) ||
        (OK != pJavaScript->FromJsval(pArguments[11], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.IsoInitVGA(head, dac_index, raster_width, raster_height, is_rvga, en_legacy_res, set_active_rstr, set_pclk, pclk_freq, sim_turbo, emu_run, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.IsoInitVGA(%08x, %08x, %08x, %08x, %08x, %08x, %08x, %08x, %08x, %08x, %08x, %08f)", head, dac_index, raster_width, raster_height, is_rvga, en_legacy_res, set_active_rstr, set_pclk, pclk_freq, sim_turbo, emu_run, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::IsoInitVGA(head, dac_index, raster_width, raster_height, is_rvga, en_legacy_res, set_active_rstr, set_pclk, pclk_freq, sim_turbo, emu_run, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_VGASetClocks)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 head;
    UINT32 set_pclk;
    UINT32 pclk_freq;
    UINT32 dac_index;
    UINT32 sim_turbo;
    UINT32 emu_run;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 7) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &head)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &set_pclk)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &pclk_freq)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &dac_index)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &sim_turbo)) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &emu_run)) ||
        (OK != pJavaScript->FromJsval(pArguments[6], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.VGASetClocks(head, set_pclk, pclk_freq, dac_index, sim_turbo, emu_run, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.VGASetClocks(%08x, %08x, %08x, %08x, %08x, %08x, %08f)", head, set_pclk, pclk_freq, dac_index, sim_turbo, emu_run, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::VGASetClocks(head, set_pclk, pclk_freq, dac_index, sim_turbo, emu_run, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_IsoProgramVPLL)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 head;
    UINT32 pclk_freq;
    UINT32 mode;
    UINT32 emu_run;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &head)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &pclk_freq)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &mode)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &emu_run)))
    {
        JS_ReportError(pContext, "Usage: DispTest.IsoProgramVPLL(head, pclk_freq, mode, emu_run)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.IsoProgramVPLL(%08x, %08x, %08x, %08x)", head, pclk_freq, mode, emu_run);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::IsoProgramVPLL(head, pclk_freq, mode, emu_run);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_IsoInitLiteVGA)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;

    UINT32 is_rvga;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &is_rvga)))
    {
        JS_ReportError(pContext, "Usage: DispTest.IsoInitLiteVGA(is_rvga)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.IsoInitLiteVGA(%08x)", is_rvga);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::IsoInitLiteVGA(is_rvga);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_IsoShutDowlwGA)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 head;
    UINT32 dac_index;
    FLOAT64 TimeoutMs;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &head)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &dac_index)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &TimeoutMs)))
    {
        JS_ReportError(pContext, "Usage: DispTest.IsoShutDowlwGA(head, dac_index, TimeoutMs)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.IsoShutDowlwGA(%08x, %08x, %08f)", head, dac_index, TimeoutMs);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::IsoShutDowlwGA(head, dac_index, TimeoutMs);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_IsoDumpImages)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    string rtl_pixel_trace_file;
    string test_name;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &rtl_pixel_trace_file))||
        (OK != pJavaScript->FromJsval(pArguments[1], &test_name)))
    {
        JS_ReportError(pContext, "Usage: DispTest.IsoDumpImages(rtl_pixel_trace_file, test_name)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.IsoDumpImages(rtl_pixel_trace_file, test_name)");
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::IsoDumpImages(rtl_pixel_trace_file, test_name, false);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_IsoIndexedRegWrVGA)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT16 port;
    UINT16 index;
    UINT16 data;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &port)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &index)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &data)))
    {
        JS_ReportError(pContext, "Usage: DispTest.IsoIndexedRegWrVGA(port, index, data)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.IsoIndexedRegWrVGA(%08x, %08x, %08x)", port, index, data);
    PrintCallStatus(msg);

    // call the function
    RC rc = DispTest::IsoIndexedRegWrVGA(port, (UINT08) index, (UINT08) data);

    // output completion status
    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_IsoIndexedRegRdVGA)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT16 port;
    UINT16 index;
    UINT16 data;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &port)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &index)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &data)))
    {
        JS_ReportError(pContext, "Usage: DispTest.IsoIndexedRegRdVGA(port, index, data)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.IsoIndexedRegRdVGA(%08x, %08x, %08x)", port, index, data);
    PrintCallStatus(msg);

    // call the function
    UINT08 temp = (UINT08) data;
    RC rc = DispTest::IsoIndexedRegRdVGA(port, (UINT08) index, temp);

    // output completion status
    PrintReturnStatus(msg, rc);

    // store return value
    if (OK != pJavaScript->ToJsval(temp, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in DispTest::IsoIndexedRegRdVGA(port, index, data)");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    // output completion status
//    PrintReturnStatus(msg, rc);
//    RETURN_RC(rc);
    return JS_TRUE;
}

C_(DispTest_FillVGABackdoor)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    string filename;
    UINT32 offset = 0;
    UINT32 size = 0x20000;
    if ((NumArguments == 1) &&
        (OK == pJavaScript->FromJsval(pArguments[0], &filename))) {
    } else if ((NumArguments == 3) &&
               (OK == pJavaScript->FromJsval(pArguments[0], &filename)) &&
               (OK == pJavaScript->FromJsval(pArguments[1], &offset)) &&
               (OK == pJavaScript->FromJsval(pArguments[2], &size))
        ) {
    } else {
        JS_ReportError(pContext, "Usage: DispTest.FillVGABackdoor(filename, offset, size)");
        return JS_FALSE;
    }

    // call the function
    RC rc = DispTest::FillVGABackdoor(filename, offset, size);

    RETURN_RC(rc);
}

C_(DispTest_SaveVGABackdoor)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    string filename;
    UINT32 offset = 0;
    UINT32 size = 0x20000;
    if ((NumArguments == 1) &&
        (OK == pJavaScript->FromJsval(pArguments[0], &filename))) {
    } else if ((NumArguments == 3) &&
               (OK == pJavaScript->FromJsval(pArguments[0], &filename)) &&
               (OK == pJavaScript->FromJsval(pArguments[1], &offset)) &&
               (OK == pJavaScript->FromJsval(pArguments[2], &size))
        ) {
    } else {
        JS_ReportError(pContext, "Usage: DispTest.SaveVGABackdoor(filename, offset, size)");
        return JS_FALSE;
    }

    // call the function
    RC rc = DispTest::SaveVGABackdoor(filename, offset, size);

    RETURN_RC(rc);
}

C_(DispTest_BackdoorVgaInit)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: DispTest.BackdoorVgaInit()");
        return JS_FALSE;
    }

    // call the function
    DispTest::BackdoorVgaInit();

    RC rc = OK;
    RETURN_RC(rc);
}

C_(DispTest_LowPower)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: DispTest.LowPower()");
        return JS_FALSE;
    }

    RETURN_RC(DispTest::LowPower());
}

C_(DispTest_LowerPower)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: DispTest.LowerPower()");
        return JS_FALSE;
    }

    RETURN_RC(DispTest::LowerPower());
}

C_(DispTest_LowestPower)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: DispTest.LowestPower()");
        return JS_FALSE;
    }

    RETURN_RC(DispTest::LowestPower());
}

C_(DispTest_BlockLevelClockGating)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: DispTest.BlockLevelClockGating()");
        return JS_FALSE;
    }

    RETURN_RC(DispTest::BlockLevelClockGating());
}

C_(DispTest_SlowLwclk)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: DispTest.SlowLwclk()");
        return JS_FALSE;
    }

    RETURN_RC(DispTest::SlowLwclk());
}

C_(DispTest_ControlPrepForResumeFromUnfrUpd)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: DispTest.ControlPrepForResumeFromUnfrUpd()");
        return JS_FALSE;
    }

    RETURN_RC(DispTest::ControlPrepForResumeFromUnfrUpd());
}

C_(DispTest_BackdoorVgaRelease)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: DispTest.BackdoorVgaRelease()");
        return JS_FALSE;
    }

    // call the function
    DispTest::BackdoorVgaRelease();

    RC rc = OK;
    RETURN_RC(rc);
}

C_(DispTest_BackdoorVgaWr08)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 offset = 0;
    UINT32 data = 0;
    if ((NumArguments != 2) ||
       (OK != pJavaScript->FromJsval(pArguments[0], &offset)) ||
       (OK != pJavaScript->FromJsval(pArguments[1], &data)) )
    {
        JS_ReportError(pContext, "Usage: DispTest.BackdoorVgaWr08(Offset, Data)");
        return JS_FALSE;
    }

    // call the function
    RC  rc;
    CHECK_RC (DispTest::BackdoorVgaWr08(offset, (UINT08)data));
//    RC rc = DispTest::BackdoorVgaWr08(offset, (UINT08)data);
//    if (rc != OK)
//    {
//        JS_ReportError(pContext, "Error in DispTest.BackdoorVgaWr08");
//        *pReturlwalue = JSVAL_NULL;
//        return JS_FALSE;
//    }
//    return JS_TRUE;

    // output completion status
////    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_BackdoorVgaRd08)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 offset = 0;
    if ((NumArguments != 1) ||
       (OK != pJavaScript->FromJsval(pArguments[0], &offset)) )
    {
        JS_ReportError(pContext, "Usage: DispTest.BackdoorVgaRd08(Offset)");
        return JS_FALSE;
    }

    // call the function
    UINT08 data;
    RC rc = DispTest::BackdoorVgaRd08(offset, &data);

    // store return value
    if ((rc != OK) || (OK != pJavaScript->ToJsval((UINT32)data, pReturlwalue)))
    {
        JS_ReportError(pContext, "Error in DispTest.BackdoorVgaRd08");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(DispTest_BackdoorVgaWr16)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 offset = 0;
    UINT32 data = 0;
    if ((NumArguments != 2) ||
       (OK != pJavaScript->FromJsval(pArguments[0], &offset)) ||
       (OK != pJavaScript->FromJsval(pArguments[1], &data)) )
    {
        JS_ReportError(pContext, "Usage: DispTest.BackdoorVgaWr16(Offset, Data)");
        return JS_FALSE;
    }

    // call the function
    RC  rc;
    CHECK_RC (DispTest::BackdoorVgaWr16(offset, (UINT16)data));
//    RC rc = DispTest::BackdoorVgaWr16(offset, (UINT16)data);
//    if (rc != OK)
//    {
//        JS_ReportError(pContext, "Error in DispTest.BackdoorVgaWr16");
//        *pReturlwalue = JSVAL_NULL;
//        return JS_FALSE;
//    }
//    return JS_TRUE;

    // output completion status
////    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_BackdoorVgaRd16)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 offset = 0;
    if ((NumArguments != 1) ||
       (OK != pJavaScript->FromJsval(pArguments[0], &offset)) )
    {
        JS_ReportError(pContext, "Usage: DispTest.BackdoorVgaRd16(Offset)");
        return JS_FALSE;
    }

    // call the function
    UINT16 data;
    RC rc = DispTest::BackdoorVgaRd16(offset, &data);

    // store return value
    if ((rc != OK) || (OK != pJavaScript->ToJsval((UINT32)data, pReturlwalue)))
    {
        JS_ReportError(pContext, "Error in DispTest.BackdoorVgaRd16");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(DispTest_BackdoorVgaWr32)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 offset = 0;
    UINT32 data = 0;
    if ((NumArguments != 2) ||
       (OK != pJavaScript->FromJsval(pArguments[0], &offset)) ||
       (OK != pJavaScript->FromJsval(pArguments[1], &data)) )
    {
        JS_ReportError(pContext, "Usage: DispTest.BackdoorVgaWr32(Offset, Data)");
        return JS_FALSE;
    }

    // call the function
    RC  rc;
    CHECK_RC (DispTest::BackdoorVgaWr32(offset, (UINT32)data));
//    RC rc = DispTest::BackdoorVgaWr32(offset, data);
//    if (rc != OK)
//    {
//        JS_ReportError(pContext, "Error in DispTest.BackdoorVgaWr32");
//        *pReturlwalue = JSVAL_NULL;
//        return JS_FALSE;
//    }
//    return JS_TRUE;

    // output completion status
////    PrintReturnStatus(msg, rc);
    RETURN_RC(rc);
}

C_(DispTest_BackdoorVgaRd32)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 offset = 0;
    if ((NumArguments != 1) ||
       (OK != pJavaScript->FromJsval(pArguments[0], &offset)) )
    {
        JS_ReportError(pContext, "Usage: DispTest.BackdoorVgaRd32(Offset)");
        return JS_FALSE;
    }

    // call the function
    UINT32 data;
    RC rc = DispTest::BackdoorVgaRd32(offset, &data);

    // store return value
    if ((rc != OK) || (OK != pJavaScript->ToJsval(data, pReturlwalue)))
    {
        JS_ReportError(pContext, "Error in DispTest.BackdoorVgaRd32");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(DispTest_SetSkipDsiSupervisor2Event)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    string funcname;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &funcname))) {
        JS_ReportError(pContext, "Usage: DispTest.SetSkipDsiSupervisor2Event(funcname)");
        return JS_FALSE;
    }

    RETURN_RC(DispTest::SetSkipDsiSupervisor2Event(funcname));
}

C_(DispTest_SetPioChannelTimeout)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Handle;
    FLOAT64 TimeoutMs;

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Handle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &TimeoutMs))) {
        JS_ReportError(pContext, "Usage: DispTest.SetPioChannelTimeout(Handle, Timeout)");
        return JS_FALSE;
    }

    RETURN_RC(DispTest::SetPioChannelTimeout(Handle, TimeoutMs));
}

C_(DispTest_SetSupervisorRestartMode)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 WhichSv;
    string RestartMode;
    bool exelwteRmSvCode = false;
    string osEvent = "";
    bool clientRestart = false;

    map<string, UINT32> RestartMap;
    RestartMap["RESUME"] =  LW5070_CTRL_CMD_SET_SV_RESTART_MODE_RESTART_MODE_RESUME;
    RestartMap["SKIP"]   =  LW5070_CTRL_CMD_SET_SV_RESTART_MODE_RESTART_MODE_SKIP;
    RestartMap["REPLAY"] =  LW5070_CTRL_CMD_SET_SV_RESTART_MODE_RESTART_MODE_REPLAY;

   if ( ((NumArguments > 5) || (NumArguments < 3)) ||

        ((OK != pJavaScript->FromJsval(pArguments[0], &WhichSv))     ||
         (OK != pJavaScript->FromJsval(pArguments[1], &RestartMode)) ||
         (OK != pJavaScript->FromJsval(pArguments[2], &exelwteRmSvCode))) ||

         ((NumArguments > 3) && (OK != pJavaScript->FromJsval(pArguments[3], &osEvent))) ||
         ((NumArguments > 4) && (OK != pJavaScript->FromJsval(pArguments[4], &clientRestart))) )
    {
        JS_ReportError(pContext,
                       "Usage: DispTest.SetSupervisorRestartMode(WhichSv, RestartMode, exelwteRmSvCode, hEvent, clientRestart)");
        return JS_FALSE;
    }

    if (RestartMap.find(RestartMode) == RestartMap.end()) {
        JS_ReportError(pContext,
                       "Usage: DispTest.SetExceptionRestartMode(WhichSv, RestartMode, exelwteRmSvCode, hEvent, clientRestart)");
        return JS_FALSE;
    }
    if (WhichSv > 2) {
        JS_ReportError(pContext,
                       "Usage: DispTest.SetExceptionRestartMode(WhichSv, RestartMode, exelwteRmSvCode, hEvent, clientRestart)");
        return JS_FALSE;
    }

    RETURN_RC(DispTest::SetSupervisorRestartMode(WhichSv, RestartMap[RestartMode],
                                                exelwteRmSvCode, osEvent, clientRestart));
}

C_(DispTest_GetSupervisorRestartMode)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 WhichSv;
    UINT32 RestartMode = 0xffffffff;
    bool exelwteRmSvCode = false;
    string osEvent = "";
    bool clientRestart = false;
    JSObject *RetArray;;

    map<UINT32, string> RestartMapRev;
    RestartMapRev[LW5070_CTRL_CMD_SET_SV_RESTART_MODE_RESTART_MODE_RESUME] = "RESUME";
    RestartMapRev[LW5070_CTRL_CMD_GET_SV_RESTART_MODE_RESTART_MODE_SKIP]   = "SKIP";
    RestartMapRev[LW5070_CTRL_CMD_GET_SV_RESTART_MODE_RESTART_MODE_REPLAY] = "REPLAY";

    static char errTxt[]  = "Usage: DispTest.GetSupervisorRestartMode(WhichSv)";

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &WhichSv)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &RetArray)))
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }
    if (WhichSv > 2) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RC rc = DispTest::GetSupervisorRestartMode(WhichSv,
                                              &RestartMode, &exelwteRmSvCode, &osEvent, &clientRestart);

    if ( RestartMapRev.find(RestartMode) == RestartMapRev.end() ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    if ((OK != pJavaScript->SetElement(RetArray, 0, RestartMapRev[RestartMode])) ||
        (OK != pJavaScript->SetElement(RetArray, 1, exelwteRmSvCode)))
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    // This may fail if the user didn't supply the optional osEvent and clientRestart
    pJavaScript->SetElement(RetArray, 2, osEvent);
    pJavaScript->SetElement(RetArray, 3, clientRestart);

    RETURN_RC(rc);
}

C_(DispTest_SetExceptionRestartMode)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Channel;
    string Reason;
    string RestartMode;
    bool Assert = false;
    bool ResumeArg = false;
    UINT32 Override = 0;
    string osEvent = "";
    bool manualRestart = false;

    map<string,UINT32> ReasonMap;
    ReasonMap["PUSHBUFFER_ERR"]      = LW5070_CTRL_CMD_GET_EXCEPTION_RESTART_MODE_REASON_PUSHBUFFER_ERR;
    ReasonMap["TRAP"]                = LW5070_CTRL_CMD_SET_EXCEPTION_RESTART_MODE_REASON_TRAP;
    ReasonMap["RESERVED_METHOD"]     = LW5070_CTRL_CMD_SET_EXCEPTION_RESTART_MODE_REASON_RESERVED_METHOD;
    ReasonMap["ILWALID_ARG"]         = LW5070_CTRL_CMD_SET_EXCEPTION_RESTART_MODE_REASON_ILWALID_ARG;
    ReasonMap["ILWALID_STATE"]       = LW5070_CTRL_CMD_SET_EXCEPTION_RESTART_MODE_REASON_ILWALID_STATE;
    ReasonMap["UNRESOLVABLE_HANDLE"] = LW5070_CTRL_CMD_SET_EXCEPTION_RESTART_MODE_REASON_UNRESOLVABLE_HANDLE;

    map<string, UINT32> RestartMap;
    RestartMap["RESUME"] =  LW5070_CTRL_CMD_SET_EXCEPTION_RESTART_MODE_RESTART_MODE_RESUME;
    RestartMap["SKIP"]   =  LW5070_CTRL_CMD_SET_EXCEPTION_RESTART_MODE_RESTART_MODE_SKIP;
    RestartMap["REPLAY"] =  LW5070_CTRL_CMD_SET_EXCEPTION_RESTART_MODE_RESTART_MODE_REPLAY;

    if ( ((NumArguments > 8) || (NumArguments < 6)) ||

        ((OK != pJavaScript->FromJsval(pArguments[0], &Channel))     ||
         (OK != pJavaScript->FromJsval(pArguments[1], &Reason))      ||
         (OK != pJavaScript->FromJsval(pArguments[2], &RestartMode)) ||
         (OK != pJavaScript->FromJsval(pArguments[3], &Assert))      ||
         (OK != pJavaScript->FromJsval(pArguments[4], &ResumeArg))   ||
         (OK != pJavaScript->FromJsval(pArguments[5], &Override))) ||

         ((NumArguments > 6) && (OK != pJavaScript->FromJsval(pArguments[6], &osEvent))) ||
         ((NumArguments > 7) && (OK != pJavaScript->FromJsval(pArguments[7], &manualRestart))) )
    {
        JS_ReportError(pContext,
                       "Usage: DispTest.SetExceptionRestartMode(Channel, Reason, RestartMode, Assert, ResumeArg, Override, hEvent, ManualRestart)");
        return JS_FALSE;
    }

    if ( (ReasonMap.find(Reason) == ReasonMap.end()) ||
         (RestartMap.find(RestartMode) == RestartMap.end()) ) {
        JS_ReportError(pContext,
                       "Usage: DispTest.SetExceptionRestartMode(Channel, Reason, RestartMode, Assert, ResumeArg, Override, hEvent, ManualRestart)");
        return JS_FALSE;
    }

    RETURN_RC(DispTest::SetExceptionRestartMode(Channel, ReasonMap[Reason], RestartMap[RestartMode],
                                                Assert, ResumeArg, Override, osEvent, manualRestart));
}

C_(DispTest_GetExceptionRestartMode)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 Channel;
    string Reason;
    UINT32 RestartMode = 0xffffffff;
    bool Assert = false;
    bool ResumeArg = false;
    UINT32 Override;
    string osEvent = "";
    bool manualRestart = false;
    JSObject *RetArray;;

    map<string,UINT32> ReasonMap;
    ReasonMap["PUSHBUFFER_ERR"]      = LW5070_CTRL_CMD_GET_EXCEPTION_RESTART_MODE_REASON_PUSHBUFFER_ERR;
    ReasonMap["TRAP"]                = LW5070_CTRL_CMD_GET_EXCEPTION_RESTART_MODE_REASON_TRAP;
    ReasonMap["RESERVED_METHOD"]     = LW5070_CTRL_CMD_GET_EXCEPTION_RESTART_MODE_REASON_RESERVED_METHOD;
    ReasonMap["ILWALID_ARG"]         = LW5070_CTRL_CMD_GET_EXCEPTION_RESTART_MODE_REASON_ILWALID_ARG;
    ReasonMap["ILWALID_STATE"]       = LW5070_CTRL_CMD_GET_EXCEPTION_RESTART_MODE_REASON_ILWALID_STATE;
    ReasonMap["UNRESOLVABLE_HANDLE"] = LW5070_CTRL_CMD_GET_EXCEPTION_RESTART_MODE_REASON_UNRESOLVABLE_HANDLE;

    map<UINT32, string> RestartMapRev;
    RestartMapRev[LW5070_CTRL_CMD_GET_EXCEPTION_RESTART_MODE_RESTART_MODE_RESUME] = "RESUME";
    RestartMapRev[LW5070_CTRL_CMD_GET_EXCEPTION_RESTART_MODE_RESTART_MODE_SKIP]   = "SKIP";
    RestartMapRev[LW5070_CTRL_CMD_GET_EXCEPTION_RESTART_MODE_RESTART_MODE_REPLAY] = "REPLAY";

    static char errTxt[]  = "Usage: DispTest.GetExceptionRestartMode(Channel, Reason, [RestartMode Assert])";

    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Channel)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &Reason)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &RetArray)))
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    if ( ReasonMap.find(Reason) == ReasonMap.end() ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RC rc = DispTest::GetExceptionRestartMode(Channel, ReasonMap[Reason],
                                              &RestartMode, &Assert, &ResumeArg, &Override, &osEvent, &manualRestart);

    if ( RestartMapRev.find(RestartMode) == RestartMapRev.end() ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    if ((OK != pJavaScript->SetElement(RetArray, 0, RestartMapRev[RestartMode])) ||
        (OK != pJavaScript->SetElement(RetArray, 1, Assert))  ||
        (OK != pJavaScript->SetElement(RetArray, 2, ResumeArg))  ||
        (OK != pJavaScript->SetElement(RetArray, 3, Override)))
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    // This may fail if the user didn't supply the optional osEvent and manualRestart
    pJavaScript->SetElement(RetArray, 4, osEvent);
    pJavaScript->SetElement(RetArray, 5, manualRestart);

    RETURN_RC(rc);
}

C_(DispTest_SetSkipFreeCount)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 channel;
    bool skip;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &channel)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &skip)))
    {
        JS_ReportError(pContext, "Usage: DispTest.SetSkipFreeCount(Channel, Skip)");
        return JS_FALSE;
    }

    // call the function
    RC rc = DispTest::SetSkipFreeCount(channel, skip);
    RETURN_RC(rc);
}

C_(DispTest_SetSwapHeads)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    bool swap;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &swap)))
    {
        JS_ReportError(pContext, "Usage: DispTest.SetSwapHeads(Swap)");
        return JS_FALSE;
    }

    // call the function
    RC rc = DispTest::SetSwapHeads(swap);
    RETURN_RC(rc);
}

C_(DispTest_SetUseHead)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    bool use_head;
    UINT32 vga_head;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &use_head)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &vga_head)))
    {
        JS_ReportError(pContext, "Usage: DispTest.SetUseHead(use_head, head)");
        return JS_FALSE;
    }

    // call the function
    RC rc = DispTest::SetUseHead(use_head, vga_head);
    RETURN_RC(rc);
}

C_(DispTest_SetVPLLRef)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.SetVPLLRef(Head, RefName, Frequency)";

    // parse arguments
    JavaScriptPtr pJavaScript;

    UINT32 Head;
    string RefName;
    UINT32 Frequency;

    map<string,UINT32> RefNameMap;
    RefNameMap["XTAL"] = LW0080_CTRL_CMD_SET_VPLL_REF_REF_NAME_XTAL;
    RefNameMap["SPPLL0"] = LW0080_CTRL_CMD_SET_VPLL_REF_REF_NAME_SPPLL0;
    RefNameMap["SPPLL1"] = LW0080_CTRL_CMD_SET_VPLL_REF_REF_NAME_SPPLL1;
    RefNameMap["EXT_REF"] = LW0080_CTRL_CMD_SET_VPLL_REF_REF_NAME_EXT_REF;
    RefNameMap["QUAL_EXT_REF"] = LW0080_CTRL_CMD_SET_VPLL_REF_REF_NAME_QUAL_EXT_REF;
    RefNameMap["EXT_SPREAD"] = LW0080_CTRL_CMD_SET_VPLL_REF_REF_NAME_EXT_SPREAD;

    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Head))    ||
        (OK != pJavaScript->FromJsval(pArguments[1], &RefName)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &Frequency)) )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    MASSERT(RefNameMap.find(RefName) != RefNameMap.end() );

    RETURN_RC(DispTest::SetVPLLRef(Head, RefNameMap[RefName], Frequency));
}

C_(DispTest_SetVPLLArchType)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.SetVPLLArchType(Head, ArchType)";

    // parse arguments
    JavaScriptPtr pJavaScript;

    UINT32 Head;
    string ArchType;

    map<string,UINT32> ArchTypeMap;
    ArchTypeMap["SINGLE_STAGE_A"] = LW0080_CTRL_CMD_SET_VPLL_ARCH_TYPE_SINGLE_STAGE_A;
    ArchTypeMap["SINGLE_STAGE_B"] = LW0080_CTRL_CMD_SET_VPLL_ARCH_TYPE_SINGLE_STAGE_B;
    ArchTypeMap["DUAL_STAGE"] = LW0080_CTRL_CMD_SET_VPLL_ARCH_TYPE_DUAL_STAGE;

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Head))    ||
        (OK != pJavaScript->FromJsval(pArguments[1], &ArchType)) )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    MASSERT(ArchTypeMap.find(ArchType) != ArchTypeMap.end() );

    RETURN_RC(DispTest::SetVPLLArchType(Head, ArchTypeMap[ArchType]));
}

C_(DispTest_SetSorSequencerStartPoint)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.SetSorSequencerStartPoint(OrNum, Enables, pdPc, puPc, puPcAlt, normalStart, safeStart, normalState, safeState, SkipWaitForVsync, SeqProgPresent, program = Array()";

    // parse arguments
    JavaScriptPtr pJavaScript;

    UINT32 OrNum;
    UINT32 enables;
    UINT32 pdPc;
    UINT32 puPc;
    UINT32 puPcAlt;
    UINT32 normalStart;
    UINT32 safeStart;
    UINT32 normalState;
    UINT32 safeState;

    bool SkipWaitForVsync;
    bool SeqProgPresent;

    UINT32 SequenceProgram[LW5070_CTRL_CMD_GET_SOR_SEQ_CTL_SEQ_PROG_SIZE];

    JsArray jsDataArray;

    if ((NumArguments != 12) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &OrNum))    ||
        (OK != pJavaScript->FromJsval(pArguments[1], &enables))    ||
        (OK != pJavaScript->FromJsval(pArguments[2], &pdPc)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &puPc)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &puPcAlt)) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &normalStart)) ||
        (OK != pJavaScript->FromJsval(pArguments[6], &safeStart)) ||
        (OK != pJavaScript->FromJsval(pArguments[7], &normalState)) ||
        (OK != pJavaScript->FromJsval(pArguments[8], &safeState)) ||
        (OK != pJavaScript->FromJsval(pArguments[9], &SkipWaitForVsync))    ||
        (OK != pJavaScript->FromJsval(pArguments[10], &SeqProgPresent)) ||
        (OK != pJavaScript->FromJsval(pArguments[11], &jsDataArray)) ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    MASSERT(jsDataArray.size() == LW5070_CTRL_CMD_GET_SOR_SEQ_CTL_SEQ_PROG_SIZE);

    for (UINT32 i = 0; i < LW5070_CTRL_CMD_GET_SOR_SEQ_CTL_SEQ_PROG_SIZE; i++) {
        if (OK != pJavaScript->FromJsval(jsDataArray[i], &(SequenceProgram[i]))) {
            JS_ReportError(pContext, "data element bad value.");
            return JS_FALSE;
        }
    }

    RETURN_RC(DispTest::ControlSetSorSequence(OrNum,
                                              REF_VAL(LW_DISPTEST_CONTROL_SET_OR_SEQUENCER_ENABLE_PUPC_ALT,enables),
                                              pdPc,
                                              REF_VAL(LW_DISPTEST_CONTROL_SET_OR_SEQUENCER_ENABLE_PDPC,enables),
                                              puPc,
                                              REF_VAL(LW_DISPTEST_CONTROL_SET_OR_SEQUENCER_ENABLE_PDPC_ALT,enables),
                                              puPcAlt,
                                              REF_VAL(LW_DISPTEST_CONTROL_SET_OR_SEQUENCER_ENABLE_NORMALSTART,enables),
                                              normalStart,
                                              REF_VAL(LW_DISPTEST_CONTROL_SET_OR_SEQUENCER_ENABLE_SAFESTART,enables),
                                              safeStart,
                                              REF_VAL(LW_DISPTEST_CONTROL_SET_OR_SEQUENCER_ENABLE_NORMALSTATE,enables),
                                              normalState,
                                              REF_VAL(LW_DISPTEST_CONTROL_SET_OR_SEQUENCER_ENABLE_SAFESTATE,enables),
                                              safeState,
                                              SkipWaitForVsync,
                                              SeqProgPresent,
                                              SequenceProgram) );
}

C_(DispTest_GetSorSequencerStartPoint)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    RC rc = OK;

    static char errTxt[]  = "Usage: DispTest.GetSorSequencerStartPoint(OrNum, RetVal = Array(), GetSeqProgram, Program = Array())";

    UINT32 OrNum;
    UINT32 puPcAlt;
    UINT32 pdPc;
    UINT32 pdPcAlt;
    UINT32 normalStart;
    UINT32 safeStart;
    UINT32 normalState;
    UINT32 safeState;
    bool  GetSeqProg;
    UINT32 SequenceProgram[LW5070_CTRL_CMD_GET_SOR_SEQ_CTL_SEQ_PROG_SIZE];

    JSObject* RetArray;
    JSObject* RetSeqProgram;

    // parse arguments
    JavaScriptPtr pJavaScript;

    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &OrNum))    ||
        (OK != pJavaScript->FromJsval(pArguments[1], &RetArray)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &GetSeqProg)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &RetSeqProgram)) )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    rc = DispTest::ControlGetSorSequence(OrNum,
                                         &puPcAlt,
                                         &pdPc,
                                         &pdPcAlt,
                                         &normalStart,
                                         &safeStart,
                                         &normalState,
                                         &safeState,
                                         GetSeqProg,
                                         SequenceProgram);

    if ((OK != pJavaScript->SetElement(RetArray, 0, puPcAlt )) ||
        (OK != pJavaScript->SetElement(RetArray, 1, pdPc )) ||
        (OK != pJavaScript->SetElement(RetArray, 2, pdPcAlt )) ||
        (OK != pJavaScript->SetElement(RetArray, 3, normalStart )) ||
        (OK != pJavaScript->SetElement(RetArray, 4, safeStart )) ||
        (OK != pJavaScript->SetElement(RetArray, 5, normalState )) ||
        (OK != pJavaScript->SetElement(RetArray, 6, safeState )) )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    if (GetSeqProg) {
        for (UINT32 i = 0; i < LW5070_CTRL_CMD_GET_SOR_SEQ_CTL_SEQ_PROG_SIZE; i++) {
            if (OK != pJavaScript->SetElement(RetSeqProgram, i, SequenceProgram[i] ) ) {
                JS_ReportError(pContext, errTxt);
                return JS_FALSE;
            }
        }
    }

    RETURN_RC(rc);
}

C_(DispTest_SetPiorSequencerStartPoint)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.SetPiorSequencerStartPoint(OrNum, Enables, pdPc, puPc, puPcAlt, normalStart, safeStart, normalState, safeState, SkipWaitForVsync, SeqProgPresent, program = Array()";

    JavaScriptPtr pJavaScript;

    UINT32 OrNum;
    UINT32 enables;
    UINT32 pdPc;
    UINT32 puPc;
    UINT32 puPcAlt;
    UINT32 normalStart;
    UINT32 safeStart;
    UINT32 normalState;
    UINT32 safeState;

    bool SkipWaitForVsync;
    bool SeqProgPresent;

    UINT32 SequenceProgram[LW5070_CTRL_CMD_GET_PIOR_SEQ_CTL_SEQ_PROG_SIZE];

    JsArray jsDataArray;

    if ((NumArguments != 12) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &OrNum))    ||
        (OK != pJavaScript->FromJsval(pArguments[1], &enables))    ||
        (OK != pJavaScript->FromJsval(pArguments[2], &pdPc)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &puPc)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &puPcAlt)) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &normalStart)) ||
        (OK != pJavaScript->FromJsval(pArguments[6], &safeStart)) ||
        (OK != pJavaScript->FromJsval(pArguments[7], &normalState)) ||
        (OK != pJavaScript->FromJsval(pArguments[8], &safeState)) ||
        (OK != pJavaScript->FromJsval(pArguments[9], &SkipWaitForVsync))    ||
        (OK != pJavaScript->FromJsval(pArguments[10], &SeqProgPresent)) ||
        (OK != pJavaScript->FromJsval(pArguments[11], &jsDataArray)) ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    MASSERT(jsDataArray.size() == LW5070_CTRL_CMD_GET_PIOR_SEQ_CTL_SEQ_PROG_SIZE);

    for (UINT32 i = 0; i < LW5070_CTRL_CMD_GET_PIOR_SEQ_CTL_SEQ_PROG_SIZE; i++) {
        if (OK != pJavaScript->FromJsval(jsDataArray[i], &(SequenceProgram[i]))) {
            JS_ReportError(pContext, "data element bad value.");
            return JS_FALSE;
        }
    }

    RETURN_RC(DispTest::ControlSetPiorSequence(OrNum,
                                              REF_VAL(LW_DISPTEST_CONTROL_SET_OR_SEQUENCER_ENABLE_PUPC_ALT,enables),
                                              pdPc,
                                              REF_VAL(LW_DISPTEST_CONTROL_SET_OR_SEQUENCER_ENABLE_PDPC,enables),
                                              puPc,
                                              REF_VAL(LW_DISPTEST_CONTROL_SET_OR_SEQUENCER_ENABLE_PDPC_ALT,enables),
                                              puPcAlt,
                                              REF_VAL(LW_DISPTEST_CONTROL_SET_OR_SEQUENCER_ENABLE_NORMALSTART,enables),
                                              normalStart,
                                              REF_VAL(LW_DISPTEST_CONTROL_SET_OR_SEQUENCER_ENABLE_SAFESTART,enables),
                                              safeStart,
                                              REF_VAL(LW_DISPTEST_CONTROL_SET_OR_SEQUENCER_ENABLE_NORMALSTATE,enables),
                                              normalState,
                                              REF_VAL(LW_DISPTEST_CONTROL_SET_OR_SEQUENCER_ENABLE_SAFESTATE,enables),
                                              safeState,
                                              SkipWaitForVsync,
                                              SeqProgPresent,
                                              SequenceProgram) );
}

C_(DispTest_GetPiorSequencerStartPoint)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    RC rc = OK;

    static char errTxt[]  = "Usage: DispTest.GetPiorSequencerStartPoint(OrNum, RetVal = Array(), GetSeqProgram, Program = Array())";

    UINT32 OrNum;
    UINT32 puPcAlt;
    UINT32 pdPc;
    UINT32 pdPcAlt;
    UINT32 normalStart;
    UINT32 safeStart;
    UINT32 normalState;
    UINT32 safeState;
    bool  GetSeqProg;
    UINT32 SequenceProgram[LW5070_CTRL_CMD_GET_PIOR_SEQ_CTL_SEQ_PROG_SIZE];

    JSObject* RetArray;
    JSObject* RetSeqProgram;

    // parse arguments
    JavaScriptPtr pJavaScript;

    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &OrNum))    ||
        (OK != pJavaScript->FromJsval(pArguments[1], &RetArray)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &GetSeqProg)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &RetSeqProgram)) )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    rc = DispTest::ControlGetPiorSequence(OrNum,
                                         &puPcAlt,
                                         &pdPc,
                                         &pdPcAlt,
                                         &normalStart,
                                         &safeStart,
                                         &normalState,
                                         &safeState,
                                         GetSeqProg,
                                         SequenceProgram);

    if ((OK != pJavaScript->SetElement(RetArray, 0, puPcAlt )) ||
        (OK != pJavaScript->SetElement(RetArray, 1, pdPc )) ||
        (OK != pJavaScript->SetElement(RetArray, 2, pdPcAlt )) ||
        (OK != pJavaScript->SetElement(RetArray, 3, normalStart )) ||
        (OK != pJavaScript->SetElement(RetArray, 4, safeStart )) ||
        (OK != pJavaScript->SetElement(RetArray, 5, normalState )) ||
        (OK != pJavaScript->SetElement(RetArray, 6, safeState )) )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    if (GetSeqProg) {
        for (UINT32 i = 0; i < LW5070_CTRL_CMD_GET_PIOR_SEQ_CTL_SEQ_PROG_SIZE; i++) {
            if (OK != pJavaScript->SetElement(RetSeqProgram, i, SequenceProgram[i] ) ) {
                JS_ReportError(pContext, errTxt);
                return JS_FALSE;
            }
        }
    }

    RETURN_RC(rc);
}

#define DISPTEST_SETSOR(field, value) \
(DRF_NUM(5070_CTRL,_CMD_SET_SOR_OP_MODE_,field,REF_VAL(LW_DISPTEST_CONTROL_SET_SOR_OP_MODE_##field,value)))

#define DISPTEST_GETSOR(field, value) \
(DRF_NUM(_DISPTEST,_CONTROL_SET_SOR_OP_MODE_,field,REF_VAL(LW5070_CTRL_CMD_GET_SOR_OP_MODE_##field,value)))

C_(DispTest_ControlSetSorOpMode)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetSorOpMode(orNumber, category, puTxda, puTxdb, puTxca, puTxcb, upper, mode, linkActA, linkActB, lvdsEn, dupSync, newMode, balanced, plldiv, rotClk, rotDat, lvdsDual)";

    JavaScriptPtr pJavaScript;

    UINT32 orNumber;
    UINT32 category;
    UINT32 puTxda;
    UINT32 puTxdb;
    UINT32 puTxca;
    UINT32 puTxcb;
    UINT32 upper;
    UINT32 mode;
    UINT32 linkActA;
    UINT32 linkActB;
    UINT32 lvdsEn;
    UINT32 lvdsDual;
    UINT32 dupSync;
    UINT32 newMode;
    UINT32 balanced;
    UINT32 plldiv;
    UINT32 rotClk;
    UINT32 rotDat;

    if (((NumArguments != 17) && (NumArguments != 18)) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &orNumber )) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &category )) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &puTxda )) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &puTxdb )) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &puTxca )) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &puTxcb )) ||
        (OK != pJavaScript->FromJsval(pArguments[6], &upper )) ||
        (OK != pJavaScript->FromJsval(pArguments[7], &mode )) ||
        (OK != pJavaScript->FromJsval(pArguments[8], &linkActA )) ||
        (OK != pJavaScript->FromJsval(pArguments[9], &linkActB )) ||
        (OK != pJavaScript->FromJsval(pArguments[10], &lvdsEn )) ||
        (OK != pJavaScript->FromJsval(pArguments[11], &dupSync )) ||
        (OK != pJavaScript->FromJsval(pArguments[12], &newMode )) ||
        (OK != pJavaScript->FromJsval(pArguments[13], &balanced )) ||
        (OK != pJavaScript->FromJsval(pArguments[14], &plldiv )) ||
        (OK != pJavaScript->FromJsval(pArguments[15], &rotClk )) ||
        (OK != pJavaScript->FromJsval(pArguments[16], &rotDat )) ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }
    if ((NumArguments == 18) &&
        (OK != pJavaScript->FromJsval(pArguments[17], &lvdsDual )) ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    category = DISPTEST_SETSOR(CATEGORY,category);
    puTxda = ( DISPTEST_SETSOR(PU_TXDA_0,puTxda) |
               DISPTEST_SETSOR(PU_TXDA_1,puTxda) |
               DISPTEST_SETSOR(PU_TXDA_2,puTxda) |
               DISPTEST_SETSOR(PU_TXDA_3,puTxda) );
    puTxdb = ( DISPTEST_SETSOR(PU_TXDB_0,puTxdb) |
               DISPTEST_SETSOR(PU_TXDB_1,puTxdb) |
               DISPTEST_SETSOR(PU_TXDB_2,puTxdb) |
               DISPTEST_SETSOR(PU_TXDB_3,puTxdb) );

    puTxca = DISPTEST_SETSOR(PU_TXCA,puTxca);
    puTxcb = DISPTEST_SETSOR(PU_TXCB,puTxcb);

    upper = DISPTEST_SETSOR(UPPER,upper);
    mode = DISPTEST_SETSOR(MODE,mode);

    linkActA = DISPTEST_SETSOR(LINKACTA,linkActA);
    linkActB = DISPTEST_SETSOR(LINKACTB,linkActB);

    lvdsEn = DISPTEST_SETSOR(LVDS_EN,lvdsEn);
    dupSync = DISPTEST_SETSOR(DUP_SYNC,dupSync);
    newMode = DISPTEST_SETSOR(NEW_MODE,newMode);
    balanced = DISPTEST_SETSOR(BALANCED,balanced);
    plldiv = DISPTEST_SETSOR(PLLDIV,plldiv);
    rotClk = DISPTEST_SETSOR(ROTCLK,rotClk);
    rotDat = DISPTEST_SETSOR(ROTDAT,rotDat);

    if (NumArguments == 17) {
      if (lvdsEn && linkActA && linkActB && !puTxcb) {
      // for backwards compatability look for calls with not enough args
      // and set dual mode as appropriate
        lvdsDual = 1;
        puTxcb = LW_DISPTEST_CONTROL_SET_SOR_OP_MODE_PU_TXCB_ENABLE;
      } else {
        lvdsDual = 0;
      }
    }

    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.ControlSetSorOpMode(LVDS Dual = %0d)", lvdsDual);
    RETURN_RC(DispTest::ControlSetSorOpMode(orNumber, category,
                                            puTxda, puTxdb,
                                            puTxca, puTxcb,
                                            upper, mode,
                                            linkActA, linkActB,
                                            lvdsEn, lvdsDual, dupSync,
                                            newMode, balanced,
                                            plldiv,
                                            rotClk, rotDat));

}

C_(DispTest_ControlSetActiveSubdevice)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetActiveSubdevice(subdeviceIndex)";

    JavaScriptPtr pJavaScript;
    UINT32        subdeviceIndex;

    if (NumArguments != 1 || OK != pJavaScript->FromJsval(pArguments[0], &subdeviceIndex)) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(DispTest::ControlSetActiveSubdevice(subdeviceIndex));
}

#define DISPTEST_SET_PIOR(field, value) \
(DRF_NUM(5070_CTRL,_CMD_SET_PIOR_OP_MODE_,field,REF_VAL(LW_DISPTEST_CONTROL_SET_PIOR_OP_MODE_##field,value)))

C_(DispTest_ControlSetPiorOpMode)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetPiorOpMode(orNumber, category, clkPolarity, clkMode, clkPhs, unusedPins, polarity, dataMuxing, clkDelay, dataDelay [, DROMaster, DROPinset])";

    JavaScriptPtr pJavaScript;

    UINT32 orNumber;
    UINT32 category;
    UINT32 clkPolarity;
    UINT32 clkMode;
    UINT32 clkPhs;
    UINT32 unusedPins;
    UINT32 polarity;
    UINT32 dataMuxing;
    UINT32 clkDelay;
    UINT32 dataDelay;
    UINT32 DroMaster;
    UINT32 DroPinset;

    if (NumArguments != 10 && NumArguments != 12) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    if (
        (OK != pJavaScript->FromJsval(pArguments[0], &orNumber    )) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &category    )) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &clkPolarity )) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &clkMode     )) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &clkPhs      )) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &unusedPins  )) ||
        (OK != pJavaScript->FromJsval(pArguments[6], &polarity    )) ||
        (OK != pJavaScript->FromJsval(pArguments[7], &dataMuxing  )) ||
        (OK != pJavaScript->FromJsval(pArguments[8], &clkDelay    )) ||
        (OK != pJavaScript->FromJsval(pArguments[9], &dataDelay   )) ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    // Handle the default parameters
    if (NumArguments == 12) {
       if ( (OK != pJavaScript->FromJsval(pArguments[10], &DroMaster    )) ||
            (OK != pJavaScript->FromJsval(pArguments[11], &DroPinset    ))) {

            JS_ReportError(pContext, errTxt);
            return JS_FALSE;
       }
    }
    else {
        DroMaster = LW_DISPTEST_CONTROL_SET_PIOR_OP_MODE_DRO_MASTER_NO;
        DroPinset = LW_DISPTEST_CONTROL_SET_PIOR_OP_MODE_DRO_DRIVE_PIN_SET_NEITHER;
    }

    category    = DISPTEST_SET_PIOR(CATEGORY,       category   );
    clkPolarity = DISPTEST_SET_PIOR(CLK_POLARITY,   clkPolarity);
    clkMode     = DISPTEST_SET_PIOR(CLK_MODE,       clkMode    );
    clkPhs      = DISPTEST_SET_PIOR(CLK_PHS,        clkPhs     );
    unusedPins  = DISPTEST_SET_PIOR(UNUSED_PINS,    unusedPins );
    polarity    = DISPTEST_SET_PIOR(POLARITY_H,     polarity   ) |
                  DISPTEST_SET_PIOR(POLARITY_V,     polarity   ) |
                  DISPTEST_SET_PIOR(POLARITY_DE,    polarity   );
    dataMuxing  = DISPTEST_SET_PIOR(DATA_MUXING,    dataMuxing );
    clkDelay    = DISPTEST_SET_PIOR(CLK_DLY,        clkDelay   );
    dataDelay   = DISPTEST_SET_PIOR(DATA_DLY,       dataDelay  );
    DroMaster   = DISPTEST_SET_PIOR(DRO_MASTER,     DroMaster  );

    RETURN_RC(DispTest::ControlSetPiorOpMode(orNumber, category,
                                            clkPolarity, clkMode,
                                            clkPhs, unusedPins,
                                            polarity, dataMuxing,
                                            clkDelay, dataDelay,
                                            DroMaster, DroPinset));

}

C_(DispTest_ControlSetUseTestPiorSettings)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetUseTestPiorSettings(bEnable)";

    JavaScriptPtr pJavaScript;

    UINT32 unEnable;

    if (NumArguments != 1) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    if (
        (OK != pJavaScript->FromJsval(pArguments[0], &unEnable    ))
       )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(DispTest::ControlSetUseTestPiorSettings(unEnable));

}

C_(DispTest_ControlGetPiorOpMode)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    RC rc = OK;

    static char errTxt[]  = "Usage: DispTest.ControlGetPiorOpMode(orNumber, RetVal = Array())";

    JavaScriptPtr pJavaScript;

    UINT32 orNumber    = 0;
    UINT32 category    = 0;
    UINT32 clkPolarity = 0;
    UINT32 clkMode     = 0;
    UINT32 clkPhs      = 0;
    UINT32 unusedPins  = 0;
    UINT32 polarity    = 0;
    UINT32 dataMuxing  = 0;
    UINT32 clkDelay    = 0;
    UINT32 dataDelay   = 0;
    JSObject* RetArray = 0;

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &orNumber    )) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &RetArray    )) )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    C_CHECK_RC(DispTest::ControlGetPiorOpMode(orNumber,
                                              &category,
                                              &clkPolarity,
                                              &clkMode,
                                              &clkPhs,
                                              &unusedPins,
                                              &polarity,
                                              &dataMuxing,
                                              &clkDelay,
                                              &dataDelay));

    if ((OK != pJavaScript->SetElement(RetArray, 0, category )) ||
        (OK != pJavaScript->SetElement(RetArray, 1, clkPolarity )) ||
        (OK != pJavaScript->SetElement(RetArray, 2, clkMode )) ||
        (OK != pJavaScript->SetElement(RetArray, 3, clkPhs )) ||
        (OK != pJavaScript->SetElement(RetArray, 4, unusedPins )) ||
        (OK != pJavaScript->SetElement(RetArray, 5, polarity )) ||
        (OK != pJavaScript->SetElement(RetArray, 6, dataMuxing )) ||
        (OK != pJavaScript->SetElement(RetArray, 7, clkDelay )) ||
        (OK != pJavaScript->SetElement(RetArray, 8, dataDelay )) )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC (rc);
}

//we'll leave the paramters in LW_DISPTEST space and colwert them the LW5070_CTRL
//space inside the implementing function.  This way if later versions need to colwert to a different define
//space, they don't have to pull some old legacy headers to interpret the parameters.
C_(DispTest_ControlSetSfBlank)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetSfBlank(sfNumber, transition, status, waitForCompletion)";

    JavaScriptPtr pJavaScript;

    UINT32 sfNumber;
    UINT32 transition;
    UINT32 status;
    UINT32 waitForCompletion;

    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &sfNumber          )) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &transition        )) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &status            )) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &waitForCompletion )) ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(DispTest::ControlSetSfBlank(sfNumber, transition,
                                          status, waitForCompletion));
}

#define DISPTEST_SET_DAC_CONFIG(field, value) \
(DRF_NUM(5070_CTRL,_CMD_SET_DAC_CONFIG_,field,REF_VAL(LW_DISPTEST_CONTROL_SET_DAC_CONFIG_##field,value)))

C_(DispTest_ControlSetDacConfig)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetDacConfig(orNumber, cpstDac0Src, cpstDac1Src, cpstDac2Src, cpstDac3Src, compDac0Src, compDac1Src, compDac2Src, compDac3Src, driveSync)";

    JavaScriptPtr pJavaScript;

    UINT32 orNumber;
    UINT32 cpstDac0Src;
    UINT32 cpstDac1Src;
    UINT32 cpstDac2Src;
    UINT32 cpstDac3Src;
    UINT32 compDac0Src;
    UINT32 compDac1Src;
    UINT32 compDac2Src;
    UINT32 compDac3Src;
    UINT32 driveSync;

    if ((NumArguments != 10) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &orNumber          )) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &cpstDac0Src)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &cpstDac1Src)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &cpstDac2Src)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &cpstDac3Src)) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &compDac0Src)) ||
        (OK != pJavaScript->FromJsval(pArguments[6], &compDac1Src)) ||
        (OK != pJavaScript->FromJsval(pArguments[7], &compDac2Src)) ||
        (OK != pJavaScript->FromJsval(pArguments[8], &compDac3Src)) ||
        (OK != pJavaScript->FromJsval(pArguments[9], &driveSync )) ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    cpstDac0Src = DISPTEST_SET_DAC_CONFIG(CPST_DAC0_SRC,cpstDac0Src);
    cpstDac1Src = DISPTEST_SET_DAC_CONFIG(CPST_DAC1_SRC,cpstDac1Src);
    cpstDac2Src = DISPTEST_SET_DAC_CONFIG(CPST_DAC2_SRC,cpstDac2Src);
    cpstDac3Src = DISPTEST_SET_DAC_CONFIG(CPST_DAC3_SRC,cpstDac3Src);
    compDac0Src = DISPTEST_SET_DAC_CONFIG(COMP_DAC0_SRC,compDac0Src);
    compDac1Src = DISPTEST_SET_DAC_CONFIG(COMP_DAC1_SRC,compDac1Src);
    compDac2Src = DISPTEST_SET_DAC_CONFIG(COMP_DAC2_SRC,compDac2Src);
    compDac3Src = DISPTEST_SET_DAC_CONFIG(COMP_DAC3_SRC,compDac3Src);
    driveSync   = DISPTEST_SET_DAC_CONFIG(DRIVE_SYNC,   driveSync  );

    RETURN_RC(DispTest::ControlSetDacConfig(orNumber,
        cpstDac0Src, cpstDac1Src, cpstDac2Src, cpstDac3Src,
        compDac0Src, compDac1Src, compDac2Src, compDac3Src,
        driveSync));

}

C_(DispTest_ControlSetErrorMask)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetErrorMask(channelClass, channelInstance, method, mode, errorCode)";

    JavaScriptPtr pJavaScript;

    UINT32 channelHandle;
    UINT32 method;
    UINT32 mode;
    UINT32 errorCode;

    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &channelHandle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &method       )) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &mode         )) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &errorCode    )) ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(DispTest::ControlSetErrorMask(channelHandle, method, mode, errorCode));
}

#define DISPTEST_SETSOR_PWR(field, value) \
(DRF_NUM(5070_CTRL,_CMD_SET_SOR_PWM_FLAGS_,field,REF_VAL(LW_DISPTEST_CONTROL_SET_SOR_PWM_FLAGS_##field,value)))

C_(DispTest_ControlSetSorPwmMode)
{
    RC rc = OK;
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetSorPwmMode(orNumber, targetFreq, actualFreq, div, resolution, dutyCycle, flags)";

    JavaScriptPtr pJavaScript;

    UINT32 orNumber;
    UINT32 targetFreq;
    UINT32 actualFreq;
    UINT32 div;
    UINT32 resolution;
    UINT32 dutyCycle;
    UINT32 flags;

    JSObject* RetArray;

    if ((NumArguments != 7) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &orNumber)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &targetFreq)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &RetArray)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &div)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &resolution)) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &dutyCycle)) ||
        (OK != pJavaScript->FromJsval(pArguments[6], &flags)) ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    flags = ( DISPTEST_SETSOR_PWR(USE_SPECIFIED_DIV,flags) |
              DISPTEST_SETSOR_PWR(PROG_DUTY_CYCLE,flags) |
              DISPTEST_SETSOR_PWR(PROG_FREQ_AND_RANGE,flags) );

    rc = DispTest::ControlSetSorPwmMode(orNumber, targetFreq,
                                        &actualFreq, div, resolution,
                                        dutyCycle, flags);

    if ((OK != pJavaScript->SetElement(RetArray, 0, actualFreq )) )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(rc);
}

C_(DispTest_ControlGetDacPwrMode)
{
    RC rc = OK;
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetDacPwrMode(orNumber, RetVal = Array())";

    JavaScriptPtr pJavaScript;
    JSObject* RetArray;

    UINT32 orNumber;
    UINT32 normalHSync;
    UINT32 normalVSync;
    UINT32 normalData;
    UINT32 normalPower;
    UINT32 safeHSync;
    UINT32 safeVSync;
    UINT32 safeData;
    UINT32 safePower;

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &orNumber)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &RetArray)) ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    rc = DispTest::ControlGetDacPwrMode(orNumber,
                                        &normalHSync,
                                        &normalVSync,
                                        &normalData,
                                        &normalPower,
                                        &safeHSync,
                                        &safeVSync,
                                        &safeData,
                                        &safePower);

    if ((OK != pJavaScript->SetElement(RetArray, 0, normalHSync )) ||
        (OK != pJavaScript->SetElement(RetArray, 1, normalVSync )) ||
        (OK != pJavaScript->SetElement(RetArray, 2, normalData  )) ||
        (OK != pJavaScript->SetElement(RetArray, 3, normalPower )) ||
        (OK != pJavaScript->SetElement(RetArray, 4, safeHSync   )) ||
        (OK != pJavaScript->SetElement(RetArray, 5, safeVSync   )) ||
        (OK != pJavaScript->SetElement(RetArray, 6, safeData    )) ||
        (OK != pJavaScript->SetElement(RetArray, 7, safePower   )) )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(rc);
}

#define DISPTEST_SETDAC_PWR(field, value) \
(DRF_NUM(5070_CTRL,_CMD_SET_DAC_PWR_FLAGS_,field,REF_VAL(LW_DISPTEST_CONTROL_SET_DAC_PWR_FLAGS_##field,value)))

C_(DispTest_ControlSetDacPwrMode)
{
    RC rc = OK;
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetDacPwrMode(orNumber, normalHSync, normalVSync, normalData, normalPower, safeHSync, safeVSync, safeData, safPower, flags)";

    JavaScriptPtr pJavaScript;

    UINT32 orNumber;
    UINT32 normalHSync;
    UINT32 normalVSync;
    UINT32 normalData;
    UINT32 normalPower;
    UINT32 safeHSync;
    UINT32 safeVSync;
    UINT32 safeData;
    UINT32 safPower;
    UINT32 flags;

    if ((NumArguments != 10) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &orNumber)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &normalHSync)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &normalVSync)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &normalData)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &normalPower)) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &safeHSync)) ||
        (OK != pJavaScript->FromJsval(pArguments[6], &safeVSync)) ||
        (OK != pJavaScript->FromJsval(pArguments[7], &safeData)) ||
        (OK != pJavaScript->FromJsval(pArguments[8], &safPower)) ||
        (OK != pJavaScript->FromJsval(pArguments[9], &flags)) ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    flags = ( DISPTEST_SETDAC_PWR(SPECIFIED_NORMAL,flags) |
              DISPTEST_SETDAC_PWR(SPECIFIED_SAFE,flags) |
              DISPTEST_SETDAC_PWR(FORCE_SWITCH,flags) );

    rc = DispTest::ControlSetDacPwrMode(orNumber, normalHSync,
                                        normalVSync, normalData,
                                        normalPower, safeHSync,
                                        safeVSync, safeData,
                                        safPower, flags);

    RETURN_RC(rc);
}

C_(DispTest_ControlSetRgUnderflowProp)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetRgUnderflowProp(head, enable, clearUnderflow, mode)";

    JavaScriptPtr pJavaScript;

    UINT32 head;
    UINT32 enable;
    UINT32 clearUnderflow;
    UINT32 mode;

    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &head)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &enable)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &clearUnderflow)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &mode)) ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(DispTest::ControlSetRgUnderflowProp(head, enable, clearUnderflow, mode));
}

C_(DispTest_ControlSetSemaAcqDelay)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetSemaAcqDelay(delayUs)";

    JavaScriptPtr pJavaScript;

    UINT32 delayUs;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &delayUs)) ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(DispTest::ControlSetSemaAcqDelay(delayUs));
}

C_(DispTest_ControlGetSorOpMode)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    RC rc = OK;

    static char errTxt[]  = "Usage: DispTest.GetSorOpMode(OrNum, category, RetVal = Array(15))";

    UINT32 orNumber;
    UINT32 category;
    UINT32 puTxda;
    UINT32 puTxdb;
    UINT32 puTxca;
    UINT32 puTxcb;
    UINT32 upper;
    UINT32 mode;
    UINT32 linkActA;
    UINT32 linkActB;
    UINT32 lvdsEn;
    UINT32 lvdsDual;
    UINT32 dupSync;
    UINT32 newMode;
    UINT32 balanced;
    UINT32 plldiv;
    UINT32 rotClk;
    UINT32 rotDat;

    JSObject* RetArray;

    // parse arguments
    JavaScriptPtr pJavaScript;

    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &orNumber))    ||
        (OK != pJavaScript->FromJsval(pArguments[1], &category))    ||
        (OK != pJavaScript->FromJsval(pArguments[2], &RetArray)) )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    rc = DispTest::ControlGetSorOpMode(orNumber,
                                       category,
                                       &puTxda, &puTxdb,
                                       &puTxca, &puTxcb,
                                       &upper, &mode,
                                       &linkActA, &linkActB,
                                       &lvdsEn, &lvdsDual, &dupSync,
                                       &newMode, &balanced,
                                       &plldiv,
                                       &rotClk, &rotDat);

    category = DISPTEST_GETSOR(CATEGORY,category);
    puTxda = ( DISPTEST_GETSOR(PU_TXDA_0,puTxda) |
               DISPTEST_GETSOR(PU_TXDA_1,puTxda) |
               DISPTEST_GETSOR(PU_TXDA_2,puTxda) |
               DISPTEST_GETSOR(PU_TXDA_3,puTxda) );
    puTxdb = ( DISPTEST_GETSOR(PU_TXDB_0,puTxdb) |
               DISPTEST_GETSOR(PU_TXDB_1,puTxdb) |
               DISPTEST_GETSOR(PU_TXDB_2,puTxdb) |
               DISPTEST_GETSOR(PU_TXDB_3,puTxdb) );

    puTxca = DISPTEST_GETSOR(PU_TXCA,puTxca);
    puTxcb = DISPTEST_GETSOR(PU_TXCB,puTxcb);

    upper = DISPTEST_GETSOR(UPPER,upper);
    mode = DISPTEST_GETSOR(MODE,mode);

    linkActA = DISPTEST_GETSOR(LINKACTA,linkActA);
    linkActB = DISPTEST_GETSOR(LINKACTB,linkActB);

    lvdsEn = DISPTEST_GETSOR(LVDS_EN,lvdsEn);
    dupSync = DISPTEST_GETSOR(DUP_SYNC,dupSync);
    newMode = DISPTEST_GETSOR(NEW_MODE,newMode);
    balanced = DISPTEST_GETSOR(BALANCED,balanced);
    plldiv = DISPTEST_GETSOR(PLLDIV,plldiv);
    rotClk = DISPTEST_GETSOR(ROTCLK,rotClk);
    rotDat = DISPTEST_GETSOR(ROTDAT,rotDat);

    if ((OK != pJavaScript->SetElement(RetArray, 0, puTxda )) ||
        (OK != pJavaScript->SetElement(RetArray, 1, puTxdb )) ||
        (OK != pJavaScript->SetElement(RetArray, 2, puTxca )) ||
        (OK != pJavaScript->SetElement(RetArray, 3, puTxcb )) ||
        (OK != pJavaScript->SetElement(RetArray, 4, upper )) ||
        (OK != pJavaScript->SetElement(RetArray, 5, mode )) ||
        (OK != pJavaScript->SetElement(RetArray, 6, linkActA )) ||
        (OK != pJavaScript->SetElement(RetArray, 7, linkActB )) ||
        (OK != pJavaScript->SetElement(RetArray, 8, lvdsEn )) ||
        (OK != pJavaScript->SetElement(RetArray, 9, dupSync )) ||
        (OK != pJavaScript->SetElement(RetArray, 10, newMode )) ||
        (OK != pJavaScript->SetElement(RetArray, 11, balanced )) ||
        (OK != pJavaScript->SetElement(RetArray, 12, plldiv )) ||
        (OK != pJavaScript->SetElement(RetArray, 13, rotClk )) ||
        (OK != pJavaScript->SetElement(RetArray, 14, rotDat )) ||
        (OK != pJavaScript->SetElement(RetArray, 15, lvdsDual ))
        )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(rc);
}

C_(DispTest_ControlSetVbiosAttentionEvent)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetVbiosAttentionEvent(funcname)";

    JavaScriptPtr pJavaScript;
    string funcname;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &funcname)) ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(DispTest::ControlSetVbiosAttentionEvent(funcname));
}

C_(DispTest_ControlSetRgFliplockProp)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetRgFliplockProp(head, maxSwapLockoutSkew, swapLockoutStart)";

    JavaScriptPtr pJavaScript;

    UINT32 head;
    UINT32 maxSwapLockoutSkew;
    UINT32 swapLockoutStart;

    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &head)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &maxSwapLockoutSkew)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &swapLockoutStart)) ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(DispTest::ControlSetRgFliplockProp(head, maxSwapLockoutSkew, swapLockoutStart));
}

C_(DispTest_ControlGetPrevModeSwitchFlags)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    JSObject *RetArray;

    UINT32 WhichHead;      // [IN]
    UINT32 blank;          // [OUT]
    UINT32 shutdown;       // [OUT]

    static char errTxt[]  = "Usage: DispTest.ControlGetPrevModeSwitchFlags( WhichHead, RetVal = Array());";

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &WhichHead)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &RetArray)))
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RC rc = DispTest::ControlGetPrevModeSwitchFlags(WhichHead, &blank, &shutdown);

    if ((OK != pJavaScript->SetElement(RetArray, 0, blank)) ||
        (OK != pJavaScript->SetElement(RetArray, 1, shutdown)) )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(rc);
}

C_(DispTest_ControlGetDacLoad)
{
    RC rc = OK;
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetDacLoad(orNumber, RetVal = Array())";

    JavaScriptPtr pJavaScript;
    JSObject* RetArray;

    UINT32 orNumber;
    UINT32 mode;
    UINT32 valDCCrt;
    UINT32 valACCrt;
    UINT32 perDCCrt;
    UINT32 perSampleCrt;
    UINT32 valDCTv;
    UINT32 valACTv;
    UINT32 perDCTv;
    UINT32 perSampleTv;
    UINT32 perAuto;
    UINT32 load;
    UINT32 status;

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &orNumber)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &RetArray)) ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    rc = DispTest::ControlGetDacLoad(orNumber,
                                     &mode,
                                     &valDCCrt,
                                     &valACCrt,
                                     &perDCCrt,
                                     &perSampleCrt,
                                     &valDCTv,
                                     &valACTv,
                                     &perDCTv,
                                     &perSampleTv,
                                     &perAuto,
                                     &load,
                                     &status);

    if ((OK != pJavaScript->SetElement(RetArray, 0, mode    )) ||
        (OK != pJavaScript->SetElement(RetArray, 1, valDCCrt   )) ||
        (OK != pJavaScript->SetElement(RetArray, 2, valACCrt   )) ||
        (OK != pJavaScript->SetElement(RetArray, 3, perDCCrt   )) ||
        (OK != pJavaScript->SetElement(RetArray, 4, perSampleCrt   )) ||
        (OK != pJavaScript->SetElement(RetArray, 5, valDCTv   )) ||
        (OK != pJavaScript->SetElement(RetArray, 6, valACTv   )) ||
        (OK != pJavaScript->SetElement(RetArray, 7, perDCTv   )) ||
        (OK != pJavaScript->SetElement(RetArray, 8, perSampleTv   )) ||
        (OK != pJavaScript->SetElement(RetArray, 9, perAuto )) ||
        (OK != pJavaScript->SetElement(RetArray, 10, load    )) ||
        (OK != pJavaScript->SetElement(RetArray, 11, status  )) )

    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(rc);
}

#define DISPTEST_SETDAC_LOAD(field, value) \
(DRF_NUM(5070_CTRL,_CMD_SET_DAC_LOAD_FLAGS_,field,REF_VAL(LW_DISPTEST_CONTROL_SET_DAC_LOAD_FLAGS_##field,value)))

C_(DispTest_ControlSetDacLoad)
{
    RC rc = OK;
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetDacLoad(orNumber, mode, "
                            "valDCCrt, valACCrt, perDCCrt, perSampleCrt, "
                            "valDCTv, valACTv, perDCTv, perSampleTv, "
                            "perAuto, flags)";

    JavaScriptPtr pJavaScript;

    UINT32 orNumber;
    UINT32 mode;
    UINT32 valDCCrt;
    UINT32 valACCrt;
    UINT32 perDCCrt;
    UINT32 perSampleCrt;
    UINT32 valDCTv;
    UINT32 valACTv;
    UINT32 perDCTv;
    UINT32 perSampleTv;
    UINT32 perAuto;
    UINT32 flags;

    if ((NumArguments != 12) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &orNumber )) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &mode     )) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &valDCCrt )) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &valACCrt )) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &perDCCrt )) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &perSampleCrt )) ||
        (OK != pJavaScript->FromJsval(pArguments[6], &valDCTv  )) ||
        (OK != pJavaScript->FromJsval(pArguments[7], &valACTv  )) ||
        (OK != pJavaScript->FromJsval(pArguments[8], &perDCTv  )) ||
        (OK != pJavaScript->FromJsval(pArguments[9], &perSampleTv  )) ||
        (OK != pJavaScript->FromJsval(pArguments[10], &perAuto  )) ||
        (OK != pJavaScript->FromJsval(pArguments[11], &flags    )) ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    flags = DISPTEST_SETDAC_LOAD(SKIP_WAIT_FOR_VALID,flags);

    rc = DispTest::ControlSetDacLoad(orNumber, mode,
                                     valDCCrt, valACCrt, perDCCrt, perSampleCrt,
                                     valDCTv, valACTv, perDCTv, perSampleTv,
                                     perAuto, flags);

    RETURN_RC(rc);
}

C_(DispTest_ControlGetOverlayFlipCount)
{
    RC rc = OK;
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlGetOverlayFlipCount(channelInstance, RetVal = Array())";

    JavaScriptPtr pJavaScript;
    JSObject* RetArray;

    UINT32 channelInstance;
    UINT32 value;

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &channelInstance)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &RetArray)) ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    rc = DispTest::ControlGetOverlayFlipCount(channelInstance, &value);

    if (OK != pJavaScript->SetElement(RetArray, 0, value))
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(rc);
}

C_(DispTest_ControlSetOverlayFlipCount)
{
    RC rc = OK;
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetOverlayFlipCount(channelInstance, forceCount)";

    JavaScriptPtr pJavaScript;

    UINT32 channelInstance;
    UINT32 forceCount;

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &channelInstance )) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &forceCount      )) )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    rc = DispTest::ControlSetOverlayFlipCount(channelInstance, forceCount);

    RETURN_RC(rc);
}

C_(DispTest_ControlSetDmiElv)
{
    RC rc = OK;
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetDmiElv(channelInstance, which_field, value)";

    JavaScriptPtr pJavaScript;

    UINT32 channelInstance;
    UINT32 what, value;

    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &channelInstance )) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &what )) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &value      )) )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    rc = DispTest::ControlSetDmiElv(channelInstance, what, value);

    RETURN_RC(rc);
}

C_(DispTest_ControlSetUnfrUpdEvent)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetUnfrUpdEvent(funcname)";

    JavaScriptPtr pJavaScript;
    string funcname;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &funcname)) ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(DispTest::ControlSetUnfrUpdEvent(funcname));
}

C_(DispTest_StartErrorLogging)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.StartErrorLogging()";

    if ((NumArguments != 0)) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(ErrorLogger::StartingTest());
}

C_(DispTest_StopErrorLoggingAndCheck)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.StopErrorLoggingAndCheck(path_to_test_interrupt)";

    JavaScriptPtr pJavaScript;

    string FileName;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &FileName))
            ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RC rc2 = ErrorLogger::WriteFile("./test.interrupt");
    RC rc3 = ErrorLogger::CompareErrorsWithFile(FileName.c_str(),ErrorLogger::Exact);
    RC rc1 = ErrorLogger::TestCompleted();

    // Check return value
    if ((rc1 != OK) || (rc2 != OK) || (rc3 != OK))
    {
        JS_ReportError(pContext, "Error in DispTest.StopErrorLoggingAndCheck");
        return JS_FALSE;
    }

    return JS_TRUE;
}

C_(DispTest_InstallErrorLoggerFilter)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.InstallErrorLoggerFilter(logger msg pattern)";

    JavaScriptPtr pJavaScript;

    string pattern;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &pattern))
            ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    DispTest::InstallErrorLoggerFilter(pattern);

    return JS_TRUE;
}

C_(DispTest_GetBaseChannelScanline)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle Handle;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Handle)))
    {
        JS_ReportError(pContext, "Usage: Value = DispTest.GetBaseChannelScanline(Handle)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.GetBaseChannelScanline(%08x)", Handle);
    PrintCallStatus(msg);

    // call the function
    UINT32 Rtlwalue;
    RC rc = DispTest::GetBaseChannelScanline(Handle, &Rtlwalue);

    // output completion status
    PrintReturnStatus(msg, rc, Rtlwalue);

    // store return value
    if ((rc != OK) || (OK != pJavaScript->ToJsval(Rtlwalue, pReturlwalue)))
    {
        JS_ReportError(pContext, "Error oclwrred in DispTest::GetBaseChannelScanline(Handle)");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(DispTest_GetChannelPut)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle Handle;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Handle)))
    {
        JS_ReportError(pContext, "Usage: Value = DispTest.GetChannelPut(Handle)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.GetChannelPut(%08x)", Handle);
    PrintCallStatus(msg);

    // call the function
    UINT32 Rtlwalue;
    RC rc = DispTest::GetChannelPut(Handle, &Rtlwalue);

    // output completion status
    PrintReturnStatus(msg, rc, Rtlwalue);

    // store return value
    if ((rc != OK) || (OK != pJavaScript->ToJsval(Rtlwalue, pReturlwalue)))
    {
        JS_ReportError(pContext, "Error oclwrred in DispTest::GetChannelPut(Handle)");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(DispTest_SetChannelPut)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle Handle;
    UINT32 newPut;

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Handle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &newPut)))
    {
        JS_ReportError(pContext, "Usage: Value = DispTest.SetChannelPut(Handle, newPut)");
        return JS_FALSE;
    }

    // call the function
    RETURN_RC(DispTest::SetChannelPut(Handle, newPut));

}

C_(DispTest_GetChannelGet)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle Handle;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Handle)))
    {
        JS_ReportError(pContext, "Usage: Value = DispTest.GetChannelGet(Handle)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.GetChannelGet(%08x)", Handle);
    PrintCallStatus(msg);

    // call the function
    UINT32 Rtlwalue;
    RC rc = DispTest::GetChannelGet(Handle, &Rtlwalue);

    // output completion status
    PrintReturnStatus(msg, rc, Rtlwalue);

    // store return value
    if ((rc != OK) || (OK != pJavaScript->ToJsval(Rtlwalue, pReturlwalue)))
    {
        JS_ReportError(pContext, "Error oclwrred in DispTest::GetChannelGet(Handle)");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(DispTest_SetChannelGet)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    LwRm::Handle Handle;
    UINT32 newGet;

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &Handle)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &newGet)))
    {
        JS_ReportError(pContext, "Usage: Value = DispTest.SetChannelGet(Handle, newGet)");
        return JS_FALSE;
    }

    // call the function
    RETURN_RC(DispTest::SetChannelGet(Handle, newGet));

}

C_(DispTest_GetCrcInterruptCount)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    string InterruptName;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &InterruptName)))
    {
        JS_ReportError(pContext, "Usage: Count = DispTest.GetCrcInterruptCount(InterruptName)");
        return JS_FALSE;
    }

    // output call status
    char msg[DISPTEST_STATUS_MESSAGE_MAXSIZE];
    snprintf(msg, DISPTEST_STATUS_MESSAGE_MAXSIZE, "DispTest.GetCrcInterruptCount(%s)", InterruptName.c_str());
    PrintCallStatus(msg);

    // call the function
    UINT32 Rtlwalue;
    RC rc = DispTest::GetCrcInterruptCount(InterruptName, &Rtlwalue);

    // output completion status
    PrintReturnStatus(msg, rc, Rtlwalue);

    // store return value
    if ((rc != OK) || (OK != pJavaScript->ToJsval(Rtlwalue, pReturlwalue)))
    {
        JS_ReportError(pContext, "Error oclwrred in DispTest::GetCrcInterruptCount(InterruptName)");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(DispTest_GetPtimer)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    RC rc = OK;

    static char errTxt[]  = "Usage: DispTest.GetPtimer(RetVal = Array(2))";

    UINT32 TimeHi;
    UINT32 TimeLo;
    JSObject *RetArray;;

    // parse arguments
    JavaScriptPtr pJavaScript;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &RetArray)) )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    rc = DispTest::GetPtimer(&TimeHi, &TimeLo);

    if ((OK != pJavaScript->SetElement(RetArray, 0, TimeLo )) ||
        (OK != pJavaScript->SetElement(RetArray, 1, TimeHi ))
      )
      {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
      }

    RETURN_RC(rc);
}

C_(DispTest_PTimerCallwlator)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    RC rc = OK;

    static char errTxt[]  = "Usage: DispTest.TimerCallwlator(LHS = Array(2), RHS = Array(2), RetVal = Array(2), Op = String)";

    UINT32 TimeHi = 0;
    UINT32 TimeLo;
    UINT64 TimeRHS, TimeLHS, TimeResult = 0;

    JSObject *RetArray;;
    JsArray LHSArray;;
    JsArray RHSArray;;
    string Op;

    // parse arguments
    JavaScriptPtr pJavaScript;

    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &LHSArray)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &RHSArray)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &RetArray)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &Op)) )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    if ( (OK != pJavaScript->FromJsval(LHSArray[0], &TimeLo)) ||
         (OK != pJavaScript->FromJsval(LHSArray[1], &TimeHi)) ) {
        JS_ReportWarning(pContext, "data element bad value.");
    }

    TimeLHS =   TimeHi;
    TimeLHS <<= 32;
    TimeLHS |=  TimeLo;

    if ( (OK != pJavaScript->FromJsval(RHSArray[0], &TimeLo)) ||
         (OK != pJavaScript->FromJsval(RHSArray[1], &TimeHi)) ) {
        JS_ReportWarning(pContext, "data element bad value.");
    }

    TimeRHS =   TimeHi;
    TimeRHS <<= 32;
    TimeRHS |=  TimeLo;

    if (Op == "+")
    {
        TimeResult = TimeLHS + TimeRHS;
    }
    else if (Op == "-")
    {
        TimeResult = TimeLHS - TimeRHS;
    }
    else if (Op == "*")
    {
        TimeResult = TimeLHS * TimeRHS;
    }
    else if (Op == "/")
    {
        TimeResult = TimeLHS / TimeRHS;
    }
    else if (Op == "%")
    {
        TimeResult = TimeLHS % TimeRHS;
    }
    else if (Op == "<")
    {
        TimeResult = (TimeLHS < TimeRHS);
    }
    else if (Op == ">")
    {
        TimeResult = (TimeLHS > TimeRHS);
    }
    else if (Op == ">=")
    {
        TimeResult = (TimeLHS >= TimeRHS);
    }
    else if (Op == "<=")
    {
        TimeResult = (TimeLHS <= TimeRHS);
    }
    else if (Op == "==")
    {
        TimeResult = (TimeLHS == TimeRHS);
    }
    else if (Op == "!=")
    {
        TimeResult = (TimeLHS != TimeRHS);
    }

    TimeLo = (UINT32) (TimeResult & 0xFFFFFFFF);
    TimeHi = (UINT32) ((TimeResult >> 32) & 0xFFFFFFFF);

    if ((OK != pJavaScript->SetElement(RetArray, 0, TimeLo )) ||
        (OK != pJavaScript->SetElement(RetArray, 1, TimeHi ))
      )
      {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
      }

    RETURN_RC(rc);
}

C_(DispTest_GetBoundGpuSubdevice)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    static char errTxt[]  = "Usage: DispTest.GetBoundGpuSubdevice()";

    if ((NumArguments != 0)) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    JavaScriptPtr pJavaScript;
    RC rc;
    GpuSubdevice *pSubdev = DispTest::GetBoundGpuSubdevice();

    // Create a JsGpuDevice object to return to JS
    // Note:  This will be cleaned up by JS Garbage Collector
    JsGpuSubdevice * pJsGpuSubdevice = new JsGpuSubdevice();
    MASSERT(pJsGpuSubdevice);
    pJsGpuSubdevice->SetGpuSubdevice(pSubdev);
    C_CHECK_RC(pJsGpuSubdevice->CreateJSObject(pContext, pObject));

    if (pJavaScript->ToJsval(pJsGpuSubdevice->GetJSObject(), pReturlwalue) != OK)
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }
    return JS_TRUE;
}

P_(DispTest_Get_gpu)
{
    MASSERT(pContext    != 0);
    MASSERT(pValue      != 0);

    JavaScriptPtr pJavaScript;
    //TODO: this will eventually need to do a lookup based on the class system
    GpuSubdevice *pSubdev = DispTest::GetBoundGpuSubdevice();

    // Create a JsGpuDevice object to return to JS
    // Note:  This will be cleaned up by JS Garbage Collector
    JsGpuSubdevice * pJsGpuSubdevice = new JsGpuSubdevice();
    MASSERT(pJsGpuSubdevice);
    pJsGpuSubdevice->SetGpuSubdevice(pSubdev);
    pJsGpuSubdevice->CreateJSObject(pContext, pObject);

    if (pJavaScript->ToJsval(pJsGpuSubdevice->GetJSObject(), pValue) != OK)
    {
        JS_ReportError(pContext, "DispTest.gpu failed to get gpu subdevice.");
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(DispTest_SetupCarveoutFb)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // Check the arguments.
    JavaScriptPtr pJavaScript;
    UINT32        carveOutFbSize;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &carveOutFbSize)))
    {
        JS_ReportError (pContext, "Usage: DispTest.SetupCarveoutFb (carveOutFbSize)");
        return JS_FALSE;
    }

    // Carve out the system memory as the FB.
    void *pMemDesc = NULL;

#if 0 // Comment out dead MCP carveout set-up.  Will it come back for hopper?
    if (OK != Platform::AllocPages (carveOutFbSize, &pMemDesc, true, 32, Memory::UC,
                                    DispTest::GetBoundGpuSubdevice()->GetGpuInst()))
    {
        JS_ReportError (pContext, "Fail to carve out system memory as the FB");
        return (JS_FALSE);
    }

    // Set up the carved out system memory as the FB.
    UINT64 physicalAddress;

    physicalAddress = (UINT64) Platform::GetPhysicalAddress (pMemDesc, 0);

    UINT64 carveoutBase = physicalAddress / LW_IXVE_C11_ZEROFB_CARVEOUT_BASE_ADR__MULTIPLE;
    if (carveoutBase > 0xFFFFFFFF)
    {
        JS_ReportError (pContext, "carveoutBase out of range");
        return (JS_FALSE);
    }

    UINT64 carveoutSize = carveOutFbSize / LW_IXVE_C11_ZEROFB_CARVEOUT_SIZE_SYSSIZE__MULTIPLE;
    if (carveoutSize > 0xFFFFFFFF)
    {
        JS_ReportError (pContext, "carveoutSize out of range");
        return (JS_FALSE);
    }

    GpuPtr ()->RegWr32 (DEVICE_BASE (LW_PCFG) + LW_IXVE_C11_ZEROFB_CARVEOUT_BASE, (UINT32)carveoutBase);
    GpuPtr ()->RegWr32 (DEVICE_BASE (LW_PCFG) + LW_IXVE_C11_ZEROFB_CARVEOUT_SIZE, (UINT32)carveoutSize);
#endif

    // Set the return value.
    *pReturlwalue = JSVAL_NULL;
    if (OK != pJavaScript->ToJsval((UINT64) pMemDesc, pReturlwalue))
    {
       JS_ReportError(pContext, "Error oclwrred in DispTest.SetupCarveoutFb (...)");
       *pReturlwalue = JSVAL_NULL;
       return JS_FALSE;
    }

    // Success.
    return JS_TRUE;
}

C_(DispTest_CleanupCarveoutFb)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // Check the arguments.
    JavaScriptPtr pJavaScript;
    UINT64        hMemDesc;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &hMemDesc)))
    {
        JS_ReportError (pContext, "Usage: DispTest.CleanupCarveoutFb (hCarveoutFb)");
        return JS_FALSE;
    }

    // Free the allocated system memory.
    Platform::FreePages ((void*) hMemDesc);

    // Success.
    return JS_TRUE;
}

C_(DispTest_GetPinsetCount)
{
    MASSERT(pContext     != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // Initialization.
    *pReturlwalue = JSVAL_NULL;

    // Get the pinset count.
    RC            rc;
    JavaScriptPtr pJavaScript;
    UINT32        uPinsetCount;

    rc = DispTest::GetPinsetCount(&uPinsetCount);
    if (rc != OK)
    {
        JS_ReportError(pContext, "Error oclwrred in DispTest.GetPinsetCount (...)");
        return (JS_FALSE);
    }

    // Set the return value.
    if (OK != pJavaScript->ToJsval((UINT64) uPinsetCount, pReturlwalue))
    {
       JS_ReportError(pContext, "Error oclwrred in DispTest.GetPinsetCount (...)");
       return JS_FALSE;
    }

    // Success.
    return JS_TRUE;
}

C_(DispTest_GetScanLockPin)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // Initialization.
    *pReturlwalue = JSVAL_NULL;

    // Check the arguments.
    JavaScriptPtr pJavaScript;
    UINT32        uPinset;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &uPinset)))
    {
        JS_ReportError(pContext, "Usage: DispTest.GetScanLockPin (uPinset)");
        return JS_FALSE;
    }

    // Get the scan-lock pin.
    RC     rc;
    UINT32 uScanLockPin;

    rc = DispTest::GetPinsetLockPins (uPinset, &uScanLockPin, NULL);
    if (rc != OK)
    {
        JS_ReportError(pContext, "Error oclwrred in DispTest.GetScanLockPin (uPinset)");
        return (JS_FALSE);
    }

    // Set the return value.
    if (OK != pJavaScript->ToJsval((UINT64) uScanLockPin, pReturlwalue))
    {
       JS_ReportError(pContext, "Error oclwrred in DispTest.GetScanLockPin (uPinset)");
       return JS_FALSE;
    }

    // Success.
    return JS_TRUE;
}

C_(DispTest_GetFlipLockPin)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // Initialization.
    *pReturlwalue = JSVAL_NULL;

    // Check the arguments.
    JavaScriptPtr pJavaScript;
    UINT32        uPinset;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &uPinset)))
    {
        JS_ReportError(pContext, "Usage: DispTest.GetFlipLockPin (uPinset)");
        return JS_FALSE;
    }

    // Get the flip-lock pin.
    RC     rc;
    UINT32 uFlipLockPin;

    rc = DispTest::GetPinsetLockPins (uPinset, NULL, &uFlipLockPin);
    if (rc != OK)
    {
        JS_ReportError(pContext, "Error oclwrred in DispTest.GetFlipLockPin (uPinset)");
        return (JS_FALSE);
    }

    // Set the return value.
    if (OK != pJavaScript->ToJsval((UINT64) uFlipLockPin, pReturlwalue))
    {
       JS_ReportError(pContext, "Error oclwrred in DispTest.GetFlipLockPin (uPinset)");
       return JS_FALSE;
    }

    // Success.
    return JS_TRUE;
}

C_(DispTest_GetStereoPin)
{
    MASSERT(pContext     != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // Initialization.
    *pReturlwalue = JSVAL_NULL;

    // Get the stereo pin.
    JavaScriptPtr pJavaScript;
    RC            rc;
    UINT32        uStereoPin;

    rc = DispTest::GetStereoPin(&uStereoPin);
    if (rc != OK)
    {
        JS_ReportError(pContext, "Error oclwrred in DispTest.GetStereoPin (...)");
        return (JS_FALSE);
    }

    // Set the return value.
    if (OK != pJavaScript->ToJsval((UINT64) uStereoPin, pReturlwalue))
    {
       JS_ReportError(pContext, "Error oclwrred in DispTest.GetStereoPin (...)");
       return JS_FALSE;
    }

    // Success.
    return JS_TRUE;
}

C_(DispTest_GetExtSyncPin)
{
    MASSERT(pContext     != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // Initialization.
    *pReturlwalue = JSVAL_NULL;

    // Get the external sync pin.
    JavaScriptPtr pJavaScript;
    RC            rc;
    UINT32        uExtSyncPin;

    rc = DispTest::GetExtSyncPin(&uExtSyncPin);
    if (rc != OK)
    {
        JS_ReportError(pContext, "Error oclwrred in DispTest.GetExtSyncPin (...)");
        return (JS_FALSE);
    }

    // Set the return value.
    if (OK != pJavaScript->ToJsval((UINT64) uExtSyncPin, pReturlwalue))
    {
       JS_ReportError(pContext, "Error oclwrred in DispTest.GetExtSyncPin (...)");
       return JS_FALSE;
    }

    // Success.
    return JS_TRUE;
}

// SMethod
C_(DispTest_GetDsiForceBits)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   const char ErrorMessage[] = "Error oclwrred in DispTest.GetDsiForceBits( WhichHead, HeadArray[6] )";

   RC rc;

   UINT32 ChangeVPLL,  NoChangeVPLL,
          Blank, NoBlank,
          Shutdown, NoShutdown,
          NoBlankShutdown, NoBlankWakeup;

   UINT32 WhichHead;
   JSObject *HeadArgs;

   if
   (
       ((NumArguments != 2))
       || (OK != pJavaScript->FromJsval(pArguments[0], &WhichHead))
       || (OK != pJavaScript->FromJsval(pArguments[1], &HeadArgs))
   )
   {
      JS_ReportError(pContext, ErrorMessage);
      return JS_FALSE;
   }

   rc = DispTest::GetDsiForceBits(WhichHead,
                                  &ChangeVPLL,      &NoChangeVPLL,
                                  &Blank,           &NoBlank,
                                  &Shutdown,        &NoShutdown,
                                  &NoBlankShutdown, &NoBlankWakeup);

   UINT32 NumElements = 0;
   if (!JS_GetArrayLength(pContext, HeadArgs, reinterpret_cast<uint32*>(&NumElements)))
   {
      JS_ReportError(pContext, ErrorMessage);
      return JS_FALSE;
   }

   if (
       (OK != pJavaScript->SetElement(HeadArgs, 0, ChangeVPLL)) ||
       (OK != pJavaScript->SetElement(HeadArgs, 1, NoChangeVPLL)) ||
       (OK != pJavaScript->SetElement(HeadArgs, 2, Blank)) ||
       (OK != pJavaScript->SetElement(HeadArgs, 3, NoBlank)) ||
       (OK != pJavaScript->SetElement(HeadArgs, 4, Shutdown)) ||
       (OK != pJavaScript->SetElement(HeadArgs, 5, NoShutdown)) ||
       ((NumElements > 5) && (OK != pJavaScript->SetElement(HeadArgs, 6, NoBlankShutdown))) ||
       ((NumElements > 6) && (OK != pJavaScript->SetElement(HeadArgs, 7, NoBlankWakeup)))
       ) {

       JS_ReportError(pContext, ErrorMessage);
       return JS_FALSE;
   }

   return JS_TRUE;
}

// STest
C_(DispTest_SetDsiForceBits)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   const char ErrorMessage[] =
       "Error oclwrred in DispTest.SetDsiForceBits( ChangeVPLL,  NoChangeVPLL, "
       "Blank,  NoBlank, "
       "Shutdown,  NoShutdown, "
       "NoBlankShutdown, NoBlankWakeup )";

   UINT32 ChangeVPLL,  NoChangeVPLL,
       Blank, NoBlank,
       Shutdown, NoShutdown;
   UINT32 WhichHead;
   UINT32 NoBlankShutdown = 0;
   UINT32 NoBlankWakeup = 0;

   if
   ( ((NumArguments < 7) || (NumArguments > 9))
     || (OK != pJavaScript->FromJsval(pArguments[0], &WhichHead))
     || (OK != pJavaScript->FromJsval(pArguments[1], &ChangeVPLL))
     || (OK != pJavaScript->FromJsval(pArguments[2], &NoChangeVPLL))
     || (OK != pJavaScript->FromJsval(pArguments[3], &Blank))
     || (OK != pJavaScript->FromJsval(pArguments[4], &NoBlank))
     || (OK != pJavaScript->FromJsval(pArguments[5], &Shutdown))
     || (OK != pJavaScript->FromJsval(pArguments[6], &NoShutdown))
     || ((NumArguments > 7) && (OK != pJavaScript->FromJsval(pArguments[7], &NoBlankShutdown)))
     || ((NumArguments > 8) && (OK != pJavaScript->FromJsval(pArguments[8], &NoBlankWakeup)))
       )
   {
      JS_ReportError(pContext, ErrorMessage);
      return JS_FALSE;
   }

   RETURN_RC(DispTest::SetDsiForceBits(WhichHead,
                                       ChangeVPLL,      NoChangeVPLL,
                                       Blank,           NoBlank,
                                       Shutdown,        NoShutdown,
                                       NoBlankShutdown, NoBlankWakeup));
}

#define DISPTEST_PROP_READWRITE(funcname,resulttype,helpmsg)             \
   P_(DispTest_Get_##funcname);                                          \
   P_(DispTest_Set_##funcname);                                          \
   static SProperty DispTest_##funcname                                  \
   (                                                                     \
      DispTest_Object,                                                   \
      #funcname,                                                         \
      0,                                                                 \
      0,                                                                 \
      DispTest_Get_##funcname,                                           \
      DispTest_Set_##funcname,                                           \
      0,                                                                 \
      helpmsg                                                            \
   );                                                                    \
   P_(DispTest_Get_##funcname)                                           \
   {                                                                     \
      resulttype result = DispTest::Get##funcname();                     \
                                                                         \
      if (OK != JavaScriptPtr()->ToJsval(result, pValue))                \
      {                                                                  \
         JS_ReportError(pContext, "Failed to get DispTest." #funcname ); \
         *pValue = JSVAL_NULL;                                           \
         return JS_FALSE;                                                \
      }                                                                  \
      return JS_TRUE;                                                    \
   }                                                                     \
   P_(DispTest_Set_##funcname)                                           \
   {                                                                     \
      resulttype Value;                                                  \
                                                                         \
      if (OK != JavaScriptPtr()->FromJsval(*pValue, &Value))             \
      {                                                                  \
         JS_ReportError(pContext, "Failed to set DispTest." #funcname);  \
         return JS_FALSE;                                                \
      }                                                                  \
                                                                         \
      if( OK != DispTest::Set##funcname(Value) )                         \
      {                                                                  \
         JS_ReportWarning(pContext, "Error Setting " #funcname);         \
         *pValue = JSVAL_NULL;                                           \
         return JS_TRUE;                                                 \
      }                                                                  \
      return JS_TRUE;                                                    \
   }

DISPTEST_PROP_READWRITE( GlobalTimeout, FLOAT64, "GlobalTimeout" );
DISPTEST_PROP_READWRITE( PClkMode, UINT32, "PClkMode" );

#define DISPTEST_PROP_READWRITE_OBJ(funcname,resulttype,helpmsg)             \
   P_(DispTest_Get_##funcname);                                          \
   P_(DispTest_Set_##funcname);                                          \
   static SProperty DispTest_##funcname                                  \
   (                                                                \
      DispTest_Object,                                                    \
      #funcname,                                                    \
      0,                                                            \
      0,                                                            \
      DispTest_Get_##funcname,                                           \
      DispTest_Set_##funcname,                                           \
      0,                                                            \
      helpmsg                                                       \
   );                                                               \
   P_(DispTest_Get_##funcname)                                           \
   {                                                                \
      DISPTEST_SET_LWRRENT_DEVICE;                                  \
      resulttype result = (resulttype)0;                            \
      if (DispTest::LwrrentDevice())                                \
      {                                                             \
          result = DispTest::LwrrentDevice()->Get##funcname();      \
      }                                                             \
                                                                    \
      if (OK != JavaScriptPtr()->ToJsval(result, pValue))           \
      {                                                             \
         JS_ReportError(pContext, "Failed to get DispTest." #funcname ); \
         *pValue = JSVAL_NULL;                                      \
         return JS_FALSE;                                           \
      }                                                             \
      return JS_TRUE;                                               \
   }                                                                \
   P_(DispTest_Set_##funcname)                                           \
   {                                                                \
      resulttype Value;                                             \
      DISPTEST_SET_LWRRENT_DEVICE;                                  \
                                                                    \
      if (OK != JavaScriptPtr()->FromJsval(*pValue, &Value))        \
      {                                                             \
         JS_ReportError(pContext, "Failed to set DispTest." #funcname);  \
         return JS_FALSE;                                           \
      }                                                             \
                                                                    \
      if( OK != DispTest::LwrrentDevice()->Set##funcname(Value) )                    \
      {                                                             \
         JS_ReportWarning(pContext, "Error Setting " #funcname);    \
         *pValue = JSVAL_NULL;                                      \
         return JS_TRUE;                                            \
      }                                                             \
      return JS_TRUE;                                               \
   }

DISPTEST_PROP_READWRITE_OBJ( DeviceIndex,   UINT32, "DeviceIndex" );

#undef DISPTEST_PROP_READWRITE

C_(DispTest_TimeSlotsCtl);
static STest DispTest_TimeSlotsCtl
(
    DispTest_Object,
    "TimeSlotsCtl",
    C_DispTest_TimeSlotsCtl,
    1,
    "Disable/Enable SF/BFM timeslot update during add_stream"
);

C_(DispTest_ActivesymCtl);
static STest DispTest_ActivesymCtl
(
    DispTest_Object,
    "ActivesymCtl",
    C_DispTest_ActivesymCtl,
    1,
    "Set LW_PDISP_SF_DP_CONFIG_ACTIVESYM_CNTL_MODE to ENABLE/DISABLE"
);

C_(DispTest_UpdateTimeSlots);
static STest DispTest_UpdateTimeSlots
(
    DispTest_Object,
    "UpdateTimeSlots",
    C_DispTest_UpdateTimeSlots,
    1,
    "Update SF/BFM with their timeslots"
);

C_(DispTest_UpdateTimeSlots)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.UpdateTimeSlots(sornum, headnum)";

    JavaScriptPtr pJavaScript;

    UINT32 sornum;
    UINT32 headnum;

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &sornum          )) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &headnum )) ) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(DispTest::UpdateTimeSlots(sornum, headnum));
}

C_(DispTest_TimeSlotsCtl)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.TimeSlotsCtl(value)";

    JavaScriptPtr pJavaScript;

    bool value;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &value))) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(DispTest::TimeSlotsCtl(value));
}

C_(DispTest_ActivesymCtl)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ActivesymCtl(value)";

    JavaScriptPtr pJavaScript;

    bool value;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &value))) {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(DispTest::ActivesymCtl(value));
}

C_(DispTest_sorSetFlushMode);
static STest DispTest_sorSetFlushMode
(
    DispTest_Object,
    "sorSetFlushMode",
    C_DispTest_sorSetFlushMode,
    5,
    "Program SOR to enter to or exit from flush mode"
);

C_(DispTest_sorSetFlushMode)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    UINT32 sornum;
    bool   enable;
    bool   bImmediate = false;
    bool   bFireAndForget = false;
    bool   bForceRgDiv=false;
    bool   bUseBFM = false;

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.sorSetFlushMode(sornum,enable,bImmediate,bFireAndForget,bForceRgDiv,bUseBFM)";

    JavaScriptPtr pJavaScript;

    if
    ( ((NumArguments < 2) || (NumArguments > 6)) ||
      (OK != pJavaScript->FromJsval(pArguments[0], &sornum)) ||
      (OK != pJavaScript->FromJsval(pArguments[1], &enable)) ||
      ((NumArguments > 2) && (OK != pJavaScript->FromJsval(pArguments[2], &bImmediate))) ||
      ((NumArguments > 3) && (OK != pJavaScript->FromJsval(pArguments[3], &bFireAndForget))) ||
      ((NumArguments > 4) && (OK != pJavaScript->FromJsval(pArguments[4], &bForceRgDiv))) ||
      ((NumArguments > 5) && (OK != pJavaScript->FromJsval(pArguments[5], &bUseBFM)))
    )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(DispTest::sorSetFlushMode(sornum,enable,bImmediate,bFireAndForget,bForceRgDiv,bUseBFM));
}

// STest
C_(DispTest_SetSPPLLSpreadPercentage)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   const char ErrorMessage[] =
       "Error oclwrred in DispTest.SetSPPLLSpreadPercentage(spread_value)";

   UINT32 spread_value;

   if
   ( ((NumArguments != 1))
     || (OK != pJavaScript->FromJsval(pArguments[0], &spread_value))
       )
   {
      JS_ReportError(pContext, ErrorMessage);
      return JS_FALSE;
   }

   RETURN_RC(DispTest::SetSPPLLSpreadPercentage(spread_value));
}

// STest
C_(DispTest_SetMscg)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   const char ErrorMessage[] =
       "Error oclwrred in DispTest.SetMscg(spread_value)";

   UINT32 spread_value;

   if
   ( ((NumArguments != 1))
     || (OK != pJavaScript->FromJsval(pArguments[0], &spread_value))
       )
   {
      JS_ReportError(pContext, ErrorMessage);
      return JS_FALSE;
   }

   RETURN_RC(DispTest::SetMscg(spread_value));
}

// STest
C_(DispTest_SetForceDpTimeslot)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   const char ErrorMessage[] =
       "Error oclwrred in DispTest.SetForceDpTimeslot\
       (sornum_value, headnum_value, timeslot_value, update_alloc_pbn=false)";

   /*sor number, head number*/
   UINT32 sornum_value, headnum_value;
   /*timeslot value for multistream, valid range is [0,63]*/
   UINT32 timeslot_value;
   /*update the allocated bandwidth's PBN according to the new timeslot_value*/
   bool update_alloc_pbn = false;

   if
   ( ((NumArguments < 3 || NumArguments > 4))
     || (OK != pJavaScript->FromJsval(pArguments[0], &sornum_value))
     || (OK != pJavaScript->FromJsval(pArguments[1], &headnum_value))
     || (OK != pJavaScript->FromJsval(pArguments[2], &timeslot_value))
     || (NumArguments > 3 && (OK != pJavaScript->FromJsval(pArguments[3], &update_alloc_pbn)))
       )
   {
      JS_ReportError(pContext, ErrorMessage);
      return JS_FALSE;
   }

   RETURN_RC(DispTest::SetForceDpTimeslot(sornum_value, headnum_value, timeslot_value, update_alloc_pbn));
}

C_(DispTest_SetDClkMode);
static STest DispTest_SetDClkMode(
   DispTest_Object,
   "SetDClkMode",
   C_DispTest_SetDClkMode,
   2,
   "Set dclk mode for a head"
);

C_(DispTest_SetDClkMode)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 head;
    UINT32 DClkMode;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &head)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &DClkMode)))
    {
        JS_ReportError(pContext, "Usage: DispTest.SetDClkMode(head, DClkMode)");
        return JS_FALSE;
    }

    // call the function
    RC rc = DispTest::SetDClkMode(head, DClkMode);
    RETURN_RC(rc);
}

C_(DispTest_GetDClkMode);
static STest DispTest_GetDClkMode(
   DispTest_Object,
   "GetDClkMode",
   C_DispTest_GetDClkMode,
   2,
   "Get dclk mode for a head"
);

C_(DispTest_GetDClkMode)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    JavaScriptPtr pJavaScript;
    JSObject *RetArray;;

    // parse arguments
    UINT32 head;
    UINT32 DClkMode;     // [OUT]

    static char errTxt[]  = "Usage: DispTest.GetDClkMode(head,RetValues = Array(DClkMode) )";

    if ((NumArguments != 2) ||
       (OK != pJavaScript->FromJsval(pArguments[0], &head)) ||
       (OK != pJavaScript->FromJsval(pArguments[1], &RetArray)))
    {
       JS_ReportError(pContext, errTxt);
       return JS_FALSE;
    }

    RC rc =  DispTest::GetDClkMode(head, &DClkMode);

    if (OK != pJavaScript->SetElement(RetArray, 0, DClkMode))
    {
       JS_ReportError(pContext, errTxt);
       return JS_FALSE;
    }

    RETURN_RC(rc);
}

C_(DispTest_DisableAutoSetDisplayId)
{
   MASSERT(pContext     != 0);
   MASSERT(pReturlwalue != 0);

   RETURN_RC(DispTest::DisableAutoSetDisplayId());
}

C_(DispTest_LookupDisplayId)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    JavaScriptPtr pJavaScript;

    // parse arguments
    UINT32 ortype;
    UINT32 ornum;
    UINT32 protocol;
    UINT32 headnum;
    UINT32 streamnum = 0;
    UINT32 displayId;     // [OUT]

    static char errTxt[]  = "Usage: DispTest.LookupDisplayId(ortype, ornum, protocol, headnum, streamnum)";

    if ((NumArguments < 4) || (NumArguments > 5)
       || (OK != pJavaScript->FromJsval(pArguments[0], &ortype))
       || (OK != pJavaScript->FromJsval(pArguments[1], &ornum))
       || (OK != pJavaScript->FromJsval(pArguments[2], &protocol))
       || (OK != pJavaScript->FromJsval(pArguments[3], &headnum))
       || ((NumArguments > 4) && (OK != pJavaScript->FromJsval(pArguments[4], &streamnum))))
    {
       JS_ReportError(pContext, errTxt);
       return JS_FALSE;
    }

    displayId = DispTest::LookupDisplayId(ortype, ornum, protocol, headnum, streamnum);

    // Set the return value.
    if (OK != pJavaScript->ToJsval((UINT64) displayId, pReturlwalue))
    {
       JS_ReportError(pContext, "Error oclwrred in DispTest.LookupDisplayId (...)");
       return JS_FALSE;
    }

    return JS_TRUE;
}

C_(DispTest_DpCtrl)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    JavaScriptPtr pJavaScript;
    JSObject *retArray;;

    // parse arguments
    UINT32 sornum;
    UINT32 protocol;
    bool laneCount_Specified;
    UINT32 laneCount;           // [OUT] + [IN]
    bool laneCount_Error;       // [OUT]
    bool linkBw_Specified;
    UINT32 linkBw;              // [OUT] + [IN]
    bool linkBw_Error;          // [OUT]
    bool enhancedFraming_Specified;
    bool enhancedFraming;
    bool isMultiStream;
    bool disableLinkConfigCheck;
    bool fakeLinkTrain;
    bool linkTrain_Error;       // [OUT]
    bool ilwalidParam_Error;    // [OUT]
    UINT32 retryTimeMs;         // [OUT]

    static char errTxt[]  = "Usage: DispTest.DpCtrl( sornum, protocol, laneCount_Specified, laneCount, linkBw_Specified, linkBw, "
                            "enhancedFraming_Specified, enhancedFraming, isMultiStream, disableLinkConfigCheck, fakeLinkTrain, "
                            "RetValues = Array(laneCount, laneCount_Error, linkBw, "
                            "linkBw_Error, linkTrain_Error, ilwalidParam_Error, retryTimeMs) )";

    if (
      ((NumArguments != 13))
      || (OK != pJavaScript->FromJsval(pArguments[0], &sornum))
      || (OK != pJavaScript->FromJsval(pArguments[1], &protocol))
      || (OK != pJavaScript->FromJsval(pArguments[2], &laneCount_Specified))
      || (OK != pJavaScript->FromJsval(pArguments[3], &laneCount))
      || (OK != pJavaScript->FromJsval(pArguments[4], &linkBw_Specified))
      || (OK != pJavaScript->FromJsval(pArguments[5], &linkBw))
      || (OK != pJavaScript->FromJsval(pArguments[6], &enhancedFraming_Specified))
      || (OK != pJavaScript->FromJsval(pArguments[7], &enhancedFraming))
      || (OK != pJavaScript->FromJsval(pArguments[8], &isMultiStream))
      || (OK != pJavaScript->FromJsval(pArguments[9], &disableLinkConfigCheck))
      || (OK != pJavaScript->FromJsval(pArguments[10], &fakeLinkTrain))
      || (OK != pJavaScript->FromJsval(pArguments[11], &retArray))
       )
    {
       JS_ReportError(pContext, errTxt);
       return JS_FALSE;
    }

    RC rc = DispTest::DpCtrl(sornum, protocol, laneCount_Specified, &laneCount, &laneCount_Error,
                    linkBw_Specified, &linkBw, &linkBw_Error, enhancedFraming_Specified, enhancedFraming,
                    isMultiStream, disableLinkConfigCheck, fakeLinkTrain, &linkTrain_Error, &ilwalidParam_Error, &retryTimeMs);

    // output values
    if ((OK != pJavaScript->SetElement(retArray, 0, laneCount)) ||
        (OK != pJavaScript->SetElement(retArray, 1, linkBw)) ||
        (OK != pJavaScript->SetElement(retArray, 2, retryTimeMs)) ||
        (OK != pJavaScript->SetElement(retArray, 3, laneCount_Error)) ||
        (OK != pJavaScript->SetElement(retArray, 4, linkBw_Error)) ||
        (OK != pJavaScript->SetElement(retArray, 5, linkTrain_Error)) ||
        (OK != pJavaScript->SetElement(retArray, 6, ilwalidParam_Error)) )
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(rc);
}

C_(DispTest_ControlSetSfDualMst);
static STest DispTest_ControlSetSfDualMst
(
    DispTest_Object,
    "ControlSetSfDualMst",
    C_DispTest_ControlSetSfDualMst,
    3,
    "Setup the single-head MST mode"
);

C_(DispTest_ControlSetSfDualMst)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetSfDualMst(sorNum, headNum, enableDualMst, isDPB)";

    JavaScriptPtr pJavaScript;

    UINT32 sorNum;
    UINT32 headNum;
    UINT32 enableDualMst;
    UINT32 isDPB = 0;

    if ((NumArguments < 3) || (NumArguments > 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &sorNum)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &headNum)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &enableDualMst)) ||
        ((NumArguments > 3) && (OK != pJavaScript->FromJsval(pArguments[3], &isDPB))))
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(DispTest::ControlSetSfDualMst(sorNum, headNum, enableDualMst, isDPB));
}

C_(DispTest_ControlSetSorXBar);
static STest DispTest_ControlSetSorXBar
(
    DispTest_Object,
    "ControlSetSorXBar",
    C_DispTest_ControlSetSorXBar,
    4,
    "Setup the SOR XBAR to connect SOR & analog link"
);

C_(DispTest_ControlSetSorXBar)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;

    static char errTxt[]  = "Usage: DispTest.ControlSetSorXBar(sorNum, protocolNum, linkPrimary, linkSecondary)";

    JavaScriptPtr pJavaScript;

    UINT32 sorNum;
    UINT32 protocolNum;
    UINT32 linkPrimary;
    UINT32 linkSecondary;

    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &sorNum)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &protocolNum)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &linkPrimary)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &linkSecondary)))
    {
        JS_ReportError(pContext, errTxt);
        return JS_FALSE;
    }

    RETURN_RC(DispTest::ControlSetSorXBar(sorNum, protocolNum, linkPrimary, linkSecondary));
}

C_(DispTest_SetHDMISinkCaps)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    DISPTEST_SET_LWRRENT_DEVICE;
    JavaScriptPtr pJs;
    UINT32  hdmiCaps;
    UINT32  displayId = 0;

    if( NumArguments != 2 ||
       (OK != pJs->FromJsval(pArguments[0], &displayId)) ||
       (OK != pJs->FromJsval(pArguments[1], &hdmiCaps)) )
    {
        JS_ReportError(pContext, "Usage: DispTest.SetHDMISinkCaps(displayId, hdmiCaps)");
        return JS_FALSE;
    }

    RETURN_RC(DispTest::SetHDMISinkCaps(displayId, hdmiCaps));
}

//
//      Copyright (c) 1999-2016 by LWPU Corporation.
//      All rights reserved.
//
