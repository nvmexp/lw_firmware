/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or diss_Closure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/preprocess.h"
#include "preproc.h"
#include "decrypt.h"
#include "core/include/utility.h"
#include "core/include/massert.h"
#include "core/include/version.h"
#include "core/include/cmdline.h"
#include "core/include/memcheck.h"

RC Preprocessor::Preprocess
(
      const char    *input
    , vector<char>  *pPreprocessedBuffer
    , char         **additionalPaths
    , UINT32         numPaths
    , bool           compressing
    , bool           cc1LineCommands
    , bool           isFile
    , StrictMode    *pStrictMode
)
{
    PROFILE_ZONE(GENERIC)

    MASSERT(pPreprocessedBuffer != nullptr);

    //
    // Initialize the preprocessor.
    //
    LwDiagUtils::Preprocessor preproc;

    preproc.SetDecryptFile(Decryptor::DecryptFile);

    if (isFile)
    {
        //
        // Specify the path of the #include directories.
        //
        //    Path1: Path of script file.
        //
        string scriptPath(input);

        string::size_type pos = scriptPath.find_last_of("/\\");
        if (string::npos == pos)
        {
            scriptPath = ".";
        }
        else
        {
            scriptPath = scriptPath.substr(0, pos).c_str();
        }

        preproc.AddSearchPath(scriptPath);
    }

    preproc.AddSearchPath(CommandLine::ProgramPath());

    if (additionalPaths && numPaths)
    {
        // Add paths from argument list (does anyone use this feature?)
        for (UINT32 p = 0; p < numPaths; p++)
        {
            preproc.AddSearchPath(additionalPaths[p]);
        }
    }

    // Add paths from the MODSP environment variable.
    vector<string> pathsFromMODSP;
    Utility::AppendElwSearchPaths(&pathsFromMODSP);
    for (size_t p = 0; p < pathsFromMODSP.size(); p++)
    {
        preproc.AddSearchPath(pathsFromMODSP[p]);
    }

    if (compressing)
        preproc.SetLineCommandMode(LwDiagUtils::Preprocessor::LineCommandNone);
    else if (CommandLine::DebugJavascript())
        preproc.SetLineCommandMode(LwDiagUtils::Preprocessor::LineCommandComment);
    else if (!cc1LineCommands)
        preproc.SetLineCommandMode(LwDiagUtils::Preprocessor::LineCommandAt);
    else
        preproc.SetLineCommandMode(LwDiagUtils::Preprocessor::LineCommandHash);

    RC rc;
    if (isFile)
    {
        CHECK_RC(Utility::InterpretModsUtilErrorCode(
                preproc.LoadFile(input)));
    }
    else
    {
        CHECK_RC(Utility::InterpretModsUtilErrorCode(
                preproc.SetBuffer(input, strlen(input))));
    }

#if defined(INCLUDE_BOARDS_DB)
    // Pass INCLUDE_BOARDS_DB to JS preprocessor. boards.js will use it to
    // decide whether to include boards.db. Therefore boards.js is going to be
    // the only place where boards.db is either included or not.
    preproc.AddMacro("INCLUDE_BOARDS_DB", "");
#endif
    // Add a define for whether or not GL is included in this mods package.
    // Allows us to conditionally include glrandom.js at preprocess time.
    if (g_INCLUDE_OGL)
    {
        preproc.AddMacro("INCLUDE_OGL", "true");
    }
    if (g_INCLUDE_OGL_ES)
    {
        preproc.AddMacro("INCLUDE_OGL_ES", "true");
    }
    if (g_INCLUDE_VULKAN)
    {
        preproc.AddMacro("INCLUDE_VULKAN", "true");
    }
    if (g_INCLUDE_LWDA)
    {
        preproc.AddMacro("INCLUDE_LWDA", "true");
    }
    if (g_INCLUDE_LWDART)
    {
        preproc.AddMacro("INCLUDE_LWDART", "true");
    }
    if (g_INCLUDE_GPU)
    {
        preproc.AddMacro("INCLUDE_GPU", "true");
    }

    if (CommandLine::DebugJavascript())
    {
        preproc.AddMacro("DEBUG_JS", "true");
    }

    if (g_BUILD_TEGRA_EMBEDDED)
    {
        preproc.AddMacro("BUILD_TEGRA_EMBEDDED", "true");
    }

    // Ensure that buffer size is zero so that data insertion starts at the
    // beginning.
    pPreprocessedBuffer->resize(0);

    //
    // Preprocess the file.
    //

    CHECK_RC(Utility::InterpretModsUtilErrorCode(
            preproc.Process(pPreprocessedBuffer)));

    if (pStrictMode != nullptr)
    {
        switch (preproc.GetStrictMode())
        {
            case LwDiagUtils::Preprocessor::StrictModeOff:
                *pStrictMode = StrictModeOff;
                break;
            case LwDiagUtils::Preprocessor::StrictModeOn:
                *pStrictMode = StrictModeOn;
                break;
            case LwDiagUtils::Preprocessor::StrictModeEngr:
                *pStrictMode = StrictModeEngr;
                break;
            case LwDiagUtils::Preprocessor::StrictModeUnspecified:
                *pStrictMode = StrictModeUnspecified;
                break;
            default:
                ::Printf(Tee::PriError, "Unknown strict mode : %d\n", preproc.GetStrictMode());
                return RC::SOFTWARE_ERROR;
        }
    }
    return OK;
}
