/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "lwmisc.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "mdiag/lwgpu.h"
#include "surfmgr.h"
#include "mdiag/lwgpu.h"

#include "gpu/utility/blocklin.h"
#include "mdiag/tests/gpu/lwgpu_subchannel.h"

#include "zlwll.h"
#include "mdiag/sysspec.h"
#include "g00x/g000/dev_graphics_nobundle.h"
#include "g00x/g000/hwproject.h"
#include "class/cl9097.h"
#include "gpu/utility/surfrdwr.h"
#include "mdiag/smc/smcengine.h"

#define ZLWLL_MAX_GPCS            32 // FIXME should be from scalability define
#define ZLWLL_MAX_TPCS            256 // FIXME should be from scalability define
#define ZLWLL_MAX_CRSTR_TILES     258 // FIXME should be from scalability define

typedef struct
{
    LW2080_CTRL_GR_INFO shaderPipeCount;
    LW2080_CTRL_GR_INFO litterNumGpcs;
} MYGRINFO;

static UINT32 zc_unsqueeze(UINT32 z12, UINT32 zformat, UINT32 z_dir, UINT32 zbits, bool zf32_as_z16, bool is_float, bool is_max)
{
    UINT32 z = 0;

    if (zformat == LW_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_ZFORMAT_ZTRICK) {

        UINT32 exponent, mantissa, ones;
        exponent = z_dir ? ((z12 >> 9) ^ 7) : (z12 >> 9);
        mantissa = z12 & 0x1FF;

        if (z_dir == 0 && exponent < 7)
            ones = 0x7E00 & (0x1F8000 >> exponent);  // leading zero, "exponent" ones, hidden zero
                                                     //   (no hidden zero if exponent = 6)
        else if (z_dir == 0 && exponent == 7)
            ones = 0x8000;                           // leading one only
        else if (z_dir == 1 && exponent < 7)
            ones = 0x8000 | (0xFE00 & (0x4000 >> exponent)); // leading one, "exponent" zeroes, hidden one
                                                     //   (no hidden one if exponent = 6)
        else //if (z_dir == 1 && exponent == 7)
            ones = 0;                                // leading zero only

        if (exponent == 7)
            z = ones | (mantissa << 6);              // leading bit, followed by "mantissa"
        else if (exponent == 6)
            z = ones | mantissa;                     // 0/1 followed by 111111/000000, followed by "mantissa"
        else
            z = ones | (mantissa << (5 - exponent)); // 0/1 followed by exponent ones/zeroes, then "mantissa"

        if (is_max)
        {
            if (exponent != 7)
                z |= 0x1F >> exponent;               // pad mantissa with ones if rounding up
            else
                z |= 0x3F;                           // pad mantissa with ones if rounding up
        }

    } else if (zformat == LW_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_ZFORMAT_FP) {

        UINT32 exponent, mantissa, ones;
        exponent = z_dir ? ((z12 >> 9) ^ 7) : (z12 >> 9);
        mantissa = z12 & 0x1FF;
        if (!z_dir)
            ones = 0xFE00 & (0x7F0000 >> exponent);  // "exponent" is leading ones; hidden zero follows
        else
            ones = 0xFE00 & (0x8000 >> exponent);    // "exponent" is leading zeroes; hidden one follows

        if (exponent == 7)
            z = ones | mantissa;                     // 7 ones or zeroes, followed by "mantissa"
        else
            z = ones | (mantissa << (6 - exponent)); // leading ones/zeroes, hidden zero/one, followed by "mantissa"

        if (is_max)
            z |= 0x3f >> exponent;                   // pad mantissa with ones if rounding up

    } else if ( zformat == LW_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_ZFORMAT_MSB ||
                (zformat == LW_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_ZFORMAT_ZF32_1 && z_dir )
               ) {
        z = (z12 << 4) | (is_max ? 0xF : 0x0);
    }
    else if (zformat == LW_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_ZFORMAT_ZF32_1 && !z_dir )
    {
        int top3bits= (z12 >> 9) &7;
        if(top3bits == 0) {
            z = ((z12&0x1ff) << 6);
            if (is_max)
                z |= 0x7f;
        }
        else if(top3bits == 7) {
            z = 0xC000 | ((z12&0x1ff) << 5);
            if (is_max)
                z |= 0x1f;
        }
        else if(top3bits == 6) {
            z = 0xBE00 | ((z12&0x1ff) << 0);
        }
        else if(top3bits == 5) {
            z = 0xBC00 | ((z12&0x1ff) << 0);
        }
        else if(top3bits == 4) {
            z = 0xB800 | ((z12&0x1ff) << 1);
            if (is_max) z |= 0x1;
        }
        else if(top3bits == 3) {
            z = 0xB000 | ((z12&0x1ff) << 2);
            if (is_max) z |= 0x3;
        }
        else if(top3bits == 2) {
            z = 0xA000 | ((z12&0x1ff) << 3);
            if (is_max) z |= 0x7;
        }
        else if(top3bits == 1) {
            z = 0x8000 | ((z12&0x1ff) << 4);
            if (is_max) z |= 0xf;
        }
    }

    if (is_float && !zf32_as_z16) // don't need to do Zlwll float-point colwerstion if we treat Z as Z16
    {
        // 32-bit float
        if (z & 0x8000)
        {
            z ^= 0x8000;  // Positive numbers are stored as 0x8000 -  0xffff
            z = (z << 16) | (is_max ? 0xFFFF : 0x0000);
        }
        else
        {
            z ^= 0xFFFF;  // Negative numbers are stored as 0x0000 - 0x7fff, with one's complement to reverse the order
            z = (z << 16) | (is_max ? 0x0000 : 0xFFFF);
        }
        // must preserve Inf; we don't store NaN, so just checking the exponent is sufficient
        if ((z & 0x7F800000) == 0x7F800000)
            z &= 0xFF800000;
    } else {
        if (zbits == 16)
        {
            z |= is_max ? 0xFF : 0x00;
        }
        else if (zbits == 24) // if zf32_as_z16 we just want the 16 MSBs
        {
            // 24-bit int
            z = (z << 8) | (is_max ? 0xFF : 0x00);
        }
    }
    if (is_float && zf32_as_z16) // Zlwll treats floating point z as fixed point
    {
        if( z == 0 && !is_max) // 0 goes to -inf
            z = 0xff800000;
        else if( z == 0xffff && is_max) // 0xffff goes to inf
            z = 0x7f800000;
        else
        {
            // Zlwll makes ZF32_as_z16 conservative by adding or subtracting one when testing against a RAM entry
            if(is_max)
                z+=1;
            else
                z-=1;
            float float_depth = (z << 8) / (float) 0xffffff; // divide  0xffffff since zlwll treats zf32_as_z16 as 16 upper bits of Z24
            z = (*(UINT32*)&float_depth);
        }
    }

    return z;
}

static bool stencil_test(int sfunc, int sref, int smask, int sval)
{
    sref &= smask;
    sval &= smask;
    switch (sfunc) {
    case LW_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SFUNC_NEVER:      return false;
    case LW_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SFUNC_LESS:       return sref<sval;
    case LW_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SFUNC_EQUAL:      return sref==sval;
    case LW_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SFUNC_LEQUAL:     return sref<=sval;
    case LW_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SFUNC_GREATER:    return sref>sval;
    case LW_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SFUNC_NOTEQUAL:   return sref!=sval;
    case LW_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SFUNC_GEQUAL:     return sref>=sval;
    case LW_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SFUNC_ALWAYS:     return true;
    default:
        ErrPrintf("DoZLwllSanityCheck: unknown stencil function!\n");
    }
    return false;
}

