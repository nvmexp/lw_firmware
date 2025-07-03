# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2016-2017,2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

from floorsweep_libs import Exceptions
from floorsweep_libs import Datalogger
from .GP10xFsInfo import GP100FsInfo

class GV100FsInfo(GP100FsInfo):
    MAX_GPC = 6
    MAX_TPC = 42
    MAX_FBP = 8
    MAX_FBIO = 16
    MAX_ROP = 16
    MAX_PES = [3,2,2]
    MAX_LWLINK = 6
    def __init__(self, **kwargs):
        super(GV100FsInfo,self).__init__(**kwargs)

    def ParseFuseInfo(self, fuseinfo):
        Exceptions.ASSERT(isinstance(fuseinfo, dict))
        
        self._fbp_disable_mask = int(fuseinfo['OPT_FBP_DISABLE'],16)
        self._fbio_disable_mask = int(fuseinfo['OPT_FBIO_DISABLE'],16)

        ropl2_disable_mask = int(fuseinfo['OPT_ROP_L2_DISABLE'],16)
        for fbp in range(self.MAX_FBP):
            self._ropl2_disable_masks[fbp] = (ropl2_disable_mask >> (fbp * self.ropl2_per_fbp)) & self.fbp_ropl2_mask
            
        self._gpc_disable_mask = int(fuseinfo['OPT_GPC_DISABLE'],16)

        pes_disable_mask = int(fuseinfo['OPT_PES_DISABLE'],16)
        for gpc in range(self.MAX_GPC):
            self._pes_disable_masks[gpc] = (pes_disable_mask >> (gpc * self.pes_per_gpc)) & self.gpc_pes_mask

        for gpc in range(self.MAX_GPC):
            self._tpc_disable_masks[gpc] = int(fuseinfo['OPT_TPC_GPC{:d}_DISABLE'.format(gpc)], 16)
        
        self._lwlink_disable_mask = int(fuseinfo['OPT_LWLINK_DISABLE'],16)

    def LogFuseInfo(self):
        fbp_disable    = '{:#x}'.format(self.fbp_disable_mask)
        fbio_disable   = '{:#x}'.format(self.fbio_disable_mask)
        ropl2_disable  =    '{}'.format(["{:#x}".format(mask) for mask in self.ropl2_disable_masks])
        gpc_disable    = '{:#x}'.format(self.gpc_disable_mask)
        pes_disable    =    '{}'.format(["{:#x}".format(mask) for mask in self.pes_disable_masks])
        tpc_disable    =    '{}'.format(["{:#x}".format(mask) for mask in self.tpc_disable_masks])
        lwlink_disable = '{:#x}'.format(self.lwlink_disable_mask)

        Datalogger.LogInfo(OPT_FBP_DISABLE    = fbp_disable)
        Datalogger.LogInfo(OPT_FBIO_DISABLE   = fbio_disable)
        Datalogger.LogInfo(OPT_FBPA_DISABLE   = fbio_disable)
        Datalogger.LogInfo(OPT_ROP_L2_DISABLE = ropl2_disable)
        Datalogger.LogInfo(OPT_GPC_DISABLE    = gpc_disable)
        Datalogger.LogInfo(OPT_PES_DISABLE    = pes_disable)
        Datalogger.LogInfo(OPT_TPC_DISABLE    = tpc_disable)
        Datalogger.LogInfo(OPT_LWLINK_DISABLE = lwlink_disable)

    def WriteFbFb(self):
        fs = ["fbp_enable:{:#x}:fbio_enable:{:#x}:fbpa_enable:{:#x}".format(self.fbp_enable_mask,
                                                                            self.fbio_enable_mask,
                                                                            self.fbio_enable_mask)]
        for fbp_idx,ropl2_mask in enumerate(self.ropl2_enable_masks):
            if ropl2_mask and ropl2_mask != self.fbp_ropl2_mask:
                fs.append("fbp_rop_l2_enable:{:d}:{:#x}".format(fbp_idx, ropl2_mask))
        fb_str = ':'.join(fs)

        with open("fb_fb.arg", "w+") as fb_fb:
            fb_fb.write("-floorsweep {}".format(fb_str))

    def FsStr(self):
        fs = ["fbp_enable:{:#x}:fbio_enable:{:#x}:fbpa_enable:{:#x}".format(self.fbp_enable_mask,
                                                                            self.fbio_enable_mask,
                                                                            self.fbio_enable_mask)]
        for fbp_idx,ropl2_mask in enumerate(self.ropl2_enable_masks):
            if ropl2_mask and ropl2_mask != self.fbp_ropl2_mask:
                fs.append("fbp_rop_l2_enable:{:d}:{:#x}".format(fbp_idx, ropl2_mask))
    
        fs.append("gpc_enable:{:#x}".format(self.gpc_enable_mask))
        for gpc_idx,pes_mask in enumerate(self.pes_enable_masks):
            if pes_mask and pes_mask != self.gpc_pes_mask:
                fs.append("gpc_pes_enable:{:d}:{:#x}".format(gpc_idx, pes_mask))
        for gpc_idx,tpc_mask in enumerate(self.tpc_enable_masks):
            if tpc_mask and tpc_mask != self.gpc_tpc_mask:
                fs.append("gpc_tpc_enable:{:d}:{:#x}".format(gpc_idx, tpc_mask))

        fs.append("lwlink_enable:{:#x}".format(self.lwlink_enable_mask))
        
        return ':'.join(fs)
