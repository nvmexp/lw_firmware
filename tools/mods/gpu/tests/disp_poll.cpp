/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2009 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
//      disp_poll.cpp - DispTest class - Event and polling related methods
//      Copyright (c) 1999-2005 by LWPU Corporation.
//      All rights reserved.
//
//      Originally part of "disp_test.cpp" - broken out 26 October 2005
//
//      Written by: Matt Craighead, Larry Coffey, et al
//      Date:       29 July 2004
//
//      Routines in this module:
//      PollRmaRegValueCheck            Static function needed by polling routine "PollRmaRegValue" to read register once and check against masked value
//      PollRmaRegValue                 Poll on register address thru rma access until it is equal to Value or TimeoutMs is reached
//      PollIORegValueCheck             Static function needed by polling routine "PollIORegValue" to read register once and check against masked value
//      PollIORegValue                  Resolve the Ctx DMA (Get Base and Size) and then poll on address Base+Offset (Check if Offset < Size) until it equals Value or TimeoutMs is reached
//      PollRegNotValueCheck            Static function needed by polling routine "PollRegNotValue" to read register once and check against masked value
//      PollRegNotValue                 Poll on register address  until it is not equal to Value or TimeoutMs is reached
//      PollRegValueCheck               Static function needed by polling routine "PollRegValue" to read register once and check against masked value
//      PollRegValue                    Resolve the Ctx DMA (Get Base and Size) and then poll on address Base+Offset (Check if Offset < Size) until it equals Value or TimeoutMs is reached
//      PollRegLessValueCheck           Static function needed by polling routine "PollRegLessValue" to read register once and check against masked value
//      PollRegLessValue                Resolve the Ctx DMA (Get Base and Size) and then poll on address Base+Offset (Check if Offset < Size) until it is less Value or TimeoutMs is reached
//      PollRegGreaterValueCheck        Static function needed by polling routine "PollRegGreaterValue" to read register once and check against masked value
//      PollRegGreaterValue             Resolve the Ctx DMA (Get Base and Size) and then Poll on address Base+Offset (Check if Offset < Size) until it greater Value or TimeoutMs is reached
//      PollRegLessEqualValueCheck      Static function needed by polling routine "PollRegLessEqualValue" to read register once and check against masked value
//      PollRegLessEqualValue           Resolve the Ctx DMA (Get Base and Size) and then poll on address Base+Offset (Check if Offset < Size) until it is less/equal Value or TimeoutMs is reached
//      PollRegGreaterEqualValueCheck   Static function needed by polling routine "PollRegGreaterEqualValue" to read register once and check against masked value
//      PollRegGreaterEqualValue        Resolve the Ctx DMA (Get Base and Size) and then poll on address Base+Offset (Check if Offset < Size) until it greater/equal Value or TimeoutMs is reached
//      PollHWValueCheck                Static function needed by polling routine "PollHWValue" to read register once and check against masked value
//      PollHWValue                     Resolve the Ctx DMA (Get Base and Size) and then poll on the HW signal (Check if Offset < Size) until it equals Value or TimeoutMs is reached
//      PollHWGreaterEqualValueCheck    Static function needed by polling routine "PollHWGreaterEqualValue" to read register once and check against masked value
//      PollHWGreaterEqualValue         Resolve the Ctx DMA (Get Base and Size) and then Poll on HW Signal (Check if Offset < Size) until it equals Value or TimeoutMs is reached
//      PollHWLessEqualValueCheck       Static function needed by polling routine "PollHWLessEqualValue" to read register once and check against masked value
//      PollHWLessEqualValue            Resolve the Ctx DMA (Get Base and Size) and then poll on HW Signal (Check if Offset < Size) until it equals Value or TimeoutMs is reached
//      PollHWNotValueCheck             Static function needed by polling routine "PollHWNotValue" to read register once and check against masked value
//      PollHWNotValue                  Resolve the Ctx DMA (Get Base and Size) and then poll on address Base+Offset (Check if Offset < Size) until it equals Value or TimeoutMs is reached
//      PollValueCheck                  Static function needed by polling routine "PollValue" to read register once and check against masked value
//      PollValue                       Resolve the Ctx DMA (Get Base and Size) and then poll on address Base+Offset (Check if Offset < Size) until it equals Value or TimeoutMs is reached
//      PollGreaterEqualValueCheck      Static function needed by polling routine "PollGreaterEqualValue" to read register once and check against masked value
//      PollGreaterEqualValue           Resolve the Ctx DMA (Get Base and Size) and then poll on address Base+Offset (Check if Offset < Size) until it greater/equals Value or TimeoutMs is reached
//      PollValueAtAddrCheck            Static function needed by polling routine "PollValueAtAddr" to read register once and check against masked value
//      PollValueAtAddr                 Resolve the Ctx DMA (Get Base and Size) and then poll on address Base+Offset (Check if Offset < Size) until it equals Value or TimeoutMs is reached
//      PollDoneCheck                   Static function needed by polling routine "PollDone" to read register once and check against masked value
//      PollDone                        Poll on Completion Field in the Notifier and exit on completion or Timeout
//      Poll2RegsGreaterValueCheck      Static function needed by polling routine "Poll2RegsGreaterValue" to read two registers once and check against masked values
//      Poll2RegsGreaterValue           Similar to PollRegGreaterValue except this polls on two registers at once and uses two values and two masks
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

#ifdef USE_DTB_DISPLAY_CLASSES
#include "gpu/js/dispt_sr.h"
#else
#include "gpu/js/dispt_sr.h"
#endif

#include "core/utility/errloggr.h"

#define DISPTEST_PRIVATE
#include "core/include/disp_test.h"
#include "gpu/include/vgacrc.h"

namespace DispTest
{
    // Local polling routines (event handlers)
    static bool PollRmaRegValueCheck(void *pArgs);
    static bool PollIORegValueCheck(void *pArgs);
    static bool PollRegNotValueCheck(void *pArgs);
    static bool PollRegValueCheck(void *pArgs);
    static bool PollRegLessValueCheck(void *pArgs);
    static bool PollRegGreaterValueCheck(void *pArgs);
    static bool Poll2RegsGreaterValueCheck(void *pArgs);
    static bool PollRegLessEqualValueCheck(void *pArgs);
    static bool PollRegGreaterEqualValueCheck(void *pArgs);
    static bool PollHWValueCheck(void *pArgs);
    static bool PollHWGreaterEqualValueCheck(void *pArgs);
    static bool PollHWLessEqualValueCheck(void *pArgs);
    static bool PollHWNotValueCheck(void *pArgs);
    static bool PollValueCheck(void *pArgs);
    static bool PollGreaterEqualValueCheck(void *pArgs);
    static bool PollValueAtAddrCheck(void *pArgs);
    static bool PollScanlineGreaterEqualValueCheck(void *pArgs);
    static bool PollScanlineLessValueCheck(void *pArgs);
    static bool PollDoneCheck(void *pArgs);
    static bool PollRGScanUnlockedCheck(void *pArgs);
    static bool PollRGFlipUnlockedCheck(void *pArgs);
    static bool PollRGScanLockedCheck(void *pArgs);
    static bool PollRGFlipLockedCheck(void *pArgs);
    static bool PollRegValueNoUpdateCheck(void *pArgs);

