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

#include "class/cl9096.h" //   GF100_ZBC_CLEAR
#include "ctrl/ctrl9096.h"
#include "zbccolwertutil.h"
#include "lwos.h"
#include "lwRmApi.h"

//used for colwersion form FP16 to srgb8
//should be in sync with //hw/lw5x/include/table_fp16_srgb8_ulp0.6.h
int linsrgb8ulp06[0x3c00] = {
#include "tableFp16srgb8ulp0.6.h"
};

using namespace zbcColwert;

static UINT32 floatToUnormA(UINT32 val, UINT32 ibits);
static bool sign(UINT32 f);
static UINT32 exponent(UINT32 f);
static UINT32 mantissa(UINT32 f);
static UINT32 floatToUnorm24Tox(UINT32 f);
static UINT08 getLinearSRGB8ulp06(UINT32 i);

//! !
//! ! bytesPerElementCTFMT--- This function returns the number of bytes
//! ! required to represent each table entry for a given FB(clocr table fmt)Format.
//! -----------------------------------------------------------------------------
int zbcColorColwertUtil::bytesPerElementCTFMT( const int fmt )
{
    switch( fmt )
    {
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_ZERO:
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_UNORM_ONE:
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_RF32_GF32_BF32_AF32:
            return 16;
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_R16_G16_B16_A16:
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_RN16_GN16_BN16_AN16:
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_RS16_GS16_BS16_AS16:
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_RU16_GU16_BU16_AU16:
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_RF16_GF16_BF16_AF16:
            return 8;
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A8R8G8B8:
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A8RL8GL8BL8:
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A2B10G10R10:
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_AU2BU10GU10RU10:
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A8B8G8R8:
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A8BL8GL8RL8:
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_AN8BN8GN8RN8:
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_AS8BS8GS8RS8:
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_AU8BU8GU8RU8:
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A2R10G10B10:
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_BF10GF11RF11:
            return 4;
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_ILWALID:
        default:
            Printf(Tee::PriHigh, "Unknown CT_FORMAT 0x%x\n", fmt);
            return 4;
    }
}

//! ***********************************************************************************
//!  ColorColwertToFB:
//!  Colwerts an input color to the given ctFormat.
//!  Used for colwerting clear colors and blend constant colors to the FB format.
//!  It also handles the packing of the pixels of the resultant data after format
//!  based colwersion.
//! ***********************************************************************************