UINT32 ZLwll::SanityCheck
(
    UINT32* zbuffer,
    MdiagSurf* surfZ,
    LW50Raster* raster,
    UINT32 w,
    UINT32 startY,
    UINT32 h,
    AAmodetype aa,
    bool check,
    LWGpuSubChannel *subch,
    UINT32 gpuSubdevIdx,
    LWGpuResource *pGpuRes
)
{
    GpuSubdevice  *pSubdev = pGpuRes->GetGpuSubdevice(gpuSubdevIdx);
    static const int ERROR_LIMIT = 5;
    int num_errors = 0;
    UINT32 region;
    if (surfZ->GetZlwllRegion() >= 0)
    {
        region = (UINT32)surfZ->GetZlwllRegion();
    }
    else
    {
        WarnPrintf("ZLwll::SanityCheck: Invalid zlwll region id: %d, skip sanity check.\n", surfZ->GetZlwllRegion());
        return 0;
    }

    LWGpuChannel *pCh = subch->channel();

    SmcEngine *pSmcEngine = pCh->GetSmcEngine();
    LwRm *pLwRm = pCh->GetLwRmPtr();
    RegHal& regHal = pGpuRes->GetRegHal(pSubdev, pLwRm, pSmcEngine);

    UINT32 zcstatus = regHal.Read32(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS, region);
    if (!regHal.GetField(zcstatus, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_VALID))
    {
        WarnPrintf("DoZLwllSanityCheck: Zlwll region %d is invalid. Skipping sanity check.\n", region);
        return 0;
    }
    UINT32 zformat = regHal.GetField(zcstatus, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_ZFORMAT);
    UINT32 sfunc = regHal.GetField(zcstatus, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SFUNC);
    UINT32 sref  = regHal.GetField(zcstatus, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SREF);
    UINT32 smask = regHal.GetField(zcstatus, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SMASK);

    UINT32 zcwidth  = regHal.Read32(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSIZE_WIDTH, region) * regHal.LookupValue(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSIZE_WIDTH__MULTIPLE);
    UINT32 zcheight = regHal.Read32(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSIZE_HEIGHT, region) * regHal.LookupValue(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSIZE_HEIGHT__MULTIPLE);
    UINT32 zcoffsetx = regHal.Read32(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCOFFSET_X, region) * regHal.LookupValue(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCOFFSET_X__MULTIPLE);
    UINT32 zcoffsety = regHal.Read32(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCOFFSET_Y, region) * regHal.LookupValue(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCOFFSET_Y__MULTIPLE);

    UINT32 zcstart  = regHal.Read32(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTART_ADDR, region);

    UINT32 ram_format = regHal.Read32(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT, region);
    bool zdir_greater = regHal.TestField(zcstatus, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_ZDIR_GREATER);
    UINT32 total_ram_size = regHal.Read32(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCRAM_SIZE_TOTAL, region);
    UINT32 per_layer_ram_size = regHal.Read32(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCRAM_SIZE_PER_LAYER, region);

    UINT32 num_sms = regHal.Read32(MODS_PGRAPH_PRI_GPC0_CRSTR_MAP_TABLE_CONFIG_NUM_ENTRIES);
    UINT32 zlwll_table_size = regHal.Read32(MODS_PGRAPH_PRI_GPC0_ZLWLL_FS_NUM_SMS);
    UINT32 row_offset = regHal.Read32(MODS_PGRAPH_PRI_GPC0_ZLWLL_RAM_ADDR_ROW_OFFSET);
    UINT32 sm_num_rcp_conservative = regHal.Read32(MODS_PGRAPH_PRI_GPC0_ZLWLL_SM_NUM_RCP_CONSERVATIVE);

    // read PRIV register to determine if we are in a mode where Zlwll treats ZF32 as fixed point
    bool zf32_as_z16 = regHal.Read32(MODS_PGRAPH_PRI_GPC0_ZLWLL_CTX_DEBUG_ZF32_AS_Z16);

    UINT32 farthest_z = 0, farthest_znear = 0, nearest_z = 0;
    UINT32 sm_in_gpc_number_map[ZLWLL_MAX_TPCS];
    loadZlwllBankMap(&sm_in_gpc_number_map[0], subch, gpuSubdevIdx, pGpuRes);

    // get required gr info
    MYGRINFO grInfo;
    LW2080_CTRL_GR_GET_INFO_PARAMS grInfoParams = {0};

    grInfo.shaderPipeCount.index          = LW2080_CTRL_GR_INFO_INDEX_SHADER_PIPE_COUNT;
    grInfo.litterNumGpcs.index            = LW2080_CTRL_GR_INFO_INDEX_LITTER_NUM_GPCS;

    grInfoParams.grInfoListSize = sizeof (grInfo) / sizeof (LW2080_CTRL_GR_INFO);
    grInfoParams.grInfoList = (LwP64)&grInfo;

    RC rc;
    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(pSubdev),
        LW2080_CTRL_CMD_GR_GET_INFO,
        (void*)&grInfoParams,
        sizeof(grInfoParams));

    if (rc != OK)
    {
        ErrPrintf("Error reading number of GPC's and LITTER_NUM_GPCS from RM: %s\n", rc.Message());
        return 1;
    }

    UINT32  gpc_map[ZLWLL_MAX_CRSTR_TILES]; // which GPC owns each tile
    loadGpcMap(&gpc_map[0], subch, gpuSubdevIdx, pGpuRes);
    UINT32 num_gpcs = 0;
    for(UINT32 i=0;i<num_sms;i++)
        num_gpcs=max(num_gpcs,gpc_map[i]);
    num_gpcs+=1; // the number of TPCs is the highest GPC id read the the raster table plus one

    const bool is_s8 = (0 != ColorUtils::StencilBits(surfZ->GetColorFormat()));

    switch(ColorUtils::ZBits(surfZ->GetColorFormat()))
    {
        case 32:
            farthest_z = zdir_greater ? 0xFF800000 : 0x7F800000;
            if(zf32_as_z16)
                farthest_znear = zdir_greater ? 0x0 : 0x37ffff00; // minimum value we encode with 0xffff in new format
            else
                farthest_znear = zdir_greater ? 0xFF800000 : 0x7F800000;
            nearest_z = zdir_greater ? 0x7F800000 : 0xFF800000;
            break;
        case 24:
            farthest_z = zdir_greater ? 0x000000 : 0xFFFFFF;
            farthest_znear = zdir_greater ? 0x000000 : 0xFFFF00;
            nearest_z = zdir_greater ? 0xFFFFFF : 0x000000;
            break;
        case 16:
            farthest_z = zdir_greater ? 0x000000 : 0xFFFF;
            farthest_znear = zdir_greater ? 0x000000 : 0xFFFF;
            nearest_z = zdir_greater ? 0xFFFF : 0x000000;
            break;
        case 0:
            // If there are no Z bits, there has to be stencil.
            if (!is_s8)
            {
                ErrPrintf("zlwll sanity check done on surface with neither Z bits nor stencil bits.\n");
                return 1;
            }
            farthest_z = 0;
            farthest_znear = 0;
            nearest_z = 0;
            break;
        default:
            ErrPrintf("DoZLwllSanityCheck: unknown depth format\n");
            return 1;
    }

    InfoPrintf("DoZLwllSanityCheck: Using Zlwll region %d  %dx%d (start=0x%08x total_ram_size=0x%08x)\n", region, zcwidth, zcheight, zcstart,total_ram_size);
    InfoPrintf("DoZLwllSanityCheck:   ramformat=%d zdir=%d zformat=%d sfunc=%d sref=0x%02x smask=0x%02x zf32_as_z16=%d\n", ram_format, zdir_greater, zformat, sfunc, sref, smask, zf32_as_z16 );
    InfoPrintf("DoZLwllSanityCheck: Reading contents of Zlwll RAMs. numGpcs=%d numTpcs=%d numEntriesInZlwllTable=%d\n",num_gpcs,num_sms,zlwll_table_size);

    vector<UINT32> znearRAM[ZLWLL_MAX_GPCS][LW_PGRAPH_ZLWLL_NUM_ZNEAR];
    vector<UINT32> zfarRAM[ZLWLL_MAX_GPCS][LW_PGRAPH_ZLWLL_NUM_ZFAR];
    vector<UINT32> stencilRAM[ZLWLL_MAX_GPCS][LW_PGRAPH_ZLWLL_NUM_ZFAR];
    UINT32 stencilBanks[ZLWLL_MAX_GPCS];
    UINT32 zfarBanks[ZLWLL_MAX_GPCS];
    UINT32 znearBanks[ZLWLL_MAX_GPCS];
    UINT32 tiles_per_hypertile_row_per_gpc[ZLWLL_MAX_GPCS];

    for(UINT32 gpcId=0;gpcId<num_gpcs;gpcId++)
    {
        znearBanks[gpcId] = regHal.Read32(MODS_PGRAPH_PRI_GPCx_ZLWLL_FS_NUM_ACTIVE_BANKS, gpcId);
        stencilBanks[gpcId] = znearBanks[gpcId];
        zfarBanks[gpcId] =  znearBanks[gpcId];
        tiles_per_hypertile_row_per_gpc[gpcId] = regHal.Read32(MODS_PGRAPH_PRI_GPCx_ZLWLL_RAM_ADDR_TILES_PER_HYPERTILE_ROW_PER_GPC, gpcId);
        InfoPrintf("DoZLwllSanityCheck: gpc:0x%dx banks:0x%x tiles_per_hypertile_row:0x%x\n",gpcId,zfarBanks[gpcId],tiles_per_hypertile_row_per_gpc[gpcId]);
    }

    // Allocate memory for the decoded Zlwll ram
    // Read back the last ram entry in each region via PRI
    // We will use it to make sure the value read via methods is sane.
    for(UINT32 gpcId=0;gpcId<num_gpcs;gpcId++)
    {
        for (UINT32 bank=0; bank < znearBanks[gpcId]; bank++) {
            znearRAM[gpcId][bank].resize(total_ram_size);
            // read back the last ram entry
            for (UINT32 i=total_ram_size-1; i<total_ram_size; i++) {
                regHal.Write32(MODS_PGRAPH_PRI_GPCx_ZLWLL_ZCRAM_INDEX, gpcId, 
                    regHal.SetField(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCRAM_INDEX_WRITE_FALSE) |
                    regHal.SetField(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCRAM_INDEX_SELECT, regHal.LookupValue(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCRAM_INDEX_SELECT_ZNEAR0) + bank) |
                    regHal.SetField(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCRAM_INDEX_ADDRESS, (zcstart) + i));
                znearRAM[gpcId][bank][i] = regHal.Read32(MODS_PGRAPH_PRI_GPCx_ZLWLL_ZCRAM_DATA, gpcId);
            }
        }
        for (UINT32 bank=0; bank < zfarBanks[gpcId]; bank++) {
            zfarRAM[gpcId][bank].resize(total_ram_size);
            // read back the last ram entry
            for (UINT32 i=total_ram_size-1; i<total_ram_size; i++) {
                regHal.Write32(MODS_PGRAPH_PRI_GPCx_ZLWLL_ZCRAM_INDEX, gpcId,
                    regHal.SetField(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCRAM_INDEX_WRITE_FALSE) |
                    regHal.SetField(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCRAM_INDEX_SELECT, regHal.LookupValue(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCRAM_INDEX_SELECT_ZFAR0) + bank) |
                    regHal.SetField(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCRAM_INDEX_ADDRESS, (zcstart) + i));
                zfarRAM[gpcId][bank][i] = regHal.Read32(MODS_PGRAPH_PRI_GPCx_ZLWLL_ZCRAM_DATA, gpcId);
            }
        }
        for (UINT32 bank=0; bank < stencilBanks[gpcId]; bank++) {
            stencilRAM[gpcId][bank].resize(total_ram_size);
            // read back the last ram entry
            for (UINT32 i=total_ram_size-1; i<total_ram_size; i++) {
                regHal.Write32(MODS_PGRAPH_PRI_GPCx_ZLWLL_ZCRAM_INDEX, gpcId,
                    regHal.SetField(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCRAM_INDEX_WRITE_FALSE) |
                    regHal.SetField(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCRAM_INDEX_SELECT, regHal.LookupValue(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCRAM_INDEX_SELECT_STENCIL0) + bank) |
                    regHal.SetField(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCRAM_INDEX_ADDRESS, (zcstart) + i));
                stencilRAM[gpcId][bank][i] = regHal.Read32(MODS_PGRAPH_PRI_GPCx_ZLWLL_ZCRAM_DATA, gpcId);
            }
        }
    }

    // Read back ZLwll RAMs by method

    if (ReadZLwllRam(region, subch, pSubdev, pLwRm) != OK)
    {
        ErrPrintf("Read ZLwll RAMs by methods fail\n");
        return 1; // 1 error
    }

    vector<UINT08> Ram(static_cast<size_t>(m_ZLwllRam->GetSize()));
    GpuDevice *gpuDev = pSubdev->GetParentDevice();
    UINT32 gpuSubdev = 0;
    for (; gpuSubdev<gpuDev->GetNumSubdevices();)
    {
        if (pSubdev == gpuDev->GetSubdevice(gpuSubdev))
            break;
        else
            gpuSubdev++;
    }
    if (gpuSubdev == gpuDev->GetNumSubdevices())
    {
        WarnPrintf("Invalid subdevice\n");
        gpuSubdev = 0;
    }
    for(UINT32 i=0;i<m_ZLwllRam->GetSize();i++)
        Ram[i]=0;
    SurfaceUtils::ReadSurface(*(m_ZLwllRam->GetSurf2D()), 0, &Ram[0], (size_t)m_ZLwllRam->GetSize(), gpuSubdev);

    // Copy the read back ram into arrays that are easier split into zfar,znear, and stencil
    for(UINT32 gpcId=0;gpcId<num_gpcs;gpcId++) {
        for (UINT32 bank=0; bank < znearBanks[gpcId]; bank++) {
            for (UINT32 i=0; i<total_ram_size; i++) {
                // Go from the Zlwll address and bank, to the address in the buffer saved via method
                // offset by the header
                int aliquotAddress = grInfo.litterNumGpcs.data * LW_PGRAPH_ZLWLL_SAVE_RESTORE_HEADER_BYTES_PER_GPC;
                // Add the offset for this GPC
                aliquotAddress += gpcId * ( total_ram_size * LW_PGRAPH_ZLWLL_BYTES_PER_ALIQUOT_PER_GPC);
                // Add the offset for the aliquot address
                aliquotAddress += i * LW_PGRAPH_ZLWLL_BYTES_PER_ALIQUOT_PER_GPC;
                // Add offset to get to RAM for this bank
                aliquotAddress += bank * LW_PGRAPH_ZLWLL_BYTES_PER_ALIQUOT_PER_GPC / LW_PGRAPH_ZLWLL_NUM_ZFAR;
                UINT32 decodedZfarRAM=((UINT32) Ram[aliquotAddress+7])<<24 | ((UINT32) Ram[aliquotAddress+6])<<16 | ((UINT32) Ram[aliquotAddress+3]) << 8 | ((UINT32)Ram[aliquotAddress+2]);
                UINT32 decodedZnearRAM=((UINT32) Ram[aliquotAddress+1]&0xf)<<12 | ((UINT32) Ram[aliquotAddress+0])<< 4 | ((UINT32) Ram[aliquotAddress+1]&0xf0)>>4;
                UINT32 decodedStencilRAM =((UINT32) Ram[aliquotAddress+5])<<8 | ((UINT32) Ram[aliquotAddress+4]);
                // sanity check that the last ram entry read via method matches what is read via PRI for last ram entry
                if(i==total_ram_size-1)
                {
                    if( zfarRAM[gpcId][bank][i] != decodedZfarRAM || stencilRAM[gpcId][bank][i] != decodedStencilRAM || znearRAM[gpcId][bank][i] != decodedZnearRAM)
                    {
                        ErrPrintf("DoZLwllSanityCheck: Zlwll rams read via PRI do not match those read via method\nFrom PRI: zfar=0x%08x stencil=0x%08x znear=0x%08x\nFrom Method: zfar=0x%08x stencil=0x%08x znear=0x%08x\n",
                            zfarRAM[gpcId][bank][i],
                            stencilRAM[gpcId][bank][i],
                            znearRAM[gpcId][bank][i],
                            decodedZfarRAM,
                            decodedStencilRAM,
                            decodedZnearRAM);
                        ErrPrintf("gpcId=%d bank=%d i=%d aliquotAddress==%d\n",gpcId,bank,i,aliquotAddress);
                        num_errors ++;

                    }
                }
                zfarRAM[gpcId][bank][i] = decodedZfarRAM;
                stencilRAM[gpcId][bank][i] = decodedStencilRAM;
                znearRAM[gpcId][bank][i] = decodedZnearRAM ;
            }
        }
    }

    UINT32 xsamples = AAwidthScale[aa];
    UINT32 ysamples = AAheightScale[aa];

    const bool is_zf32 = ColorUtils::IsFloat(surfZ->GetColorFormat());
    // From tesla: XXX ideally, we shouldn't pass the zbuffer as a parameter, it should already be set as the base address in raster
    // ... but it's not
    raster->SetBase((uintptr_t)zbuffer);

    // allocate zlwll image
    m_Image.resize(w * h);
    InfoPrintf("zlwllImage is %d x %d pixels from Y = %d, surface is %d x %d pixels\n",
        w, h, startY, zcwidth, zcheight);
    for (UINT32 y_ms=startY; y_ms<h; y_ms++) {
        for (UINT32 x_ms=0; x_ms<w; x_ms++) {
            UINT32 partition=0, address=0, bit;
            UINT32 gpc=0;
            UINT32 znear, zfar;
            bool sfail;

            UINT32 y = y_ms / ysamples;
            UINT32 x = x_ms / xsamples;
            bool outOfBounds=false;
            int whichHighResTile=0;
            if (x < zcoffsetx || x >= zcoffsetx + zcwidth || y < zcoffsety || y >= zcoffsety + zcheight) {
                outOfBounds=true;
            } else {

                int X=x>>4;
                int Y=y>>4;
                address=0;
                // 1)Address += hypertile_column * tiles_per_hypertileColumn_per_GPC
                int hypertile_column = int(((UINT64)(X)) * sm_num_rcp_conservative / LW_PGRAPH_PRI_GPC0_ZLWLL_SM_NUM_RCP_CONSERVATIVE__MAX);
                gpc = gpc_map[(X+Y*row_offset)%num_sms];
                int tilesPerColumn=tiles_per_hypertile_row_per_gpc[gpc]*zcheight;
                tilesPerColumn/=16; // tile is always 16 high
                address += hypertile_column * tilesPerColumn;

                //2)address += tiles_from_top * (tiles_per_hypertile_row_per_GPC)
                address +=  Y *  tiles_per_hypertile_row_per_gpc[gpc];
                //3)address += SM# for that pratilwlary x in that hypertile row
                address += sm_in_gpc_number_map[(X+Y*row_offset)%zlwll_table_size];

                partition = address % LW_PGRAPH_ZLWLL_NUM_ZFAR;
                address = address / LW_PGRAPH_ZLWLL_NUM_ZFAR;

                if (ram_format == LW_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT_HIGH_RES_Z)
                {
                    whichHighResTile = address & 0x1;
                    address = address >> 1;
                }

                if(gpc >=  num_gpcs || partition >= znearBanks[gpc]) {
                    ErrPrintf("DoZLwllSanityCheck: Trying to read from non-existant GPC or Zlwll bank. GPC=%d bank=%d\n", gpc, partition);
                }

                // 4)Address += layer index * tiles_per_layer_per_GPC
                if(address >= total_ram_size || address >= per_layer_ram_size)
                    outOfBounds=true;
               // FIXME add support for multiple layers (address computation and bounds checking)
              //address += rtArrayIndex*region_table[regionId].zlwll_ram_size_per_layer;
                //5) add base address
                //address+=zcstart;
            }
            if(outOfBounds) {
                znear = nearest_z;
                zfar = farthest_z;
                sfail = false;
            }
            else {
                // znear
                if (ram_format == LW_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT_HIGH_RES_Z) {
                    bit = ((y - zcoffsety) / 8)%2; // top or bottom half of 16x16
                    if (znearRAM[gpc][partition][address] & (0x8 >> (bit + whichHighResTile * 2)))  // check if the 16x8 is at the far plane
                        znear = farthest_znear;
                    else
                        znear = zc_unsqueeze((znearRAM[gpc][partition][address]>>4) & 0xFFF, zformat, zdir_greater,
                            ColorUtils::ZBits(surfZ->GetColorFormat()),
                            zf32_as_z16, is_zf32, zdir_greater);
                }
                else {
                    bit = ((x/8) & 1) + ((y/8) & 1) * 2;
                    if (znearRAM[gpc][partition][address] & (0x8 >> bit))  // check if the 16x8 is at the far plane
                        znear = farthest_znear;
                    else
                        znear = zc_unsqueeze((znearRAM[gpc][partition][address]>>4) & 0xFFF, zformat, zdir_greater,
                            ColorUtils::ZBits(surfZ->GetColorFormat()),
                            zf32_as_z16, is_zf32, zdir_greater);
                }
                // zfar/stencil
                // ToDo: need to support the new format parser
                if (ram_format == LW_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT_LOW_RES_Z) {
                    bit  = ((x/4) & 3) + ((y/2) & 3) * 4; // 16x16 tile -> upper/lower 16x8 -> 4x2 subtiles
                    // upper 16x8 is in zfar, lower one is in stencil
                    if ((!(y&8) && (stencilRAM[gpc][partition][address] & (0x0001 << bit))) ||
                        ((y&8) && (   zfarRAM[gpc][partition][address] & (0x0001 << bit))))

                          zfar = zc_unsqueeze((zfarRAM[gpc][partition][address]>>16) & 0xFFF, zformat, zdir_greater,
                                ColorUtils::ZBits(surfZ->GetColorFormat()),
                                zf32_as_z16, is_zf32, !zdir_greater);
                    else
                        zfar = farthest_z;
                    sfail = false;
                }
                else if (ram_format == LW_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT_LOW_RES_Z_2X4) {

                    int sx = (x >> 1) & 0x7;   // 0 1 2 3 ... for 0-2, 3-4, 5-6, 7-8 ...
                    // swap pair
                    // want 0 1 4 5 2 3 6 7 ... for 0-2, 3-4, 5-6, 7-8 ...
                    if(sx ==2 || sx ==3)
                        sx +=2;
                    else if(sx == 4 || sx ==5)
                        sx -=2;
                    int  sy = (y << 1) & 0x18;   // 0 4 8 12 for 0-1, 2-3, 4-5, 6-7 or 8-9, 10-11, 12-13, 14-15
                    bit = (sx + (sy&0x8));
                    // upper 16x8 is in zfar, lower one is in stencil
                    if ((!(y&8) && (stencilRAM[gpc][partition][address] & (0x0001 << bit))) ||
                        ((y&8) && (   zfarRAM[gpc][partition][address] & (0x0001 << bit))))

                          zfar = zc_unsqueeze((zfarRAM[gpc][partition][address]>>16) & 0xFFF, zformat, zdir_greater,
                                ColorUtils::ZBits(surfZ->GetColorFormat()),
                                zf32_as_z16, is_zf32, !zdir_greater);
                    else
                        zfar = farthest_z;
                    sfail = false;
                }
                else if (ram_format == LW_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT_LOW_RES_ZS) {
                    bit  = ((x/4) & 3) + ((y/4) & 3) * 4; // 16x16 tile -> 4x4 subtiles

                    if (zfarRAM[gpc][partition][address] & (0x1 << bit))
                        zfar = zc_unsqueeze((zfarRAM[gpc][partition][address]>>16) & 0xFFF, zformat, zdir_greater,
                            ColorUtils::ZBits(surfZ->GetColorFormat()),
                            zf32_as_z16, is_zf32, !zdir_greater);
                    else
                        zfar = farthest_z;
                    sfail = (stencilRAM[gpc][partition][address] & (0x1 >> bit)) != 0;
                }
                else { // HIGH_RES_Z
                    bit  = ((x/4) & 3) + ((y/4) & 3) * 4; // 16x16 tile -> 4x4 subtiles

                    if ((whichHighResTile == 1 &&    zfarRAM[gpc][partition][address] & (0x1 << bit)) ||
                        (whichHighResTile == 0 && stencilRAM[gpc][partition][address] & (0x1 << bit)) )
                        zfar = zc_unsqueeze((zfarRAM[gpc][partition][address]>>16) & 0xFFF, zformat, zdir_greater,
                            ColorUtils::ZBits(surfZ->GetColorFormat()),
                            zf32_as_z16, is_zf32, !zdir_greater);
                    else
                        zfar = farthest_z;
                    sfail = false;
                }
            }
            // Fetch framebuffer Z and stencil values
            PixelFB pix = raster->GetPixel(x_ms + y_ms * w);
            UINT32 fbz = pix.Z();
            UINT32 fbs = pix.Stencil();
            if (check) {
                // Z sanity check
                if (ColorUtils::ZBits(surfZ->GetColorFormat()) == 0)
                {
                    // If there are no Z bits, only sanity check stencil.
                }
                else if (!ColorUtils::IsFloat(surfZ->GetColorFormat())) {
                    if ((!zdir_greater && (fbz < znear || fbz > zfar)) ||
                         (zdir_greater && (fbz > znear || fbz < zfar)))
                    {
                        if (num_errors < ERROR_LIMIT)
                            ErrPrintf("DoZLwllSanityCheck:  (%d,%d) framebuffer z = %06x, but zlwll has znear = %06x, zfar = %06x gpc=0x%x partition=%x address0x%x rawZnear=0x%08x rawZfar=0x%08x rawStencil=0x%08x\n",
                                      x, y, fbz, znear, zfar, gpc, partition, address,znearRAM[gpc][partition][address],zfarRAM[gpc][partition][address],stencilRAM[gpc][partition][address]);
                        num_errors ++;
                    }
                } else {
                    if ((!zdir_greater && ((*(float*)&fbz) < (*(float*)&znear)  ||  (*(float*)&fbz) > (*(float*)&zfar))) ||
                         (zdir_greater && ((*(float*)&fbz) > (*(float*)&znear)  ||  (*(float*)&fbz) < (*(float*)&zfar))))
                    {
                        if (num_errors < ERROR_LIMIT)
                            ErrPrintf("DoZLwllSanityCheck:  (%d,%d) framebuffer z = %08x, but zlwll has znear = %08x, zfar = %08x gpc=0x%x partition=%x address0x%x\n",
                                      x, y, fbz, znear, zfar, gpc, partition, address);
                        num_errors ++;
                    }
                }
                // Stencil sanity check
                if (is_s8 && sfail && stencil_test(sfunc, sref, smask, fbs)) {
                    if (num_errors < ERROR_LIMIT)
                        ErrPrintf("DoZLwllSanityCheck:  (%d,%d) framebuffer stencil = %02x passes the stencil criterion sfunc=%d sref=0x%02x smask=0x%02x, but zlwll has sfail=1\n",
                                  x, y, fbs, sfunc, sref, smask);
                    num_errors ++;
                }
            }
        }
    }
    // and now check the data
    InfoPrintf("DoZLwllSanityCheck: Checking Zlwll depth consistency for  (%d,%d)-(%d,%d)... \n", 0,0,w,h);

    if (num_errors)
        ErrPrintf("DoZLwllSanityCheck: Total of %d errors found\n", num_errors);
    else
        InfoPrintf("DoZLwllSanityCheck: Success!\n");
    return num_errors;
}