    // DispTest member data
    extern  int m_nLwrrentDevice;
    extern  int m_nNumDevice;
}

/****************************************************************************************
 * DispTest::PollRmaRegValueCheck
 *
 *  Arguments Description:
 *  - pArgs: pointer to list of arguments:
 *    - Address: Address for the GPU register to Poll
 *    - Value: the exit value, the default is 'true'
 *    - Mask: the Mask to mark the field of interest within the Register
 *
 *  Functional Description:
 *  - Static function needed by polling routine "PollRmaRegValue" to read register once and check against masked value
 ***************************************************************************************/
static bool DispTest::PollRmaRegValueCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    UINT32 Address = *(ppArgs[0]);
    UINT32 Value   = *(ppArgs[1]);
    UINT32 Mask    = *(ppArgs[2]);

    // Read Value
    UINT32 ReadValue;
    DispTest::RmaRegRd32(Address, &ReadValue);

    // update CRCs
    DispTest::Crlwpdate();
    return ((Value & Mask) == ( ReadValue & Mask));
}

/****************************************************************************************
 * DispTest::PollRmaRegValue
 *
 *  Arguments Description:
 *  - Address: Address for the GPU register to Poll
 *  - Value: the exit value, the default is 'true'
 *  - Mask: the Mask to mark the field of interest within the Register
 *  - TimeoutMs: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Poll on register address thru rma access until it is equal to
 *    Value or TimeoutMs is reached.
 ***************************************************************************************/
RC DispTest::PollRmaRegValue
(
    UINT32 Address,
    UINT32 Value,
    UINT32 Mask,
    FLOAT64 TimeoutMs
)
{
    const UINT32 *pArgs[3] = {&Address, &Value, &Mask};
    Tasker::PollFunc pPollRegValueCheck = &DispTest::PollRmaRegValueCheck;
    return POLLWRAP_HW(pPollRegValueCheck, pArgs, TimeoutMs);
}

/****************************************************************************************
 * DispTest::PollIORegValueCheck
 *
 *  Arguments Description:
 *  - pArgs: pointer to list of arguments:
 *    - Address: Address for the GPU register to Poll
 *    - Value: the exit value, the default is 'true'
 *    - Mask: the Mask to mark the field of interest within the Register
 *
 *  Functional Description:
 *  - Static function needed by polling routine "PollIORegValue" to read register once and check against masked value
 ***************************************************************************************/
static bool DispTest::PollIORegValueCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT16 **ppArgs = (UINT16**)(pArgs);
    UINT16 Address = *(ppArgs[0]);
    UINT08 Value   = *(UINT08*)(ppArgs[1]);
    UINT08 Mask    = *(UINT08*)(ppArgs[2]);

    // check for value at memory location
    UINT08 Rtlwalue;
    Platform::PioRead08(Address,&Rtlwalue);

    // update CRCs
    DispTest::Crlwpdate();
    return (Value == (Rtlwalue & Mask));
}

/****************************************************************************************
 * DispTest::PollIORegValue
 *
 *  Arguments Description:
 *  - Address: Address for the GPU register to Poll
 *  - Value: the exit value, the default is 'true'
 *  - Mask: the Mask to mark the field of interest within the Register
 *  - TimeoutMs: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Poll on address Base+Offset (Check if Offset < Size) until it equals
 *    Value or TimeoutMs is reached.
 ***************************************************************************************/
RC DispTest::PollIORegValue
(
    UINT16 Address,
    UINT16 Value,
    UINT16 Mask,
    FLOAT64 TimeoutMs
)
{
    UINT16 *pArgs[3] = {&Address, &Value, &Mask};
    Tasker::PollFunc pPollIORegValueCheck = &DispTest::PollIORegValueCheck;
    return POLLWRAP_HW(pPollIORegValueCheck, pArgs, TimeoutMs);
}

/****************************************************************************************
 * DispTest::PollRegNotValueCheck
 *
 *  Arguments Description:
 *  - pArgs: pointer to list of arguments:
 *    - Address: Address for the GPU register to Poll
 *    - Value: the exit value, the default is 'true'
 *    - Mask: the Mask to mark the field of interest within the Register
 *
 *  Functional Description:
 *  - Static function needed by polling routine "PollRegNotValue" to read register once and check against masked value
 ***************************************************************************************/
static bool DispTest::PollRegNotValueCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    UINT32 Address = *(ppArgs[0]);
    UINT32 Value   = *(ppArgs[1]);
    UINT32 Mask    = *(ppArgs[2]);

    // Read Value
    UINT32 ReadValue = DispTest::GetBoundGpuSubdevice()->RegRd32(Address);

    // update CRCs
    DispTest::Crlwpdate();
    // check for value at memory location
    return (Value != ( ReadValue & Mask));
}

/****************************************************************************************
 * DispTest::PollRegNotValue
 *
 *  Arguments Description:
 *  - Address: Address for the GPU register to Poll
 *  - Value: the exit value, the default is 'true'
 *  - Mask: the Mask to mark the field of interest within the Register
 *  - TimeoutMs: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Poll on register address  until it is not equal to
 *    Value or TimeoutMs is reached.
 ***************************************************************************************/
RC DispTest::PollRegNotValue
(
    UINT32 Address,
    UINT32 Value,
    UINT32 Mask,
    FLOAT64 TimeoutMs
)
{
    const UINT32 *pArgs[3] = {&Address, &Value, &Mask};
    Tasker::PollFunc pPollRegNotValueCheck = &DispTest::PollRegNotValueCheck;
    return POLLWRAP_HW(pPollRegNotValueCheck, pArgs, TimeoutMs);
}

