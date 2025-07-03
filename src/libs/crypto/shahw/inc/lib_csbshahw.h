/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SHAHW_CSB_H
#define SHAHW_CSB_H

/* ------------------------ System Includes -------------------------------- */
/*!
 * @file  shahw_csb.h
 * @brief Maps the falcon registers needed for SHA engine bus read and write
 *        based on which falcon is compiling the sha library with it. The flags
 *        are set in the makefile.
 *
 */
#include "lwuproc.h"
#include "dev_falcon_v4.h"
#include "csb.h"

/* ------------------------ Macros and Defines ----------------------------- */
/*
 * Based on the variables set in the makefile, include the relevant
 * falcon based header file.
 * Also define CSB_REG for each of the falcons.
*/
#ifdef SEC2_RTOS
    #include "dev_sec_csb.h"

    #ifndef CSB_REG
        #define CSB_REG(reg)                   LW_CSEC_FALCON##reg
    #endif

    #ifndef CSB_DEF
        #define CSB_DEF(def)                   LW_CSEC_FALCON##def
    #endif

    #ifndef CSB_DRF_VAL
        #define CSB_DRF_VAL(r,f,v)             DRF_VAL(_CSEC,r,f,v)
    #endif

    #ifndef CSB_DRF_DEF
        #define CSB_DRF_DEF(r,f,c)             DRF_DEF(_CSEC,r,f,c)
    #endif

    #ifndef CSB_DRF_NUM
        #define CSB_DRF_NUM(r,f,n)             DRF_NUM(_CSEC,r,f,n)
    #endif

    #ifndef CSB_FLD_SET_DRF
        #define CSB_FLD_SET_DRF(r,f,c,v)       FLD_SET_DRF(_CSEC,r,f,c,v)
    #endif

    #ifndef CSB_FLD_SET_DRF_NUM
        #define CSB_FLD_SET_DRF_NUM(r,f,c,v)   FLD_SET_DRF_NUM(_CSEC,r,f,c,v)
    #endif

    #ifndef CSB_FLD_TEST_DRF
        #define CSB_FLD_TEST_DRF(r,f,c,v)      FLD_TEST_DRF(_CSEC,r,f,c,v)
    #endif
#elif GSPLITE_RTOS
    #include "dev_gsp_csb.h"

    #ifndef CSB_REG
        #define CSB_REG(reg)                   LW_CGSP_FALCON##reg
    #endif

    #ifndef CSB_DEF
        #define CSB_DEF(def)                   LW_CGSP_FALCON##def
    #endif

    #ifndef CSB_DRF_VAL
        #define CSB_DRF_VAL(r,f,v)             DRF_VAL(_CGSP,r,f,v)
    #endif

    #ifndef CSB_DRF_DEF
        #define CSB_DRF_DEF(r,f,c)             DRF_DEF(_CGSP,r,f,c)
    #endif

    #ifndef CSB_DRF_NUM
        #define CSB_DRF_NUM(r,f,n)             DRF_NUM(_CGSP,r,f,n)
    #endif

    #ifndef CSB_FLD_SET_DRF
        #define CSB_FLD_SET_DRF(r,f,c,v)       FLD_SET_DRF(_CGSP,r,f,c,v)
    #endif

    #ifndef CSB_FLD_SET_DRF_NUM
        #define CSB_FLD_SET_DRF_NUM(r,f,c,v)   FLD_SET_DRF_NUM(_CGSP,r,f,c,v)
    #endif

    #ifndef CSB_FLD_TEST_DRF
        #define CSB_FLD_TEST_DRF(r,f,c,v)      FLD_TEST_DRF(_CGSP,r,f,c,v)
    #endif
#else
    // No others are supported for now.
    ct_assert(0);
#endif

#endif // SHAHW_CSB_H