RC zbcColorColwertUtil::ColorColwertToFB(cARGB cin, int ctFormat, cARGB* pRes)
{
    UINT32 ured, ugreen, ublue, ualpha;
    UINT32 r =0, g = 0, b = 0, a = 0;
    RC            rc;

    pRes->cout[0].i = 0;
    pRes->cout[1].i = 0;
    pRes->cout[2].i = 0;
    pRes->cout[3].i = 0;

    ured   = cin.red.ui;
    ugreen = cin.green.ui;
    ublue  = cin.blue.ui;
    ualpha  = cin.alpha.ui;

    switch (ctFormat) {
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_R16_G16_B16_A16:
            r = floatToUnorm16(ured)  & 0x0FFFF;
            g = floatToUnorm16(ugreen)  & 0x0FFFF;
            b = floatToUnorm16(ublue)  & 0x0FFFF;
            a = floatToUnorm16(ualpha)  & 0x0FFFF;
            pRes->cout[0].ui = (g << 16) | r;
            pRes->cout[1].ui = (a << 16) | b;
            pRes->cout[2].ui = pRes->cout[0].ui;
            pRes->cout[3].ui = pRes->cout[1].ui;
            break;
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_RN16_GN16_BN16_AN16:
            r = floatToSnorm16(ured) & 0x0FFFF;
            g = floatToSnorm16(ugreen) & 0x0FFFF;
            b = floatToSnorm16(ublue) & 0x0FFFF;
            a = floatToSnorm16(ualpha) & 0x0FFFF;
            pRes->cout[0].ui = (g << 16) | r;
            pRes->cout[1].ui = (a << 16) | b;
            pRes->cout[2].ui = pRes->cout[0].ui;
            pRes->cout[3].ui = pRes->cout[1].ui;
            break;
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_RS16_GS16_BS16_AS16:
            r = int32ToSint16(ured) & 0x0FFFF;
            g = int32ToSint16(ugreen) & 0x0FFFF;
            b = int32ToSint16(ublue) & 0x0FFFF;
            a = int32ToSint16(ualpha) & 0x0FFFF;
            pRes->cout[0].ui = (g << 16) | r;
            pRes->cout[1].ui = (a << 16) | b;
            pRes->cout[2].ui = pRes->cout[0].ui;
            pRes->cout[3].ui = pRes->cout[1].ui;
            break;
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_RU16_GU16_BU16_AU16:
            r = uint32ToUint16(ured) & 0x0FFFF;
            g = uint32ToUint16(ugreen) & 0x0FFFF;
            b = uint32ToUint16(ublue) & 0x0FFFF;
            a = uint32ToUint16(ualpha) & 0x0FFFF;
            pRes->cout[0].ui = (g << 16) | r;
            pRes->cout[1].ui = (a << 16) | b;
            pRes->cout[2].ui = pRes->cout[0].ui;
            pRes->cout[3].ui = pRes->cout[1].ui;
            break;
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_RF16_GF16_BF16_AF16:
            r = fp32ToFp16(ured) & 0x0FFFF;
            g = fp32ToFp16(ugreen) & 0x0FFFF;
            b = fp32ToFp16(ublue) & 0x0FFFF;
            a = fp32ToFp16(ualpha) & 0x0FFFF;
            pRes->cout[0].ui = (g << 16) | r;
            pRes->cout[1].ui = (a << 16) | b;
            pRes->cout[2].ui = pRes->cout[0].ui;
            pRes->cout[3].ui = pRes->cout[1].ui;
            break;
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A8R8G8B8:
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A8B8G8R8:
            r = floatToUnorm08(ured) & 0xFF;
            g = floatToUnorm08(ugreen) & 0xFF;
            b = floatToUnorm08(ublue) & 0xFF;
            a = floatToUnorm08(ualpha) & 0xFF;
            if (ctFormat == LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A8R8G8B8)
                pRes->cout[0].ui = (a << 24) | ( r << 16) | (g << 8) | b;
            else
                pRes->cout[0].ui = (a << 24) | ( b << 16) | (g << 8) | r;
            pRes->cout[1].ui = pRes->cout[0].ui;
            pRes->cout[2].ui = pRes->cout[0].ui;
            pRes->cout[3].ui = pRes->cout[0].ui;
            break;
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A8RL8GL8BL8:
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A8BL8GL8RL8:
            //   for sRGB, clamp the values in [0.0, 1.0] range as UNORM8
            //   Clamp to SRGB range, as per dx10 spec. (bug 272138)
            //   NaN -> 0
            if (u32IsNaN(ured  )) ured   = ToU032(0);
            if (u32IsNaN(ugreen)) ugreen = ToU032(0);
            if (u32IsNaN(ublue )) ublue  = ToU032(0);
            if (u32IsNaN(ualpha)) ualpha = ToU032(0);
            //
            //   Negative -> 0
            //   Since all values in our case are of U032
            //   format check for the sign bit for negative.
            //
            if (ured >> 31)     ured    = 0x00000000;
            if (ugreen >> 31)   ugreen  = 0x00000000;
            if (ublue >> 31)    ublue   = 0x00000000;
            if (ualpha >> 31)   ualpha  = 0x00000000;
            //   Greater than 1.0 -> 1.0
            if (ured   > FP32_1) ured     = FP32_1;
            if (ugreen > FP32_1) ugreen   = FP32_1;
            if (ublue  > FP32_1) ublue    = FP32_1;
            if (ualpha > FP32_1) ualpha   = FP32_1;
            //
            r = (colwLinearFp16sRGB8(fp32ToFp16(ured) & 0xFFFF)) & 0xFF;
            g = (colwLinearFp16sRGB8(fp32ToFp16(ugreen) & 0xFFFF)) & 0xFF;
            b = (colwLinearFp16sRGB8(fp32ToFp16(ublue) & 0xFFFF)) & 0xFF;
            a = (fp16ToUnorm08(fp32ToFp16(ualpha) & 0xFFFF)) & 0xFF;

            if (ctFormat == LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A8RL8GL8BL8)
                pRes->cout[0].ui = (a << 24) | ( r << 16) | (g << 8) | b;
            else
                pRes->cout[0].ui = (a << 24) | ( b << 16) | (g << 8) | r;
            pRes->cout[1].ui = pRes->cout[0].ui;
            pRes->cout[2].ui = pRes->cout[0].ui;
            pRes->cout[3].ui = pRes->cout[0].ui;
            break;
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_AN8BN8GN8RN8:
            r = floatToSnorm08(ured) & 0xFF;
            g = floatToSnorm08(ugreen) & 0xFF;
            b = floatToSnorm08(ublue) & 0xFF;
            a = floatToSnorm08(ualpha) & 0xFF;
            pRes->cout[0].ui = (a << 24) | ( b << 16) | (g << 8) | r;
            pRes->cout[1].ui = pRes->cout[0].ui;
            pRes->cout[2].ui = pRes->cout[0].ui;
            pRes->cout[3].ui = pRes->cout[0].ui;
            break;
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_AS8BS8GS8RS8:
            r = int32ToSint08(ured) & 0xFF;
            g = int32ToSint08(ugreen) & 0xFF;
            b = int32ToSint08(ublue) & 0xFF;
            a = int32ToSint08(ualpha)& 0xFF;
            pRes->cout[0].ui = (a << 24) | ( b << 16) | (g << 8) | r;
            pRes->cout[1].ui = pRes->cout[0].ui;
            pRes->cout[2].ui = pRes->cout[0].ui;
            pRes->cout[3].ui = pRes->cout[0].ui;
            break;
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_AU8BU8GU8RU8:
            r = uint32ToUint08(ured) & 0xFF;
            g = uint32ToUint08(ugreen) & 0xFF;
            b = uint32ToUint08(ublue) & 0xFF;
            a = uint32ToUint08(ualpha) & 0xFF;
            pRes->cout[0].ui = (a << 24) | ( b << 16) | (g << 8) | r;
            pRes->cout[1].ui = pRes->cout[0].ui;
            pRes->cout[2].ui = pRes->cout[0].ui;
            pRes->cout[3].ui = pRes->cout[0].ui;
            break;
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A2B10G10R10:
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A2R10G10B10:
            r = floatToUnorm10(ured) & 0x3ff;
            g = floatToUnorm10(ugreen) & 0x3ff;
            b = floatToUnorm10(ublue) & 0x3ff;
            a = (unorm10ToUnorm2(floatToUnorm10(ualpha) & 0x3ff)) & 0x3;
            if (ctFormat == LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_A2B10G10R10)
                pRes->cout[0].ui = (a << 30) | (b << 20) | (g << 10) | r;
            else
                pRes->cout[0].ui = (a << 30) | (r << 20) | (g << 10) | b;
            pRes->cout[1].ui = pRes->cout[0].ui;
            pRes->cout[2].ui = pRes->cout[0].ui;
            pRes->cout[3].ui = pRes->cout[0].ui;
            break;
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_AU2BU10GU10RU10:
            r = uint32ToUint10(ured) & 0x3ff;
            g = uint32ToUint10(ugreen) & 0x3ff;
            b = uint32ToUint10(ublue) & 0x3ff;
            a = uint32ToUint02(ualpha) & 0x3;
            pRes->cout[0].ui = (a << 30) | (b << 20) | (g << 10) | r;
            pRes->cout[1].ui = pRes->cout[0].ui;
            pRes->cout[2].ui = pRes->cout[0].ui;
            pRes->cout[3].ui = pRes->cout[0].ui;
            break;
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_BF10GF11RF11:
            r = fp16ToFp11(fp32ToFp16(ured)) & 0x7ff;
            g = fp16ToFp11(fp32ToFp16(ugreen)) & 0x7ff;
            b = fp16ToFp10(fp32ToFp16(ublue)) & 0x3ff;
            a = 0;
            pRes->cout[0].ui = (b << 22) | (g << 11) | r;
            pRes->cout[1].ui = pRes->cout[0].ui;
            pRes->cout[2].ui = pRes->cout[0].ui;
            pRes->cout[3].ui = pRes->cout[0].ui;
            break;
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_ZERO:
            pRes->cout[0].ui = 0x00000000;
            pRes->cout[1].ui = 0x00000000;
            pRes->cout[2].ui = 0x00000000;
            pRes->cout[3].ui = 0x00000000;
            break;
        case LW9096_CTRL_CMD_SET_ZBC_COLOR_CLEAR_FMT_VAL_UNORM_ONE:
            pRes->cout[0].ui = floatToUnormOne(ured);
            pRes->cout[1].ui = floatToUnormOne(ugreen);
            pRes->cout[2].ui = floatToUnormOne(ublue);
            pRes->cout[3].ui = floatToUnormOne(ualpha);
            break;
        default:
            //   All 32-bit formats are simple pass-through, since there are only int and float, no normalized.
            //   Also pass through 2D formats like O8R8G8B8 and not-yet-supported formats like AF16.
            pRes->cout[0].ui = cin.red.ui;
            pRes->cout[1].ui = cin.green.ui;
            pRes->cout[2].ui = cin.blue.ui;
            pRes->cout[3].ui = cin.alpha.ui;
            break;
    }
    return rc;
}

