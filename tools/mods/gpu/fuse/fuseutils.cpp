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

#include "gpu/fuse/fuseutils.h"
#include "core/include/utility.h"

#include "core/include/cnslmgr.h"

#include <map>

namespace FuseUtils
{
    Tee::Priority d_VerbosePri = Tee::PriLow;

    string d_FuseKey = "";
    UINT32 d_TimeoutMs = 1000;
    bool   d_UseDummyFuses = false;
    string d_FuseFile = "";
    bool   d_IgnoreAte = false;
    bool   d_ShowFusingPrompt = false;
    map<Device::LwDeviceId, string> fuseFilenames =
    {
#if LWCFG(GLOBAL_ARCH_MAXWELL)
        { Device::LwDeviceId::GM107,  "gm107_f.xml"  },
        { Device::LwDeviceId::GM108,  "gm108_f.xml"  },
        { Device::LwDeviceId::GM200,  "gm200_f.json"  },
        { Device::LwDeviceId::GM204,  "gm204_f.json"  },
        { Device::LwDeviceId::GM206,  "gm206_f.json"  },
#endif
#if LWCFG(GLOBAL_ARCH_PASCAL)
        { Device::LwDeviceId::GP100,  "gp100_f.json"  },
        { Device::LwDeviceId::GP102,  "gp102_f.json"  },
        { Device::LwDeviceId::GP104,  "gp104_f.json"  },
        { Device::LwDeviceId::GP106,  "gp106_f.json"  },
        { Device::LwDeviceId::GP107,  "gp107_f.json"  },
        { Device::LwDeviceId::GP108,  "gp108_f.json"  },
#endif
#if LWCFG(GLOBAL_ARCH_VOLTA)
        { Device::LwDeviceId::GV100,  "gv100_f.json"  },
#endif
#if LWCFG(GLOBAL_LWSWITCH_IMPL_SVNP01)
        { Device::LwDeviceId::SVNP01, "svnp01_f.json" },
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU102)
        { Device::LwDeviceId::TU102,  "tu102_f.json"  },
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU104)
        { Device::LwDeviceId::TU104,  "tu104_f.json"  },
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU106)
        { Device::LwDeviceId::TU106,  "tu106_f.json"  },
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU116)
        { Device::LwDeviceId::TU116,  "tu116_f.json"  },
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU117)
        { Device::LwDeviceId::TU117,  "tu117_f.json"  },
#endif

#if LWCFG(GLOBAL_GPU_IMPL_GA100)
        { Device::LwDeviceId::GA100,  "ga100_f.json"  },
#endif
#if LWCFG(GLOBAL_LWSWITCH_IMPL_LR10)
        { Device::LwDeviceId::LR10,  "lr10_f.json"  },
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA102)
        { Device::LwDeviceId::GA102,  "ga102_f.json"  },
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA103)
        { Device::LwDeviceId::GA103,  "ga103_f.json"  },
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA104)
        { Device::LwDeviceId::GA104,  "ga104_f.json"  },
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA106)
        { Device::LwDeviceId::GA106,  "ga106_f.json"  },
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA107)
        { Device::LwDeviceId::GA107,  "ga107_f.json"  },
#endif

// TODO : Update name once fuse file created
#if LWCFG(GLOBAL_GPU_IMPL_AD102)
        { Device::LwDeviceId::AD102,  "ga102_f.json"  },  
#endif
#if LWCFG(GLOBAL_GPU_IMPL_AD103)
        { Device::LwDeviceId::AD103,  "ga102_f.json"  },  
#endif
#if LWCFG(GLOBAL_GPU_IMPL_AD104)
        { Device::LwDeviceId::AD104,  "ga102_f.json"  },  
#endif
#if LWCFG(GLOBAL_GPU_IMPL_AD106)
        { Device::LwDeviceId::AD106,  "ga102_f.json"  },  
#endif
#if LWCFG(GLOBAL_GPU_IMPL_AD107)
        { Device::LwDeviceId::AD107,  "ga102_f.json"  },  
#endif

#if LWCFG(GLOBAL_GPU_IMPL_GH100)
        { Device::LwDeviceId::GH100,  "gh100_f.json"  },
