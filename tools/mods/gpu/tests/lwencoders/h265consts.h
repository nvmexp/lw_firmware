/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef H265CONSTS_H
#define H265CONSTS_H

#include <cstddef>

#include "core/include/types.h"

namespace H265
{

const size_t maxTLayer = 64;

extern const UINT08 default4x4ScalingList[16];
extern const UINT08 default8x8IntraScalingList[64];
extern const UINT08 default8x8InterScalingList[64];
       const UINT32 numScalingMatrixSizes = 4;
       const UINT32 maxNumScalingMatrices = 6;
       const UINT32 maxScalingListCoefNum = 64;
extern const UINT32 numScalingMatricesPerSize[numScalingMatrixSizes];
extern const UINT32 scalingListSize[numScalingMatrixSizes];
extern const UINT32 scalingList1DSize[numScalingMatrixSizes];

const UINT32 maxNumRefPics = 16;
const UINT32 maxNumLongTermRefPicsSps = 33;
const UINT32 maxDpbPicBuf = 6; // see A.4.1 of ITU-T H.265
const UINT32 maxMaxDpbSize = 16; // see (A-2) of ITU-T H.265

}
#endif
