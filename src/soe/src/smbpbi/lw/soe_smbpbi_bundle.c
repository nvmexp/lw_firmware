/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   soe_smbpbi_bundle.c
 * @brief  SMBus Post-Box Interface. Handling of bundled requests
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------- Application Includes --------------------------- */
#include "soe_objsmbpbi.h"
#include "soe_smbpbi_async.h"

/* ------------------------- Types and Enums -------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
LwU8 s_smbpbiBundleRulesCheck(LwU32 *pDispRules, LwU32 rulesOffset, LwU8 reqCnt, LwU8 *pRuleCnt, LwU32 *pCmd);
void s_smbpbiApplyRule(LwU32 rule, LwU8 reqIdx, LwU32 dataOut, LwU32 extData, LwU32 *pCmd, LwU32 *pData, LwU32 *pExtData);

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/*!
 * Standard/default Result Disposition Rules
 */
static LwU32    dispRulesStd[] =
{
    DRF_NUM(_MSGBOX, _DISP_RULE, _REQ_IDX, 0)           |
    DRF_DEF(_MSGBOX, _DISP_RULE, _SRC_REG, _DATA)       |
    DRF_NUM(_MSGBOX, _DISP_RULE, _SRC_RIGHT_BIT, 0)     |
    DRF_NUM(_MSGBOX, _DISP_RULE, _SRC_WIDTH, 7)         |
    DRF_DEF(_MSGBOX, _DISP_RULE, _DST_REG, _STATUS)     |
    DRF_NUM(_MSGBOX, _DISP_RULE, _DST_RIGHT_BIT, 0),

    DRF_NUM(_MSGBOX, _DISP_RULE, _REQ_IDX, 1)           |
    DRF_DEF(_MSGBOX, _DISP_RULE, _SRC_REG, _DATA)       |
    DRF_NUM(_MSGBOX, _DISP_RULE, _SRC_RIGHT_BIT, 0)     |
    DRF_NUM(_MSGBOX, _DISP_RULE, _SRC_WIDTH, 7)         |
    DRF_DEF(_MSGBOX, _DISP_RULE, _DST_REG, _STATUS)     |
    DRF_NUM(_MSGBOX, _DISP_RULE, _DST_RIGHT_BIT, 8),

    DRF_NUM(_MSGBOX_, DISP_RULE, _REQ_IDX, 2)           |
    DRF_DEF(_MSGBOX_, DISP_RULE, _SRC_REG, _DATA)       |
    DRF_NUM(_MSGBOX_, DISP_RULE, _SRC_RIGHT_BIT, 0)     |
    DRF_NUM(_MSGBOX_, DISP_RULE, _SRC_WIDTH, 7)         |
    DRF_DEF(_MSGBOX_, DISP_RULE, _DST_REG, _STATUS)     |
    DRF_NUM(_MSGBOX_, DISP_RULE, _DST_RIGHT_BIT, 16),

    DRF_NUM(_MSGBOX, _DISP_RULE, _REQ_IDX, 0)           |
    DRF_DEF(_MSGBOX, _DISP_RULE, _SRC_REG, _DATA)       |
    DRF_NUM(_MSGBOX, _DISP_RULE, _SRC_RIGHT_BIT, 8)     |
    DRF_NUM(_MSGBOX, _DISP_RULE, _SRC_WIDTH, 15)        |
    DRF_DEF(_MSGBOX, _DISP_RULE, _DST_REG, _DATA)       |
    DRF_NUM(_MSGBOX, _DISP_RULE, _DST_RIGHT_BIT, 0),

    DRF_NUM(_MSGBOX, _DISP_RULE, _REQ_IDX, 1)           |
    DRF_DEF(_MSGBOX, _DISP_RULE, _SRC_REG, _DATA)       |
    DRF_NUM(_MSGBOX, _DISP_RULE, _SRC_RIGHT_BIT, 8)     |
    DRF_NUM(_MSGBOX, _DISP_RULE, _SRC_WIDTH, 7)         |
    DRF_DEF(_MSGBOX, _DISP_RULE, _DST_REG, _DATA)       |
    DRF_NUM(_MSGBOX, _DISP_RULE, _DST_RIGHT_BIT, 16),

    DRF_NUM(_MSGBOX, _DISP_RULE, _REQ_IDX, 2)           |
    DRF_DEF(_MSGBOX, _DISP_RULE, _SRC_REG, _DATA)       |
    DRF_NUM(_MSGBOX, _DISP_RULE, _SRC_RIGHT_BIT, 8)     |
    DRF_NUM(_MSGBOX, _DISP_RULE, _SRC_WIDTH, 7)         |
    DRF_DEF(_MSGBOX, _DISP_RULE, _DST_REG, _DATA)       |
    DRF_NUM(_MSGBOX, _DISP_RULE, _DST_RIGHT_BIT, 24),

    DRF_NUM(_MSGBOX, _DISP_RULE, _REQ_IDX, 0)           |
    DRF_DEF(_MSGBOX, _DISP_RULE, _SRC_REG, _DATA)       |
    DRF_NUM(_MSGBOX, _DISP_RULE, _SRC_RIGHT_BIT, 24)    |
    DRF_NUM(_MSGBOX, _DISP_RULE, _SRC_WIDTH, 7)         |
    DRF_DEF(_MSGBOX, _DISP_RULE, _DST_REG, _EXT_DATA)   |
    DRF_NUM(_MSGBOX, _DISP_RULE, _DST_RIGHT_BIT, 0),

    DRF_NUM(_MSGBOX, _DISP_RULE, _REQ_IDX, 1)           |
    DRF_DEF(_MSGBOX, _DISP_RULE, _SRC_REG, _DATA)       |
    DRF_NUM(_MSGBOX, _DISP_RULE, _SRC_RIGHT_BIT, 16)    |
    DRF_NUM(_MSGBOX, _DISP_RULE, _SRC_WIDTH, 15)        |
    DRF_DEF(_MSGBOX, _DISP_RULE, _DST_REG, _EXT_DATA)   |
    DRF_NUM(_MSGBOX, _DISP_RULE, _DST_RIGHT_BIT, 8),

    DRF_NUM(_MSGBOX, _DISP_RULE, _REQ_IDX, 2)           |
    DRF_DEF(_MSGBOX, _DISP_RULE, _SRC_REG, _DATA)       |
    DRF_NUM(_MSGBOX, _DISP_RULE, _SRC_RIGHT_BIT, 16)    |
    DRF_NUM(_MSGBOX, _DISP_RULE, _SRC_WIDTH, 7)         |
    DRF_DEF(_MSGBOX, _DISP_RULE, _DST_REG, _EXT_DATA)   |
    DRF_NUM(_MSGBOX, _DISP_RULE, _DST_RIGHT_BIT, 24),
};