// really should use WWDX table
void ZLwll::loadGpcMap(UINT32  *gpcMap, LWGpuSubChannel *subch, UINT32 gpuSubdevIdx, LWGpuResource *pGpuRes)
{

    const UINT32 tiles_in_map = 6;
    UINT32 maps;
    GpuSubdevice *pSubdev = pGpuRes->GetGpuSubdevice(gpuSubdevIdx);
    SmcEngine *pSmcEngine = subch->channel()->GetSmcEngine();
    RegHal& regHal = pGpuRes->GetRegHal(pSubdev, subch->channel()->GetLwRmPtr(), pSmcEngine);

    //LW_PGRAPH_PRI_GPCS_CRSTR_GPC_MAP was made indexible for gv100 and _SIZE defined.
    //Before this the CRSTR_GPC_MAP registers where hardcoded from 0-5
    RefManual* refMan = pSubdev->GetRefManual();
    const RefManualRegister* pReg = refMan->FindRegister("LW_PGRAPH_PRI_GPCS_CRSTR_GPC_MAP");
    if (pReg)
    {
        maps = pReg->GetArraySizeI();
        for (UINT32 i=0; i < maps; i++)
        {
            gpcMap[tiles_in_map*i+0]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset(i), gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPCS_CRSTR_GPC_MAP_TILE0, i);
            gpcMap[tiles_in_map*i+1]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset(i), gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPCS_CRSTR_GPC_MAP_TILE1, i);
            gpcMap[tiles_in_map*i+2]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset(i), gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPCS_CRSTR_GPC_MAP_TILE2, i);
            gpcMap[tiles_in_map*i+3]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset(i), gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPCS_CRSTR_GPC_MAP_TILE3, i);
            gpcMap[tiles_in_map*i+4]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset(i), gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPCS_CRSTR_GPC_MAP_TILE4, i);
            gpcMap[tiles_in_map*i+5]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset(i), gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPCS_CRSTR_GPC_MAP_TILE5, i);
        }
    }
    else // Prior to Volta
    {
        // Each MAPi register is at an offset of i*4 from MAP0
        // and have the same field bit positions
        const RefManualRegister* pReg = refMan->FindRegister("LW_PGRAPH_PRI_GPCS_CRSTR_GPC_MAP0");
        for (UINT32 i=0; i < 6; i++)
        {
            gpcMap[tiles_in_map*i+0]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset() + i*4, gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPCS_CRSTR_GPC_MAP_TILE0, i);
            gpcMap[tiles_in_map*i+1]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset() + i*4, gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPCS_CRSTR_GPC_MAP_TILE1, i);
            gpcMap[tiles_in_map*i+2]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset() + i*4, gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPCS_CRSTR_GPC_MAP_TILE2, i);
            gpcMap[tiles_in_map*i+3]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset() + i*4, gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPCS_CRSTR_GPC_MAP_TILE3, i);
            gpcMap[tiles_in_map*i+4]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset() + i*4, gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPCS_CRSTR_GPC_MAP_TILE4, i);
            gpcMap[tiles_in_map*i+5]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset() + i*4, gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPCS_CRSTR_GPC_MAP_TILE5, i);
        }
    }
}

