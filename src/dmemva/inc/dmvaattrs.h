/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#define ATTR_OVLY(ov)               __attribute__ ((section(ov)))
#define ATTR_ALIGNED(align)         __attribute__ ((aligned(align)))
#define ALWAYS_INLINE               __attribute__ ((always_inline))
#define NOINLINE                    __attribute__ ((noinline))
#define NORETURN                    __attribute__ ((noreturn))
#define PACKED                      __attribute__ ((packed))
#define UNUSED                      __attribute__ ((unused))