ct_assert(LW_ARRAY_ELEMENTS(dispRulesStd) <= LW_MSGBOX_PARAM_MAX_DISP_RULES);

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Public Functions ------------------------------- */

LwU8
smbpbiBundleLaunch
(
    LwU32       *pCmd,
    LwU32       *pData,
    LwU32       *pExtData
)
{
    LwU8                    status = LW_MSGBOX_CMD_STATUS_SUCCESS;
    LwBool                  bSuccess;
    LwBool                  bSkipping;
    LwU8                    reqCnt;
    LwU8                    ruleCnt;
    LwU8                    reqIdx;
    LwU32                   bundleOffset;
    LwU32                   rulesOffset;
    LW_MSGBOX_IND_REQUEST   indReq;
    LwU32                   dispRules[LW_MSGBOX_PARAM_MAX_DISP_RULES];

    reqCnt = DRF_VAL(_MSGBOX, _CMD, _ARG1_BUNDLE_REQUEST_COUNT, *pCmd);
    ruleCnt = DRF_VAL(_MSGBOX, _CMD, _ARG1_BUNDLE_DISP_RULE_COUNT, *pCmd);

    if ((reqCnt > LW_MSGBOX_PARAM_MAX_BUNDLE_SIZE) ||
       (reqCnt == 0) ||
       (ruleCnt > LW_MSGBOX_PARAM_MAX_DISP_RULES))
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG1;
        goto smbpbiBundleLaunch_exit;
    }

    status = smbpbiScratchPtr2Offset(DRF_VAL(_MSGBOX, _CMD, _ARG2, *pCmd),
                                     SCRATCH_BANK_READ, &bundleOffset);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiBundleLaunch_exit;
    }

    rulesOffset = bundleOffset + reqCnt * sizeof(LW_MSGBOX_IND_REQUEST);

    status = s_smbpbiBundleRulesCheck(dispRules, rulesOffset, reqCnt,
                                      &ruleCnt, pCmd);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiBundleLaunch_exit;
    }

    // Execute the individual requests

    bSuccess = LW_TRUE;
    bSkipping = LW_FALSE;
    for (reqIdx = 0; reqIdx < reqCnt; ++reqIdx)
    {
        LwU32   offset = bundleOffset +
                         (reqIdx * sizeof(LW_MSGBOX_IND_REQUEST));
        LwU8    reqStatus = LW_MSGBOX_CMD_STATUS_NULL;

        // Read in the individual request
        status = smbpbiScratchRead(offset, &indReq,
                                   sizeof(LW_MSGBOX_IND_REQUEST));
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiBundleLaunch_exit;
        }

        indReq.dataOut = indReq.dataIn;
        indReq.cmdStatus = FLD_SET_DRF_NUM(_MSGBOX, _CMD, _STATUS, reqStatus,
                                           indReq.cmdStatus);

        if (!bSkipping)
        {
            // Dispatch the request
            reqStatus = smbpbiHandleMsgBoxRequest(&indReq.cmdStatus,
                                                  &indReq.dataOut,
                                                  &indReq.extData);
            indReq.cmdStatus = FLD_SET_DRF_NUM(_MSGBOX, _CMD, _STATUS,
                                               reqStatus,
                                               indReq.cmdStatus);
        }

        // Write back the individual request
        status = smbpbiScratchWrite(offset, &indReq, sizeof(LW_MSGBOX_IND_REQUEST));
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiBundleLaunch_exit;
        }

        if (bSkipping)
        {
            continue;
        }

        if (reqStatus == LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            LwU8    ruleIdx;

            //
            // Apply the Result Disposition Rules.
            // Only the rules referring to the current individual
            // request are being applied.
            //
            for (ruleIdx = 0; ruleIdx < ruleCnt; ++ruleIdx)
            {
                s_smbpbiApplyRule(dispRules[ruleIdx], reqIdx,
                                  indReq.dataOut, indReq.extData,
                                  pCmd, pData, pExtData);
            }
        }
        else
        {
            bSuccess = LW_FALSE;
            bSkipping = FLD_TEST_DRF(_MSGBOX, _CMD, _ON_BUNDLE_FAILURE, _STOP, indReq.cmdStatus);
        }
    }

    status = bSuccess ? LW_MSGBOX_CMD_STATUS_SUCCESS : LW_MSGBOX_CMD_STATUS_PARTIAL_FAILURE;

