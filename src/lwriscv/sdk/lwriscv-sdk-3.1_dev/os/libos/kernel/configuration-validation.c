#include "libos-config.h"
#include "lwriscv.h"

#if !LIBOS_FEATURE_PAGING && LIBOS_LWRISCV < LIBOS_LWRISCV_2_0
#   error Unsupported: MPU isolation requires RISC-V 2.0 or better
#endif

#if LIBOS_CONFIG_MPU_PAGESIZE != 4096 && LIBOS_LWRISCV < LIBOS_LWRISCV_2_0
#   error Unsupported: LWRISC-V 1.x requires 4kb page size
#endif