#endif
#if LWCFG(GLOBAL_LWSWITCH_IMPL_LS10)
        { Device::LwDeviceId::LS10,   "ls10_f.json"  },
#endif
    };

    string GetFuseFilename(Device::LwDeviceId devId)
    {
        string fileName = fuseFilenames[devId];
        if (d_FuseFile != "")
        {
            // if the fuse file is specified via cmd line use that instead
            fileName = d_FuseFile;
        }

        if (fileName == "")
        {
            return "unknown";
        }
        return fileName;
    }

    void SetTimeoutMs(UINT32 newTimeout)
    {
        d_TimeoutMs = newTimeout;
    }

    UINT32 GetTimeoutMs()
    {
        return d_TimeoutMs;
    }

    void SetL2FuseKey(const string& newFuseKey)
    {
        d_FuseKey = newFuseKey;
    }

    RC ReadL2FuseKeyFromFile(string* pL2Key)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    void SetVerbosePriority(Tee::Priority pri)
    {
        d_VerbosePri = pri;
    }

    Tee::Priority GetVerbosePriority()
    {
        if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        {
            return Tee::PriNone;
        }
        return d_VerbosePri;
    }

    Tee::Priority GetPrintPriority()
    {
        if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        {
            return Tee::PriNone;
        }
        return Tee::PriNormal;
    }

    Tee::Priority GetWarnPriority()
    {
        if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        {
            return Tee::PriNone;
        }
        return Tee::PriWarn;
    }

    Tee::Priority GetErrorPriority()
    {
        if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        {
            return Tee::PriNone;
        }
        return Tee::PriError;
    }

    void SetUseDummyFuses(bool useDummyFuses)
    {
        d_UseDummyFuses = useDummyFuses;
    }

    bool GetUseDummyFuses()
    {
        return d_UseDummyFuses;
    }
    
    void SetShowFusingPrompt(bool showFusingPrompt)
    {
        d_ShowFusingPrompt = showFusingPrompt;
    }
    
    bool GetShowFusingPrompt()
    {
        return d_ShowFusingPrompt;
    }

    RC EncryptFuseFile(string filename, string keyname, bool isFuseKey)
    {
        // TODO: port this implementation from GM10xFuse
        return RC::UNSUPPORTED_FUNCTION;
    }

    void  SetFuseFile(string fileName)
    {
        d_FuseFile = fileName;
    }

    void SetIgnoreAte(bool bIgnoreAte)
    {
        d_IgnoreAte = bIgnoreAte;
    }

    bool GetIgnoreAte()
    {
        return d_IgnoreAte;
    }
}

RC FusePromptBase::SetPromptMsg(string msg)
{
    m_PromptMsg = msg;
    return RC::OK;
}

string FusePromptBase::GetPromptMsg()
{
    return m_PromptMsg;
}

FusePromptBase::FusePromptCharacters FusePromptBase::GetFusePromptCharacters()
{
    FusePromptCharacters defaultPromptChars;
    return defaultPromptChars;
}

RC FusePromptBase::PromptUser()
{
    RC rc;

    FusePromptCharacters fusePromptCharacters = GetFusePromptCharacters();
    
    ConsoleManager::ConsoleContext consoleCtx;
    Console *pRealConsole = consoleCtx.AcquireRealConsole(true);
    
    UINT32 timeSpent = 0;
    Printf(Tee::PriAlways, "%s\n", GetPromptMsg().c_str());
    while (true)
    {
        if (pRealConsole->KeyboardHit())
        {
            char ch = pRealConsole->GetKey();
            ch = tolower(ch);
            if (ch == fusePromptCharacters.yesChar)
            {
                 break;
            }
            else if (ch == fusePromptCharacters.noChar)
            {
                 return RC::EXIT_OK;
            }
        }
        Tasker::Sleep(PER_LOOP_SLEEP_TIME_MS);
        timeSpent += PER_LOOP_SLEEP_TIME_MS;
        if (m_PromptTimeout && timeSpent > m_PromptTimeout)
        {
            if (!m_AssumeSuccessOnPromptTimeout)
            {
                Printf(Tee::PriError, "Prompt timeout expired.\n");
                return RC::TIMEOUT_ERROR;
            }
            break;
        }
     }
     return rc;
}
