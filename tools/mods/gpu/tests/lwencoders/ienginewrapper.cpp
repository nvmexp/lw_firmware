/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2018 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <memory.h>

#include "lwmisc.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "rm.h"
#include "core/include/tasker.h"
#include "core/include/types.h"

#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl2080/ctrl2080cipher.h"

#include "ctr64encryptor.h"

#include "ienginewrapper.h"

struct SessionKeyEstablishedParams
{
    SessionKeyEstablishedParams(GpuSubdevice *subDev)
      : gpuSubdev(subDev)
      , result(OK)
    {}

    GpuSubdevice *gpuSubdev;
    RC            result;
};

static bool IsSessionKeyEstablished(void *args)
{
    SessionKeyEstablishedParams *params = static_cast<SessionKeyEstablishedParams *>(args);

    LW2080_CTRL_CMD_CIPHER_SESSION_KEY_STATUS_PARAMS keyExchStatus;
    memset(&keyExchStatus, 0, sizeof(keyExchStatus));

    params->result = LwRmPtr()->ControlBySubdevice(
        params->gpuSubdev,
        LW2080_CTRL_CMD_CIPHER_SESSION_KEY_STATUS,
        &keyExchStatus,
        sizeof(keyExchStatus)
    );

    if (0 != params->result)
    {
        return true;
    }

    return 0 != (keyExchStatus.status &
                 DRF_DEF(2080_CTRL_CMD_CIPHER_SESSION_KEY, _STATUS, _ESTABLISHED, _TRUE));
}

RC LWDEC::Engine::DoSetupContentKey(GpuSubdevice *subDev, FLOAT64 skTimeout)
{
    RC rc;

    UINT32 wrappedSessionKey[AES::DW] = {};
    UINT32 wrappedContentKey[AES::DW] = {};
    UINT08 keyIncrement = GetKeyIncrement();

    if (Platform::Fmodel == Platform::GetSimulationMode())
    {
        // the magic numbers are taken from //hw/gpu_ip/unit/msdec/5.0_merge/cmod/encryptor/src/encryptor.cpp
        static const UINT32 ks[] = { 0xc0cccc01, 0x947c41b1, 0xd594814f, 0x82f2974b };
        static const UINT32 sk[] = { 0x03020100, 0x07060504, 0x0b0a0908, 0x0f0e0d0c };
        static const UINT32 ck[] = { 0x13121110, 0x17161514, 0x1b1a1918, 0x1f1e1d1c };
        static const UINT32 iv[] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };

        SetContentKey(ck);
        SetInitVector(iv);

        UINT32 rk[AES::RK];
        AES::CalcRoundKey(ks, rk);
        AES::RijndaelEncrypt(rk, sk, wrappedSessionKey);

        UINT32 decrementedContentKey[AES::DW];
        memcpy(decrementedContentKey, ck, sizeof(decrementedContentKey));

        // 128-bit decrement
        UINT08 *p = reinterpret_cast<UINT08*>(&decrementedContentKey[0]);
        reverse(p, p + sizeof(decrementedContentKey));
        UINT64 *msWord = reinterpret_cast<UINT64 *>(p + 8);
        UINT64 *lsWord = reinterpret_cast<UINT64 *>(p);
        if (*lsWord < keyIncrement)
        {
            *msWord -= 1;
        }
        *lsWord -= keyIncrement;
        reverse(p, p + sizeof(decrementedContentKey));

        AES::CalcRoundKey(sk, rk);
        AES::RijndaelEncrypt(rk, decrementedContentKey, wrappedContentKey);

        SetWrappedSessionKey(wrappedSessionKey);
        SetWrappedContentKey(wrappedContentKey);
    }
    else
    {
        // wait until a session key is established
        SessionKeyEstablishedParams skEstParams(subDev);
        POLLWRAP_HW(IsSessionKeyEstablished, &skEstParams, skTimeout);
        CHECK_RC(skEstParams.result);

        // get the session key
        LW2080_CTRL_CMD_CIPHER_SESSION_KEY_PARAMS sessionKeyParams;
        memset(&sessionKeyParams, 0, sizeof(sessionKeyParams));
        CHECK_RC(LwRmPtr()->ControlBySubdevice(
            subDev,
            LW2080_CTRL_CMD_CIPHER_SESSION_KEY,
            &sessionKeyParams,
            sizeof(sessionKeyParams)
        ));
        memcpy(&wrappedSessionKey[0], sessionKeyParams.sKey, 16);

        // encrypt the content key
        LW2080_CTRL_CMD_CIPHER_AES_ENCRYPT_PARAMS aesEncryptParams;
        memset(&aesEncryptParams, 0, sizeof(aesEncryptParams));
        memcpy(aesEncryptParams.pt, GetContentKey(), 16);

        // 128-bit decrement
        reverse(&aesEncryptParams.pt[0], &aesEncryptParams.pt[0] + NUMELEMS(aesEncryptParams.pt));
        UINT64 *msWord = reinterpret_cast<UINT64 *>(&aesEncryptParams.pt[0] + 8);
        UINT64 *lsWord = reinterpret_cast<UINT64 *>(&aesEncryptParams.pt[0]);
        if (*lsWord < keyIncrement)
        {
            *msWord -= 1;
        }
        *lsWord -= keyIncrement;
        reverse(&aesEncryptParams.pt[0], &aesEncryptParams.pt[0] + NUMELEMS(aesEncryptParams.pt));

        CHECK_RC(LwRmPtr()->ControlBySubdevice(
            subDev,
            LW2080_CTRL_CMD_CIPHER_AES_ENCRYPT,
            &aesEncryptParams,
            sizeof(aesEncryptParams)
        ));
        memcpy(wrappedContentKey, aesEncryptParams.ct, 16);

        SetWrappedSessionKey(wrappedSessionKey);
        SetWrappedContentKey(wrappedContentKey);
    }

    return rc;
}
