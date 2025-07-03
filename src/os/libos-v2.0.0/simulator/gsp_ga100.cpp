#include "../kernel/riscv-isa.h"
#include "sdk/lwpu/inc/lwmisc.h"
#include "sdk/lwpu/inc/lwtypes.h"
#include <assert.h>
#include <drivers/common/inc/hwref/ampere/ga100/dev_master.h>
#include <drivers/common/inc/hwref/ampere/ga100/dev_gsp.h>
#include <drivers/common/inc/hwref/ampere/ga100/dev_gsp_riscv_csr_64.h>
#include <drivers/common/inc/swref/ampere/ga100/dev_riscv_csr_64_addendum.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory.h>
#include <sdk/lwpu/inc/lwtypes.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include "gsp.h"

LwU32 gsp::priv_read_GA100(LwU64 addr)
{
    switch (addr)
    {
    case LW_PMC_BOOT_42:
        return REF_DEF(LW_PMC_BOOT_42_CHIP_ID, _GA100);

    default:
        return priv_read_TU11X(addr);
    }
}
