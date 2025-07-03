/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "../preproc.h"
#include <string.h>

using namespace LwDiagUtils;

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        Printf(PriNormal, "Missing argument - source file\n");
        return 1;
    }

    Preprocessor preproc;

    for (int iarg=2; iarg < argc; iarg++)
    {
        if (0 == strcmp(argv[iarg], "-comments"))
        {
            preproc.DisableCommentCollapsing();
        }
        else if (0 == strcmp(argv[iarg], "-I"))
        {
            ++iarg;
            if (iarg == argc)
            {
                Printf(PriNormal, "Missing directory argument\n");
                return 1;
            }
            preproc.AddSearchPath(argv[iarg]);
        }
        else
        {
            Printf(PriNormal, "Unrecognized command line option: %s\n", argv[iarg]);
            return 1;
        }
    }

    if (OK != preproc.LoadFile(argv[1]))
    {
        Printf(PriNormal, "Failed to load file - %s\n", argv[1]);
        return 1;
    }

    vector<char> out;
    const EC ec = preproc.Process(&out);

    if (ec != OK)
    {
        out.push_back('\n');  // In case input file has no terminating newline
        char buffer[32];
        sprintf(buffer, "#errcode %u\n", static_cast<unsigned>(ec));
        for (const char* pBuffer = &buffer[0]; *pBuffer != '\0'; ++pBuffer)
        {
            out.push_back(*pBuffer);
        }
    }

    Printf(PriNormal, "%.*s", static_cast<int>(out.size()), &out[0]);

    return 0;
}