//! -----------------------------------------------------------------------------
//!  u32IsNaN :Used to check if the passed in UINT32 value is "Not A Number".
//! -----------------------------------------------------------------------------
bool zbcColwert::u32IsNaN(UINT32 val)
{
    int exp = (val >> 23) & 0xff;
    int mant = (val & 0x7fffff);
    return (exp == 0xff && mant != 0);
}

//! -----------------------------------------------------------------------------
//!  Local routines used for color format colwersion
//! -----------------------------------------------------------------------------

//! -----------------------------------------------------------------------------
//!  UINT to UINT format colwersion routines.
//! -----------------------------------------------------------------------------
//! -----------------------------------------------------------------------------
//!  uint32ToUintA: Colwert a given UINT32 value to another UNIT format based
//!  on the number bits specifeid in destination format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::uint32ToUintA( UINT32 din, int ibits )
{
    unsigned int dout;
    unsigned int uidin = static_cast<unsigned int>(din);
    unsigned int max = (1 << ibits) - 1; //  example if ibits=8 then imax = (256-1) = 255
    dout = uidin;
    if (uidin > max )
        dout = max;
    return (dout);
}

//! -----------------------------------------------------------------------------
//!  uint32ToUint16: Colwert a given UINT32 value to UINT16 format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::uint32ToUint16( UINT32 din )
{
  return (UINT32)(uint32ToUintA( din, 16));
}

