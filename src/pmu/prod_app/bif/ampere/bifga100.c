/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   bifga100.c
 * @brief  Contains BIF routines specific to GA100.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmu_objpmu.h"

#include "dev_lw_xp.h"
#include "dev_lw_xve.h"
#include "dev_trim.h"
#include "dev_lw_xp_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_bif_private.h"
#include "pmu_objbif.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * @brief Set EOM parameters
 * 
 * @param[in] eomMode       Mode of EOM (By default FOM)
 * @param[in] eomNblks      Number of blocks
 * @param[in] eomNerrs      Number of errors
 * @param[in] eomBerEyeSel  BER eye select
 * @param[in] eomPamEyeSel  PAM eye select
 *
 * @return FLCN_OK  Return FLCN_OK once these parameters sent by MODS are set
 */

FLCN_STATUS
bifSetEomParameters_GA100
(
    LwU8 eomMode,
    LwU8 eomNblks,
    LwU8 eomNerrs,
    LwU8 eomBerEyeSel,
    LwU8 eomPamEyeSel
)
{
    LwU32 tempRegVal = 0;
    LwU32 wData      = 0;
    LwU32 rData      = 0;
     
    //
    // UPHY_*_CTRL0 register writes
    // To not alter the other bits of LW_UPHY_DLM_AE_EOM_MEAS_CTRL0,
    // we first read the register, then change only the required field
    // (EOM Mode) and finally write the value
    //
    tempRegVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_8, _CFG_RESET, _INIT, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_RDS, 0x1, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDS, 0x0, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_ADDR, 
                                 LW_UPHY_DLM_AE_EOM_MEAS_CTRL0, tempRegVal);
    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_8, tempRegVal);

    rData = DRF_VAL(_XP, _PEX_PAD_CTL_9, _CFG_RDATA, 
                    REG_RD32(FECS, LW_XP_PEX_PAD_CTL_9));

    rData = FLD_SET_DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_CTRL0, 
                            _FOM_MODE, 0x2, rData);

    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_RDS, 0x0, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDS, 0x1, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, 
                                 _CFG_WDATA, rData, tempRegVal);
    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_8, tempRegVal);

    //
    // UPHY_*_CTRL1 register writes
    // Since we are altering all the bits of LW_UPHY_DLM_AE_EOM_MEAS_CTRL1,
    // we directly set the values of nerrs and nblks
    //
    wData = DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_CTRL1, _BER_YH_NBLKS_MAX, eomNblks) |
            DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_CTRL1, _BER_YL_NBLKS_MAX, eomNblks) |
            DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_CTRL1, _BER_YH_NERRS_MIN, eomNerrs) |
            DRF_NUM(_UPHY_DLM, _AE_EOM_MEAS_CTRL1, _BER_YL_NERRS_MIN, eomNerrs);
    tempRegVal = 0;
    tempRegVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_8, _CFG_RESET, _INIT, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDS, 0x1, tempRegVal);
    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_ADDR,
                                 LW_UPHY_DLM_AE_EOM_MEAS_CTRL1, tempRegVal);

    tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDATA, wData,
                                 tempRegVal);

    REG_WR32(FECS, LW_XP_PEX_PAD_CTL_8, tempRegVal);

    return FLCN_OK;
}

/*!
 * @brief Read PEX UPHY DLN registers
 *
 * @param[in]   regAddress         Register whose value needs to be read
 * @param[in]   laneSelectMask     Which lane to read from
 * @param[out]  pRegValue          The value of the register
 *
 * @return FLCN_OK After reading the 
 */

FLCN_STATUS
bifGetUphyDlnCfgSpace_GA100
(
    LwU32 regAddress,
    LwU32 laneSelectMask,
    LwU16 *pRegValue
)
{
  LwU32 tempRegVal = 0;
  LwU32 rData      = 0;

  tempRegVal = REG_RD32(FECS, LW_XP_PEX_PAD_CTL_SEL);
  tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_SEL, _LANE_SELECT,
                               BIT(laneSelectMask), tempRegVal);
  REG_WR32(FECS, LW_XP_PEX_PAD_CTL_SEL, tempRegVal);

  tempRegVal = 0;
  tempRegVal = FLD_SET_DRF(_XP, _PEX_PAD_CTL_8, _CFG_RESET, _INIT, tempRegVal);
  tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_RDS, 0x1, tempRegVal);
  tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_WDS, 0x0, tempRegVal);
  tempRegVal = FLD_SET_DRF_NUM(_XP, _PEX_PAD_CTL_8, _CFG_ADDR, regAddress,
                               tempRegVal);
  REG_WR32(FECS, LW_XP_PEX_PAD_CTL_8, tempRegVal);

  rData = DRF_VAL(_XP, _PEX_PAD_CTL_9, _CFG_RDATA,
                  REG_RD32(FECS, LW_XP_PEX_PAD_CTL_9));
  *pRegValue = rData;

  return FLCN_OK;
}
