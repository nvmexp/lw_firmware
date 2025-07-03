# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import math

from .FsInfo import FsInfo
from floorsweep_libs import Datalogger
from floorsweep_libs import Exceptions

class GP10xFsInfo(FsInfo):
    MAX_PES = []
    def __init__(self, **kwargs):
        if (self.MAX_PES == []):
            raise NotImplementedError("FsInfo MAX parameters unset")

        self._pes_disable_masks = [0x0 for x in range(self.MAX_GPC)]

        super(GP10xFsInfo,self).__init__(**kwargs)

    def Copy(self, copy):
        super(GP10xFsInfo,self).Copy(copy)
        self._pes_disable_masks = copy.pes_disable_masks

    def ParseFuseInfo(self, fuseinfo):
        super(GP10xFsInfo,self).ParseFuseInfo(fuseinfo)

        pes_disable_mask = int(fuseinfo['OPT_PES_DISABLE'],16)
        for gpc in range(self.MAX_GPC):
            self._pes_disable_masks[gpc] = (pes_disable_mask >> (gpc * self.pes_per_gpc)) & self.gpc_pes_mask

    def ParseInitMasks(self, kwargs):
        super(GP10xFsInfo,self).ParseInitMasks(kwargs)
        self._pes_disable_masks = self._GetMasks(kwargs, self._pes_disable_masks, "pes", self.gpc_pes_mask)

    def FsStr(self):
        fs = [super(GP10xFsInfo,self).FsStr()]
        for gpc_idx,pes_mask in enumerate(self.pes_enable_masks):
            if pes_mask and pes_mask != self.gpc_pes_mask:
                fs.append("gpc_pes_enable:{:d}:{:#x}".format(gpc_idx, pes_mask))
        return ':'.join(fs)

    def PrintChipInfo(self):
        super(GP10xFsInfo,self).PrintChipInfo()
        Datalogger.PrintLog("... MAX_GPC_PES_MASK    {:b}".format(self.gpc_pes_mask))
    def PrintFuseInfo(self):
        super(GP10xFsInfo,self).PrintFuseInfo()
        Datalogger.PrintLog("... OPT_PES_DISABLE    {}".format(["{:#x}".format(mask) for mask in self.pes_disable_masks]))
    def LogFuseInfo(self):
        super(GP10xFsInfo,self).LogFuseInfo()
        pes_disable = '{:#x}'.format(int(''.join(['{:0{width}b}'.format(mask,width=self.pes_per_gpc) for mask in reversed(self.pes_disable_masks)]),2))
        Datalogger.LogInfo(OPT_PES_DISABLE = pes_disable)

    def CloneTpc(self):
        return self.__class__(gpc_disable_mask  = self.gpc_disable_mask,
                              pes_disable_masks = self.pes_disable_masks,
                              tpc_disable_masks = self.tpc_disable_masks)

    def ClearTpc(self):
        return self.__class__(clone = self, gpc_disable_mask  = 0x0,
                                            pes_disable_masks = [0x0 for x in range(self.MAX_GPC)],
                                            tpc_disable_masks = [0x0 for x in range(self.MAX_GPC)])

    # Read only property accessors
    @property
    def pes_disable_masks(self):
        return self._pes_disable_masks[:] # return a copy, not a reference
    @property
    def pes_enable_masks(self):
        return [((mask ^ self.gpc_pes_mask) & self.gpc_pes_mask) for mask in self._pes_disable_masks]
    def gpc_pes_disable_mask(self, gpc):
        return self._pes_disable_masks[gpc]
    def gpc_pes_enable_mask(self, gpc):
        return (self.gpc_pes_disable_mask(gpc) ^ self.gpc_pes_mask) & self.gpc_pes_mask
    def gpc_pes_tpc_disable_mask(self, gpc, pes):
        mask = self._tpc_disable_masks[gpc]
        for i in range(0,pes):
            mask >>= self.tpc_per_pes(i)
        return mask & self.pes_tpc_mask(pes)
    def gpc_pes_tpc_enable_mask(self, gpc, pes):
        return (self.gpc_pes_tpc_disable_mask(gpc, pes) ^ self.pes_tpc_mask(pes)) & self.pes_tpc_mask(pes)

    # Read only helper attributes
    @property
    def fbio_per_fbp(self):
        return int(self.MAX_FBIO / self.MAX_FBP)
    @property
    def fbp_fbio_mask(self):
        return (1 << self.fbio_per_fbp) - 1
    @property
    def pes_per_gpc(self):
        return len(self.MAX_PES)
    @property
    def gpc_pes_mask(self):
        return (1 << self.pes_per_gpc) - 1
    def tpc_per_pes(self, pes):
        return self.MAX_PES[pes]
    def pes_tpc_mask(self, pes):
        return (1 << self.MAX_PES[pes]) - 1
    def gpc_pes_tpc_mask(self, pes):
        mask = self.pes_tpc_mask(pes)
        for i in range(0,pes):
            mask <<= self.tpc_per_pes(i)
        return mask

    def MakeGpcConsistent(self):
        for gpc in range(self.MAX_GPC):
            gpc_disable = (1 << gpc)
            for pes in range(self.pes_per_gpc):
                pes_disable = (1 << pes)
                if (pes_disable & self.gpc_pes_disable_mask(gpc)):
                    self._tpc_disable_masks[gpc] |= self.gpc_pes_tpc_mask(pes)
                if ((self.gpc_pes_tpc_disable_mask(gpc,pes) & self.pes_tpc_mask(pes)) == self.pes_tpc_mask(pes)):
                    self._pes_disable_masks[gpc] |= pes_disable
            gpc_tpc_disable = (self.gpc_tpc_disable_mask(gpc) & self.gpc_tpc_mask)
            if ((self.gpc_disable_mask & gpc_disable) or
                    ((self.gpc_pes_disable_mask(gpc) & self.gpc_pes_mask) == self.gpc_pes_mask) or
                    (gpc_tpc_disable == self.gpc_tpc_mask) or
                    (self.only_gpc and gpc_tpc_disable != 0)):
                self._gpc_disable_mask |= gpc_disable
                self._pes_disable_masks[gpc] = self.gpc_pes_mask
                self._tpc_disable_masks[gpc] = self.gpc_tpc_mask

    def MakeFBConsistent(self):
        for fbp in range(self.MAX_FBP):
            fbp_disable = (1 << fbp)
            fbio_disable = self.fbp_fbio_mask << (fbp * self.fbio_per_fbp)
            fbp_ropl2_disable = (self.fbp_ropl2_disable_mask(fbp) & self.fbp_ropl2_mask)
            if ((self.fbp_disable_mask & fbp_disable) or
                    ((self.fbio_disable_mask & fbio_disable) == fbio_disable) or
                    (fbp_ropl2_disable == self.fbp_ropl2_mask) or
                    (self.only_fbp and fbp_ropl2_disable != 0)):
                self._fbp_disable_mask |= fbp_disable
                self._fbio_disable_mask |= fbio_disable
                self._ropl2_disable_masks[fbp] = self.fbp_ropl2_mask

    def __or__(self, other):
        Exceptions.ASSERT(self.__class__ == other.__class__, "Cannot or {} with {}".format(self.__class__, other.__class__))
        or_ = super(GP10xFsInfo,self).__or__(other)
        return self.__class__(clone = or_,
                              gpc_disable_mask    = (self.gpc_disable_mask | other.gpc_disable_mask),
                              pes_disable_masks   = [(x|y) for x,y in zip(self.pes_disable_masks,other.pes_disable_masks)],
                              tpc_disable_masks   = [(x|y) for x,y in zip(self.tpc_disable_masks,other.tpc_disable_masks)])

    def __xor__(self, other):
        Exceptions.ASSERT(self.__class__ == other.__class__, "Cannot or {} with {}".format(self.__class__, other.__class__))
        xor_ = super(GP10xFsInfo,self).__xor__(other)
        return self.__class__(clone = xor_,
                              gpc_disable_mask    = (self.gpc_disable_mask ^ other.gpc_disable_mask),
                              pes_disable_masks   = [(x^y) for x,y in zip(self.pes_disable_masks,other.pes_disable_masks)],
                              tpc_disable_masks   = [(x^y) for x,y in zip(self.tpc_disable_masks,other.tpc_disable_masks)])

    def __and__(self, other):
        Exceptions.ASSERT(self.__class__ == other.__class__, "Cannot and {} with {}".format(self.__class__, other.__class__))
        and_ = super(GP10xFsInfo,self).__and__(other)
        return self.__class__(clone = and_,
                              gpc_disable_mask    = (self.gpc_disable_mask & other.gpc_disable_mask),
                              pes_disable_masks   = [(x&y) for x,y in zip(self.pes_disable_masks,other.pes_disable_masks)],
                              tpc_disable_masks   = [(x&y) for x,y in zip(self.tpc_disable_masks,other.tpc_disable_masks)])

    def __eq__(self, other):
        if self.__class__ != other.__class__:
            return False
        return (super(GP10xFsInfo,self).__eq__(other) and
                self.gpc_disable_mask == other.gpc_disable_mask and
                self.pes_disable_masks == other.pes_disable_masks and
                self.tpc_disable_masks == other.tpc_disable_masks)

    def IterateColdGpc(self):
        for gpc in range(self.MAX_GPC):
            gpc_mask = (1 << gpc)
            if gpc_mask & self.gpc_disable_mask:
                continue
            gpc_disable_mask = self.gpc_disable_mask
            gpc_disable_mask |= gpc_mask
            yield self.__class__(clone = self.ClearTpc(), gpc_disable_mask = gpc_disable_mask,
                                                          pes_disable_masks = self.pes_disable_masks,
                                                          tpc_disable_masks = self.tpc_disable_masks)

    def IsSinglePes(self):
        return (1 == sum([int(i) for mask in self.pes_enable_masks for i in '{:b}'.format(mask)]))

    def SplitTpc(self):
        if self.IsSinglePes():
            gpc_idx = int(math.log(self.gpc_enable_mask,2))
            pes_idx = int(math.log(self.gpc_pes_enable_mask(gpc_idx),2))
            left_tpc_mask,right_tpc_mask = self._SplitMask(self.gpc_pes_tpc_enable_mask(gpc_idx,pes_idx))

            left_tpc_masks = [0x0 for x in range(self.MAX_GPC)]
            right_tpc_masks = [0x0 for x in range(self.MAX_GPC)]
            left_tpc_masks[gpc_idx] = left_tpc_mask
            right_tpc_masks[gpc_idx] = right_tpc_mask
            for i in range(0,pes_idx):
                left_tpc_masks[gpc_idx] <<= self.tpc_per_pes(i)
                right_tpc_masks[gpc_idx] <<= self.tpc_per_pes(i)

            left_tpc = self.__class__(clone = self.ClearTpc(), tpc_enable_masks = left_tpc_masks)
            right_tpc = self.__class__(clone = self.ClearTpc(), tpc_enable_masks = right_tpc_masks)
            return left_tpc,right_tpc
        elif self.IsSingleGpc():
            gpc_idx = int(math.log(self.gpc_enable_mask,2))
            left_pes_mask,right_pes_mask = self._SplitMask(self.gpc_pes_enable_mask(gpc_idx))

            left_pes_masks = [0x0 for x in range(self.MAX_GPC)]
            right_pes_masks = [0x0 for x in range(self.MAX_GPC)]
            left_pes_masks[gpc_idx] = left_pes_mask
            right_pes_masks[gpc_idx] = right_pes_mask

            left_pes  = self.__class__(clone = self.ClearTpc(), pes_enable_masks = left_pes_masks,
                                                               tpc_enable_masks = self.tpc_enable_masks)
            right_pes = self.__class__(clone = self.ClearTpc(), pes_enable_masks = right_pes_masks,
                                                               tpc_enable_masks = self.tpc_enable_masks)
            return left_pes,right_pes
        else:
            left_gpc_mask,right_gpc_mask = self._SplitMask(self.gpc_enable_mask)
            left_gpc  = self.__class__(clone = self.ClearTpc(), gpc_enable_mask  = left_gpc_mask,
                                                               pes_enable_masks = self.pes_enable_masks,
                                                               tpc_enable_masks = self.tpc_enable_masks)
            right_gpc = self.__class__(clone = self.ClearTpc(), gpc_enable_mask  = right_gpc_mask,
                                                               pes_enable_masks = self.pes_enable_masks,
                                                               tpc_enable_masks = self.tpc_enable_masks)
            return left_gpc,right_gpc

