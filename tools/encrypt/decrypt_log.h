/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
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
    LwDiagUtils::EC DecryptLog
    (
        FILE*           pInFile,
        vector<UINT08>* pDecryptBuffer
    );
}