//! -----------------------------------------------------------------------------
//!  uint32ToUint10: Colwert a given UINT32 value to UINT10 format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::uint32ToUint10( UINT32 din )
{
  return (UINT32)(uint32ToUintA( din, 10));
}

//! -----------------------------------------------------------------------------
//!  uint32ToUint08: Colwert a given UINT32 value to UINT08 format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::uint32ToUint08( UINT32 din )
{
  return (UINT32)(uint32ToUintA( din, 8));
}

//! -----------------------------------------------------------------------------
//!  uint32ToUint02: Colwert a given UINT32 value to UINT02 format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::uint32ToUint02( UINT32 din )
{
  return (UINT32)(uint32ToUintA( din, 2));
}

//! -----------------------------------------------------------------------------
//!  ToU032: Colwert a given input data to U032 format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::ToU032(UINT32 num)
{
    return (uint32ToUintA( num, 32));
}

//! -----------------------------------------------------------------------------
//!  ToU016: Colwert a given input data to U016 format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::ToU016(UINT32 num)
{
    return (uint32ToUintA( num, 16));
}

//! -----------------------------------------------------------------------------
//!  ToU011: Colwert a given input data to U011 format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::ToU011(UINT32 num)
{
    return (uint32ToUintA( num, 11));
}

//! -----------------------------------------------------------------------------
//!  ToU010: Colwert a given input data to U010 format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::ToU010(UINT32 num)
{
    return (uint32ToUintA( num, 10));
}

