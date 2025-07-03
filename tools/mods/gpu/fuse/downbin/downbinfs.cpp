/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "downbinfs.h"

string Downbin::ToString(FsUnit fsUnit)
{
    switch (fsUnit)
    {
        case FsUnit::CPC         : return "cpc";
        case FsUnit::GPC         : return "gpc";
        case FsUnit::FBIO        : return "fbio";
        case FsUnit::FBP         : return "fbp";
        case FsUnit::FBPA        : return "fbpa";
        case FsUnit::HALF_FBPA   : return "half_fbpa";
        case FsUnit::IOCTRL      : return "lwlink";
        case FsUnit::LTC         : return "ltc";
        case FsUnit::L2_SLICE    : return "l2slice";
        case FsUnit::LWDEC       : return "lwdec";
        case FsUnit::LWENC       : return "lwenc";
        case FsUnit::OFA         : return "ofa";
        case FsUnit::PERLINK     : return "perlink";
        case FsUnit::PES         : return "pes";
        case FsUnit::ROP         : return "rop";
        case FsUnit::SYSPIPE     : return "sys_pipe";
        case FsUnit::TPC         : return "tpc";
        case FsUnit::LWJPG       : return "lwjpg";
        case FsUnit::NONE        : return "none";
    }

    // Shouldn't reach here
    return "unknown";
}

RC Downbin::GetUGpuGroupEnMasks
(
    const GpuSubdevice &subdev,
    FsUnit groupFsUnit,
    const FsMask& inputGroupsMask,
    vector<FsMask> *pUGpuGpcEnMasks
)
{
   MASSERT(pUGpuGpcEnMasks);
   for (UINT32 ugpu = 0; ugpu < subdev.GetMaxUGpuCount(); ugpu++)
   {
       FsMask ugpuGroupMask;
       INT32 mask;
       switch (groupFsUnit)
       {
           case FsUnit::GPC :
               mask = subdev.GetUGpuGpcMask(ugpu);
               break;
           case FsUnit::FBP :
               mask = subdev.GetUGpuFbpMask(ugpu);
               break;
           case FsUnit::LTC :
               mask = subdev.GetUGpuLtcMask(ugpu);
               break;
           default:
               Printf(Tee::PriError, "Invalid Fs Unit!");
               return RC::SOFTWARE_ERROR;
       }

       MASSERT(mask != -1);

       // mask of enabled FsUnits in the uGPU
       ugpuGroupMask.SetMask(~mask & inputGroupsMask.GetMask());
       pUGpuGpcEnMasks->push_back(ugpuGroupMask);
   }

   return RC::OK;
}

bool Downbin::IsDownbinFlagSet(DownbinFlag value, DownbinFlag flags)
{
    return (static_cast<UINT32>(value & flags) != 0);
}

Downbin::DownbinFlag
Downbin::operator&(DownbinFlag lhs, DownbinFlag rhs)
{
    return static_cast<DownbinFlag>(static_cast<UINT32>(lhs) & static_cast<UINT32>(rhs));
}
