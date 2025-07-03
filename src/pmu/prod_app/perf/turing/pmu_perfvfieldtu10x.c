/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_perf_private.h"
#include "perf/3x/vfe_var_single_sensed_fuse.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/*!
 * @brief Lookup table used by perfVfieldRegAddrValidate_TU10X.
 *
 * Maps VFIELD ID to LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_SEGMENTS_MAX
 * number (lwrrently 1) of valid (hardware) register addresses.
 */
static LwU32 s_vfieldRegAddrLookupTable_TU10X[]
    GCC_ATTRIB_SECTION("dmem_perfVfe", "s_vfieldRegAddrLookupTable_TU10X") =
{
    0x00000000, 0x00, 0x00,
    0x00000000, 0x00, 0x00,
    0x00000000, 0x00, 0x00,
    0x00000000, 0x00, 0x00,
    0x00000000, 0x00, 0x00,
    0x00118FB0, 0x00, 0x00,
    0x00000000, 0x00, 0x00,
    0x000214A8, 0x00, 0x00,
    0x000214A0, 0x00, 0x00,
    0x000212D4, 0x00, 0x00,
    0x000214A4, 0x00, 0x00,
    0x00000000, 0x00, 0x00,
    0x0000147C, 0x00, 0x00,
    0x0000147C, 0x00, 0x00,
    0x00021E08, 0x00, 0x00,
    0x00021E0C, 0x00, 0x00,
    0x0011802C, 0x00, 0x00,
    0x00021E1C, 0x00, 0x00,
    0x00021E1C, 0x00, 0x00,
    0x00021E1C, 0x00, 0x00,
    0x00021E1C, 0x00, 0x00,
    0x00021178, 0x00, 0x00,
    0x00021178, 0x00, 0x00,
    0x000214AC, 0x00, 0x00
};

/*
 * s_vfieldRegAddrLookupTable_TU10X is laid out under the assumption that
 * LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_SEGMENTS_MAX is 1 and will need
 * to be updated if that ever changes
 */
ct_assert(LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_SEGMENTS_MAX == 3);

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/*!
 * @brief Validate if a register address is valid for a given VFIELD ID
 *
 * @param[in] vfieldId     VFIELD ID to validate against
 * @param[in] segmentIdx   VFIELD fuse segment index
 * @param[in] regAddr      Register address to validate
 *
 * @return FLCN_OK                    If the register address is valid
 * @return FLCN_ERR_ILWALID_ARGUMENT  If the register address is invalid
 */
FLCN_STATUS
perfVfieldRegAddrValidate_TU10X
(
    LwU8  vfieldId,
    LwU8  segmentIdx,
    LwU32 regAddr
)
{
    return vfeVarRegAddrValidateAgainstTable(
                vfieldId, segmentIdx, regAddr, s_vfieldRegAddrLookupTable_TU10X,
                LW_ARRAY_ELEMENTS(s_vfieldRegAddrLookupTable_TU10X));
}
