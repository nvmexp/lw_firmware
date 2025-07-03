/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWRISCV_SBI_H
#define LWRISCV_SBI_H
#include <stdint.h>
#include <lwriscv/gcc_attrs.h>

//! @brief Standardized SBI error codes.
typedef enum
{
    SBI_SUCCESS             = 0,
    SBI_ERR_FAILURE         = -1,
    SBI_ERR_NOT_SUPPORTED   = -2,
    SBI_ERR_ILWALID_PARAM   = -3,
    SBI_ERR_DENIED          = -4,
    SBI_ERR_ILWALID_ADDRESS = -5,

    SBI_ERROR_CODE__PADDING = -0x7FFFFFFFFFFFFFFFLL, // force to be signed 64bit type
} SBI_ERROR_CODE;

_Static_assert(sizeof(SBI_ERROR_CODE) == sizeof(int64_t), "SBI_ERROR_CODE size must be 8 bytes.");

//! @brief SBI extension IDs.
typedef enum
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
typedef enum
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
typedef struct
{
    //! @brief Error code.
    SBI_ERROR_CODE error;
    //! @brief Return value from the SBI call (or 0 if a void type SBI call).
    int64_t value;
} SBI_RETURN_VALUE;

//-------------------------------------------------------------------------------------------------
#define SBICALL GCC_ATTR_ALWAYSINLINE static inline
#define CLOBBER_SBICALL "s0", "s1", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory"
/* For some reason marking TP and GP as clobbers doesn't actually work, so we manually save them */
#define SBICALL_BODY \
  "mv s0, gp\n" \
  "mv s1, tp\n" \
  "ecall\n" \
  "mv tp, s1\n" \
  "mv gp, s0\n" \

SBICALL SBI_RETURN_VALUE sbicall0( int32_t extension, int32_t function )
{
    register int32_t a7 __asm__( "a7" ) = extension;
    register int32_t a6 __asm__( "a6" ) = function;
    register uint64_t a1 __asm__( "a1" );
    register uint64_t a0 __asm__( "a0" );
    __asm__ volatile (SBICALL_BODY
                  : "+r"( a0 ), "+r"( a1 ), "+r"( a6 ), "+r"( a7 )
                  :
                  : "ra", CLOBBER_SBICALL, "a2", "a3", "a4", "a5");
    return (SBI_RETURN_VALUE) { .error = (SBI_ERROR_CODE) a0, .value = (int64_t)a1 };
}

SBICALL SBI_RETURN_VALUE sbicall1( int32_t extension, int32_t function, uint64_t arg0 )
{
    register int32_t a7 __asm__( "a7" ) = extension;
    register int32_t a6 __asm__( "a6" ) = function;
    register uint64_t a1 __asm__( "a1" );
    register uint64_t a0 __asm__( "a0" ) = arg0;
    __asm__ volatile (SBICALL_BODY
                  : "+r"( a0 ), "+r"( a1 ), "+r"( a6 ), "+r"( a7 )
                  :
                  : "ra", CLOBBER_SBICALL, "a2", "a3", "a4", "a5");
    return (SBI_RETURN_VALUE) { .error = (SBI_ERROR_CODE) a0, .value = (int64_t)a1 };
}

SBICALL SBI_RETURN_VALUE sbicall2( int32_t extension, int32_t function, uint64_t arg0, uint64_t arg1 )
{
    register int32_t a7 __asm__( "a7" ) = extension;
    register int32_t a6 __asm__( "a6" ) = function;
    register uint64_t a1 __asm__( "a1" ) = arg1;
    register uint64_t a0 __asm__( "a0" ) = arg0;
    __asm__ volatile (SBICALL_BODY
                  : "+r"( a0 ), "+r"( a1 ), "+r"( a6 ), "+r"( a7 )
                  :
                  : "ra", CLOBBER_SBICALL, "a2", "a3", "a4", "a5");
    return (SBI_RETURN_VALUE) { .error = (SBI_ERROR_CODE) a0, .value = (int64_t)a1 };
}

