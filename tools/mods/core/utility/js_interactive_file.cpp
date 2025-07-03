/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/xp.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <memory>

/// JS interface for Xp::InteractiveFile.
///
/// Useful for files in sysfs.
/// Can also be used to read entire files at once.
///
/// The InteractiveFile can be used in JS like this:
///
/// var f = new InteractiveFile;          // Creates unopened file object
/// var f = new InteractiveFile("/path"); // Opens a file in read-only mode
/// var f = new InteractiveFile("/path", true); // Opens a file in read-write mode
/// f.Open("/path");                      // Opens a file in read-only mode
/// f.Open("/path", true);                // Opens a file in read-write mode
/// f.Close();                            // Closes a file
/// var lwrPath = f.Path;                 // Path property has the current path
/// var val = f.Read();                   // Reads the whole file as a string
/// var val = parseInt(f.Read());         // Reads the whole file as a number
/// f.Write(value);                       // Writes a value into the file
/// f.Wait(1000);                         // Waits for 1000ms or until the file changes
///
/// All functions except Read() return an RC, so they can be wrapped with CHECK_RC.
///
static JSBool C_InteractiveFile_constructor
(
    JSContext* cx,
    JSObject*  obj,
    uintN      argc,
    jsval*     argv,
    jsval*     rval
)
{
    const char usage[] = "Usage: InteractiveFile([path[, writable]])";

    if (argc > 2)
    {
        JS_ReportError(cx, usage);
        return JS_FALSE;
    }

    unique_ptr<Xp::InteractiveFile> pIFile(new Xp::InteractiveFile);

    if (argc > 0)
    {
        JavaScriptPtr pJs;

        string path;
        if (OK != pJs->FromJsval(argv[0], &path))
        {
            JS_ReportError(cx, usage);
            return JS_FALSE;
        }

        Xp::InteractiveFile::OpenFlags flags = Xp::InteractiveFile::ReadOnly;
        if (argc > 1)
        {
            bool writable = false;
            if (OK != pJs->FromJsval(argv[1], &writable))
            {
                JS_ReportError(cx, usage);
                return JS_FALSE;
            }
            if (writable)
            {
                flags = Xp::InteractiveFile::ReadWrite;
            }
        }

        if (pIFile->Open(path.c_str(), flags) != OK)
        {
            const string error = "Failed to open " + path;
            JS_ReportError(cx, error.c_str());
            return JS_FALSE;
        }
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, obj, "Help", &C_Global_Help, 1, 0);

    // Tie the InteractiveFile wrapper to the new object
    return JS_SetPrivate(cx, obj, pIFile.release());
};

static void C_InteractiveFile_finalize
(
    JSContext* ctx,
    JSObject*  obj
)
{
    MASSERT(ctx != 0);
    MASSERT(obj != 0);

    Xp::InteractiveFile* const pIFile = static_cast<Xp::InteractiveFile*>(
            JS_GetPrivate(ctx, obj));

    if (pIFile)
    {
        delete pIFile;
    }
};

static JSClass InteractiveFile_class =
{
    "InteractiveFile",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_InteractiveFile_finalize
};

static SObject InteractiveFile_Object
(
    "InteractiveFile",
    InteractiveFile_class,
    nullptr,
    0U,
    "InteractiveFile JS Object",
    C_InteractiveFile_constructor
);