/****************************************************************************************
 * DispTest::PollRegValueCheck
 *
 *  Arguments Description:
 *  - pArgs: pointer to list of arguments:
 *  - Address: Address for the GPU register to Poll
 *  - Value: the exit value, the default is 'true'
 *  - Mask: the Mask to mark the field of interest within the Register
 *
 *  Functional Description:
 *  - Static function needed by polling routine "PollRegValue" to read register once and check against masked value
 ***************************************************************************************/
static bool DispTest::PollRegValueCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    UINT32 Address = *(ppArgs[0]);
    UINT32 Value   = *(ppArgs[1]);
    UINT32 Mask    = *(ppArgs[2]);

    // Read Value
    UINT32 ReadValue = DispTest::GetBoundGpuSubdevice()->RegRd32(Address);

    // update CRCs
    DispTest::Crlwpdate();

    // check for value at memory location
    return ((Value & Mask) == (ReadValue & Mask));
}

/****************************************************************************************
 * DispTest::PollRegValue
 *
 *  Arguments Description:
 *  - Address: Address for the GPU register to Poll
 *  - Value: the exit value, the default is 'true'
 *  - Mask: the Mask to mark the field of interest within the Register
 *  - TimeoutMs: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Poll on address Base+Offset (Check if Offset < Size) until it equals
 *    Value or TimeoutMs is reached.
 ***************************************************************************************/
RC DispTest::PollRegValue
(
    UINT32 Address,
    UINT32 Value,
    UINT32 Mask,
    FLOAT64 TimeoutMs
)
{
    const UINT32 *pArgs[3] = {&Address, &Value, &Mask};
    Tasker::PollFunc pPollRegValueCheck = &DispTest::PollRegValueCheck;
    return POLLWRAP_HW(pPollRegValueCheck, pArgs, TimeoutMs);
}

/****************************************************************************************
 * DispTest::PollRegLessValueCheck
 *
 *  Arguments Description:
 *  - pArgs: Pointer to list of arguments:
 *    - Address: Address for the GPU register to Poll
 *    - Value: the exit value, the default is 'true'
 *    - Mask: the Mask to mark the field of interest within the Register
 *
 *  Functional Description:
 *  - Static function needed by polling routine "PollRegLessValue" to read register once and check against masked value
 ***************************************************************************************/
static bool DispTest::PollRegLessValueCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    UINT32 Address = *(ppArgs[0]);
    UINT32 Value   = *(ppArgs[1]);
    UINT32 Mask    = *(ppArgs[2]);

    // Read Value
    UINT32 ReadValue = DispTest::GetBoundGpuSubdevice()->RegRd32(Address);

    // update CRCs
    DispTest::Crlwpdate();

    // check for value at memory location
    return ((ReadValue & Mask) < (Value & Mask));
}

/****************************************************************************************
 * DispTest::PollRegLessValue
 *
 *  Arguments Description:
 *  - Address: Address for the GPU register to Poll
 *  - Value: the exit value, the default is 'true'
 *  - Mask: the Mask to mark the field of interest within the Register
 *  - TimeoutMs: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Poll on address Base+Offset (Check if Offset < Size) until it is less
 *    Value or TimeoutMs is reached.
 ***************************************************************************************/
RC DispTest::PollRegLessValue
(
    UINT32 Address,
    UINT32 Value,
    UINT32 Mask,
    FLOAT64 TimeoutMs
)
{
    const UINT32 *pArgs[3] = {&Address, &Value, &Mask};
    Tasker::PollFunc pPollRegLessValueCheck = &DispTest::PollRegLessValueCheck;
    return POLLWRAP_HW(pPollRegLessValueCheck, pArgs, TimeoutMs);
}

/****************************************************************************************
 * DispTest::PollRegGreaterValueCheck
 *
 *  Arguments Description:
 *  - pArgs: Pointer to list of arguments:
 *    - Address: Address for the GPU register to Poll
 *    - Value: the exit value, the default is 'true'
 *    - Mask: the Mask to mark the field of interest within the Register
 *
 *  Functional Description:
 *  - Static function needed by polling routine "PollRegGreaterValue" to read register once and check against masked value
 ***************************************************************************************/
static bool DispTest::PollRegGreaterValueCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    UINT32 Address = *(ppArgs[0]);
    UINT32 Value   = *(ppArgs[1]);
    UINT32 Mask    = *(ppArgs[2]);

    // Read Value
    UINT32 ReadValue = DispTest::GetBoundGpuSubdevice()->RegRd32(Address);

    // update CRCs
    DispTest::Crlwpdate();

    // check for value at memory location
    return ((ReadValue & Mask) > (Value & Mask));
}

/****************************************************************************************
 * DispTest::PollRegGreaterValue
 *
 *  Arguments Description:
 *  - Address: Address for the GPU register to Poll
 *  - Value: the exit value, the default is 'true'
 *  - Mask: the Mask to mark the field of interest within the Register
 *  - TimeoutMs: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Poll on address Base+Offset (Check if Offset < Size) until it greater
 *    Value or TimeoutMs is reached.
 ***************************************************************************************/
RC DispTest::PollRegGreaterValue
(
    UINT32 Address,
    UINT32 Value,
    UINT32 Mask,
    FLOAT64 TimeoutMs
)
{
    const UINT32 *pArgs[3] = {&Address, &Value, &Mask};
    Tasker::PollFunc pPollRegGreaterValueCheck = &DispTest::PollRegGreaterValueCheck;
    return POLLWRAP_HW(pPollRegGreaterValueCheck, pArgs, TimeoutMs);
}

/****************************************************************************************
 * DispTest::Poll2RegsGreaterValueCheck
 *
 *  Arguments Description:
 *  - pArgs: Pointer to list of arguments:
 *    - Address: Address for the first GPU register to Poll
 *    - Value: the first exit value, the default is 'true'
 *    - Mask: the first Mask to mark the field of interest within the Register
 *    - Address2: Address for the second GPU register to Poll
 *    - Value2: the second exit value, the default is 'true'
 *    - Mask2: the Mask to mark the field of interest within the second Register
 *
 *  Functional Description:
 *  - Static function needed by polling routine "Poll2RegsGreaterValue" to read two registers once and check against masked values
 ***************************************************************************************/