class GP100FsInfo(GP10xFsInfo):
    MAX_GPC = 6
    MAX_TPC = 30
    MAX_FBP = 8
    MAX_FBIO = 16
    MAX_ROP = 16
    MAX_PES = [3,2]
    MAX_LWLINK = 4
    def __init__(self, **kwargs):
        self._lwlink_disable_mask = 0x0
        super(GP100FsInfo,self).__init__(**kwargs)

    def Copy(self, copy):
        super(GP100FsInfo,self).Copy(copy)
        self._lwlink_disable_mask = copy._lwlink_disable_mask

    def ParseFuseInfo(self, fuseinfo):
        super(GP100FsInfo,self).ParseFuseInfo(fuseinfo)
        self._lwlink_disable_mask = int(fuseinfo['OPT_LWLINK_DISABLE'],16)

    def ParseInitMasks(self, kwargs):
        super(GP100FsInfo,self).ParseInitMasks(kwargs)
        self._lwlink_disable_mask = self._GetMask(kwargs, self._lwlink_disable_mask, "lwlink", self.lwlink_mask)

    def FsStr(self):
        fs = super(GP100FsInfo,self).FsStr()
        return fs + " -rmkey RMLwLinkEnable {:#x}".format(self.lwlink_enable_mask)

    def __str__(self):
        fs = super(GP100FsInfo,self).__str__()
        return fs.replace(" -rmkey RMLwLinkEnable ", ":lwlink:")

    def PrintChipInfo(self):
        super(GP100FsInfo,self).PrintChipInfo()
        Datalogger.PrintLog("... MAX_LWLINK_MASK     {:b}".format(self.lwlink_mask))
    def PrintFuseInfo(self):
        super(GP100FsInfo,self).PrintFuseInfo()
        Datalogger.PrintLog("... OPT_LWLINK_DISABLE {:#x}".format(self.lwlink_disable_mask))
    def LogFuseInfo(self):
        super(GP100FsInfo,self).LogFuseInfo()
        lwlink_disable = '{:#x}'.format(self.lwlink_disable_mask)
        Datalogger.LogInfo(OPT_LWLINK_DISABLE = lwlink_disable)

    @property
    def lwlink_disable_mask(self):
        return self._lwlink_disable_mask
    @property
    def lwlink_enable_mask(self):
        return (self._lwlink_disable_mask ^ self.lwlink_mask) & self.lwlink_mask
    @property
    def lwlink_mask(self):
        return (1 << self.MAX_LWLINK) - 1

    def CloneLwlink(self):
        return self.__class__(lwlink_disable_mask = self.lwlink_disable_mask)

    def MakeFBConsistent(self):
        for fbp in range(self.MAX_FBP):
            fbp_disable = (1 << fbp)
            fbp_pair_disable = (1 << (fbp ^ 1))
            fbio_disable = self.fbp_fbio_mask << (fbp * self.fbio_per_fbp)
            fbio_pair_disable = self.fbp_fbio_mask << ((fbp ^ 1) * self.fbio_per_fbp)
            if ((self.fbp_disable_mask & fbp_disable) or
                    (self.fbio_disable_mask & fbio_disable) or
                    (self.fbp_ropl2_disable_mask(fbp) & self.fbp_ropl2_mask)):
                self._fbp_disable_mask |= fbp_disable
                self._fbp_disable_mask |= fbp_pair_disable
                self._fbio_disable_mask |= fbio_disable
                self._fbio_disable_mask |= fbio_pair_disable
                self._ropl2_disable_masks[fbp] = self.fbp_ropl2_mask
                self._ropl2_disable_masks[fbp^1] = self.fbp_ropl2_mask

    def __ilwert__(self):
        ilwert = super(GP100FsInfo,self).__ilwert__()
        return self.__class__(clone = ilwert, lwlink_disable_mask = self.lwlink_enable_mask)

    def __or__(self, other):
        Exceptions.ASSERT(self.__class__ == other.__class__, "Cannot or {} with {}".format(self.__class__, other.__class__))
        or_ = super(GP100FsInfo,self).__or__(other)
        return self.__class__(clone = or_, lwlink_disable_mask = (self.lwlink_disable_mask | other.lwlink_disable_mask))

    def IterateRop(self):
        for fbp in range(0, self.MAX_FBP, 2):
            fbp_mask = (1 << fbp)
            fbp1_mask = (1 << (fbp ^ 1))
            if (fbp_mask & self.fbp_disable_mask) or (fbp1_mask & self.fbp_disable_mask):
                continue
            fbp_enable_mask = fbp_mask
            fbp_enable_mask |= fbp1_mask
            yield self.__class__(clone = self.ClearFB(), fbp_enable_mask = fbp_enable_mask)

    def IterateColdFbp(self):
        for fbp in range(0, self.MAX_FBP, 2):
            fbp_mask = (1 << fbp)
            fbp1_mask = (1 << (fbp ^ 1))
            fbp_disable_mask = fbp_mask
            fbp_disable_mask |= fbp1_mask
            if fbp_disable_mask & self.fbp_disable_mask:
                continue
            if (fbp_disable_mask | self.fbp_disable_mask) == self.fbp_mask:
                continue
            yield self.__class__(clone = self.ClearFB(), fbp_disable_mask = fbp_disable_mask)

    def IterateLwlink(self):
        for lwlink in range(self.MAX_LWLINK):
            lwlink_mask = (1 << lwlink)
            if lwlink_mask & self.lwlink_disable_mask:
                continue
            lwlink_enable_mask = (1 << lwlink)
            yield self.__class__(clone = self, lwlink_enable_mask = lwlink_enable_mask)

