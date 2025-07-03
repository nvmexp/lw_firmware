/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file    gf100ecid.cpp
 * @brief   Implement class GF100-specific reading of ECID.
 */

#include "gf100ecid.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/platform.h"
#include "core/include/utility.h"

#include "fermi/gf100/dev_top.h"

EnableGF100FuseRegisters::EnableGF100FuseRegisters(GpuSubdevice* pGpuSub)
    : m_pGpuSub(pGpuSub)
{
    m_DebugSave = pGpuSub->RegRd32(LW_PTOP_DEBUG_1);
    pGpuSub->RegWr32(LW_PTOP_DEBUG_1,
            FLD_SET_DRF(_PTOP, _DEBUG_1, _FUSES_VISIBLE, _ENABLED,
                m_DebugSave));
}

EnableGF100FuseRegisters::~EnableGF100FuseRegisters()
{
    m_pGpuSub->RegWr32(LW_PTOP_DEBUG_1, m_DebugSave);
}

// This is functionally the same as GetGT212RawEcid with the following exceptions:
// - The register offsets are different.
// - LW_FUSE_OPT_OPS_RESERVED field is included.
// - The ability to reverse all of the fields has been added. Needed for UnlockHost2Jtag().
// - We use CopyBits() API insted of playing around with the specific 32bit boundries.
RC GetGF100RawEcid(GpuSubdevice * pSubdev, vector<UINT32>*pEcid, bool bReverseFields)
{
    const Platform::SimulationMode Mode = Platform::GetSimulationMode();
    if ((Mode != Platform::Hardware) && (Mode != Platform::RTL))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    EnableGF100FuseRegisters EnableFuses(pSubdev);
    RegHal &regs = pSubdev->Regs();

    if (!regs.HasReadAccess(MODS_FUSE_OPT_VENDOR_CODE) ||
        !regs.HasReadAccess(MODS_FUSE_OPT_FAB_CODE) ||
        !regs.HasReadAccess(MODS_FUSE_OPT_LOT_CODE_0) ||
        !regs.HasReadAccess(MODS_FUSE_OPT_LOT_CODE_1) ||
        !regs.HasReadAccess(MODS_FUSE_OPT_WAFER_ID) ||
        !regs.HasReadAccess(MODS_FUSE_OPT_X_COORDINATE) ||
        !regs.HasReadAccess(MODS_FUSE_OPT_Y_COORDINATE) ||
        !regs.HasReadAccess(MODS_FUSE_OPT_OPS_RESERVED))
    {
        return RC::PRIV_LEVEL_VIOLATION;
    }

    const UINT32 size = (regs.LookupMaskSize(MODS_FUSE_OPT_VENDOR_CODE_DATA)
                     + regs.LookupMaskSize(MODS_FUSE_OPT_FAB_CODE_DATA)
                     + regs.LookupMaskSize(MODS_FUSE_OPT_LOT_CODE_0_DATA)
                     + regs.LookupMaskSize(MODS_FUSE_OPT_LOT_CODE_1_DATA)
                     + regs.LookupMaskSize(MODS_FUSE_OPT_WAFER_ID_DATA)
                     + regs.LookupMaskSize(MODS_FUSE_OPT_X_COORDINATE_DATA)
                     + regs.LookupMaskSize(MODS_FUSE_OPT_Y_COORDINATE_DATA)
                     + regs.LookupMaskSize(MODS_FUSE_OPT_OPS_RESERVED_DATA)
                     + 31) / 32;
    pEcid->clear();
    pEcid->resize(size);

    vector<UINT32> srcArray;
    srcArray.push_back(regs.Read32(MODS_FUSE_OPT_VENDOR_CODE_DATA));  //VendorCode
    srcArray.push_back(regs.Read32(MODS_FUSE_OPT_FAB_CODE_DATA));     //FabCode
    srcArray.push_back(regs.Read32(MODS_FUSE_OPT_LOT_CODE_0_DATA));   //LotCode0
    srcArray.push_back(regs.Read32(MODS_FUSE_OPT_LOT_CODE_1_DATA));   //LotCode1
    srcArray.push_back(regs.Read32(MODS_FUSE_OPT_WAFER_ID_DATA));     //WaferId
    srcArray.push_back(regs.Read32(MODS_FUSE_OPT_X_COORDINATE_DATA)); //XCoordinate
    srcArray.push_back(regs.Read32(MODS_FUSE_OPT_Y_COORDINATE_DATA)); //YCoordinate
    srcArray.push_back(regs.Read32(MODS_FUSE_OPT_OPS_RESERVED_DATA)); //Rsvd

    vector<UINT32> numBits;
    numBits.push_back(regs.LookupMaskSize(MODS_FUSE_OPT_VENDOR_CODE_DATA));
    numBits.push_back(regs.LookupMaskSize(MODS_FUSE_OPT_FAB_CODE_DATA));
    numBits.push_back(regs.LookupMaskSize(MODS_FUSE_OPT_LOT_CODE_0_DATA));
    numBits.push_back(regs.LookupMaskSize(MODS_FUSE_OPT_LOT_CODE_1_DATA));
    numBits.push_back(regs.LookupMaskSize(MODS_FUSE_OPT_WAFER_ID_DATA));
    numBits.push_back(regs.LookupMaskSize(MODS_FUSE_OPT_X_COORDINATE_DATA));
    numBits.push_back(regs.LookupMaskSize(MODS_FUSE_OPT_Y_COORDINATE_DATA));
    numBits.push_back(regs.LookupMaskSize(MODS_FUSE_OPT_OPS_RESERVED_DATA));

    if (bReverseFields)
    {
        std::reverse(srcArray.begin(), srcArray.end());
        std::reverse(numBits.begin(), numBits.end());
    }

    UINT32 dstOffset = 0;
    for (unsigned i=0; i < srcArray.size(); i++)
    {
        Utility::CopyBits(srcArray, *pEcid, i*32, dstOffset, numBits[i]);
        dstOffset += numBits[i];
    }

    // swap the indices so ascii printing shows MSB on left and LSB on right.
    std::reverse(pEcid->begin(), pEcid->end());

    return (OK);
}
//------------------------------------------------------------------------------------------------
// Print the ECID info using the  Universal ECID String Format
// Universal ECID String Format: <A>-<B><C>-<WW>_x<X>_y<Y> using the table below
//--------------------------------------------------------------------------------------- 
//| Field  | LW FUSENAME         | Bits  |   Printed as      | Notes                     |
//|--------|---------------------|-------|-------------------|---------------------------|
//| <A>    | OPT_VENDOR_CODE     |  4    |   String          |   see Vendors[] below     |
//|--------|---------------------|-------|-------------------|---------------------------|
//| <B>    | OPT_FAB_CODE        |  6    |   Alphanumeric    |   see LotCode2Char[]      |
//|--------|---------------------|-------|-------------------|---------------------------|
//| <C>    | OPT_LOT_CODE_1 |    |  60   |   Alphanumeric    |   see LotCode2Char[],     |
//|        | OPT_LOT_CODE_0      |       |                   |   no leading zeros        |
//|--------|---------------------|-------|-------------------|---------------------------|
//| <WW>   | OPT_WAFER_ID        |  6    |  Unsigned Integer |  leading zero if < 10     |
//|--------|---------------------|-------|-------------------|---------------------------|
//| <X>    | OPT_X_COORDINATE    |  9    |  Signed Integer   |  no leading zeros         |
//|--------|---------------------|-------|-------------------|---------------------------|
//| <Y>    | OPT_Y_COORDINATE    |  9    |  Unsigned Integer |  no leading zeros         |
//|--------|---------------------|-------|-------------------|---------------------------|
//|RESERVED| OPT_OPS_RESERVED    |  6    |  n/a              |                           |
//|--------|---------------------|-------|-------------------|---------------------------|
//|TOT BITS| -                   |  100  |                   |                           |
//---------------------------------------------------------------------------------------
RC GetGF100CookedEcid(GpuSubdevice* pSubdev, string* pEcid)
{
    const Platform::SimulationMode Mode = Platform::GetSimulationMode();
    if ((Mode != Platform::Hardware) && (Mode != Platform::RTL))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    EnableGF100FuseRegisters EnableFuses(pSubdev);

    const UINT32 VendorCode     = pSubdev->Regs().Read32(MODS_FUSE_OPT_VENDOR_CODE_DATA);
    const UINT32 LotCode0       = pSubdev->Regs().Read32(MODS_FUSE_OPT_LOT_CODE_0_DATA);
    const UINT32 LotCode1       = pSubdev->Regs().Read32(MODS_FUSE_OPT_LOT_CODE_1_DATA);
    const UINT64 LotCode        = (UINT64)(LotCode1) << 32 | (UINT64)(LotCode0);
    const UINT32 FabCode        = pSubdev->Regs().Read32(MODS_FUSE_OPT_FAB_CODE_DATA);
    const int    XCoordinate    = pSubdev->Regs().Read32(MODS_FUSE_OPT_X_COORDINATE_DATA);
    const UINT32 YCoordinate    = pSubdev->Regs().Read32(MODS_FUSE_OPT_Y_COORDINATE_DATA);
    const UINT32 WaferId        = pSubdev->Regs().Read32(MODS_FUSE_OPT_WAFER_ID_DATA);

    char LotCodeCooked[16] = "*****";
    const char LotCode2Char[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                "????????????????????????????????????";
    vector <const char *> Vendors = 
    {
        "N/A", "TSMC", "UMC", "CHRT", "SAMSUNG", "IBM", "SMIC", "TOSHIBA", "SONY"
    };
    // range check the VendorCode
    if (VendorCode == 0)
        Printf(Tee::PriWarn, "ECID Vendor code is invalid and likely unfused.\n");

    if (VendorCode >= Vendors.size())
    {
        MASSERT(!"Ecid VendorCode is invalid");
        Printf(Tee::PriError, "ECID Vendor code is out of range.\n");
        return RC::SOFTWARE_ERROR;
    }

    for (UINT32 CharIdx = 0; CharIdx < 10; CharIdx++)
        LotCodeCooked[9-CharIdx] = LotCode2Char[(LotCode >> 6*CharIdx) & 0x3F];
    LotCodeCooked[10] = '\0';

    //Note foundry lot codes do not start with zero, so no leading zeros in the string are allowed
    UINT32 lotCodeIdx = 0;
    while (LotCodeCooked[lotCodeIdx] == '0')
    {
        lotCodeIdx++;
    }
    const int xCoordinateSignBit =
        pSubdev->Regs().LookupMaskSize(MODS_FUSE_OPT_X_COORDINATE_DATA) - 1;


    *pEcid = Utility::StrPrintf("%s-%c%s-%02u_x%1d_y%1u",
            Vendors[VendorCode],
            LotCode2Char[FabCode & 0x3F],
            &LotCodeCooked[lotCodeIdx],
            WaferId,
            XCoordinate - 2 * (XCoordinate & (1 << xCoordinateSignBit)),
            YCoordinate);

    return OK;
}