void ZLwll::loadZlwllBankMap(UINT32  *zlwll_bank_in_gpc_number_map, LWGpuSubChannel *subch, UINT32 gpuSubdevIdx, LWGpuResource *pGpuRes)
{
    const UINT32 tiles_in_map = 8;
    UINT32 maps;

    //LW_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP was made indexible for gv100 and _SIZE defined.
    //Before this the LW_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP registers where hardcoded from 0-3
    GpuSubdevice *pSubdev = pGpuRes->GetGpuSubdevice(gpuSubdevIdx);
    SmcEngine *pSmcEngine = subch->channel()->GetSmcEngine();
    RegHal& regHal = pGpuRes->GetRegHal(pSubdev, subch->channel()->GetLwRmPtr(), pSmcEngine);
    RefManual* refMan = pSubdev->GetRefManual();
    const RefManualRegister* pReg = refMan->FindRegister("LW_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP");
    if (pReg)
    {
        maps = pReg->GetArraySizeI();
        for (UINT32 i=0; i < maps; i++)
        {
            zlwll_bank_in_gpc_number_map[tiles_in_map*i+0]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset(i), gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP_TILE_0, i);
            zlwll_bank_in_gpc_number_map[tiles_in_map*i+1]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset(i), gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP_TILE_1, i);
            zlwll_bank_in_gpc_number_map[tiles_in_map*i+2]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset(i), gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP_TILE_2, i);
            zlwll_bank_in_gpc_number_map[tiles_in_map*i+3]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset(i), gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP_TILE_3, i);
            zlwll_bank_in_gpc_number_map[tiles_in_map*i+4]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset(i), gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP_TILE_4, i);
            zlwll_bank_in_gpc_number_map[tiles_in_map*i+5]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset(i), gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP_TILE_5, i);
            zlwll_bank_in_gpc_number_map[tiles_in_map*i+6]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset(i), gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP_TILE_6, i);
            zlwll_bank_in_gpc_number_map[tiles_in_map*i+7]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset(i), gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP_TILE_7, i);
        }
    }
    else // prior to Volta
    {
        // Each LW_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAPi register is at an offset of i*4 from LW_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP0
        // and have the same field bit positions
        const RefManualRegister* pReg = refMan->FindRegister("LW_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP0");
        for (UINT32 i=0; i < 4; i++)
        {
            zlwll_bank_in_gpc_number_map[tiles_in_map*i+0]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset() + i*4, gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP_TILE_0, i);
            zlwll_bank_in_gpc_number_map[tiles_in_map*i+1]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset() + i*4, gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP_TILE_1, i);
            zlwll_bank_in_gpc_number_map[tiles_in_map*i+2]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset() + i*4, gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP_TILE_2, i);
            zlwll_bank_in_gpc_number_map[tiles_in_map*i+3]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset() + i*4, gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP_TILE_3, i);
            zlwll_bank_in_gpc_number_map[tiles_in_map*i+4]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset() + i*4, gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP_TILE_4, i);
            zlwll_bank_in_gpc_number_map[tiles_in_map*i+5]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset() + i*4, gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP_TILE_5, i);
            zlwll_bank_in_gpc_number_map[tiles_in_map*i+6]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset() + i*4, gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP_TILE_6, i);
            zlwll_bank_in_gpc_number_map[tiles_in_map*i+7]  = regHal.GetField(pGpuRes->RegRd32(pReg->GetOffset() + i*4, gpuSubdevIdx, pSmcEngine), MODS_PGRAPH_PRI_GPC0_ZLWLL_SM_IN_GPC_NUMBER_MAP_TILE_7, i);
        }
    }
}