smbpbiBundleLaunch_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * If the client provided no disposition rules,
 * supply the default rules instead.
 * Otherwise, fetch the client's rules from the scratch
 * and check them for correctness.
 *
 * @param[in]       rulesOffset scratch space offset for client's rules
 * @param[in]       reqCnt      number of individual requests in the bundle
 * @param[in,out]   pRuleCnt    number of rules
 * @param[out]      pDispRules  destination rules array
 * @param[out]      pCmd        clients cmd register. We put the failing
 *                              rule idx there.
 *
 * @return      MSGBOX status
 *              _SUCCESS            normally
 *              _ERR_ARG1           too many rules
 *              _ERR_DISPOSITION    rule boundary check failed
 *              others              bubble up from inferiors
 */
LwU8
s_smbpbiBundleRulesCheck
(
    LwU32   *pDispRules,
    LwU32   rulesOffset,
    LwU8    reqCnt,
    LwU8    *pRuleCnt,
    LwU32   *pCmd
)
{
    LwU8    reg_max_lbit[] =
                    {
                        DRF_EXTENT(LW_MSGBOX_CMD_EXT_STATUS),   // Status
                        DRF_EXTENT(LW_MSGBOX_DATA_REG),         // Data
                        DRF_EXTENT(LW_MSGBOX_EXT_DATA_REG)      // ExtData
                    };
    LwU8    ruleIdx;
    LwU8    status;

    if (*pRuleCnt == 0)
    {
        // Copy standard disposition rules
        memcpy(pDispRules, dispRulesStd, sizeof(dispRulesStd));
        *pRuleCnt = LW_ARRAY_ELEMENTS(dispRulesStd);
        return LW_MSGBOX_CMD_STATUS_SUCCESS;
    }

    if (*pRuleCnt >= LW_MSGBOX_PARAM_MAX_DISP_RULES)
    {
        return LW_MSGBOX_CMD_STATUS_ERR_ARG1;
    }

    for (ruleIdx = 0; ruleIdx < *pRuleCnt; ++ruleIdx)
    {
        LwU8    src_reg;
        LwU8    src_rbit;
        LwU8    src_lbit;
        LwU8    src_widthl1;    // src bit field width less 1
        LwU8    dst_reg;
        LwU8    dst_rbit;
        LwU8    dst_lbit;
        LwU32   rule;

        status = smbpbiScratchRead32(rulesOffset + ruleIdx * sizeof(LwU32),
                                     &rule);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            break;
        }

        if (DRF_VAL(_MSGBOX, _DISP_RULE, _REQ_IDX, rule) >= reqCnt)
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_DISPOSITION;
            break;
        }

        src_reg = DRF_VAL(_MSGBOX, _DISP_RULE, _SRC_REG, rule);
        src_rbit = DRF_VAL(_MSGBOX, _DISP_RULE, _SRC_RIGHT_BIT, rule);
        src_widthl1 = DRF_VAL(_MSGBOX, _DISP_RULE, _SRC_WIDTH, rule);
        src_lbit = src_rbit + src_widthl1;

        dst_reg = DRF_VAL(_MSGBOX, _DISP_RULE, _DST_REG, rule);
        dst_rbit = DRF_VAL(_MSGBOX, _DISP_RULE, _DST_RIGHT_BIT, rule);
        dst_lbit = dst_rbit + src_widthl1;

        if ((src_reg > LW_MSGBOX_DISP_RULE_SRC_REG_MAX)     ||
            (src_reg == LW_MSGBOX_DISP_RULE_SRC_REG_STATUS) ||
            (src_lbit > reg_max_lbit[dst_reg])              ||
            (dst_reg > LW_MSGBOX_DISP_RULE_DST_REG_MAX)     ||
            (dst_lbit > reg_max_lbit[dst_reg]))
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_DISPOSITION;
            break;
        }
        pDispRules[ruleIdx] = rule;
    }

    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        *pCmd = FLD_SET_DRF_NUM(_MSGBOX, _CMD, _EXT_STATUS, ruleIdx, *pCmd);
    }

    return status;
}

