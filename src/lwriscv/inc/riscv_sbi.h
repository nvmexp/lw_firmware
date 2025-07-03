/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifdef LWRISCV_SDK
#include <lwriscv/sbi.h>
#else /* LWRISCV_SDK */

#ifndef RISCV_SBI_H_
#define RISCV_SBI_H_

#include <lwctassert.h>

//! @brief Standardized SBI error codes.
typedef enum _SBI_ERROR_CODE
{
    SBI_SUCCESS             = 0,
    SBI_ERR_FAILURE         = -1,
    SBI_ERR_NOT_SUPPORTED   = -2,
    SBI_ERR_ILWALID_PARAM   = -3,
    SBI_ERR_DENIED          = -4,
    SBI_ERR_ILWALID_ADDRESS = -5,

    SBI_ERROR_CODE__PADDING = -0x7FFFFFFFFFFFFFFFLL, // force to be signed 64bit type
} SBI_ERROR_CODE;

ct_assert(sizeof(SBI_ERROR_CODE) == sizeof(LwS64));

//! @brief SBI extension IDs.
typedef enum _SBI_EXTENSION_ID
{
    /*! @brief Legacy set_timer extension
     *
     * void sbi_set_timer(uint64_t stime_value);
     *
     * @param[in]   stime_value     New mtimecmp value
     */
    SBI_EXTENSION_SET_TIMER = 0x00,

    /*! @brief Legacy shutdown extension
     *
     * NORETURN void sbi_shutdown(void);
     */
    SBI_EXTENSION_SHUTDOWN = 0x08,

    /*! @brief LWPU-specified extension.
     *
     * Function IDs are of the enum type SBI_LWIDIA_FUNCTION_ID.
     */
    SBI_EXTENSION_LWIDIA = 0x090001EB,
} SBI_EXTENSION_ID;

//! @brief LWPU-specific SBI functions.
typedef enum _SBI_LWIDIA_FUNCTION_ID
{
    SBI_LWFUNC_FIRST                 = 0,
    SBI_LWFUNC_PARTITION_SWITCH      = 0,
    SBI_LWFUNC_RELEASE_PRIV_LOCKDOWN = 1,
    SBI_LWFUNC_TRACECTL_SET          = 2,
    SBI_LWFUNC_FBIF_TRANSCFG_SET     = 3,
    SBI_LWFUNC_FBIF_REGIONCFG_SET    = 4,
    SBI_LWFUNC_TFBIF_TRANSCFG_SET    = 5,
    SBI_LWFUNC_TFBIF_REGIONCFG_SET   = 6,
} SBI_LWIDIA_FUNCTION_ID;

//! @brief SBI return value structure.
typedef struct _SBI_RETURN_VALUE
{
    //! @brief Error code.
    SBI_ERROR_CODE error;
    //! @brief Return value from the SBI call (or 0 if a void type SBI call).
    LwS64 value;
} SBI_RETURN_VALUE;

//-------------------------------------------------------------------------------------------------
#define SBICALL __attribute__( ( always_inline ) ) static inline
#define CLOBBER_SBICALL "s0", "s1", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory"
/* For some reason marking TP and GP as clobbers doesn't actually work, so we manually save them */
#define SBICALL_BODY \
  "mv s0, gp\n" \
  "mv s1, tp\n" \
  "ecall\n" \
  "mv tp, s1\n" \
  "mv gp, s0\n" \

SBICALL SBI_RETURN_VALUE sbicall0( LwS32 extension, LwS32 function )
{
    register LwS32 a7 __asm__( "a7" ) = extension;
    register LwS32 a6 __asm__( "a6" ) = function;
    register LwU64 a1 __asm__( "a1" );
    register LwU64 a0 __asm__( "a0" );
    __asm__ volatile (SBICALL_BODY
                  : "+r"( a0 ), "+r"( a1 ), "+r"( a6 ), "+r"( a7 )
                  :
                  : "ra", CLOBBER_SBICALL, "a2", "a3", "a4", "a5");
    return (SBI_RETURN_VALUE) { .error = a0, .value = (LwS64)a1 };
}

SBICALL SBI_RETURN_VALUE sbicall1( LwS32 extension, LwS32 function, LwU64 arg0 )
{
    register LwS32 a7 __asm__( "a7" ) = extension;
    register LwS32 a6 __asm__( "a6" ) = function;
    register LwU64 a1 __asm__( "a1" );
    register LwU64 a0 __asm__( "a0" ) = arg0;
    __asm__ volatile (SBICALL_BODY
                  : "+r"( a0 ), "+r"( a1 ), "+r"( a6 ), "+r"( a7 )
                  :
                  : "ra", CLOBBER_SBICALL, "a2", "a3", "a4", "a5");
    return (SBI_RETURN_VALUE) { .error = a0, .value = (LwS64)a1 };
}

