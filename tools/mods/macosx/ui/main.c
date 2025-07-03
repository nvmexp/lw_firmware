/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/* main.c */

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include "main.h"

static bool
pathForTool(CFStringRef toolName, char path[MAXPATHLEN])
{
    CFBundleRef bundle;
    CFURLRef resources;
    CFURLRef toolURL;
    Boolean success = true;

    bundle = CFBundleGetMainBundle();
    if (!bundle)
        return FALSE;

    resources = CFBundleCopyResourcesDirectoryURL(bundle);
    if (!resources)
        return FALSE;

    toolURL = CFURLCreateCopyAppendingPathComponent(NULL, resources, toolName, FALSE);
    if (!toolURL)
        return FALSE;

    success = CFURLGetFileSystemRepresentation(resources, TRUE, (UInt8 *)path, MAXPATHLEN);

    CFRelease(resources);
    CFRelease(toolURL);
    return 0;
}

static OSStatus LowRunAppleScript(const void* text, long textLength,
                                    AEDesc *resultData) {
    ComponentInstance theComponent;
    AEDesc scriptTextDesc;
    OSStatus err;
    OSAID scriptID, resultID;

        /* set up locals to a known state */
    theComponent = NULL;
    AECreateDesc(typeNull, NULL, 0, &scriptTextDesc);
    scriptID = kOSANullScript;
    resultID = kOSANullScript;

        /* open the scripting component */
    theComponent = OpenDefaultComponent(kOSAComponentType,
                    typeAppleScript);
    if (theComponent == NULL) { err = paramErr; goto bail; }

        /* put the script text into an aedesc */
    err = AECreateDesc(typeChar, text, textLength, &scriptTextDesc);
    if (err != noErr) goto bail;

        /* compile the script */
    err = OSACompile(theComponent, &scriptTextDesc,
                    kOSAModeNull, &scriptID);
    if (err != noErr) goto bail;

        /* run the script */
    err = OSAExelwte(theComponent, scriptID, kOSANullScript,
                    kOSAModeNull, &resultID);

        /* collect the results - if any */
    if (resultData != NULL) {
        AECreateDesc(typeNull, NULL, 0, resultData);
        if (err == errOSAScriptError) {
            OSAScriptError(theComponent, kOSAErrorMessage,
                        typeChar, resultData);
        } else if (err == noErr && resultID != kOSANullScript) {
            OSADisplay(theComponent, resultID, typeChar,
                        kOSAModeNull, resultData);
        }
    }
bail:
    AEDisposeDesc(&scriptTextDesc);
    if (scriptID != kOSANullScript) OSADispose(theComponent, scriptID);
    if (resultID != kOSANullScript) OSADispose(theComponent, resultID);
    if (theComponent != NULL) CloseComponent(theComponent);
    return err;
}

    /* SimpleRunAppleScript compiles and runs the AppleScript in
    the c-style string provided as a parameter.  The result returned
    indicates the success of the operation. */
static OSStatus SimpleRunAppleScript(const char* theScript) {
    return LowRunAppleScript(theScript, strlen(theScript), NULL);
}

int main(int argc, char *argv[])
{
    char lwpath[MAXPATHLEN];
    pathForTool(CFSTR("Resources"), lwpath);
    char cmd[256] = "ignoring application responses\n";
    strcat(cmd, "       tell application \"Terminal\"\n");
    strcat(cmd, "               do script with command \"cd \\\"");
    strcat(cmd, lwpath);
    strcat(cmd, "\\\"; ./mods.sh\"\n");
    strcat(cmd, "       end tell\n");
    strcat(cmd, "end ignoring\n");
    SimpleRunAppleScript(cmd);
    IFDEBUG(fprintf(stderr, "MODS Tool Command is\n%s\n", cmd);)

    return 0;
}