static bool DispTest::Poll2RegsGreaterValueCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    UINT32 Address = *(ppArgs[0]);
    UINT32 Value   = *(ppArgs[1]);
    UINT32 Mask    = *(ppArgs[2]);
    UINT32 Address2 = *(ppArgs[3]);
    UINT32 Value2   = *(ppArgs[4]);
    UINT32 Mask2    = *(ppArgs[5]);

    // Read Value
    UINT32 ReadValue = DispTest::GetBoundGpuSubdevice()->RegRd32(Address);
    UINT32 ReadValue2 = DispTest::GetBoundGpuSubdevice()->RegRd32(Address2);

    // update CRCs
    DispTest::Crlwpdate();

    // check for value at memory location
    return ( ((ReadValue & Mask) > (Value & Mask)) || ((ReadValue2 & Mask2) > (Value2 & Mask2)));
}

/****************************************************************************************
 * DispTest::Poll2RegsGreaterValue
 *
 *  Arguments Description:
 *  - Address: Address for the GPU register to Poll
 *  - Value: the exit value, the default is 'true'
 *  - Mask: the Mask to mark the field of interest within the Register
 *  - Address2: Address for the second GPU register to Poll
 *  - Value2: the second exit value, the default is 'true'
 *  - Mask2: the Mask to mark the field of interest within the second Register
 *  - TimeoutMs: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Poll on both registers until both are greater than thier respective values
 *    or TimeoutMs is reached.
 ***************************************************************************************/
RC DispTest::Poll2RegsGreaterValue(UINT32 Address, UINT32 Value, UINT32 Mask,
                                   UINT32 Address2, UINT32 Value2, UINT32 Mask2,
                                   FLOAT64 TimeoutMs)
{
    const UINT32 *pArgs[6] = {&Address, &Value, &Mask, &Address2, &Value2, &Mask2};
    Tasker::PollFunc pPoll2RegsGreaterValueCheck = &DispTest::Poll2RegsGreaterValueCheck;
    return POLLWRAP_HW(pPoll2RegsGreaterValueCheck, pArgs, TimeoutMs);
}

/****************************************************************************************
 * DispTest::PollRegLessEqualValueCheck
 *
 *  Arguments Description:
 *  - pArgs: Pointer to list of arguments:
 *    - Address: Address for the GPU register to Poll
 *    - Value: the exit value, the default is 'true'
 *    - Mask: the Mask to mark the field of interest within the Register
 *
 *  Functional Description:
 *  - Static function needed by polling routine "PollRegLessEqualValue" to read register once and check against masked value
 ***************************************************************************************/
static bool DispTest::PollRegLessEqualValueCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    UINT32 Address = *(ppArgs[0]);
    UINT32 Value   = *(ppArgs[1]);
    UINT32 Mask    = *(ppArgs[2]);

    // Read Value
    UINT32 ReadValue = DispTest::GetBoundGpuSubdevice()->RegRd32(Address);

    // update CRCs
    DispTest::Crlwpdate();

    // check for value at memory location
    return ((ReadValue & Mask) <= (Value & Mask));
}

/****************************************************************************************
 * DispTest::PollRegLessEqualValue
 *
 *  Arguments Description:
 *  - Address: Address for the GPU register to Poll
 *  - Value: the exit value, the default is 'true'
 *  - Mask: the Mask to mark the field of interest within the Register
 *  - TimeoutMs: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Poll on address Base+Offset (Check if Offset < Size) until it is less/equal
 *    Value or TimeoutMs is reached.
 ***************************************************************************************/
RC DispTest::PollRegLessEqualValue
(
    UINT32 Address,
    UINT32 Value,
    UINT32 Mask,
    FLOAT64 TimeoutMs
)
{
    const UINT32 *pArgs[3] = {&Address, &Value, &Mask};
    Tasker::PollFunc pPollRegLessEqualValueCheck = &DispTest::PollRegLessEqualValueCheck;
    return POLLWRAP_HW(pPollRegLessEqualValueCheck, pArgs, TimeoutMs);
}

/****************************************************************************************
 * DispTest::PollRegGreaterEqualValueCheck
 *
 *  Arguments Description:
 *  - pArgs: Pointer to list of arguments:
 *    - Address: Address for the GPU register to Poll
 *    - Value: the exit value, the default is 'true'
 *    - Mask: the Mask to mark the field of interest within the Register
 *
 *  Functional Description:
 *  - Static function needed by polling routine "PollRegGreaterEqualValue" to read register once and check against masked value
 ***************************************************************************************/
static bool DispTest::PollRegGreaterEqualValueCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    UINT32 Address = *(ppArgs[0]);
    UINT32 Value   = *(ppArgs[1]);
    UINT32 Mask    = *(ppArgs[2]);

    // Read Value
    UINT32 ReadValue = DispTest::GetBoundGpuSubdevice()->RegRd32(Address);

    // update CRCs
    DispTest::Crlwpdate();

    // check for value at memory location
    return ((ReadValue & Mask) >= (Value & Mask));
}

/****************************************************************************************
 * DispTest::PollRegGreaterEqualValue
 *
 *  Arguments Description:
 *  - Address: Address for the GPU register to Poll
 *  - Value: the exit value, the default is 'true'
 *  - Mask: the Mask to mark the field of interest within the Register
 *  - TimeoutMs: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Poll on address Base+Offset (Check if Offset < Size) until it greater/equal
 *    Value or TimeoutMs is reached.
 ***************************************************************************************/
RC DispTest::PollRegGreaterEqualValue
(
    UINT32 Address,
    UINT32 Value,
    UINT32 Mask,
    FLOAT64 TimeoutMs
)
{
    const UINT32 *pArgs[3] = {&Address, &Value, &Mask};
    Tasker::PollFunc pPollRegGreaterEqualValueCheck = &DispTest::PollRegGreaterEqualValueCheck;
    return POLLWRAP_HW(pPollRegGreaterEqualValueCheck, pArgs, TimeoutMs);
}

/****************************************************************************************
 * DispTest::PollHWValueCheck
 *
 *  Arguments Description:
 *  - pArgs: Pointer to list of arguments:
 *    - name:
 *    - index:
 *    - Value: the exit value, the default is 'true'
 *    - Mask: the Mask to mark the field of interest within the Register
 *    - Size:
 *
 *  Functional Description:
 *  - Static function needed by polling routine "PollHWValue" to read register once and check against masked value
 ***************************************************************************************/
static bool DispTest::PollHWValueCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    const char * name = (const char *)ppArgs[0];
    UINT32 index = *(ppArgs[1]);
    UINT32 Value  = *(ppArgs[2]);
    UINT32 Mask   = *(ppArgs[3]);
    UINT32 Size   = *(ppArgs[4]);

    // check for value at memory location
    UINT32 Rtlwalue;
    Platform::EscapeRead(name, index, Size, &Rtlwalue);

    return ((Value & Mask) == (Rtlwalue & Mask));
}

