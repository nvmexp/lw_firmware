/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// JS Encrypting utility

#pragma once

#include "lwdiagutils.h"
#include <vector>
#include <stdio.h>

namespace Decryptor
{
    LwDiagUtils::EC DecryptFile
    (
        FILE*           pInFile,
        vector<UINT08>* pDecryptBuffer
    );

    LwDiagUtils::EC DecryptTraceFile
    (
        FILE*           pInFile,
        vector<UINT08>* pDecryptBuffer
    );

    LwDiagUtils::EC DecryptDataArray
    (
        const UINT08*   data,
        size_t          dataSize,
        vector<UINT08>* pDecryptBuffer
    );

    LwDiagUtils::EC DecryptTraceDataArray
    (
        const UINT08*   data,
        size_t          dataSize,
        vector<UINT08>* pDecryptBuffer
    );

    LwDiagUtils::EC GetDecryptedSize
    (
        const UINT08*   data,
        size_t          dataSize,
        size_t*         pDecryptedSize
    );
}
