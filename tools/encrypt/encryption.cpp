/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2019 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <functional>

#include "encryption.h"
#include "lwdiagutils.h"

//! \brief Preprocess a file (in C preprocessor fashion)
//!
//! \param input               : Input file name to preprocess
//! \param pPreprocessedBuffer : Output buffer that the input file is
//!                              preprocessed into
//! \param additionalPaths     : Additional search paths for the file
//! \param numPaths            : Number of entries in the additional paths
//! \param preprocDefs         : Preprocessor definitions
//! \param numDefs             : Number of entries in the preprocessor definitions
//!
//! \return OK if successful
static LwDiagUtils::EC EcPreprocessFile
(
    const char     *input,
    vector<UINT08> *pPreprocessedBuffer,
    char          **additionalPaths,
    UINT32          numPaths,
    char          **preprocDefs,
    UINT32          numDefs
)
{
    LwDiagUtils::Printf(LwDiagUtils::PriHigh,
                        "Encryption : No file pre-processor installed\n");
    return LwDiagUtils::SOFTWARE_ERROR;
}

//! Callback function variables
static Encryption::PreprocessFileFunc s_PreprocessFileFunc = EcPreprocessFile;

//! \brief Initialize the library by setting the callback functions
//!
//! \param ppff : Callback function to call for preprocessing a file
void Encryption::Initialize(PreprocessFileFunc ppff)
{
    if (ppff)
        s_PreprocessFileFunc = ppff;
}

//! \brief Free allocations and reset callbacks
void Encryption::Shutdown()
{
    s_PreprocessFileFunc = EcPreprocessFile;
}

//! \brief Preprocess a file (in C preprocessor fashion) - calls the
//!        preprocessor callback
//!
//! \param input               : Input file name to preprocess
//! \param pPreprocessedBuffer : Output buffer that the input file is
//!                              preprocessed into
//! \param additionalPaths     : Additional search paths for include files
//! \param numPaths            : Number of entries in the additional paths
//! \param preprocDefs         : Preprocessor definitions
//! \param numDefs             : Number of entries in the preprocessor definitions
//!
//! \return OK if successful
LwDiagUtils::EC Encryption::PreprocessFile
(
    const char     *input,
    vector<UINT08> *pPreprocessedBuffer,
    char          **additionalPaths,
    UINT32          numPaths,
    char          **preprocDefs,
    UINT32          numDefs
)
{
    return s_PreprocessFileFunc(input, pPreprocessedBuffer,
                                additionalPaths, numPaths,
                                preprocDefs, numDefs);
}