MdiagSurf*  ZLwll::GetZLwllStorage(GpuSubdevice *pSubdev, LwRm::Handle hVASpace, LwRm *pLwRm, SmcEngine *pSmcEngine)
{
    RC rc;
    unique_ptr<MdiagSurf> buff(new MdiagSurf);

    // get required gr info
    MYGRINFO grInfo;
    LW2080_CTRL_GR_GET_INFO_PARAMS grInfoParams = {0};

    grInfo.shaderPipeCount.index          = LW2080_CTRL_GR_INFO_INDEX_SHADER_PIPE_COUNT;
    grInfo.litterNumGpcs.index            = LW2080_CTRL_GR_INFO_INDEX_LITTER_NUM_GPCS;

    grInfoParams.grInfoListSize = sizeof (grInfo) / sizeof (LW2080_CTRL_GR_INFO);
    grInfoParams.grInfoList = (LwP64)&grInfo;

    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(pSubdev),
        LW2080_CTRL_CMD_GR_GET_INFO,
        (void*)&grInfoParams,
        sizeof(grInfoParams));

    if (rc != OK)
    {
        ErrPrintf("Error reading number of GPC's and LITTER_NUM_GPCS from RM: %s\n", rc.Message());
        return NULL;
    }

    // callwlate size and allocate zlwll storage
    DebugPrintf("Number of GPC's: %d\n", grInfo.shaderPipeCount.data);
    UINT32 num_aliquots;
    if (Platform::GetSimulationMode() != Platform::Amodel)
    {
        LW2080_CTRL_GR_GET_ZLWLL_INFO_PARAMS grZlwllParams = {0};

        rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(pSubdev),
            LW2080_CTRL_CMD_GR_GET_ZLWLL_INFO,
            (void *) &grZlwllParams,
            sizeof(grZlwllParams));

        if (rc != OK)
        {
            ErrPrintf("Error reading ZLWLL Info from RM: %s\n", rc.      Message());
            return NULL;
        }

        num_aliquots = grZlwllParams.aliquotTotal;
    }
    else
    {
        // Can get zlwll params with Amodel.  But Amodel
        // doesn't implement zlwll anyway, so using
        // zero for the number of aliquots is fine.
        num_aliquots = 0;
    }

    UINT64 size = (grInfo.shaderPipeCount.data)*(num_aliquots * LW_PGRAPH_ZLWLL_BYTES_PER_ALIQUOT_PER_GPC) +
         grInfo.litterNumGpcs.data * LW_PGRAPH_ZLWLL_SAVE_RESTORE_HEADER_BYTES_PER_GPC;
    buff->SetArrayPitch(size);
    buff->SetColorFormat(ColorUtils::Y8);
    // Put storage into system memory to avoid aperture read
    // bug 525014 for details
    buff->SetLocation(Memory::Coherent);
    buff->SetGpuVASpace(hVASpace);
    if (m_Params)
    {
        if ((m_Params->ParamPresent("-va_reverse") > 0) ||
            (m_Params->ParamPresent("-va_reverse_zlwll") > 0))
        {
            buff->SetVaReverse(true);
        }
        else
        {
            buff->SetVaReverse(false);
        }
        if ((m_Params->ParamPresent("-pa_reverse") > 0) ||
            (m_Params->ParamPresent("-pa_reverse_zlwll") > 0))
        {
            buff->SetPaReverse(true);
        }
        else
        {
            buff->SetPaReverse(false);
        }
    }
    buff->Alloc(pSubdev->GetParentDevice(), pLwRm);

    return buff.release();
}

