/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2019,2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <cstdio>
#include <string>

#include "lwdiagutils.h"

namespace Encryptor
{
    //! \brief Compress and encrypt a file
    //!
    //! \param fileName            : The filename to encrypt
    //! \param outFileName         : The filename to write to
    //! \param additionalPaths     : Additional search paths for include files
    //! \param numPaths            : Number of entries in the additional paths
    //! \param preprocDefs         : Preprocessor definitions
    //! \param numDefs             : Number of entries in the preprocessor definitions
    //! \param boundFile           : Generate bound JS header file
    //! \param preprocessFile      : Run preprocessor on the input file
    LwDiagUtils::EC EncryptFile(
        const string &fileName,
        const string &outFileName,
        char        **additionalPaths,
        UINT32        numPaths,
        char        **preprocDefs,
        UINT32        numDefs,
        bool          boundFile,
        bool          preprocessFile
    );

    LwDiagUtils::EC EncryptTraceFile(const string &fileName, const string &outFileName);
};
