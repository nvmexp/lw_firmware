/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_SIGNATURE_H
#define INCLUDED_SIGNATURE_H

#include "lwSha256.h"

namespace Signature
{
    enum SignMethod
    {
        METHOD_HMAC_SHA256,
    };

    struct Signature
    {
        const char *KeyName;
        SignMethod Method;
        const char *Prefix;
    };

    static const Signature LWRRENT_SIGNATURE =
    {
        "M1V1",
        METHOD_HMAC_SHA256,
        "JbnOFsdfoiMkfsdfsdkmlsdfmasduaskldfma"
    };

    static const char VERSION_STRING[] = "Version: ";
    // First 7 characters must be the same here for a faster processing:
    static const char SIGNATURE_STRING[] = "_______ Signature: ";

    static const char HEX2ASCII[] = "0123456789ABCDEF";

#ifndef LW_MODS

    static const Signature ALL_SIGNATURES[] =
    {
        LWRRENT_SIGNATURE,
    };
    const unsigned int NUM_SIGNATURES = sizeof(ALL_SIGNATURES)/sizeof(ALL_SIGNATURES[0]);

#endif

    struct HmacSha256State
    {
        lw_sha256_ctx ctx;
        LwU8 opad[LW_SHA256_BLOCK_SIZE];
    };

    void HmacSha256Intro
    (
        HmacSha256State *state,
        const char *key
    );

    void HmacSha256Outro
    (
        HmacSha256State *state,
        LwU8 digest[LW_SHA256_DIGEST_SIZE]
    );
}

#endif