class GP10yFsInfo(GP10xFsInfo):
    def __init__(self, **kwargs):
        super(GP10yFsInfo,self).__init__(**kwargs)

    @property
    def half_fbpa_disable_mask(self):
        # half_fbpa_disable = 1 means the half fbpa behavior is disabled,
        # ie. both halves of the fbpa are enabled
        mask = 0x0
        for fbp,ropl2_mask in enumerate(self.ropl2_disable_masks):
            if ropl2_mask == 0x0:
                mask |= (1<<fbp)
        return mask
    @property
    def half_fbpa_enable_mask(self):
        return (self.half_fbpa_disable_mask ^ self.fbp_mask) & self.fbp_mask

    def FsStr(self):
        fs = super(GP10yFsInfo,self).FsStr()
        return fs + ":half_fbpa_enable:{:#x}".format(self.half_fbpa_enable_mask)

    def __str__(self):
        return self.FsStr().replace("_enable", "")

class GP104FsInfo(GP10yFsInfo):
    MAX_GPC = 4
    MAX_TPC = 20
    MAX_FBP = 4
    MAX_FBIO = 4
    MAX_ROP = 8
    MAX_PES = [2,2,1]
    NUM_KAPPA = 5

    # Need to hard-code the kappa frequency offsets for GP104
    # See https://lwbugswb.lwpu.com/LwBugs5/SWBug.aspx?bugid=2017191&cmtNo=10
    FREQ_OFFSETS = [0, 28.8, 57.5, 86.3, 115]

    def __init__(self, **kwargs):
        self._kappa_bin = 0
        super(GP104FsInfo,self).__init__(**kwargs)

    def Copy(self, copy):
        super(GP104FsInfo,self).Copy(copy)
        self._kappa_bin = copy._kappa_bin

    def CloneKappa(self):
        return self.__class__(kappa_bin = self.kappa_bin)

    def ParseInitMasks(self, kwargs):
        super(GP104FsInfo,self).ParseInitMasks(kwargs)
        if "kappa_bin" in kwargs:
            self._kappa_bin = kwargs.pop("kappa_bin")

    def PrintChipInfo(self):
        super(GP104FsInfo,self).PrintChipInfo()
        Datalogger.PrintLog("... NUM_KAPPA_BINS      {}".format(self.NUM_KAPPA))
    def PrintFuseInfo(self):
        super(GP104FsInfo,self).PrintFuseInfo()
        Datalogger.PrintLog("... OPT_KAPPA_INFO     {}".format(self.kappa_bin))
    def LogFuseInfo(self):
        super(GP104FsInfo,self).LogFuseInfo()
        Datalogger.LogInfo(OPT_KAPPA_INFO = self.kappa_bin)

    @property
    def kappa_bin(self):
        return self._kappa_bin

    def FsStr(self):
        fs = super(GP104FsInfo,self).FsStr()
        # Need to manually set the frequency offset for GP104 because there is no actual kappa fuse yet
        return fs + " -freq_clk_domain_offset_khz gpc {}".format(self.FREQ_OFFSETS[self.kappa_bin])

    def __str__(self):
        fs = self.FsStr().replace(" -freq_clk_domain_offset_khz gpc ", ":kappa:")
        # replace frequency offset with its bin number. 0 will just be replaced with 0
        for (bin,offset) in enumerate(self.FREQ_OFFSETS):
            fs = fs.replace("{}".format(offset),"{}".format(bin))
        return fs

    def __or__(self, other):
        Exceptions.ASSERT(self.__class__ == other.__class__, "Cannot or {} with {}".format(self.__class__, other.__class__))
        or_ = super(GP104FsInfo,self).__or__(other)
        return self.__class__(clone = or_, kappa_bin = other.kappa_bin)

    def IterateKappa(self):
        for bin in range(1, self.NUM_KAPPA):
            yield self.__class__(clone = self, kappa_bin = bin)