//! -----------------------------------------------------------------------------
//!  ToU008: Colwert a given input data to U008 format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::ToU008(UINT32 num)
{
    return (uint32ToUintA( num, 8));
}

//! -----------------------------------------------------------------------------
//!  ToU002: Colwert a given input data to U002 format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::ToU002(UINT32 num)
{
    return (uint32ToUintA( num, 2));
}

//! -----------------------------------------------------------------------------
//!  FP to FP format colwersion routines.(FP : Floating Point)
//! -----------------------------------------------------------------------------
//! -----------------------------------------------------------------------------
//!  fp32ToFp16: Colwert from FP32 to FP16 format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::fp32ToFp16( UINT32 in)
{
    fluint fl;
    fl.ui = in;
    return fp32ToFp16(fl.f);
}

//! -----------------------------------------------------------------------------
//!  fp16ToFp11: Colwert from FP16 to FP11 format.
//! -----------------------------------------------------------------------------
UINT16 zbcColwert::fp16ToFp11( UINT16 din)
{
    // S1E5M10 -> E5M6: negative -> zero, and simply truncate otherwise
    if (din & 0x8000)
        return 0;
    return din >> 4;

}

UINT32 zbcColwert::fp32ToFp16(float f)
{
    UINT32 a = *(UINT32 *)&f;
    int EMAX16 = 0x1f;  // 2^5 - 1
    int EMAX32 = 0Xff;  // 2^8 - 1
    int exp, m, s;
    UINT32 fp16;

    // sign bit.
    s = (a>>31) & 0x1;

    // exponent.
    exp = (a>>23) & 0xff;

    // mantisa. last 23 bits
    m = a & 0x7fffff;

    // +Zero/-Zero
    if( (exp == 0) && (m == 0) )
    {
        fp16 = 0;
    }

    // DENORM
    // a. smallest 32bit normalized value = 1/(2^30)
    // b. smallest 16bit nonzero value    = 1/(2^24)
    // since (a)<(b), denormalized 32bit values truncated to zero in 16bit fp.
    else if( (exp == 0) && (m !=0) )
    {
        fp16 = 0;
    }

    // +Inf/-Inf
    else if( (exp == EMAX32) && (m == 0) )
    {
        fp16 = EMAX16<<10;
    }

    // NaN
    else if( (exp == EMAX32) && (m != 0) )
    {
        fp16 = (EMAX16<<10) | 0x3ff;
    }

    // NORMALIZED
    else
    {
        int bias16 = 0xf;   // 2^4 - 1
        int bias32 = 0x7f;  // 2^7 - 1
        int exp16 = exp - bias32 + bias16;
        int m16;

        // Inf
        if( (exp16 >= EMAX16) || (exp16==EMAX16-1 && m>(0x3ff<<13)) )
        {
            fp16 = EMAX16<<10;
        }

        // normalize
        else if( exp16>0 )
        {
            m16 = m>>13;
            fp16 = (exp16<<10) | m16;
        }

        // denormalize
        else if( exp16 > -10 )
        {
            m16 = 0x200 | (m>>14); // first 9 bits of mantisa | 10 0000 0000 (because it's normalized)
            fp16 = m16>>(-exp16);
            exp16 = 0;
        }

        // truncate to zero
        else
        {
            fp16 = 0;
        }
    }
    if( s==1 )
    {
        fp16 |= 0x00008000;
    }
    return fp16;
}

//! -----------------------------------------------------------------------------
//!  fp16ToFp10: Colwert from FP16 to FP10 format.
//! -----------------------------------------------------------------------------
UINT16 zbcColwert::fp16ToFp10( UINT16 din)
{
    // S1E5M10 -> E5M5: negative -> zero, and simply truncate otherwise
    if (din & 0x8000)
        return 0;
    return din >> 5;
}

