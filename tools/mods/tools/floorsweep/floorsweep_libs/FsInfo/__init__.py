# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

from . import FsInfo
from . import GM20xFsInfo
from . import GP10xFsInfo
from . import VoltaFsInfo
from . import TuringFsInfo
from . import GA100FsInfo
from . import GA10xFsInfo
from . import GH100FsInfo

GM200FsInfo = GM20xFsInfo.GM200FsInfo
GM204FsInfo = GM20xFsInfo.GM204FsInfo
GM206FsInfo = GM20xFsInfo.GM206FsInfo

GP100FsInfo = GP10xFsInfo.GP100FsInfo
GP102FsInfo = GP10xFsInfo.GP102FsInfo
GP104FsInfo = GP10xFsInfo.GP104FsInfo
GP106FsInfo = GP10xFsInfo.GP106FsInfo
GP107FsInfo = GP10xFsInfo.GP107FsInfo
GP108FsInfo = GP10xFsInfo.GP108FsInfo

GV100FsInfo = VoltaFsInfo.GV100FsInfo

TU102FsInfo = TuringFsInfo.TU102FsInfo
TU104FsInfo = TuringFsInfo.TU104FsInfo
TU106FsInfo = TuringFsInfo.TU106FsInfo
TU116FsInfo = TuringFsInfo.TU116FsInfo
TU117FsInfo = TuringFsInfo.TU117FsInfo

GA100FsInfo = GA100FsInfo.GA100FsInfo

GA102FsInfo = GA10xFsInfo.GA102FsInfo
GA103FsInfo = GA10xFsInfo.GA103FsInfo
GA104FsInfo = GA10xFsInfo.GA104FsInfo
GA106FsInfo = GA10xFsInfo.GA106FsInfo
GA107FsInfo = GA10xFsInfo.GA107FsInfo

GH100FsInfo = GH100FsInfo.GH100FsInfo