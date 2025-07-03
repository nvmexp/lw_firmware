/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file zbccolwertutil.h
//! \brief DS to FB color colwersion utility.
//!

#ifndef INCLUDED_ZBCCOLWERTUTIL_H
#define INCLUDED_ZBCCOLWERTUTIL_H

#include <string> // Only use <> for built-in C++ stuff
#include "lwRmApi.h"
#include "core/include/utility.h"
#include "class/cl9096.h" // GF100_ZBC_CLEAR
#include "ctrl/ctrl9096.h"

#define FP16_1                      0x3c00
#define FP32_1                      0x3f800000

union fluint
{
    float f;
    unsigned int ui;
    int i;
};

//! Helper Class cARGB to be used for color colwersion

struct cARGB
{
    fluint red, green, blue, alpha, cout[4];
};

class zbcColorColwertUtil
{
public:
    //
    // Member functions
    //

    RC ColorColwertToFB(cARGB cin, int ctFormat, cARGB* pRes);
    int bytesPerElementCTFMT( const int fmt );
};

//! -----------------------------------------------------------------------------
//!  List of standard colwersion routines.
//! -----------------------------------------------------------------------------
namespace zbcColwert
{
    bool u32IsNaN(UINT32 val) ;
    UINT32 uint32ToUintA( UINT32 din, int ibits );
    UINT32 uint32ToUint16( UINT32 din );
    UINT32 uint32ToUint10( UINT32 din );
    UINT32 uint32ToUint08( UINT32 din );
    UINT32 uint32ToUint02( UINT32 din );
    UINT32 ToU032(UINT32 num);
    UINT32 ToU016(UINT32 num);
    UINT32 ToU011(UINT32 num);
    UINT32 ToU010(UINT32 num);
    UINT32 ToU008(UINT32 num);
    UINT32 ToU002(UINT32 num);
    UINT32 fp32ToFp16( UINT32 in);
    UINT16 fp16ToFp11( UINT16 din);
    UINT16 fp16ToFp10( UINT16 din);
    UINT32 fp16ToFp32( UINT16 in );
    UINT32 fp32ToFp16(float f);
    UINT32 floatToUnorm(UINT32 val, UINT32 ibits);
    UINT32 floatToUnorm16(UINT32 val);
    UINT32 floatToUnorm10(UINT32 val);
    UINT32 floatToUnorm08(UINT32 val);
    UINT32 floatToUnormOne(UINT32 val);
    float fp16ToFloat( UINT16 f );
    UINT32 fp16ToUnorm08( UINT32 f );
    UINT32 floatToSnorm16(UINT32 val);
    UINT32 floatToSnorm08(UINT32 val);
    int int32ToSintA( UINT32 din, int ibits );
    UINT32 int32ToSint32( UINT32 din );
    UINT32 int32ToSint16( UINT32 din );
    UINT32 int32ToSint08( UINT32 din );
    UINT32 unorm10ToUnorm2( UINT32 unorm10 );
    UINT08 colwLinearFp16sRGB8 (UINT16 ufp16);
    UINT32 floatToSnorm(UINT32 val, UINT32 ibits);
    UINT16 uint32ToFP16(UINT32 in);
};

#endif