RC ZLwll::ReadZLwllRam(UINT32 region, LWGpuSubChannel *subch, GpuSubdevice *pSubdev, LwRm *pLwRm)
{
    RC rc = OK;

    GpuDevice* pGpuDev = pSubdev->GetParentDevice();
    MASSERT(pGpuDev);
    Channel* pCh = subch->channel()->GetModsChannel();
    UINT32 gpuSubdeviceNum = pGpuDev->GetNumSubdevices();
    if (gpuSubdeviceNum > 1)
    {
        // SLI system: Add the mask.
        UINT32 gpuSubdevice = pSubdev->GetSubdeviceInst();

        MASSERT(gpuSubdevice < gpuSubdeviceNum);
        UINT32 subdevMask = 1 << gpuSubdevice;

        CHECK_RC(pCh->WriteSetSubdevice(subdevMask));
    }

    if (m_ZLwllRam.get())
    {
        DebugPrintf("ZLwll::ReadZLwllRam: Releaseing ZLwll Ram\n");
    }

    m_ZLwllRam.reset(GetZLwllStorage(pSubdev, subch->channel()->GetVASpaceHandle(), pLwRm, subch->channel()->GetSmcEngine()));
    if (m_ZLwllRam.get() == 0)
    {
        ErrPrintf("Error to get ZLwll buffer\n");
        return RC::SOFTWARE_ERROR;
    }

    UINT64 offset_start = m_ZLwllRam->GetCtxDmaOffsetGpu();
    UINT64 offset_end   = offset_start + m_ZLwllRam->GetSize();
    UINT32 addrA = (UINT32)(offset_start >> 32);
    UINT32 addrB = (UINT32)(offset_start & 0xffffffff);
    UINT32 addrC = (UINT32)(offset_end >> 32);
    UINT32 addrD = (UINT32)(offset_end & 0xffffffff);

    if ((addrA & 0xffffff00) || (addrC & 0xffffff00)) // Work around
    {
        ErrPrintf("SetZlwllStorageA/B/C/D can only accept a 40-bit address\n");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(subch->MethodWriteRC(LW9097_SET_ZLWLL_STORAGE_A,
        DRF_NUM(9097, _SET_ZLWLL_STORAGE_A, _ADDRESS_UPPER, addrA)));
    CHECK_RC(subch->MethodWriteRC(LW9097_SET_ZLWLL_STORAGE_B,
        DRF_NUM(9097, _SET_ZLWLL_STORAGE_B, _ADDRESS_LOWER, addrB)));
    CHECK_RC(subch->MethodWriteRC(LW9097_SET_ZLWLL_STORAGE_C,
        DRF_NUM(9097, _SET_ZLWLL_STORAGE_C, _LIMIT_ADDRESS_UPPER, addrC)));
    CHECK_RC(subch->MethodWriteRC(LW9097_SET_ZLWLL_STORAGE_D,
        DRF_NUM(9097, _SET_ZLWLL_STORAGE_D, _LIMIT_ADDRESS_LOWER, addrD)));
    CHECK_RC(subch->MethodWriteRC(LW9097_SET_ACTIVE_ZLWLL_REGION,
        DRF_NUM(9097, _SET_ACTIVE_ZLWLL_REGION, _ID, region)));
    CHECK_RC(subch->MethodWriteRC(LW9097_STORE_ZLWLL,
        DRF_NUM(9097, _STORE_ZLWLL, _V, 0)));
    CHECK_RC(subch->MethodWriteRC(LW9097_ZLWLL_SYNC,
        DRF_NUM(9097, _ZLWLL_SYNC, _V, 0)));

    // Bug 2856285: There is a HW bug since Fermi in ZLwll HW, it doesn't 
    // synchronize its memory writes with FlushPendingWrite. So while the store
    // is in flight in HW, WFI can finish early. The WAR suggested by Zlwll 
    // team is to do a Load after a Store to guarantee the Store has completed
    CHECK_RC(subch->MethodWriteRC(LW9097_LOAD_ZLWLL,
        DRF_NUM(9097, _LOAD_ZLWLL, _V, 0)));

    CHECK_RC (subch->channel()->WaitForIdleRC());

    // Flush Fb before CPU read
    InfoPrintf("%s: Flush Fb before CPU read.\n", __FUNCTION__);

    LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS param;
    memset(&param, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));
    param.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FB_FLUSH, _YES, param.flags);
    if (!m_Params->ParamPresent("-no_l2_flush"))
    {
        param.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _FULL_CACHE, param.flags);
        param.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES, param.flags);
        param.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES, param.flags);
    }

    CHECK_RC(pLwRm->Control(
            pLwRm->GetSubdeviceHandle(pSubdev),
            LW2080_CTRL_CMD_FB_FLUSH_GPU_CACHE,
            &param,
            sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS)));

    param.flags = FLD_SET_DRF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _APERTURE, _SYSTEM_MEMORY, param.flags);

    CHECK_RC(pLwRm->Control(
            pLwRm->GetSubdeviceHandle(pSubdev),
            LW2080_CTRL_CMD_FB_FLUSH_GPU_CACHE,
            &param,
            sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS)));

    if (gpuSubdeviceNum > 1)
    {
        // SLI system: revert the mask.
        CHECK_RC(pCh->WriteSetSubdevice(Channel:: AllSubdevicesMask));
    }

    return rc;
}

