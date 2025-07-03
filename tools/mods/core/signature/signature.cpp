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

#include <string.h>
#include "signature.h"

void Signature::HmacSha256Intro
(
    HmacSha256State *state,
    const char *key
)
{
    LwU8 K0[LW_SHA256_BLOCK_SIZE] = {};
    LwU8 ipad[LW_SHA256_BLOCK_SIZE];

    size_t keylen = strlen(key);

    if (keylen <= sizeof(K0))
    {
        memcpy(K0, key, keylen);
    }
    else
    {
        lw_sha256((const LwU8*)key, (LwU32)keylen, K0);
    }

    for (int idx = 0; idx < LW_SHA256_BLOCK_SIZE; idx++)
    {
        ipad[idx]        = K0[idx] ^ 0x36;
        state->opad[idx] = K0[idx] ^ 0x5c;
    }

    lw_sha256_init(&(state->ctx));

    lw_sha256_update(&(state->ctx), ipad, sizeof(ipad));
}

void Signature::HmacSha256Outro
(
    HmacSha256State *state,
    LwU8 digest[LW_SHA256_DIGEST_SIZE]
)
{
    LwU8 digestFirstPass[LW_SHA256_DIGEST_SIZE];

    lw_sha256_final(&(state->ctx), digestFirstPass);

    lw_sha256_ctx secondPassCtx;
    lw_sha256_init(&secondPassCtx);
    lw_sha256_update(&secondPassCtx, state->opad, LW_SHA256_BLOCK_SIZE);
    lw_sha256_update(&secondPassCtx, digestFirstPass, LW_SHA256_DIGEST_SIZE);
    lw_sha256_final(&secondPassCtx, digest);
}
