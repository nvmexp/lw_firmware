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

/**
 * @file rmctrlutils.h
 * @brief The utility class declaration for all LWCTRL commands.
 *
 * This header file declars util class for LW RM control commands.
 */

#include "core/include/lwrm.h"

#ifndef INCLUDED_RMT_UTILITY_H
#define INCLUDED_RMT_UTILITY_H

class RmtCrtlUtil
{
public:
    RmtCrtlUtil();
    ~RmtCrtlUtil();

    RC CheckCtrlCmdNull
    (
        LwRm::Handle hObject
    );

    RC CheckCtrlCmdParam
    (
        LwRm::Handle hObject,
        UINT32 cmd,
        void   *pParams,
        UINT32 paramsSize
    );

private:
    // Lwrrently empty

};

#endif // INCLUDED_RMT_UTILITY_H

