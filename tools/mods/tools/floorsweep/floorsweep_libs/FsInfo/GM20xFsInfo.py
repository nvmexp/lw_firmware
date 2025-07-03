# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2015,2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

from .FsInfo import FsInfo
from floorsweep_libs import Datalogger
from floorsweep_libs import Exceptions

class GM200FsInfo(FsInfo):
    MAX_GPC = 6
    MAX_TPC = 24
    MAX_FBP = 6
    MAX_FBIO = 6
    MAX_ROP = 12
    def __init__(self, **kwargs):
        super(GM200FsInfo,self).__init__(**kwargs)

class GM204FsInfo(FsInfo):
    MAX_GPC = 4
    MAX_TPC = 16
    MAX_FBP = 4
    MAX_FBIO = 4
    MAX_ROP = 8
    def __init__(self, **kwargs):
        super(GM204FsInfo,self).__init__(**kwargs)

class GM206FsInfo(FsInfo):
    MAX_GPC = 2
    MAX_TPC = 8
    MAX_FBP = 2
    MAX_FBIO = 2
    MAX_ROP = 4
    def __init__(self, **kwargs):
        super(GM206FsInfo,self).__init__(**kwargs)
