/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyrigh 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_vgpuga10x.c
 * @brief  Contains VGPU routines specific to GA10X.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_bar0.h"
#include "pmu_oslayer.h"
#include "dev_lw_xve.h"
#include "dbgprintf.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_vgpu_private.h"
#include "ctrl/ctrl0080/ctrl0080gpu.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Update the per-VF BAR1 size with the requested value and
 *          callwate the number of VFs that can be made with it.
 * @param[in]   vfBar1SizeMB         Requested per-VF BAR1 size
 * @param[out]  pNumVfs              Pointer to the number of VFs that can be made with the
 *                                   requested per-VF BAR1 size.
 *
 * @return  @ref FLCN_OK                     Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT   The input value is invalid
 * @return  @ref FLCN_ERR_ILLEGAL_OPERATION  VFs are enabled so we can't
 *                                           update the registers
 */
FLCN_STATUS
vgpuVfBar1SizeUpdate_GA10X
(
    LwU32  vfBar1SizeMB,
    LwU32 *pNumVfs
)
{
    FLCN_STATUS status          = FLCN_OK;
    LwU32       vfBar1SizeMBIdx = BIT_IDX_32(vfBar1SizeMB);
    LwU32       vfBar1Size      = 0;
    LwU32       totalBar1SizeMB;
    LwU32       newNumVfs;
    LwU32       regVal;

    // Exit if the total BAR1 size is invalid
    status = vgpuVfBar1SizeGet_GA10X(&totalBar1SizeMB);
    if (status != FLCN_OK)
    {
        dbgPrintf(LEVEL_ERROR,
                  "Invalid total BAR1 size 0x%x.\n",
                  totalBar1SizeMB);

        goto vgpuVfBar1SizeUpdate_GA10X_exit;
    }

	//
    // Exit if VFs are lwrrently enabled because we can't update
    // the BAR1 size or number of VFs when the signal's high
    //
    regVal = REG_RD32(BAR0, DEVICE_BASE(LW_PCFG) + LW_XVE_SRIOV_CAP_HDR2);
    if (FLD_TEST_DRF(_XVE, _SRIOV_CAP_HDR2, _VF_ENABLE, _TRUE, regVal))
    {
        dbgPrintf(LEVEL_ERROR,
                  "VFs are lwrrently enabled.\n");

        status = FLCN_ERR_ILLEGAL_OPERATION;
        goto vgpuVfBar1SizeUpdate_GA10X_exit;
    }

    //
    // Exit if the requested size is greater than the total BAR1 size,
    // if the requested size is less than the minimum, or if the size
    // is not a power of 2 since such configurations are not supported
    //
    if ((vfBar1SizeMB > totalBar1SizeMB)                       ||
        (vfBar1SizeMB < LW0080_CTRL_GPU_VGPU_VF_BAR1_SIZE_MIN) ||
        !ONEBITSET(vfBar1SizeMB))
    {
        dbgPrintf(LEVEL_ERROR,
                  "Size 0x%x is out of bounds or not a power of 2.\n",
                  vfBar1SizeMB);

        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto vgpuVfBar1SizeUpdate_GA10X_exit;
    }

    // Decode MB-defined value to one in the register's scope
    vfBar1Size = vfBar1SizeMBIdx - BIT_IDX_32(LW0080_CTRL_GPU_VGPU_VF_BAR1_SIZE_MIN);

    //
    // Callwlate the number of VFs that can be created using the new per-VF BAR1 size by
    // dividing totalBar1SizeMB by vfBar1SizeMB. Since the values are guaranteed to be powers
    // of 2, it has been optimized to a bit shift.
    //
    newNumVfs = totalBar1SizeMB >> vfBar1SizeMBIdx;
    *pNumVfs  = newNumVfs;

    regVal = REG_RD32(BAR0, DEVICE_BASE(LW_PCFG) + LW_XVE_VF_BAR1_SIZE);
    BAR0_REG_WR32(DEVICE_BASE(LW_PCFG) + LW_XVE_VF_BAR1_SIZE,
            FLD_SET_DRF_NUM(_XVE, _VF_BAR1_SIZE, _NUM_WR_BITS, vfBar1Size, regVal));

    regVal = REG_RD32(BAR0, DEVICE_BASE(LW_PCFG) + LW_XVE_SRIOV_CFG0);
    BAR0_REG_WR32(DEVICE_BASE(LW_PCFG) + LW_XVE_SRIOV_CFG0,
        FLD_SET_DRF_NUM(_XVE, _SRIOV_CFG0, _TOTALVFS, newNumVfs, regVal));

vgpuVfBar1SizeUpdate_GA10X_exit:
    return status;
}

/*!
 * @brief   Get the total BAR1 size of the GPU in MB.
 *
 * @param[out]  pBar1SizeMB Pointer to the total BAR1 size in MB.
 *
 * @return  @ref FLCN_OK     Success
 * @return  @ref FLCN_ERROR  The retrieved BAR1 size is invalid
 */
FLCN_STATUS
vgpuVfBar1SizeGet_GA10X
(
    LwU32 *pBar1SizeMB
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       regVal;
    LwU32       bar1Size;

    regVal   = REG_RD32(BAR0, DEVICE_BASE(LW_PCFG) + LW_XVE_BAR1_CONFIG);
    bar1Size = DRF_VAL(_XVE, _BAR1_CONFIG, _SIZE, regVal);

    // Exit if the value exceeds the register's specs
    if (bar1Size > LW_XVE_BAR1_CONFIG_SIZE_512G)
    {
        dbgPrintf(LEVEL_ERROR,
                  "BAR1 size 0x%x exceeds maximum 0x%x.\n",
                  bar1Size, LW_XVE_BAR1_CONFIG_SIZE_512G);

        status = FLCN_ERROR;
        goto vgpuVfBar1InfoGet_GA10X_exit;
    }

    // Callwlate BAR1 size in MB
    *pBar1SizeMB = LW0080_CTRL_GPU_VGPU_VF_BAR1_SIZE_MIN << bar1Size;

vgpuVfBar1InfoGet_GA10X_exit:
    return status;
}