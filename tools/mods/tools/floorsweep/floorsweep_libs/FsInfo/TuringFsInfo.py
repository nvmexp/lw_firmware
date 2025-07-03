# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

from floorsweep_libs import Exceptions
from floorsweep_libs import Datalogger
from .GP10xFsInfo import GP10xFsInfo
from .GP10xFsInfo import GP107FsInfo

class TU10xFsInfo(GP10xFsInfo):
    def __init__(self, **kwargs):
        super(TU10xFsInfo,self).__init__(**kwargs)
    
    @property
    def half_fbpa_disable_mask(self):
        # half_fbpa_disable = 1 means the half fbpa behavior is disabled,
        # ie. both halves of the fbpa are enabled
        mask = 0x0
        for fbp,ropl2_mask in enumerate(self.ropl2_disable_masks):
            # Turing has a 'bug' that the half fbpa behavior cannot be active
            # if the FBPA is disabled (i.e. both ROP/L2s are disabled)
            if ropl2_mask == 0x0 or ropl2_mask == 0x3:
                # Turing has a 'bug' that the half fbpa register field is twice as long as needed
                # and it only looks at the even bit shifted values.
                mask |= (0x3<<(fbp*2))
        return mask
    @property
    def half_fbpa_mask(self):
        return (1 << (self.MAX_FBP * 2)) - 1
    @property
    def half_fbpa_enable_mask(self):
        return (self.half_fbpa_disable_mask ^ self.half_fbpa_mask) & self.half_fbpa_mask
    
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
            # Turing has a bug/feature where every chip has 3 bits per GPC in OPT_PES_DISABLE
            # regardless of how many PES's per GPC the chip actually has
            self._pes_disable_masks[gpc] = (pes_disable_mask >> (gpc * 3)) & self.gpc_pes_mask

        for gpc in range(self.MAX_GPC):
            self._tpc_disable_masks[gpc] = int(fuseinfo['OPT_TPC_GPC{:d}_DISABLE'.format(gpc)], 16)

    def LogFuseInfo(self):
        fbp_disable    = '{:#x}'.format(self.fbp_disable_mask)
        fbio_disable   = '{:#x}'.format(self.fbio_disable_mask)
        ropl2_disable  =    '{}'.format(["{:#x}".format(mask) for mask in self.ropl2_disable_masks])
        gpc_disable    = '{:#x}'.format(self.gpc_disable_mask)
        pes_disable    =    '{}'.format(["{:#x}".format(mask) for mask in self.pes_disable_masks])
        tpc_disable    =    '{}'.format(["{:#x}".format(mask) for mask in self.tpc_disable_masks])

        Datalogger.LogInfo(OPT_FBP_DISABLE    = fbp_disable)
        Datalogger.LogInfo(OPT_FBIO_DISABLE   = fbio_disable)
        Datalogger.LogInfo(OPT_FBPA_DISABLE   = fbio_disable)
        Datalogger.LogInfo(OPT_ROP_L2_DISABLE = ropl2_disable)
        Datalogger.LogInfo(OPT_GPC_DISABLE    = gpc_disable)
        Datalogger.LogInfo(OPT_PES_DISABLE    = pes_disable)
        Datalogger.LogInfo(OPT_TPC_DISABLE    = tpc_disable)

    def WriteFbFb(self):
        fs = ["fbp_enable:{0:#x}:fbio_enable:{1:#x}:fbpa_enable:{1:#x}".format(self.fbp_enable_mask,
                                                                               self.fbio_enable_mask)]
        fs.append("half_fbpa_enable:{:#x}".format(self.half_fbpa_enable_mask))
        for fbp_idx,ropl2_mask in enumerate(self.ropl2_enable_masks):
            if ropl2_mask and ropl2_mask != self.fbp_ropl2_mask:
                fs.append("fbp_rop_l2_enable:{:d}:{:#x}".format(fbp_idx, ropl2_mask))
        fb_str = ':'.join(fs)

        with open("fb_fb.arg", "w+") as fb_fb:
            fb_fb.write("-floorsweep {}".format(fb_str))

    def FsStr(self):
        fs = ["fbp_enable:{0:#x}:fbio_enable:{1:#x}:fbpa_enable:{1:#x}".format(self.fbp_enable_mask,
                                                                               self.fbio_enable_mask)]
        fs.append("half_fbpa_enable:{:#x}".format(self.half_fbpa_enable_mask))
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
            
        fs = ':'.join(fs)
        return fs
        
class TU102FsInfo(TU10xFsInfo):
    MAX_GPC = 6
    MAX_TPC = 36
    MAX_FBP = 6
    MAX_FBIO = 6
    MAX_ROP = 12
    MAX_PES = [2,2,2]
    def __init__(self, **kwargs):
        super(TU102FsInfo,self).__init__(**kwargs)

class TU104FsInfo(TU10xFsInfo):
    MAX_GPC = 6
    MAX_TPC = 24
    MAX_FBP = 4
    MAX_FBIO = 4
    MAX_ROP = 8
    MAX_PES = [2,2]
    def __init__(self, **kwargs):
        super(TU104FsInfo,self).__init__(**kwargs)

class TU106FsInfo(TU10xFsInfo):
    MAX_GPC = 3
    MAX_TPC = 18
    MAX_FBP = 4
    MAX_FBIO = 4
    MAX_ROP = 8
    MAX_PES = [2,2,2]
    def __init__(self, **kwargs):
        super(TU106FsInfo,self).__init__(**kwargs)

class TU11xFsInfo(TU10xFsInfo):
    def __init__(self, **kwargs):
        super(TU11xFsInfo,self).__init__(**kwargs)

class TU116FsInfo(TU11xFsInfo):
    MAX_GPC = 3
    MAX_TPC = 12
    MAX_FBP = 3
    MAX_FBIO = 3
    MAX_ROP = 6
    MAX_PES = [2,2]
    def __init__(self, **kwargs):
        super(TU116FsInfo,self).__init__(**kwargs)

class TU117FsInfo(TU11xFsInfo):
    MAX_GPC = 2
    MAX_TPC = 8
    MAX_FBP = 2
    MAX_FBIO = 2
    MAX_ROP = 4
    MAX_PES = [2,2]
    def __init__(self, **kwargs):
        super(TU117FsInfo,self).__init__(**kwargs)

    # TU117 has even-odd floorsweeping just like GP107, so copy its functions
    MakeFBConsistent = GP107FsInfo.__dict__['MakeFBConsistent']
    IterateRop = GP107FsInfo.__dict__['IterateRop']
