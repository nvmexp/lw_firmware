/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2009 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// JS Encrypting utility

#pragma once

#ifndef INCLUDED_PREPROCESS_H
#define INCLUDED_PREPROCESS_H

#include "rc.h"
#include <string>
#include <vector>

namespace Preprocessor
{
      enum StrictMode
      {
          StrictModeOn
         ,StrictModeOff
         ,StrictModeEngr
         ,StrictModeUnspecified
      };
      RC Preprocess(const char * Input, vector<char> * pPreprocessedBuffer,
                    char **additionalPaths,
                    UINT32 numPaths, bool compressing,
                    bool cc1LineCommands, bool IsFile,
                    StrictMode *pStrictMode);

};

#endif // ! INCLUDED_ENCRYPTOR_H