SBICALL SBI_RETURN_VALUE sbicall3( int32_t extension, int32_t function, uint64_t arg0, uint64_t arg1, uint64_t arg2 )
{
    register int32_t a7 __asm__( "a7" ) = extension;
    register int32_t a6 __asm__( "a6" ) = function;
    register uint64_t a2 __asm__( "a2" ) = arg2;
    register uint64_t a1 __asm__( "a1" ) = arg1;
    register uint64_t a0 __asm__( "a0" ) = arg0;
    __asm__ volatile (SBICALL_BODY
                  : "+r"( a0 ), "+r"( a1 ), "+r"( a2 ), "+r"( a6 ), "+r"( a7 )
                  :
                  : "ra", CLOBBER_SBICALL, "a3", "a4", "a5");
    return (SBI_RETURN_VALUE) { .error = (SBI_ERROR_CODE) a0, .value = (int64_t)a1 };
}

SBICALL SBI_RETURN_VALUE sbicall4( int32_t extension, int32_t function, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3 )
{
    register int32_t a7 __asm__( "a7" ) = extension;
    register int32_t a6 __asm__( "a6" ) = function;
    register uint64_t a3 __asm__( "a3" ) = arg3;
    register uint64_t a2 __asm__( "a2" ) = arg2;
    register uint64_t a1 __asm__( "a1" ) = arg1;
    register uint64_t a0 __asm__( "a0" ) = arg0;
    __asm__ volatile (SBICALL_BODY
                  : "+r"( a0 ), "+r"( a1 ), "+r"( a2 ), "+r"( a3 ), "+r"( a6 ), "+r"( a7 )
                  :
                  : "ra", CLOBBER_SBICALL, "a4", "a5");
    return (SBI_RETURN_VALUE) { .error = (SBI_ERROR_CODE) a0, .value = (int64_t)a1 };
}

SBICALL SBI_RETURN_VALUE sbicall5( int32_t extension, int32_t function, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4 )
{
    register int32_t a7 __asm__( "a7" ) = extension;
    register int32_t a6 __asm__( "a6" ) = function;
    register uint64_t a4 __asm__( "a4" ) = arg4;
    register uint64_t a3 __asm__( "a3" ) = arg3;
    register uint64_t a2 __asm__( "a2" ) = arg2;
    register uint64_t a1 __asm__( "a1" ) = arg1;
    register uint64_t a0 __asm__( "a0" ) = arg0;
    __asm__ volatile (SBICALL_BODY
                  : "+r"( a0 ), "+r"( a1 ), "+r"( a2 ), "+r"( a3 ), "+r"( a4 ), "+r"( a6 ), "+r"( a7 )
                  :
                  : "ra", CLOBBER_SBICALL, "a5");
    return (SBI_RETURN_VALUE) { .error = (SBI_ERROR_CODE) a0, .value = (int64_t)a1 };
}

SBICALL SBI_RETURN_VALUE sbicall6( int32_t extension, int32_t function, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5 )
{
    register int32_t a7 __asm__( "a7" ) = extension;
    register int32_t a6 __asm__( "a6" ) = function;
    register uint64_t a5 __asm__( "a5" ) = arg5;
    register uint64_t a4 __asm__( "a4" ) = arg4;
    register uint64_t a3 __asm__( "a3" ) = arg3;
    register uint64_t a2 __asm__( "a2" ) = arg2;
    register uint64_t a1 __asm__( "a1" ) = arg1;
    register uint64_t a0 __asm__( "a0" ) = arg0;
    __asm__ volatile (SBICALL_BODY
                  : "+r"( a0 ), "+r"( a1 ), "+r"( a2 ), "+r"( a3 ), "+r"( a4 ), "+r"( a5 ), "+r"( a6 ), "+r"( a7 )
                  :
                  : "ra", CLOBBER_SBICALL);
    return (SBI_RETURN_VALUE) { .error = (SBI_ERROR_CODE) a0, .value = (int64_t)a1 };
}

#undef SBICALL_BODY
#undef SBICALL
#undef CLOBBER_SBICALL

#endif // LWRISCV_SBI_H