void
s_smbpbiApplyRule
(
    LwU32   rule,
    LwU8    reqIdx,
    LwU32   dataOut,
    LwU32   extData,
    LwU32   *pCmd,
    LwU32   *pData,
    LwU32   *pExtData
)
{
    LwU32   src[] = {0, dataOut, extData};
    LwU32   *dst[] = {pCmd, pData, pExtData};
    LwU32   val;
    LwU8    src_reg;
    LwU8    src_rbit;
    LwU8    src_lbit;
    LwU8    src_widthl1;    // src bit field width less 1
    LwU8    dst_reg;
    LwU8    dst_rbit;
    LwU8    dst_lbit;

    if (DRF_VAL(_MSGBOX, _DISP_RULE, _REQ_IDX, rule) != reqIdx)
    {
        return;
    }

    src_reg = DRF_VAL(_MSGBOX, _DISP_RULE, _SRC_REG, rule);
    src_rbit = DRF_VAL(_MSGBOX, _DISP_RULE, _SRC_RIGHT_BIT, rule);
    src_widthl1 = DRF_VAL(_MSGBOX, _DISP_RULE, _SRC_WIDTH, rule);
    src_lbit = src_rbit + src_widthl1;

    dst_reg = DRF_VAL(_MSGBOX, _DISP_RULE, _DST_REG, rule);
    dst_rbit = DRF_VAL(_MSGBOX, _DISP_RULE, _DST_RIGHT_BIT, rule);
    dst_lbit = dst_rbit + src_widthl1;

    val = (src[src_reg] >> DRF_SHIFT(src_lbit : src_rbit))
                            & DRF_MASK(src_lbit : src_rbit);
    *dst[dst_reg] &= ~DRF_SHIFTMASK(dst_lbit : dst_rbit);
    *dst[dst_reg] |= val << DRF_SHIFT(dst_lbit : dst_rbit);
}

