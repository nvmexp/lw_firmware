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
#include "kernel/lwriscv-2.0/sbi.h"

void gsp::pmp_assert_partition_has_access(LwU64 physical, LwU64 size, TranslationKind kind)
{
    LwU32 pmpMask = 0;
    if (kind & (translateUserLoad|translateSupervisorLoad))
      pmpMask |= PMP_ACCESS_MODE_READ;

    if (kind & (translateUserStore | translateSupervisorStore))
      pmpMask |= PMP_ACCESS_MODE_WRITE;

    if (kind & (translateUserExelwte | translateSupervisorExelwte))
      pmpMask |= PMP_ACCESS_MODE_EXELWTE;

    // Lookup the physical address in the PMP
#if !LIBOS_TINY
    bool allowed = false;
    for (LwU64 pmpIndex = 0; pmpIndex < 16; pmpIndex++)
    {
        if (!partition[partitionIndex].pmp[pmpIndex].end)
            continue;

        assert(partition[partitionIndex].pmp[pmpIndex].begin < partition[partitionIndex].pmp[pmpIndex].end);

        if (physical >= partition[partitionIndex].pmp[pmpIndex].begin && 
           (partition[partitionIndex].pmp[pmpIndex].end - physical) >= size &&
           (partition[partitionIndex].pmp[pmpIndex].pmpAccess & pmpMask) == pmpMask) {
            //printf("PMP %d: %llx-%llx\n", partitionIndex, partition[partitionIndex].pmp[pmpIndex].begin, partition[partitionIndex].pmp[pmpIndex].end);
            allowed = true;
            break;
        }
    }

    assert(allowed && "PMP violation");
#endif    
}