//! -----------------------------------------------------------------------------
//!  fp16ToFp32: Colwert from FP16 to FP32 format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::fp16ToFp32( UINT16 in )
{
    unsigned int fp16 = (in & 0x0000ffff);
    int EMAX16 = 0x1f;  //   2^5 - 1
    int EMAX32 = 0Xff;  //   2^8 - 1
    int exp, m, s;
    unsigned int fp32;

    fp16 &= 0x0000ffff ;

    //   sign bit.
    s = (fp16>>15) & 0x1;

    //   exponent.
    exp = (fp16>>10) & 0x1f;

    //   mantisa. last 10 bits
    m = fp16 & 0x3ff;

    //   +Zero/-Zero
    if( (exp == 0) && (m == 0) )
    {
        fp32=0;
    }

    //   DENORM  val = (-1)^s * 2^(1-BIAS) * m/(2^M)
    //               ==(-1)^s * m/(2^24)
    else if( (exp == 0) && (m != 0) )
    {
        float val = (float)(m/16777216.0);
        memcpy(&fp32, &val, sizeof(val));
    }

    //   +Inf/-Inf
    else if( (exp == EMAX16) && (m == 0) )
    {
        fp32 = EMAX32<<23;
    }

    //   NaN
    else if( (exp == EMAX16) && (m != 0) )
    {
        fp32 = (EMAX32<<23) | 0x7ffff;
    }

    //   NORMALIZED.
    else
    {
        //   colwersion.
        int bias16 = 0xf;   //   2^4 - 1
        int bias32 = 0x7f;  //   2^7 - 1
        int exp32 = exp - bias16 + bias32;

        fp32 = (exp32<<23) | (m<<13);
    }

    //   sign bit.
    if( s==1 )
    {
        fp32 |= 0x80000000;
    }

    return UINT32( fp32 );
}

//! -----------------------------------------------------------------------------
//!  Float to UNORM format colwersion routines.
//! -----------------------------------------------------------------------------
//! -----------------------------------------------------------------------------
//!  floatToUnorm16: Colwert from FLOAT to UNORM16 format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::floatToUnorm16(UINT32 val)
{
    UINT32 val32;
    val32 = floatToUnorm(val, 16);
    return val32;
}

//! -----------------------------------------------------------------------------
//!  floatToUnorm02: Colwert from FLOAT to UNORM02 format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::floatToUnorm10(UINT32 val)
{
    return floatToUnorm(val, 10);
}

//! -----------------------------------------------------------------------------
//!  floatToUnorm08: Colwert from FLOAT to UNORM08 format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::floatToUnorm08(UINT32 val)
{
    return floatToUnorm(val, 8);
}

//! -----------------------------------------------------------------------------
//!  floatToUnorm_ONE: Colwert from FLOAT to UNORM_ONE format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::floatToUnormOne(UINT32 val)
{
    fluint fl;
    fl.ui = val;
    if (((int)(fl.f)) < 1)
    {
        Printf(Tee::PriHigh, "ERROR: INVALID DATA . Should be greater than or equal to 1.0 \n");
    }
    return 0xffffffff;

}

//! -----------------------------------------------------------------------------
//!  fp16ToFloat: Colwert from FP16 to FP32 format(represented as float).
//! -----------------------------------------------------------------------------
float zbcColwert::fp16ToFloat( UINT16 f )
{
    fluint fl;
    fl.ui = fp16ToFp32( f );
    return fl.f;
}

//! -----------------------------------------------------------------------------
//!  fp16ToUnorm08: Colwert from FP16 to UNORM08 format.
//! -----------------------------------------------------------------------------
 UINT32 zbcColwert::fp16ToUnorm08( UINT32 f )
 {
      fluint fl;
      fl.f = fp16ToFloat( f );
    return floatToUnorm( fl.ui , 8) ;
}

//! -----------------------------------------------------------------------------
//!  floatToSnorm16:Colwert from FLOAT to SNORM16(signed normalized 16 bit FMT).
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::floatToSnorm16(UINT32 val)
{
    return floatToSnorm(val, 16);
}

//! -----------------------------------------------------------------------------
//!  floatToSnorm08:Colwert from FLOAT to SNORM08(signed normalized 8 bit FMT).
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::floatToSnorm08(UINT32 val)
{
    return floatToSnorm(val, 8);
}