/****************************************************************************************
 * DispTest::PollHWValue
 *
 *  Arguments Description:
 *  - name:
 *  - index:
 *  - Value: the exit value, the default is 'true'
 *  - Mask: the Mask to mark the field of interest within the Register
 *  - Size:
 *  - TimeoutMs: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Poll on the HW signal (Check if Offset < Size) until it equals
 *    Value or TimeoutMs is reached.
 ***************************************************************************************/
RC DispTest::PollHWValue
(
    const char * name,
    UINT32 index,
    UINT32 Value,
    UINT32 Mask,
    UINT32 Size,
    FLOAT64 TimeoutMs
)
{
    const UINT32 *pArgs[5] = {(const UINT32*)name, &index, &Value, &Mask, &Size};
    Tasker::PollFunc pPollHWValueCheck = &DispTest::PollHWValueCheck;
    return POLLWRAP_HW(pPollHWValueCheck, pArgs, TimeoutMs);
}

/****************************************************************************************
 * DispTest::PollHWGreaterEqualValueCheck
 *
 *  Arguments Description:
 *  - pArgs: Pointer to list of arguments:
 *    - name:
 *    - index:
 *    - Value: the exit value, the default is 'true'
 *    - Mask: the Mask to mark the field of interest within the Register
 *    - Size:
 *
 *  Functional Description:
 *  - Static function needed by polling routine "PollHWGreaterEqualValue" to read register once and check against masked value
 ***************************************************************************************/
static bool DispTest::PollHWGreaterEqualValueCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    const char * name = (const char *)ppArgs[0];
    UINT32 index = *(ppArgs[1]);
    UINT32 Value  = *(ppArgs[2]);
    UINT32 Mask   = *(ppArgs[3]);
    UINT32 Size   = *(ppArgs[4]);

    // check for value at memory location
    UINT32 Rtlwalue;
    Platform::EscapeRead(name,index,Size, &Rtlwalue);

    return ((Rtlwalue & Mask) >= (Value & Mask));
}

/****************************************************************************************
 * DispTest::PollHWGreaterEqualValue
 *
 *  Arguments Description:
 *  - name:
 *  - index:
 *  - Value: the exit value, the default is 'true'
 *  - Mask: the Mask to mark the field of interest within the Register
 *  - Size:
 *  - TimeoutMs: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Resolve the Ctx DMA (Get Base and Size)
 *  - Poll on HW Signal (Check if Offset < Size) until it equals
 *    Value or TimeoutMs is reached.
 ***************************************************************************************/
RC DispTest::PollHWGreaterEqualValue
(
    const char * name,
    UINT32 index,
    UINT32 Value,
    UINT32 Mask,
    UINT32 Size,
    FLOAT64 TimeoutMs
)
{
    const UINT32 *pArgs[5] = {(const UINT32*)name, &index, &Value, &Mask, &Size};
    Tasker::PollFunc pPollHWValueCheck = &DispTest::PollHWGreaterEqualValueCheck;
    return POLLWRAP_HW(pPollHWValueCheck, pArgs, TimeoutMs);
}

/****************************************************************************************
 * DispTest::PollHWLessEqualValueCheck
 *
 *  Arguments Description:
 *  - pArgs: Pointer to list of arguments
 *    - name:
 *    - index:
 *    - Value: the exit value, the default is 'true'
 *    - Mask: the Mask to mark the field of interest within the Register
 *    - Size:
 *
 *  Functional Description:
 *  - Static function needed by polling routine "PollHWLessEqualValue" to read register once and check against masked value
 ***************************************************************************************/
static bool DispTest::PollHWLessEqualValueCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    const char * name = (const char *)ppArgs[0];
    UINT32 index = *(ppArgs[1]);
    UINT32 Value  = *(ppArgs[2]);
    UINT32 Mask   = *(ppArgs[3]);
    UINT32 Size   = *(ppArgs[4]);

    // check for value at memory location
    UINT32 Rtlwalue;
    Platform::EscapeRead(name,index,Size, &Rtlwalue);

    return ((Rtlwalue & Mask) <= (Value & Mask));
}

/****************************************************************************************
 * DispTest::PollHWLessEqualValue
 *
 *  Arguments Description:
 *  - name:
 *  - index:
 *  - Value: the exit value, the default is 'true'
 *  - Mask: the Mask to mark the field of interest within the Register
 *  - Size:
 *  - TimeoutMs: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Poll on HW Signal (Check if Offset < Size) until it equals
 *    Value or TimeoutMs is reached.
 ***************************************************************************************/
RC DispTest::PollHWLessEqualValue
(
    const char * name,
    UINT32 index,
    UINT32 Value,
    UINT32 Mask,
    UINT32 Size,
    FLOAT64 TimeoutMs
)
{
    const UINT32 *pArgs[5] = {(const UINT32*)name, &index, &Value, &Mask, &Size};
    Tasker::PollFunc pPollHWValueCheck = &DispTest::PollHWLessEqualValueCheck;
    return POLLWRAP_HW(pPollHWValueCheck, pArgs, TimeoutMs);
}

/****************************************************************************************
 * DispTest::PollHWNotValueCheck
 *
 *  Arguments Description:
 *  - pArgs: Pointer to list of arguments:
 *    - name:
 *    - index:
 *    - Value: the exit value, the default is 'true'
 *    - Mask: the Mask to mark the field of interest within the Register
 *    - Size:
 *
 *  Functional Description:
 *  - Static function needed by polling routine "PollHWNotValue" to read register once and check against masked value
 ***************************************************************************************/
static bool DispTest::PollHWNotValueCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    const char * name = (const char *)ppArgs[0];
    UINT32 index = *(ppArgs[1]);
    UINT32 Value  = *(ppArgs[2]);
    UINT32 Mask   = *(ppArgs[3]);
    UINT32 Size   = *(ppArgs[4]);

    // check for value at memory location
    UINT32 Rtlwalue;
    Platform::EscapeRead(name,index,Size, &Rtlwalue);

    return ((Value & Mask) != (Rtlwalue & Mask));
}

/****************************************************************************************
 * DispTest::PollHWNotValue
 *
 *  Arguments Description:
 *  - name:
 *  - index:
 *  - Value: the exit value, the default is 'true'
 *  - Mask: the Mask to mark the field of interest within the Register
 *  - Size:
 *  - TimeoutMs: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Resolve the Ctx DMA (Get Base and Size)
 *  - Poll on address Base+Offset (Check if Offset < Size) until it equals
 *    Value or TimeoutMs is reached.
 ***************************************************************************************/
