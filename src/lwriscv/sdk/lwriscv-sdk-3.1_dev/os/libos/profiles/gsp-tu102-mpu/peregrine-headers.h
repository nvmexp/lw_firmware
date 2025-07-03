#   include "turing/tu102/dev_gsp_riscv_csr_64.h"
#   include "turing/tu102/lw_gsp_riscv_address_map.h"
#   include "turing/tu102/dev_falcon_v4.h"
#   include "turing/tu102/dev_fbif_v4.h"
#   include "turing/tu102/dev_riscv_pri.h"
#   include "turing/tu102/dev_falcon_v4.h"
#   include "turing/tu102/dev_gsp_pri.h"

#define SPLAT(x,y) x##y
#define CHIP(x) SPLAT(LW_PGSP,x)

#   define FALCON_BASE     LW_FALCON_GSP_BASE
#   define PEREGRINE_BASE  LW_FALCON2_GSP_BASE
#   define FBIF_BASE       (CHIP(_FBIF_TRANSCFG)(0) + LW_PFALCON_FBIF_TRANSCFG(0))