//! -----------------------------------------------------------------------------
//!  INT to SINT(signed integer) format colwersion routines.
//! -----------------------------------------------------------------------------
//! -----------------------------------------------------------------------------
//!  int32ToSintA: CGeneric Routine to colwert from INT32 to the signed integer
//!  format based on the specied number of integer bits in the target format.
//! -----------------------------------------------------------------------------
int zbcColwert::int32ToSintA( UINT32 din, int ibits )
{
    int dout;
    int idin = din;
    int imin =  (-1 * (1 << (ibits-1)));// example if ibits=8 then imin = -1*(128) = -128
    int imax =  (1 << (ibits-1)) - 1;   // example if ibits=8 then imax = (128-1) = 127
    dout = idin;
    if (idin < imin)
        dout = imin;
    if (idin > imax)
        dout = imax;
    return (dout);
}

//! -----------------------------------------------------------------------------
//!  int32ToSint32: Colwert from INT32 to SINT32 format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::int32ToSint32( UINT32 din )
{
  return int32ToSintA( din, 32);
}

//! -----------------------------------------------------------------------------
//!  int32ToSint16: Colwert from INT32 to SINT16 format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::int32ToSint16( UINT32 din )
{
  return int32ToSintA( din, 16);
}

//! -----------------------------------------------------------------------------
//!  int32ToSint08: Colwert from INT32 to SINT08 format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::int32ToSint08( UINT32 din )
{
  return int32ToSintA( din, 8);
}

//! -----------------------------------------------------------------------------
//!  unorm10ToUnorm2: Colwert from UNORM10 to UNORM02 format.
//! -----------------------------------------------------------------------------

UINT32 zbcColwert::unorm10ToUnorm2(UINT32 u10)
{
    /*  Only 4 bits of the u10 are needed for the colwersion.
     *  u10        -> u2
     *
     *  0000000000 -> 0
     *  ...
     *  0010111111 -> 0
     *  0011000000 -> 1
     *  ...
     *  0111111111 -> 1
     *  1000000000 -> 2
     *  ...
     *  1100111111 -> 2
     *  1101000000 -> 3
     *  ...
     *  1111111111 -> 3
     */
    UINT32 temp = u10 >> 6;
    if (temp < 3)
        return 0;
    else if (temp < 8)
        return 1;
    else if (temp < 13)
        return 2;
    else
        return 3;
}

//! -----------------------------------------------------------------------------
//!  colwLinearFp16sRGB8: Colwert any input data in UINT16 format to sRGB8 format.
//! -----------------------------------------------------------------------------
UINT08 zbcColwert::colwLinearFp16sRGB8(UINT16 ufp16)
{
    int ifp16 = ufp16;
    int isrgb;

    if (ifp16 & 0x8000)//  negative number
        ifp16 = 0;

    if (((ifp16 & 0x7C00) == 0x7C00) && //  NAN
        (ifp16 & 0x03FF))
        ifp16 = 0;

    if (ifp16 < FP16_1)
        isrgb = getLinearSRGB8ulp06(ifp16);
    else
        isrgb = 0xff;

    return (ToU008(isrgb));
}

//! -----------------------------------------------------------------------------
//!  floatToSnorm: Colwert any input data in Float format to SNORM format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::floatToSnorm(UINT32 val, UINT32 ibits)
{
    if (sign(val))// Check for the sign bit.
    {
        val = -(INT32)floatToUnorm(val & 0x7FFFFFFF, ibits-1);
        val &= UINT32((UINT64(1) << ibits) - 1);
    }
    else
    {
        val = floatToUnorm(val, ibits-1);
    }
    return val;
}