RC DispTest::PollHWNotValue
(
    const char * name,
    UINT32 index,
    UINT32 Value,
    UINT32 Mask,
    UINT32 Size,
    FLOAT64 TimeoutMs
)
{
    const UINT32 *pArgs[5] = {(const UINT32*)name, &index, &Value, &Mask, &Size};
    Tasker::PollFunc pPollHWNotValueCheck = &DispTest::PollHWNotValueCheck;
    return POLLWRAP_HW(pPollHWNotValueCheck, pArgs, TimeoutMs);
}

/****************************************************************************************
 * DispTest::PollValueCheck
 *
 *  Arguments Description:
 *  - pArgs: Pointer to list of arguments
 *    - Handle: handle to CtxDMA describing the memory area of interest.
 *    - Offset: offset within the memory area
 *    - Value: the exit value, the default is 'true'
 *
 *  Functional Description:
 *  - Static function needed by polling routine "PollValue" to read register once and check against masked value
 ***************************************************************************************/
static bool DispTest::PollValueCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    UINT32 *pMem = ppArgs[0];
    UINT32 Value = *(ppArgs[1]);
    UINT32 Mask  = *(ppArgs[2]);
    //Return Data
    UINT32 RtnData = MEM_RD32((void *)(pMem));

    int LwrrentDev = DispTest::m_nLwrrentDevice;

    for (int i = 0; i < DispTest::m_nNumDevice; i++ ) {
        DispTest::BindDevice(i);
        // update CRCs
        DispTest::Crlwpdate();
    }
    DispTest::BindDevice(LwrrentDev);
    // check for value at memory location
    return ((Value & Mask) == (Mask & RtnData));
}

/****************************************************************************************
 * DispTest::PollValue
 *
 *  Arguments Description:
 *  - Handle: handle to CtxDMA describing the memory area of interest.
 *  - Offset: offset within the memory area
 *  - Value: the exit value, the default is 'true'
 *  - TimeoutMs: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Resolve the Ctx DMA (Get Base and Size)
 *  - Poll on address Base+Offset (Check if Offset < Size) until it equals
 *    Value or TimeoutMs is reached.
 ***************************************************************************************/
RC DispTest::PollValue
(
    LwRm::Handle Handle,
    UINT32 Offset,
    UINT32 Value,
    UINT32 Mask,
    FLOAT64 TimeoutMs
)
{
    // get context dma for the handle
    DmaContext *HandleCtxDma = GetDmaContext(Handle);
    if (!HandleCtxDma)
    {
        Printf(Tee::PriHigh, "Can't Find Ctx DMA for Handle\n");
        return RC::BAD_PARAMETER;
    }

    // get base pointer and limit
    UINT32 *pHandleBase = (UINT32*)(HandleCtxDma->Address);
    UINT64 HandleLimit = HandleCtxDma->Limit;

    // check that offset is within limit
    if (Offset >= HandleLimit)
    {
        Printf(Tee::PriHigh, "Offset Greater than Limit of Memory Area\n");
        return RC::BAD_PARAMETER;
    }

    // pack arguments to poll function
    UINT32 *pMem = pHandleBase + Offset;
    UINT32 *pArgs[3] = { pMem, &Value , &Mask};
    Tasker::PollFunc pPollValueCheck = &DispTest::PollValueCheck;

    return POLLWRAP_HW(pPollValueCheck, pArgs, TimeoutMs);
}

/****************************************************************************************
 * DispTest::PollGreaterEqualValueCheck
 *
 *  Arguments Description:
 *  - pArgs: Pointer to list of arguments
 *    - Handle: handle to CtxDMA describing the memory area of interest.
 *    - Offset: offset within the memory area
 *    - Value: the exit value, the default is 'true'
 *
 *  Functional Description:
 *  - Static function needed by polling routine "PollGreaterEqualValue" to read register once and check against masked value
 ***************************************************************************************/
static bool DispTest::PollGreaterEqualValueCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    UINT32 *pMem = ppArgs[0];
    UINT32 Value = *(ppArgs[1]);
    UINT32 Mask  = *(ppArgs[2]);
    //Return Data
    UINT32 RtnData = MEM_RD32((void *)(pMem));

    int LwrrentDev = DispTest::m_nLwrrentDevice;

    for (int i = 0; i < DispTest::m_nNumDevice; i++ )
    {
        DispTest::BindDevice(i);
        // update CRCs
        DispTest::Crlwpdate();
    }
    DispTest::BindDevice(LwrrentDev);
    // check for value at memory location
    if ((Mask & RtnData) >= (Value & Mask))
    {
        printf("poll value %d get %d\n", (Value & Mask), (Mask & RtnData));
        return true;
    }
    else
    {
        return false;
    }

}

/****************************************************************************************
 * DispTest::PollGreaterEqualValue
 *
 *  Arguments Description:
 *  - Handle: handle to CtxDMA describing the memory area of interest.
 *  - Offset: offset within the memory area
 *  - Value: the exit value, the default is 'true'
 *  - TimeoutMs: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Resolve the Ctx DMA (Get Base and Size)
 *  - Poll on address Base+Offset (Check if Offset < Size) until it is larger
 *    Value or TimeoutMs is reached.
 ***************************************************************************************/
RC DispTest::PollGreaterEqualValue
(
    LwRm::Handle Handle,
    UINT32 Offset,
    UINT32 Value,
    UINT32 Mask,
    FLOAT64 TimeoutMs
)
{
    // get context dma for the handle
    DmaContext *HandleCtxDma = GetDmaContext(Handle);
    if (!HandleCtxDma)
    {
        Printf(Tee::PriHigh, "Can't Find Ctx DMA for Handle\n");
        return RC::BAD_PARAMETER;
    }

    // get base pointer and limit
    UINT32 *pHandleBase = (UINT32*)(HandleCtxDma->Address);
    UINT64 HandleLimit = HandleCtxDma->Limit;

    // check that offset is within limit
    if (Offset >= HandleLimit)
    {
        Printf(Tee::PriHigh, "Offset Greater than Limit of Memory Area\n");
        return RC::BAD_PARAMETER;
    }

    // pack arguments to poll function
    UINT32 *pMem = pHandleBase + Offset;
    UINT32 *pArgs[3] = { pMem, &Value , &Mask};
    Tasker::PollFunc pPollGreaterEqualValueCheck = &DispTest::PollGreaterEqualValueCheck;

    return POLLWRAP_HW(pPollGreaterEqualValueCheck, pArgs, TimeoutMs);
}