P_(InteractiveFile_Get_Path);
static SProperty Path
(
    InteractiveFile_Object,
    "Path",
    0,
    string(),
    InteractiveFile_Get_Path,
    nullptr,
    JSPROP_READONLY,
    "returns path of the lwrrently opened file"
);
P_(InteractiveFile_Get_Path)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJs;
    Xp::InteractiveFile* const pIFile =
        JS_GET_PRIVATE(Xp::InteractiveFile, pContext, pObject, "InteractiveFile");
    if (pIFile != nullptr)
    {
        if (pJs->ToJsval(pIFile->GetPath(), pValue) == OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

JS_STEST_LWSTOM(InteractiveFile,
                Open,
                2,
                "Open a file.")
{
    STEST_HEADER(1, 2, "Usage: Open(path[, writable])");
    STEST_PRIVATE(Xp::InteractiveFile, pIFile, "InteractiveFile");
    STEST_ARG(0, string, path);
    STEST_OPTIONAL_ARG(1, bool, writable, false);
    const Xp::InteractiveFile::OpenFlags flags =
        writable ? Xp::InteractiveFile::ReadWrite : Xp::InteractiveFile::ReadOnly;
    RETURN_RC(pIFile->Open(path.c_str(), flags));
}

JS_STEST_LWSTOM(InteractiveFile,
                OpenAppend,
                1,
                "Open a file for appending.")
{
    STEST_HEADER(1, 1, "Usage: OpenAppend(path)");
    STEST_PRIVATE(Xp::InteractiveFile, pIFile, "InteractiveFile");
    STEST_ARG(0, string, path);
    RETURN_RC(pIFile->Open(path.c_str(), Xp::InteractiveFile::Append));
}

JS_STEST_LWSTOM(InteractiveFile,
                Create,
                1,
                "Create or truncate a file.")
{
    STEST_HEADER(1, 1, "Usage: Create(path)");
    STEST_PRIVATE(Xp::InteractiveFile, pIFile, "InteractiveFile");
    STEST_ARG(0, string, path);
    RETURN_RC(pIFile->Open(path.c_str(), Xp::InteractiveFile::Create));
}

JS_STEST_LWSTOM(InteractiveFile,
                Close,
                0,
                "Close a file.")
{
    STEST_HEADER(0, 0, "Usage: Close()");
    STEST_PRIVATE(Xp::InteractiveFile, pIFile, "InteractiveFile");
    pIFile->Close();
    RETURN_RC(OK);
}

JS_STEST_LWSTOM(InteractiveFile,
                Wait,
                1,
                "Waits for the file to change.")
{
    STEST_HEADER(1, 1, "Usage: Wait(timeoutMs)");
    STEST_ARG(0, FLOAT64, timeoutMs);
    STEST_PRIVATE(Xp::InteractiveFile, pIFile, "InteractiveFile");
    RETURN_RC(pIFile->Wait(timeoutMs));
}

JS_SMETHOD_LWSTOM(InteractiveFile,
                  Read,
                  0,
                  "Reads contents of the file.")
{
    STEST_HEADER(0, 0, "Usage: Read()");
    STEST_PRIVATE(Xp::InteractiveFile, pIFile, "InteractiveFile");
    string str;
    RC rc = pIFile->Read(&str);
    if (rc != OK)
    {
        // Replace with a future macro, e.g. RETURN_EXC_RC
        JS_SetPendingException(pContext, INT_TO_JSVAL(rc));
        return JS_FALSE;
    }
    rc = pJavaScript->ToJsval(str, pReturlwalue);
    if (rc != OK)
    {
        Printf(Tee::PriError, "Failed to colwert \"%s\" (len=%u) to JS string."
               "  Value was read from %s\n", str.c_str(), static_cast<unsigned>(str.size()),
               pIFile->GetPath().c_str());

        // Replace with a future macro, e.g. RETURN_EXC_RC
        JS_SetPendingException(pContext, INT_TO_JSVAL(rc));
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(InteractiveFile,
                  ReadBinary,
                  0,
                  "Reads contents of the file as binary.")
{
    STEST_HEADER(0, 0, "Usage: ReadBinary()");
    STEST_PRIVATE(Xp::InteractiveFile, pIFile, "InteractiveFile");
    vector<UINT08> data;
    RC rc = pIFile->ReadBinary(&data);
    if (rc != RC::OK)
    {
        pJavaScript->Throw(pContext, rc, Utility::StrPrintf(
                    "Could not read binary data from %s\n",
                    pIFile->GetPath().c_str()));
        return JS_FALSE;
    }
    rc = pJavaScript->ToJsval(data, pReturlwalue);
    if (rc != RC::OK)
    {
        pJavaScript->Throw(pContext, rc, Utility::StrPrintf(
                    "Failed to colwert %u bytes to JS array."
                    "  Value was read from %s\n",
                    static_cast<unsigned>(data.size()),
                    pIFile->GetPath().c_str()));
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_STEST_LWSTOM(InteractiveFile,
                Write,
                1,
                "Writes an integer or a string to the file.")
{
    STEST_HEADER(1, 1, "Usage: Write(value)");
    STEST_PRIVATE(Xp::InteractiveFile, pIFile, "InteractiveFile");
    RC rc;
    if (JS_TypeOfValue(pContext, pArguments[0]) == JSTYPE_NUMBER)
    {
        INT32 value = 0;
        C_CHECK_RC(pJavaScript->FromJsval(pArguments[0], &value));
        C_CHECK_RC(pIFile->Write(static_cast<int>(value)));
    }
    else
    {
        string value;
        C_CHECK_RC(pJavaScript->FromJsval(pArguments[0], &value));
        C_CHECK_RC(pIFile->Write(value));
    }
    RETURN_RC(OK);
}