class GP102FsInfo(GP10yFsInfo):
    MAX_GPC = 6
    MAX_TPC = 30
    MAX_FBP = 6
    MAX_FBIO = 6
    MAX_ROP = 12
    MAX_PES = [2,2,1]
    def __init__(self, **kwargs):
        super(GP102FsInfo,self).__init__(**kwargs)

class GP106FsInfo(GP10yFsInfo):
    MAX_GPC = 2
    MAX_TPC = 10
    MAX_FBP = 3
    MAX_FBIO = 3
    MAX_ROP = 6
    MAX_PES = [2,2,1]
    def __init__(self, **kwargs):
        super(GP106FsInfo,self).__init__(**kwargs)

class GP107FsInfo(GP10xFsInfo):
    MAX_GPC = 2
    MAX_TPC = 6
    MAX_FBP = 2
    MAX_FBIO = 2
    MAX_ROP = 4
    MAX_PES = [3]
    def __init__(self, **kwargs):
        super(GP107FsInfo,self).__init__(**kwargs)

    def MakeFBConsistent(self, sku_fbp_config=None):
        super(self.__class__,self).MakeFBConsistent()
        if sku_fbp_config:
            for fbp in range(self.MAX_FBP):
                fbp_disable = (1 << fbp)
                if (sku_fbp_config.CheckConfig(fbp_disable) == 0
                        and self.fbp_ropl2_disable_mask(fbp) & self.fbp_ropl2_mask):
                    self._fbp_disable_mask |= fbp_disable
                    self._fbio_disable_mask |= fbp_disable
                    self._ropl2_disable_masks[fbp] = self.fbp_ropl2_mask

    def IterateRop(self):
        # GP107 needs to disable ROPs in pairs asymmetrically, so there are only
        # two possible configurations
        yield self.__class__(clone = self.ClearFB(), ropl2_enable_masks = [0x1, 0x2])
        yield self.__class__(clone = self.ClearFB(), ropl2_enable_masks = [0x2, 0x1])

class GP108FsInfo(GP10xFsInfo):
    MAX_GPC = 1
    MAX_TPC = 3
    MAX_FBP = 1
    MAX_FBIO = 1
    MAX_ROP = 2
    MAX_PES = [3]
    def __init__(self, **kwargs):
        super(GP108FsInfo,self).__init__(**kwargs)