/****************************************************************************************
 * DispTest::PollValueAtAddrCheck
 *
 *  Arguments Description:
 *  - pArgs: Pointer to list of arguments:
 *    - Address: Memory Address to poll at (This is a physical address).
 *    - Offset: offset within the memory area
 *    - Value: the exit value, the default is 'true'
 *
 *  Functional Description:
 *  - Static function needed by polling routine "PollValueAtAddr" to read register once and check against masked value
 ***************************************************************************************/
static bool DispTest::PollValueAtAddrCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    PHYSADDR Mem = *((PHYSADDR*)ppArgs[0]);
    UINT32 Value = *(ppArgs[1]);
    UINT32 Mask  = *(ppArgs[2]);
    //Return Data
    UINT32 RtnData = Platform::PhysRd32(Mem);

    // update CRCs
    DispTest::Crlwpdate();
    // check for value at memory location
    return ((Value & Mask) == (Mask & RtnData));
}

/****************************************************************************************
 * DispTest::PollValueAtAddr
 *
 *  Arguments Description:
 *  - Address: Memory Address to poll at (This is a physical address).
 *  - Offset: offset within the memory area
 *  - Value: the exit value, the default is 'true'
 *  - TimeoutMs: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Resolve the Ctx DMA (Get Base and Size)
 *  - Poll on address Base+Offset (Check if Offset < Size) until it equals
 *    Value or TimeoutMs is reached.
 ***************************************************************************************/
RC DispTest::PollValueAtAddr
(
    PHYSADDR Base,
    UINT32 Offset,
    UINT32 Value,
    UINT32 Mask,
    FLOAT64 TimeoutMs
)
{
    // pack arguments to poll function
    PHYSADDR MemAdd = (Base + Offset);
    UINT32 *pArgs[3] = { (UINT32*)&MemAdd, &Value, &Mask};
    Tasker::PollFunc pPollValueCheck = &DispTest::PollValueAtAddrCheck;

    return POLLWRAP_HW(pPollValueCheck, pArgs, TimeoutMs);
}

/****************************************************************************************
 * DispTest::PollScanlineGreaterEqualValueCheck
 *
 *  Arguments Description:
 *  - Handle: Base channel handle
 *  - Value: the exit value, the default is 'true'
 *  - Timeout: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Poll on scanline
*  - Static function needed by polling routine "PollPollScanlineGreaterEqualValue" to read scanline once and check against value
 ***************************************************************************************/
static bool DispTest::PollScanlineGreaterEqualValueCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    LwRm::Handle Handle = *(ppArgs[0]);
    UINT32 Value  = *(ppArgs[1]);
    //Return Data
    UINT32 RtnData;

    DispTest::GetBaseChannelScanline(Handle, &RtnData);

    // check for scanline value
    return (RtnData >= Value);
}

/****************************************************************************************
 * DispTest::PollScanlineGreaterEqualValue
 *
 *  Arguments Description:
 *  - Handle: Base channel handle
 *  - Value: the exit value, the default is 'true'
 *  - TimeoutMs: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Poll on scanline until
 *    Value or TimeoutMs is reached.
 ***************************************************************************************/
RC DispTest::PollScanlineGreaterEqualValue
(
    LwRm::Handle Handle,
    UINT32 Value,
    FLOAT64 TimeoutMs
)
{
    // pack arguments to poll function
    UINT32 *pArgs[2] = { &Handle, &Value };
    Tasker::PollFunc pPollScanlineGreaterEqualValueCheck = &DispTest::PollScanlineGreaterEqualValueCheck;

    return POLLWRAP_HW(pPollScanlineGreaterEqualValueCheck, pArgs, TimeoutMs);
}

/****************************************************************************************
 * DispTest::PollScanlineLessValueCheck
 *
 *  Arguments Description:
 *  - Handle: Base channel handle
 *  - Value: the exit value, the default is 'true'
 *  - Timeout: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Poll on scanline
*  - Static function needed by polling routine "PollPollScanlineLessValue" to read scanline once and check against value
 ***************************************************************************************/
static bool DispTest::PollScanlineLessValueCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    LwRm::Handle Handle = *(ppArgs[0]);
    UINT32 Value  = *(ppArgs[1]);
    //Return Data
    UINT32 RtnData;

    DispTest::GetBaseChannelScanline(Handle, &RtnData);

    // check for scanline value
    return (RtnData < Value);
}

/****************************************************************************************
 * DispTest::PollScanlineLessValue
 *
 *  Arguments Description:
 *  - Handle: Base channel handle
 *  - Value: the exit value, the default is 'true'
 *  - TimeoutMs: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Poll on scanline until
 *    Value or TimeoutMs is reached.
 ***************************************************************************************/
RC DispTest::PollScanlineLessValue
(
    LwRm::Handle Handle,
    UINT32 Value,
    FLOAT64 TimeoutMs
)
{
    // pack arguments to poll function
    UINT32 *pArgs[2] = { &Handle, &Value };
    Tasker::PollFunc pPollScanlineLessValueCheck = &DispTest::PollScanlineLessValueCheck;

    return POLLWRAP_HW(pPollScanlineLessValueCheck, pArgs, TimeoutMs);
}

/****************************************************************************************
 * DispTest::PollDoneCheck
 *
 *  Arguments Description:
 *  - pArgs: Pointer to list of arguments:
 *    - Handle: Handle to the Notifier (Channel Notifier or Head CRC Notifier)
 *    - Offset: double-word offset to the word in the memory region that
 *      contains the done bit.
 *    - Bit: the bit in the 32 bit word that contains the done bit
 *    - Value: the exit value, the default is 'true'
 *
 *  Functional Description:
 *  - Static function needed by polling routine "PollDone" to read register once and check against masked value
 ***************************************************************************************/
static bool DispTest::PollDoneCheck(void *pArgs)
{
    // extract memory pointer, value, and bitmask
    UINT32 **ppArgs = (UINT32**)(pArgs);
    UINT32 *pMem = ppArgs[0];
    UINT32 Value = *(ppArgs[1]);
    UINT32 Bitmask = *(ppArgs[2]);
    // check for value at memory location
    UINT32 RtnData = MEM_RD32((void *)(pMem));

    // update CRCs
    DispTest::Crlwpdate();

    return (Value == (RtnData & Bitmask));
}

