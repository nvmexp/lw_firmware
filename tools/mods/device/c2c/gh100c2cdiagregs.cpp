/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gh100c2cdiagregs.h"

const LwU32 Gh100C2cDiagRegs::s_C2CLinkStatusRegs[] = {
                            LW_PC2C_C2C0_PL_TX_TR_STATUS,
                            LW_PC2C_C2C1_PL_TX_TR_STATUS,
                            LW_PC2C_C2C2_PL_TX_TR_STATUS,
                            LW_PC2C_C2C3_PL_TX_TR_STATUS,
                            LW_PC2C_C2C4_PL_TX_TR_STATUS,
                            LW_PC2C_C2C5_PL_TX_TR_STATUS,
                            LW_PC2C_C2C6_PL_TX_TR_STATUS,
                            LW_PC2C_C2C7_PL_TX_TR_STATUS,
                            LW_PC2C_C2C8_PL_TX_TR_STATUS,
                            LW_PC2C_C2C9_PL_TX_TR_STATUS,
                          };

const LwU32 Gh100C2cDiagRegs::s_C2CLinkFreqRegs[] = {
                            LW_PC2C_C2CS0_REFPLL_COEFF,
                            LW_PC2C_C2CS1_REFPLL_COEFF,
                          };

const LwU32 Gh100C2cDiagRegs::s_C2CLinkFreqRegsLink0 = LW_PC2C_C2C0_REFPLL_COEFF;

const LwU32 Gh100C2cDiagRegs::s_C2CCrcCntRegs[] = {
                            LW_PC2C_C2C0_DL_RX_DBG2_CRC_COUNT_STATUS,
                            LW_PC2C_C2C1_DL_RX_DBG2_CRC_COUNT_STATUS,
                            LW_PC2C_C2C2_DL_RX_DBG2_CRC_COUNT_STATUS,
                            LW_PC2C_C2C3_DL_RX_DBG2_CRC_COUNT_STATUS,
                            LW_PC2C_C2C4_DL_RX_DBG2_CRC_COUNT_STATUS,
                            LW_PC2C_C2C5_DL_RX_DBG2_CRC_COUNT_STATUS,
                            LW_PC2C_C2C6_DL_RX_DBG2_CRC_COUNT_STATUS,
                            LW_PC2C_C2C7_DL_RX_DBG2_CRC_COUNT_STATUS,
                            LW_PC2C_C2C8_DL_RX_DBG2_CRC_COUNT_STATUS,
                            LW_PC2C_C2C9_DL_RX_DBG2_CRC_COUNT_STATUS,
                          };

const LwU32 Gh100C2cDiagRegs::s_C2CReplayCntRegs[] = {
                            LW_PC2C_C2C0_DL_TX_DBG3_REPLAY_COUNT_STATUS,
                            LW_PC2C_C2C1_DL_TX_DBG3_REPLAY_COUNT_STATUS,
                            LW_PC2C_C2C2_DL_TX_DBG3_REPLAY_COUNT_STATUS,
                            LW_PC2C_C2C3_DL_TX_DBG3_REPLAY_COUNT_STATUS,
                            LW_PC2C_C2C4_DL_TX_DBG3_REPLAY_COUNT_STATUS,
                            LW_PC2C_C2C5_DL_TX_DBG3_REPLAY_COUNT_STATUS,
                            LW_PC2C_C2C6_DL_TX_DBG3_REPLAY_COUNT_STATUS,
                            LW_PC2C_C2C7_DL_TX_DBG3_REPLAY_COUNT_STATUS,
                            LW_PC2C_C2C8_DL_TX_DBG3_REPLAY_COUNT_STATUS,
                            LW_PC2C_C2C9_DL_TX_DBG3_REPLAY_COUNT_STATUS,
                          };

