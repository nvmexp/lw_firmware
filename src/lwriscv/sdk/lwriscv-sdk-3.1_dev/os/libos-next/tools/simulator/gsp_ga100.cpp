#include "libriscv.h"
#include "lwmisc.h"
#include "lwtypes.h"
#include <assert.h>
#include "peregrine-headers.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include "gsp.h"

LwU32 gsp::priv_read(LwU64 addr)
{
    switch (addr)
    {
    case LW_PMC_BOOT_42:
        return REF_DEF(LW_PMC_BOOT_42_CHIP_ID, _GA100);

    default:
        return priv_read_TU11X(addr);
    }
}