/****************************************************************************************
 * DispTest::PollDone
 *
 *  Arguments Description:
 *  - Handle: Handle to the Notifier (Channel Notifier or Head CRC Notifier)
 *  - Offset: double-word offset to the word in the memory region that
 *    contains the done bit.
 *  - Bit: the bit in the 32 bit word that contains the done bit
 *  - Value: the exit value, the default is 'true'
 *  - TimeoutMs: the maximum time to poll before returning
 *
 *  Functional Description:
 *  - Poll on Completion Field in the Notifier. Exit on completion or Timeout.
 ***************************************************************************************/
RC DispTest::PollDone
(
    LwRm::Handle Handle,
    UINT32 Offset,
    UINT32 Bit,
    bool Value,
    FLOAT64 TimeoutMs
)
{
    // get context dma for the handle
    DmaContext *HandleCtxDma = GetDmaContext(Handle);
    if (!HandleCtxDma)
    {
        Printf(Tee::PriHigh, "Can't Find Ctx DMA for Handle\n");
        return RC::BAD_PARAMETER;
    }

    // get base pointer and limit
    UINT32 *pHandleBase = (UINT32*)(HandleCtxDma->Address);
    UINT64 HandleLimit = HandleCtxDma->Limit;

    // check that offset is within limit
    if (Offset >= HandleLimit)
    {
        Printf(Tee::PriHigh, "Offset Greater than Limit of Memory Area\n");
        return RC::BAD_PARAMETER;
    }

    // construct bitmasks
    UINT32 Bitmask = (1 << Bit);
    UINT32 Value32 = (Value) ? (1) : (0);
    Value32 = Value32 << Bit;

    // pack arguments to poll function
    UINT32 *pMem = pHandleBase + Offset;
    UINT32 *pArgs[3] = { pMem, &Value32, &Bitmask };
    Tasker::PollFunc pPollDoneCheck = &DispTest::PollDoneCheck;

    return POLLWRAP_HW(pPollDoneCheck, pArgs, TimeoutMs);
}

static bool DispTest::PollRGScanUnlockedCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    UINT32 Head = *(ppArgs[0]);

    UINT32 scanLocked;
    UINT32 flipLocked;

    // Read Value
    DispTest::GetRGStatus(Head, &scanLocked, &flipLocked);

    UINT32 Unlocked = !scanLocked;

    // update CRCs
    DispTest::Crlwpdate();
    // check for value at memory location
    return (Unlocked != 0);
}

RC DispTest::PollRGScanUnlocked(UINT32 Head, FLOAT64 TimeoutMs)
{
    const UINT32 *pArgs[1] = {&Head};
    Tasker::PollFunc pPollRGScanUnlockedCheck = &DispTest::PollRGScanUnlockedCheck;
    return POLLWRAP_HW(pPollRGScanUnlockedCheck, pArgs, TimeoutMs);
}

static bool DispTest::PollRGFlipUnlockedCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    UINT32 Head = *(ppArgs[0]);

    UINT32 scanLocked;
    UINT32 flipLocked;

    // Read Value
    DispTest::GetRGStatus(Head, &scanLocked, &flipLocked);

    UINT32 Unlocked = !flipLocked;

    // update CRCs
    DispTest::Crlwpdate();
    // check for value at memory location
    return (Unlocked != 0);
}

RC DispTest::PollRGFlipUnlocked(UINT32 Head, FLOAT64 TimeoutMs)
{
    const UINT32 *pArgs[1] = {&Head};
    Tasker::PollFunc pPollRGFlipUnlockedCheck = &DispTest::PollRGFlipUnlockedCheck;
    return POLLWRAP_HW(pPollRGFlipUnlockedCheck, pArgs, TimeoutMs);
}

static bool DispTest::PollRGScanLockedCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    UINT32 Head = *(ppArgs[0]);

    UINT32 scanLocked;
    UINT32 flipLocked;

    // Read Value
    DispTest::GetRGStatus(Head, &scanLocked, &flipLocked);

    UINT32 Locked = scanLocked;

    // update CRCs
    DispTest::Crlwpdate();
    // check for value at memory location
    return (Locked != 0);
}

RC DispTest::PollRGScanLocked(UINT32 Head, FLOAT64 TimeoutMs)
{
    const UINT32 *pArgs[1] = {&Head};
    Tasker::PollFunc pPollRGScanLockedCheck = &DispTest::PollRGScanLockedCheck;
    return POLLWRAP_HW(pPollRGScanLockedCheck, pArgs, TimeoutMs);
}

static bool DispTest::PollRGFlipLockedCheck(void *pArgs)
{
    // extract memory pointer and value
    UINT32 **ppArgs = (UINT32**)(pArgs);
    UINT32 Head = *(ppArgs[0]);

    UINT32 scanLocked;
    UINT32 flipLocked;

    // Read Value
    DispTest::GetRGStatus(Head, &scanLocked, &flipLocked     );

    UINT32 Locked = flipLocked;

    // update CRCs
    DispTest::Crlwpdate();
    // check for value at memory location
    return (Locked != 0);
}

RC DispTest::PollRGFlipLocked(UINT32 Head, FLOAT64 TimeoutMs)
{
    const UINT32 *pArgs[1] = {&Head};
    Tasker::PollFunc pPollRGFlipLockedCheck = &DispTest::PollRGFlipLockedCheck;
    return POLLWRAP_HW(pPollRGFlipLockedCheck, pArgs, TimeoutMs);
}

static bool DispTest::PollRegValueNoUpdateCheck(void *pArgs)
{
    UINT32 *pUINT32Args = (UINT32*)(pArgs);
    UINT32 Address = pUINT32Args[0];
    UINT32 Value   = pUINT32Args[1];
    UINT32 Mask    = pUINT32Args[2];

    // Read Value
    UINT32 ReadValue = DispTest::GetBoundGpuSubdevice()->RegRd32(Address);

    return ((Value & Mask) == (ReadValue & Mask));
}

RC DispTest::PollRegValueNoUpdate
(
    UINT32 Address,
    UINT32 Value,
    UINT32 Mask,
    FLOAT64 TimeoutMs
)
{
    UINT32 Args[3] = {Address, Value, Mask};
    Tasker::PollFunc pPollRegValueNoUpdateCheck = &DispTest::PollRegValueNoUpdateCheck;
    return POLLWRAP_HW(pPollRegValueNoUpdateCheck, Args, TimeoutMs);
}

//
//      Copyright (c) 1999-2005 by LWPU Corporation.
//      All rights reserved.
//