//! -----------------------------------------------------------------------------
//!  floatToUnorm: Generic Function Colwert any input data in Float format to
//!                UNORM format.
//! -----------------------------------------------------------------------------
UINT32 zbcColwert::floatToUnorm(UINT32 val, UINT32 ibits)
{
    // NaN -> zero
    if ((exponent(val) == 255) && (mantissa(val) != 0))
        return 0;

    // negative -> zero
    if (sign(val))
        return 0;

    if (ibits == 2)
    {
        UINT32 u10 = floatToUnormA(val, 10);
        return unorm10ToUnorm2(u10);
    }
    else if (ibits == 5)
    {
        UINT32 u8 = floatToUnormA(val, 8);
        return (u8 >> 3);
    }
    else if (ibits == 6)
    {
        UINT32 u8 = floatToUnormA(val, 8);
        return (u8 >> 2);
    }
    else if (ibits == 24)
    {
        return floatToUnorm24Tox(val);
    }
    else
    {
        return floatToUnormA(val, ibits);
    }

}

UINT16 zbcColwert::uint32ToFP16(UINT32 in)
{
    return Utility::Float32ToFloat16(((FLOAT32)in / 0xFF));
}

//! -----------------------------------------------------------------------------
//!  floatToUnormA: Static helper function to colwert any input data in Float
//!                 format to UNORM format.
//! -----------------------------------------------------------------------------
static UINT32 floatToUnormA(UINT32 val, UINT32 ibits)
{
    UINT32 fbits = 4;
    // ibits = integer bits
    // fbits = fraction bits (or guard bits)
    float f = *(float *)&val;

    if (f <= 0)
        return 0;
    if (f >= 1)
        return ( (UINT64(1)<<ibits) - 1);

    UINT64 xn = UINT64(f * (UINT64(1) << (ibits + fbits)));
    UINT64 x = xn >> ibits;
    UINT64 h = (1 << (fbits - 1)) - 1;

    return UINT32((xn - x + h) >> fbits);

}

//! -----------------------------------------------------------------------------
//!  floatToUnormA: Static helper function to colwert any input data in Float
//!                 format to UNORM format.
// !                Taken from //hw/lw5x/clib/lwshared/float_to_unorm.cpp
//                  toksvig agorithm for colwersion, modified to yield 0.5->0x800000
//! -----------------------------------------------------------------------------
static UINT32 floatToUnorm24Tox(UINT32 f)
{
    int exp = exponent(f) - 127;
    UINT32 mant = mantissa(f) | 1 <<23; // 24 bits

    // -Inf -> 0.0
    if (sign(f) == 1 && exp == (255-127) && mant == 0x800000)
        return 0;

    // +Inf -> 1.0
    if (!sign(f) && exp == (255-127) && mant == 0x800000)
        return 0xffffff;

    // very small -> zero
    if (exp<-25) // f<2^-25
        return 0;

    // large -> one
    if (exp>=0) // f>=1.0
        return 0xffffff;

    // one -> half+eps : decrement mantissa
    if ((exp==-1) && (mant!=0x800000)) // 0.5<f<1.0
        return --mant;

    // exactly half -> return 0x800000
    if ((exp==-1) && (mant==0x800000)) // f==0.5
        return 0x800000;

    // below half, exponent in -25..-2, conditionally increment shifted mantissa
    int shift = -exp-2; // 0..23
    UINT32 before_round = mant>>shift;
    if (before_round&1) // round up
        if (shift>2 || mant&((1<<shift)-1))
            return before_round/2+1;
    return before_round/2;
}

//! -----------------------------------------------------------------------------
//!  sign: Return the sign bit information for any given UINT32 val.
//! -----------------------------------------------------------------------------
static bool sign(UINT32 f)
{
    return (f & 0x80000000) != 0;
}

//! -----------------------------------------------------------------------------
//!  exponent: Return the Exponent part of any given UINT32 val.
//! -----------------------------------------------------------------------------
static UINT32 exponent(UINT32 f)
{
    return (f & 0x7F800000) >> 23;
}

//! -----------------------------------------------------------------------------
//!  mantissa: Return the Mantissa part of any given UINT32 val.
//! -----------------------------------------------------------------------------
static UINT32 mantissa(UINT32 f)
{
    return (f & 0x007FFFFF);
}

//! -----------------------------------------------------------------------------
//!  mantissa: Return the Mantissa part of any given UINT32 val.
//! -----------------------------------------------------------------------------
static UINT08 getLinearSRGB8ulp06(UINT32 i)
{
    return linsrgb8ulp06[i];
}
