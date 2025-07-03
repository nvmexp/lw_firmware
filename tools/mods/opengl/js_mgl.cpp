/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------

// Mods opengl utilities.
#include "modsgl.h"
#include "core/include/tee.h"
#include "core/include/jscript.h"
#include "core/include/script.h"

C_(SuppressGLExtension);
static STest S_SuppressGLExtension
(
    "SuppressGLExtension",
    C_SuppressGLExtension,
    1,
    "Force mods GL tests to pretend this extension is not supported"
);
C_(SuppressGLExtension)
{
    JavaScriptPtr pJavaScript;
    string extension;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &extension)))
    {
        JS_ReportError(pContext, "Usage: SuppressGLExtension(\"GL_LW_whatever\")");
        return JS_FALSE;
    }
    mglSuppressExtension(extension);

    RETURN_RC(OK);
}

C_(UnsuppressGLExtension);
static STest S_UnSuppressGLExtension
(
    "UnsuppressGLExtension",
    C_UnsuppressGLExtension,
    1,
    "Remove a suppressed GL extension from the suppression list."
);
C_(UnsuppressGLExtension)
{
    JavaScriptPtr pJavaScript;
    string extension;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &extension)))
    {
        JS_ReportError(pContext, "Usage: UnsuppressGLExtension(\"GL_LW_whatever\")");
        return JS_FALSE;
    }
    mglUnsuppressExtension(extension);

    RETURN_RC(OK);
}
