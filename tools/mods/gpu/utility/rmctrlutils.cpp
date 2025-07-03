/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmctrlutils.cpp
//! \brief The utility class definition for all LWCTRL commands.
//!
//! The utility can be used across the LWCTRL commands as it actually provides
//! generic framework for all LWCTRL commands, where-in parameter checkings for
//! commands is done.This framework will also provide the device, subdevice
//! access for the RM tests which needs such setup. Such accesses can be
//! used in the Setup() function of any LWCTRL related RM tests.

#include "rmctrlutils.h"
#include "core/include/tee.h"

//! \brief RmtCrtlUtil constructor.
//!
//-----------------------------------------------------------------------------
RmtCrtlUtil::RmtCrtlUtil()
{
}

//! \brief RmtCrtlUtil destructor.
//!
//-----------------------------------------------------------------------------
RmtCrtlUtil::~RmtCrtlUtil()
{
}

//! \brief CheckCtrlCmdNull function.
//!
//! This is the generic function for testing NULL Command usage for any
//! Object-Engine configuration
//!
//! \return RC -> OK if everything as expected, specific RC if something
//!         failed.
//-----------------------------------------------------------------------------
RC RmtCrtlUtil::CheckCtrlCmdNull(LwRm::Handle hObject)
{
    LwRmPtr pLwRm;
    RC rc;

    //_CTRL_CMD_NULL tests
    CHECK_RC(pLwRm->Control(hObject, 0, 0, 0));

    return rc;
}

//! \brief CheckCtrlCmdParam function.
//!
//! This is the generic function checking LWCTRL command behavior for incorrect
//! inputs. Expected output for all such cases is that command should actually
//! report expected error. This function does following tests:
//! Param size mismatch, Null Param pointer and zero Param Size.
//!
//! \return RC -> OK if everything as expected, LWRM_ERROR if something
//!         failed.
//-----------------------------------------------------------------------------
RC RmtCrtlUtil::CheckCtrlCmdParam(LwRm::Handle hObject,
                                  UINT32 cmd,
                                  void   *pParams,
                                  UINT32 paramsSize)
{
    LwRmPtr pLwRm;
    RC rc;
    RC errorRc;

    // input parameter checking

    // param size mismatch
    errorRc = pLwRm->Control(hObject, cmd, pParams, (paramsSize - 1));

    if (errorRc != RC::LWRM_ILWALID_PARAM_STRUCT)
    {
        Printf(Tee::PriHigh,
            "CheckCtrlCmdParam: Unexpected RM Control error\n");
        Printf(Tee::PriHigh,
             "CheckCtrlCmdParam: Expected invalid param structure, Got %s\n",
               errorRc.Message());

        // return generic error
        return (RC::LWRM_ERROR);
    }

    // null pParams pointer
    errorRc = pLwRm->Control(hObject, cmd, NULL, paramsSize);

    if (errorRc != RC::LWRM_ILWALID_ARGUMENT)
    {
        Printf(Tee::PriHigh,
            "CheckCtrlCmdParam: Unexpected RM Control error\n");
        Printf(Tee::PriHigh,
             "CheckCtrlCmdParam: Expected invalid argument, Got %s\n",
               errorRc.Message());

        // return generic error
        return (RC::LWRM_ERROR);
    }

    // zero pParamSize
    errorRc = pLwRm->Control(hObject, cmd, pParams, 0);

    if (errorRc != RC::LWRM_ILWALID_ARGUMENT)
    {
        Printf(Tee::PriHigh,
            "CheckCtrlCmdParam: Unexpected RM Control error\n");
        Printf(Tee::PriHigh,
             "CheckCtrlCmdParam: Expected invalid argument, Got %s\n",
               errorRc.Message());

        // return generic error
        return (RC::LWRM_ERROR);
    }

    return rc;
}

