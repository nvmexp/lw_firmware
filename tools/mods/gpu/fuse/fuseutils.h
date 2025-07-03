/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/device.h"
#include "core/include/utility.h"
#include "core/include/cnslmgr.h"

namespace FuseUtils
{
    // Timeout for fusing operations
    void SetTimeoutMs(UINT32 newTimeout);
    UINT32 GetTimeoutMs();

    // The L2 fuse key to use when decrypting secure uCode packages
    // L2 key encrypts the binary
    // L1 key encrypts the L2 key
    // The daughter card/file hold the encrypted L2 key
    // Note: if this is empty, MODS will assume any package
    //       it tries to read is not encrypted
    void SetL2FuseKey(const string& l2FuseKey);
    RC ReadL2FuseKeyFromFile(string* pL2Key);

    // Print priority for informational prints
    // (or PriNone if the SelwrityUnlockLevel is too low)
    Tee::Priority GetPrintPriority();
    Tee::Priority GetVerbosePriority();
    void SetVerbosePriority(Tee::Priority pri);

    // Print priority for warning and error prints
    // (or PriNone if the SelwrityUnlockLevel is too low)
    Tee::Priority GetWarnPriority();
    Tee::Priority GetErrorPriority();

    // Use SW array instead of physical fuse macro (for testing)
    void SetUseDummyFuses(bool useDummyFuses);
    bool GetUseDummyFuses();

    void SetShowFusingPrompt(bool showFusingPrompt);
    bool GetShowFusingPrompt();

    // Returns the name of the base fuse file for a particular chip (e.g. gv100_f.json)
    string GetFuseFilename(Device::LwDeviceId devId);

    RC EncryptFuseFile(string filename, string keyname, bool isFuseKey);
    void SetFuseFile(string fileName);

    // Ignore checks against ate fuses
    void SetIgnoreAte(bool ignoreAte);
    bool GetIgnoreAte();
};

class FusePromptBase
{
public:
    struct FusePromptCharacters
    {
        char yesChar;
        char noChar;
        FusePromptCharacters()
        :yesChar('y')
        ,noChar('n')
        {};
    };
    FusePromptBase() = default;
    ~FusePromptBase() = default;
     
    FusePromptCharacters GetFusePromptCharacters();
    RC PromptUser();
    RC SetPromptMsg(string msg);

private:
    string m_PromptMsg;

    static constexpr UINT32 PER_LOOP_SLEEP_TIME_MS = 500;
    static constexpr UINT32 MAX_WAIT_TIME_MS = 15 * 1000;

    UINT32 m_PromptTimeout = MAX_WAIT_TIME_MS;
    bool m_AssumeSuccessOnPromptTimeout = false;

    string GetPromptMsg();
};