SBICALL SBI_RETURN_VALUE sbicall2( LwS32 extension, LwS32 function, LwU64 arg0, LwU64 arg1 )
{
    register LwS32 a7 __asm__( "a7" ) = extension;
    register LwS32 a6 __asm__( "a6" ) = function;
    register LwU64 a1 __asm__( "a1" ) = arg1;
    register LwU64 a0 __asm__( "a0" ) = arg0;
    __asm__ volatile (SBICALL_BODY
                  : "+r"( a0 ), "+r"( a1 ), "+r"( a6 ), "+r"( a7 )
                  :
                  : "ra", CLOBBER_SBICALL, "a2", "a3", "a4", "a5");
    return (SBI_RETURN_VALUE) { .error = a0, .value = (LwS64)a1 };
}

SBICALL SBI_RETURN_VALUE sbicall3( LwS32 extension, LwS32 function, LwU64 arg0, LwU64 arg1, LwU64 arg2 )
{
    register LwS32 a7 __asm__( "a7" ) = extension;
    register LwS32 a6 __asm__( "a6" ) = function;
    register LwU64 a2 __asm__( "a2" ) = arg2;
    register LwU64 a1 __asm__( "a1" ) = arg1;
    register LwU64 a0 __asm__( "a0" ) = arg0;
    __asm__ volatile (SBICALL_BODY
                  : "+r"( a0 ), "+r"( a1 ), "+r"( a2 ), "+r"( a6 ), "+r"( a7 )
                  :
                  : "ra", CLOBBER_SBICALL, "a3", "a4", "a5");
    return (SBI_RETURN_VALUE) { .error = a0, .value = (LwS64)a1 };
}

SBICALL SBI_RETURN_VALUE sbicall4( LwS32 extension, LwS32 function, LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3 )
{
    register LwS32 a7 __asm__( "a7" ) = extension;
    register LwS32 a6 __asm__( "a6" ) = function;
    register LwU64 a3 __asm__( "a3" ) = arg3;
    register LwU64 a2 __asm__( "a2" ) = arg2;
    register LwU64 a1 __asm__( "a1" ) = arg1;
    register LwU64 a0 __asm__( "a0" ) = arg0;
    __asm__ volatile (SBICALL_BODY
                  : "+r"( a0 ), "+r"( a1 ), "+r"( a2 ), "+r"( a3 ), "+r"( a6 ), "+r"( a7 )
                  :
                  : "ra", CLOBBER_SBICALL, "a4", "a5");
    return (SBI_RETURN_VALUE) { .error = a0, .value = (LwS64)a1 };
}

SBICALL SBI_RETURN_VALUE sbicall5( LwS32 extension, LwS32 function, LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 arg4 )
{
    register LwS32 a7 __asm__( "a7" ) = extension;
    register LwS32 a6 __asm__( "a6" ) = function;
    register LwU64 a4 __asm__( "a4" ) = arg4;
    register LwU64 a3 __asm__( "a3" ) = arg3;
    register LwU64 a2 __asm__( "a2" ) = arg2;
    register LwU64 a1 __asm__( "a1" ) = arg1;
    register LwU64 a0 __asm__( "a0" ) = arg0;
    __asm__ volatile (SBICALL_BODY
                  : "+r"( a0 ), "+r"( a1 ), "+r"( a2 ), "+r"( a3 ), "+r"( a4 ), "+r"( a6 ), "+r"( a7 )
                  :
                  : "ra", CLOBBER_SBICALL, "a5");
    return (SBI_RETURN_VALUE) { .error = a0, .value = (LwS64)a1 };
}

SBICALL SBI_RETURN_VALUE sbicall6( LwS32 extension, LwS32 function, LwU64 arg0, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 arg4, LwU64 arg5 )
{
    register LwS32 a7 __asm__( "a7" ) = extension;
    register LwS32 a6 __asm__( "a6" ) = function;
    register LwU64 a5 __asm__( "a5" ) = arg5;
    register LwU64 a4 __asm__( "a4" ) = arg4;
    register LwU64 a3 __asm__( "a3" ) = arg3;
    register LwU64 a2 __asm__( "a2" ) = arg2;
    register LwU64 a1 __asm__( "a1" ) = arg1;
    register LwU64 a0 __asm__( "a0" ) = arg0;
    __asm__ volatile (SBICALL_BODY
                  : "+r"( a0 ), "+r"( a1 ), "+r"( a2 ), "+r"( a3 ), "+r"( a4 ), "+r"( a5 ), "+r"( a6 ), "+r"( a7 )
                  :
                  : "ra", CLOBBER_SBICALL);
    return (SBI_RETURN_VALUE) { .error = a0, .value = (LwS64)a1 };
}

#undef SBICALL_BODY
#undef SBICALL
#undef CLOBBER_SBICALL

#endif // RISCV_SBI_H_
#endif /* LWRISCV_SDK */
