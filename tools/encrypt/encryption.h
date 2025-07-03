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

/**
 * @file   encryption.h
 * @brief  Contains generic interface into the encryption library.
 *
 */

#pragma once

#include <functional>

#include "lwdiagutils.h"

//! The Encryption namespace.
namespace Encryption
{
    // Callback functions that applications linking with this library can
    // provide
    typedef std::function<
        LwDiagUtils::EC (const char *, vector<UINT08> *, char **, UINT32, char **, UINT32)
      > PreprocessFileFunc;

    //! Library initialization routine, sane defaults are provided if this is
    //! not called
    void Initialize(PreprocessFileFunc ppff);

    //! Free allocations and reset callbacks
    void Shutdown();

    //! Local version of PreprocessFile, calls the preprocess file callback
    LwDiagUtils::EC PreprocessFile
    (
        const char     *input,
        vector<UINT08> *pPreprocessedBuffer,
        char          **additionalPaths,
        UINT32          numPaths,
        char          **preprocDefs,
        UINT32          numDefs
    );
}
