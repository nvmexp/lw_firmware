/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2022 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwltrex.h"

//-----------------------------------------------------------------------------
/*static*/ LwLinkTrex::ReductionOp LwLinkTrex::StringToReductionOp(const string& str)
{
    static map<string,LwLinkTrex::ReductionOp> reduceOpMap =
    {
        {"S16_FADD", RO_S16_FADD},
        {"S16_FADD_MP", RO_S16_FADD_MP},
        {"S16_FMIN", RO_S16_FMIN},
        {"S16_FMAX", RO_S16_FMAX},
        {"S32_IMIN", RO_S32_IMIN},
        {"S32_IMAX", RO_S32_IMAX},
        {"S32_IXOR", RO_S32_IXOR},
        {"S32_IAND", RO_S32_IAND},
        {"S32_IOR", RO_S32_IOR},
        {"S32_IADD", RO_S32_IADD},
        {"S32_FADD", RO_S32_FADD},
        {"S64_IMIN", RO_S64_IMIN},
        {"S64_IMAX", RO_S64_IMAX},
        {"U16_FADD", RO_U16_FADD},
        {"U16_FADD_MP", RO_U16_FADD_MP},
        {"U16_FMIN", RO_U16_FMIN},
        {"U16_FMAX", RO_U16_FMAX},
        {"U32_IMIN", RO_U32_IMIN},
        {"U32_IMAX", RO_U32_IMAX},
        {"U32_IXOR", RO_U32_IXOR},
        {"U32_IAND", RO_U32_IAND},
        {"U32_IOR", RO_U32_IOR},
        {"U32_IDD", RO_U32_IADD},
        {"U64_IMIN", RO_U64_IMIN},
        {"U64_IMAX", RO_U64_IMAX},
        {"U64_IXOR", RO_U64_IXOR},
        {"U64_IAND", RO_U64_IAND},
        {"U64_IOR", RO_U64_IOR},
        {"U64_IADD", RO_U64_IADD},
        {"U64_FADD", RO_U64_FADD}
    };

    string upperStr = str;
    transform(upperStr.begin(), upperStr.end(), upperStr.begin(), ::toupper);

    if (reduceOpMap.find(upperStr) == reduceOpMap.end())
        return RO_ILWALID;

    return reduceOpMap[upperStr];
}