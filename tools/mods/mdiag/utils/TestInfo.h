/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _TESTINFO_H_
#define _TESTINFO_H_

namespace TestEnums
{
    enum TEST_STATUS
    {
        TEST_NOT_STARTED        = 0,
        TEST_INCOMPLETE         = 1,
        TEST_SUCCEEDED          = 2,
        TEST_FAILED             = 3,
        TEST_FAILED_TIMEOUT     = 4,
        TEST_FAILED_CRC         = 5,
        TEST_NO_RESOURCES       = 6,
        TEST_CRC_NON_EXISTANT   = 7,
        TEST_FAILED_PERFORMANCE = 8,
        TEST_FAILED_ECOV        = 9,
        TEST_ECOV_NON_EXISTANT  = 10,
        TEST_START_PRIVATE      = 1000
    };
};

#endif // _TESTINFO_H_
