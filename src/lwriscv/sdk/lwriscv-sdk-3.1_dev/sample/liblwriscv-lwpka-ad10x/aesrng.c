/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <liblwriscv/libc.h>
#include <liblwriscv/print.h>

#include "tegra_se.h"

#include "aesrng.h"

/*!
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 * This code is for testing and libCCC usage purposes only!
 * Do not replicate blindly for production usecases. Consult with your local crypto expert!
 * 
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 * */

#define RN_SIZE_BYTES 4

void verifyAesGenrndRng(void)
{
    status_t                           ret                  = 0;
    engine_t                           *engine              = NULL;
    struct se_data_params              input_params         = {0};
    struct se_engine_rng_context       econtext             = {0};
    uint8_t                            dst[RN_SIZE_BYTES]   = {0};

    printf ("[START] Select engine\n");
    ret = ccc_select_engine((const struct engine_s **)&engine, ENGINE_CLASS_AES, ENGINE_SE0_AES1);
    printf ("[END] Select engine = 0x%x\n", ret);

    econtext.engine             = engine;
    input_params.dst            = (uint8_t *)dst;
    input_params.output_size    = RN_SIZE_BYTES;

    printf ("[START] Generate random numbers\n");

    ret = engine_genrnd_generate_locked(&input_params, &econtext);
    printf ("Retcode = %x, Random number = %02x %02x %02x %02x\n", ret, dst[0], dst[1], dst[2], dst[3]);
    
    ret = engine_genrnd_generate_locked(&input_params, &econtext);
    printf ("Retcode = %x, Random number = %02x %02x %02x %02x\n", ret, dst[0], dst[1], dst[2], dst[3]);
    
    ret = engine_genrnd_generate_locked(&input_params, &econtext);
    printf ("Retcode = %x, Random number = %02x %02x %02x %02x\n", ret, dst[0], dst[1], dst[2], dst[3]);
    
    ret = engine_genrnd_generate_locked(&input_params, &econtext);
    printf ("Retcode = %x, Random number = %02x %02x %02x %02x\n", ret, dst[0], dst[1], dst[2], dst[3]);
    
    ret = engine_genrnd_generate_locked(&input_params, &econtext);
    printf ("Retcode = %x, Random number = %02x %02x %02x %02x\n", ret, dst[0], dst[1], dst[2], dst[3]);

    printf ("[END] Generate random numbers\n");
}