void ZLwll::PrintZLwllInfo(const set<UINT32> &ids, UINT32 gpuSubdevIdx, 
    LWGpuResource *pGpuRes, LwRm* pLwRm, SmcEngine* pSmcEngine) const
{
    typedef set<UINT32>::const_iterator zlwll_ids;
    zlwll_ids zend = ids.end();
    GpuSubdevice  *pSubdev = pGpuRes->GetGpuSubdevice(gpuSubdevIdx);
    RegHal& regHal = pGpuRes->GetRegHal(pSubdev, pLwRm, pSmcEngine);

    for (zlwll_ids i = ids.begin(); i != zend; ++i)
    {
        // Todo: loop all smc engines
        
        UINT32 region = regHal.Read32(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION, *i);
        UINT32 size   = regHal.Read32(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSIZE, *i);
        UINT32 start  = regHal.Read32(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTART, *i);
        UINT32 status = regHal.Read32(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS, *i);

        char func_name[16];
        if (regHal.TestField(status, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SFUNC_NEVER))
        {
            sprintf(func_name, "NEVER");
        }
        else if (regHal.TestField(status, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SFUNC_LESS))
        {
            sprintf(func_name, "LESS");
        }
        else if (regHal.TestField(status, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SFUNC_EQUAL))
        {
            sprintf(func_name, "EQUAL");
        }
        else if (regHal.TestField(status, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SFUNC_LEQUAL))
        {
            sprintf(func_name, "LEQUAL");
        }
        else if (regHal.TestField(status, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SFUNC_GREATER))
        {
            sprintf(func_name, "GREATER");
        }
        else if (regHal.TestField(status, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SFUNC_NOTEQUAL))
        {
            sprintf(func_name, "NOTEQUAL");
        }
        else if (regHal.TestField(status, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SFUNC_GEQUAL))
        {
            sprintf(func_name, "GEQUAL");
        }
        else if (regHal.TestField(status, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SFUNC_ALWAYS))
        {
            sprintf(func_name, "ALWAYS");
        }
        else
        {
            MASSERT(!"Unknown value in LW_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SFUNC");
        }

        char ramformat[16];
        if (regHal.TestField(region, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT_LOW_RES_Z))
        {
            sprintf(ramformat, "LOW_RES_Z");
        }
        else if (regHal.TestField(region, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT_LOW_RES_Z_2X4))
        {
            sprintf(ramformat, "LOW_RES_Z_2X4");
        }
        else if (regHal.TestField(region, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT_HIGH_RES_Z))
        {
            sprintf(ramformat, "HIGH_RES_Z");
        }
        else if (regHal.TestField(region, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT_LOW_RES_ZS))
        {
            sprintf(ramformat, "LOW_RES_ZS");
        }
        else if (regHal.TestField(region, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT_Z_16X8_4X4))
        {
            sprintf(ramformat, "Z_16X8_4X4");
        }
        else if (regHal.TestField(region, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT_Z_8X8_4X2))
        {
            sprintf(ramformat, "Z_8X8_4X2");
        }
        else if (regHal.TestField(region, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT_Z_8X8_2X4))
        {
            sprintf(ramformat, "Z_8X8_2X4");
        }
        else if (regHal.TestField(region, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT_Z_16X16_4X8))
        {
            sprintf(ramformat, "Z_16X16_4X8");
        }
        else if (regHal.TestField(region, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT_Z_4X8_2X2))
        {
            sprintf(ramformat, "Z_4X8_2X2");
        }
        else if (regHal.TestField(region, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT_ZS_16X8_4X2))
        {
            sprintf(ramformat, "ZS_16X8_4X2");
        }
        else if (regHal.TestField(region, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT_ZS_16X8_2X4))
        {
            sprintf(ramformat, "ZS_16X8_2X4");
        }
        else if (regHal.TestField(region, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT_ZS_8X8_2X2))
        {
            sprintf(ramformat, "ZS_8X8_2X2");
        }
        else if (regHal.TestField(region, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT_Z_4X8_1X1))
        {
            sprintf(ramformat, "Z_4X8_1X1");
        }
        else if (regHal.TestField(region, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT_Z_NONE))
        {
            sprintf(ramformat, "Z_NONE");
        }
        else
        {
            MASSERT(!"Unknown value in LW_PGRAPH_PRI_GPC0_ZLWLL_ZCREGION_RAMFORMAT");
        }

        char zformat[8];
        if (regHal.TestField(status, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_ZFORMAT_MSB))
        {
            sprintf(zformat, "MSB");
        }
        else if (regHal.TestField(status, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_ZFORMAT_FP))
        {
            sprintf(zformat, "FP");
        }
        else if (regHal.TestField(status, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_ZFORMAT_ZTRICK))
        {
            sprintf(zformat, "ZTRICK");
        }
        else if (regHal.TestField(status, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_ZFORMAT_ZF32_1))
        {
            sprintf(zformat, "ZF32_1");
        }
        else
        {
            MASSERT(!"Unknown value in LW_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_ZFORMAT");
        }

        InfoPrintf("regionID=0x%x ramFormat=%s start=%#x width=%#x height=%#x direction=%s "
                   "zFormat=%s sFunc=%s sRef=%#x sMaks=%#x\n",
                *i,
                ramformat,
                regHal.GetField(start, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTART_ADDR) * regHal.LookupValue(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTART_ADDR__MULTIPLE),
                regHal.GetField(size, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSIZE_WIDTH) * regHal.LookupValue(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSIZE_WIDTH__MULTIPLE),
                regHal.GetField(size, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSIZE_HEIGHT) * regHal.LookupValue(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSIZE_HEIGHT__MULTIPLE),
                regHal.TestField(status, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_ZDIR_LESS) ? "LESS" : "GREATER",
                zformat,
                func_name,
                regHal.GetField(status, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SREF),
                regHal.GetField(status, MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_SMASK));
    }
}

void ZLwll::SetZlwllValidBit(const set<UINT32> &ids, LWGpuSubChannel *subch, UINT32 gpuSubdevIdx, LWGpuResource *pGpuRes)
{
    LWGpuChannel *pCh = subch->channel();
    SmcEngine *pSmcEngine = pCh->GetSmcEngine();
    LwRm* pLwRm = pCh->GetLwRmPtr();
    RegHal& regHal = pGpuRes->GetRegHal(gpuSubdevIdx, pLwRm, pSmcEngine);

    typedef set<UINT32>::const_iterator zlwll_ids;
    zlwll_ids zend = ids.end();

    for (zlwll_ids i = ids.begin(); i != zend; ++i)
    {
        regHal.Write32(MODS_PGRAPH_PRI_GPC0_ZLWLL_ZCSTATUS_VALID_TRUE, *i);
    }
}
