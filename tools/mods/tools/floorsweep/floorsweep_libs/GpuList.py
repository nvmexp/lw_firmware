# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

from . import FsInfo
from . import GpuInfo

def GetGpuClasses(gpu_name):
    return {
                "GM200" : (FsInfo.GM200FsInfo, GpuInfo.GpuInfo)
              , "GM204" : (FsInfo.GM204FsInfo, GpuInfo.GpuInfo)
              , "GM206" : (FsInfo.GM206FsInfo, GpuInfo.GpuInfo)

              , "GP100" : (FsInfo.GP100FsInfo, GpuInfo.GP100GpuInfo)
              , "GP102" : (FsInfo.GP102FsInfo, GpuInfo.GP10xGpuInfo)
              , "GP104" : (FsInfo.GP104FsInfo, GpuInfo.GP104GpuInfo)
              , "GP106" : (FsInfo.GP106FsInfo, GpuInfo.GP10xGpuInfo)
              , "GP107" : (FsInfo.GP107FsInfo, GpuInfo.GP107GpuInfo)
              , "GP108" : (FsInfo.GP108FsInfo, GpuInfo.GP10xGpuInfo)

              , "GV100" : (FsInfo.GV100FsInfo, GpuInfo.GP100GpuInfo)

              , "TU102" : (FsInfo.TU102FsInfo, GpuInfo.GP10xGpuInfo)
              , "TU104" : (FsInfo.TU104FsInfo, GpuInfo.GP10xGpuInfo)
              , "TU106" : (FsInfo.TU106FsInfo, GpuInfo.GP10xGpuInfo)
              , "TU116" : (FsInfo.TU116FsInfo, GpuInfo.GP10xGpuInfo)
              , "TU117" : (FsInfo.TU117FsInfo, GpuInfo.GP107GpuInfo)

              , "GA100" : (FsInfo.GA100FsInfo, GpuInfo.GA100GpuInfo)

              , "GA102" : (FsInfo.GA102FsInfo, GpuInfo.GA10xGpuInfo)
              , "GA103" : (FsInfo.GA103FsInfo, GpuInfo.GA10xGpuInfo)
              , "GA104" : (FsInfo.GA104FsInfo, GpuInfo.GA10xGpuInfo)
              , "GA106" : (FsInfo.GA106FsInfo, GpuInfo.GA10xGpuInfo)
              , "GA107" : (FsInfo.GA107FsInfo, GpuInfo.GA10xGpuInfo)

              , "GH100" : (FsInfo.GH100FsInfo, GpuInfo.GH100GpuInfo)
           }[gpu_name]
