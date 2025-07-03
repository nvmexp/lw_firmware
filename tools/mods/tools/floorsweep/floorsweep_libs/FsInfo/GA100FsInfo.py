# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import math
import re

from floorsweep_libs import Exceptions
from floorsweep_libs import Datalogger
from .VoltaFsInfo import GV100FsInfo

class GA100FsInfo(GV100FsInfo):
    MAX_GPC = 8
    MAX_TPC = 64
    MAX_FBP = 12
    MAX_FBIO = 24
    MAX_ROP = 24
    MAX_PES = [3,3,2]
    MAX_LWLINK = 12
    MAX_UGPU = 2
    MAX_SYSPIPE = 8
    MAX_PCE = 18
    MAX_LCE = 10
    MAX_LWDEC = 5
    MAX_OFA = 1
    MAX_LWJPG = 1
    def __init__(self, **kwargs):
        # Track defective FBs separate from disabled FBs because due to L2NOC an FBP could be disabled
        # even if it isn't necessarily defective, but it could be re-enabled if enough GPCs are found to be defective as well
        self._fbp_defective_mask = 0x0
        self._fbio_defective_mask = 0x0
        self._ropl2_defective_masks = [0x0 for x in range(self.MAX_FBP)]
        self._syspipe_disable_mask = 0x0
        self._syspipe_selected = -1
        self._ce_disable_mask = 0x0
        self._lwdec_disable_mask = 0x0
        self._ofa_disable_mask = 0x0
        self._lwjpg_disable_mask = 0x0

        self._override_l2noc = False
        super(GA100FsInfo,self).__init__(**kwargs)

    def ParseOverrideArgs(self, kwargs):
        if "override_l2noc" in kwargs:
            self._override_l2noc = kwargs.pop("override_l2noc")
        super(GA100FsInfo,self).ParseOverrideArgs(kwargs)

    def Copy(self, copy):
        super(GA100FsInfo,self).Copy(copy)
        self._fbp_defective_mask = copy.fbp_defective_mask
        self._fbio_defective_mask = copy.fbio_defective_mask
        self._ropl2_defective_masks = copy.ropl2_defective_masks
        self._syspipe_disable_mask = copy.syspipe_disable_mask
        self._syspipe_selected = copy.syspipe_selected
        self._ce_disable_mask = copy.ce_disable_mask
        self._lwdec_disable_mask = copy.lwdec_disable_mask
        self._ofa_disable_mask = copy.ofa_disable_mask
        self._lwjpg_disable_mask = copy.lwjpg_disable_mask

        self._override_l2noc = copy._override_l2noc

    def ParseFuseInfo(self, fuseinfo):
        super(GA100FsInfo,self).ParseFuseInfo(fuseinfo)
        self._fbp_defective_mask = self.fbp_disable_mask
        self._fbio_defective_mask = self.fbio_disable_mask
        self._ropl2_defective_masks = self.ropl2_disable_masks
        self._syspipe_disable_mask = int(fuseinfo['OPT_SYS_PIPE_DISABLE'],16)
        if (self._syspipe_disable_mask & 0x1):
            raise Exceptions.UnsupportedHardware("syspipe0 is disabled!")
        self._ce_disable_mask = 0x0
        self._lwdec_disable_mask = int(fuseinfo['OPT_LWDEC_DISABLE'], 16)
        self._ofa_disable_mask = int(fuseinfo['OPT_OFA_DISABLE'],16)
        self._lwjpg_disable_mask = int(fuseinfo['OPT_LWJPG_DISABLE'],16)
        if int(fuseinfo['SPARE_BIT_0'],16):
            # If spare bit 0 is set, then bits 1-9 indicate pairs of disabled CEs
            for x in range(1,10):
                if int(fuseinfo['SPARE_BIT_{}'.format(x)], 16):
                    self._ce_disable_mask |= (0x3 << ((x-1)*2))

    def ParseInitMasks(self, kwargs):
        super(GA100FsInfo,self).ParseInitMasks(kwargs)
        if "fbp_defective_mask" in kwargs:
            self._fbp_defective_mask = kwargs.pop("fbp_defective_mask")
        if "fbio_defective_mask" in kwargs:
            self._fbio_defective_mask = kwargs.pop("fbio_defective_mask")
        if ("ropl2_defective_masks") in kwargs:
            Exceptions.ASSERT(isinstance(kwargs["ropl2_defective_masks"], list), "ropl2_defective_masks is not a list")
            self._ropl2_defective_masks = kwargs.pop("ropl2_defective_masks")
        self._syspipe_disable_mask = self._GetMask(kwargs, self._syspipe_disable_mask, "syspipe", self.syspipe_mask)
        if "syspipe_selected" in kwargs:
            self._syspipe_selected = kwargs.pop("syspipe_selected")
        self._ce_disable_mask = self._GetMask(kwargs, self._ce_disable_mask, "ce", self.ce_mask)
        self._lwdec_disable_mask = self._GetMask(kwargs, self._lwdec_disable_mask, "lwdec", self.lwdec_mask)
        self._ofa_disable_mask = self._GetMask(kwargs, self._ofa_disable_mask, "ofa", self.ofa_mask)
        self._lwjpg_disable_mask = self._GetMask(kwargs, self._lwjpg_disable_mask, "lwjpg", self.lwjpg_mask)

    def FsStr(self):
        fs = super(GA100FsInfo,self).FsStr()
        if self.lwdec_disable_mask:
            fs += ":lwdec_enable:{:#x}".format(self.lwdec_enable_mask)
        if self.ofa_disable_mask:
            fs += ":ofa_enable:{:#x}".format(self.ofa_enable_mask)
        if self.lwjpg_disable_mask:
            fs += ":lwjpg_enable:{:#x}".format(self.lwjpg_enable_mask)
        if self.syspipe_disable_mask:
            fs += ":syspipe_enable:{:#x}".format(self.syspipe_enable_mask)
        if self.syspipe_selected >= 0:
            # By default don't include the partition command. Only if we're directly testing the syspipes.
            fs_part = " -smc_partitions part:full:"
            for syspipe in range(self.syspipe_selected+1):
                if syspipe == self.syspipe_selected:
                    fs_part += "A"
                    break
                if self.syspipe_enable_mask & (1 << syspipe):
                    # -smc_partitions relies on a logical index
                    fs_part += "0-"
            fs += fs_part
        if self.ce_disable_mask:
            if bin(self.ce_enable_mask).count('1') != 2:
                raise Exceptions.SoftwareError("Can only selectviely enable 2 PCEs")
            ce_idx = bin(self.ce_enable_mask)[::-1].find('1')
            if ce_idx % 2:
                raise Exceptions.SoftwareError("Invalid CE disable mask: {:#x}".format(self.ce_disable_mask))
            map = ['f','f','f','f','f','f','f','f']
            map1 = ['f','f','f','f','f','f','f','f']
            map2 = ['f','f','f','f','f','f','f','f']
            if ce_idx < 8:
                map[7-ce_idx] = '2'
                map[7-(ce_idx+1)] = '3'
            elif ce_idx < 16:
                map1[15-ce_idx] = '2'
                map1[15-(ce_idx+1)] = '3'
            elif ce_idx < 24:
                map2[23-ce_idx] = '2'
                map2[23-(ce_idx+1)] = '3'
            fs += ("-rmkey RmCePceMap {:#x} -rmkey RmCePceMap1 {:#x} -rmkey RmCePceMap2 {:#x} -rmkey RmCeGrceShared 0x32"
                   .format(int(''.join(map1),16), int(''.join(map2),16), int(''.join(map3),16)))

        return fs

    def __str__(self):
        fs = super(GA100FsInfo,self).__str__()
        fs = fs.replace("syspipe_enable", "syspipe")
        fs = re.sub(" -smc_partitions part:full:(0-)*A", ":{}".format(self.syspipe_selected), fs)
        fs = fs.replace("lwdec_enable", "lwdec")
        fs = fs.replace("ofa_enable", "ofa")
        fs = fs.replace("lwjpg_enable", "lwjpg")
        fs = re.sub(" -rmkey RmCePceMap .* -rmkey RmCePceMap1 .* -rmkey RmCePceMap2 .* -rmkey RmCeGrceShared 0x32", ":ce:{:#x}".format(self.ce_enable_mask), fs)
        return fs

    def PrintChipInfo(self):
        super(GA100FsInfo,self).PrintChipInfo()
        Datalogger.PrintLog("... MAX_SYSPIPE_MASK    {:b}".format(self.syspipe_mask))
        Datalogger.PrintLog("... MAX_CE_MASK         {:b}".format(self.ce_mask))
        Datalogger.PrintLog("... MAX_LWDEC_MASK      {:b}".format(self.lwdec_mask))
        Datalogger.PrintLog("... MAX_OFA_MASK        {:b}".format(self.ofa_mask))
        Datalogger.PrintLog("... MAX_LWJPG_MASK      {:b}".format(self.lwjpg_mask))

    def PrintFuseInfo(self):
        super(GA100FsInfo,self).PrintFuseInfo()
        Datalogger.PrintLog("... OPT_SYS_PIPE_DISABLE {:#x}".format(self.syspipe_disable_mask))
        Datalogger.PrintLog("... OPT_CE_DISABLE      {:#x}".format(self.ce_disable_mask))
        Datalogger.PrintLog("... OPT_LWDEC_DISABLE   {:#x}".format(self.lwdec_disable_mask))
        Datalogger.PrintLog("... OPT_OFA_DISABLE     {:#x}".format(self.ofa_disable_mask))
        Datalogger.PrintLog("... OPT_LWJPG_DISABLE   {:#x}".format(self.lwjpg_disable_mask))

    def PrintDefectiveFuseInfo(self):
        super(GA100FsInfo,self).PrintDefectiveFuseInfo()
        Datalogger.PrintLog("... OPT_FBP_DEFECTIVE   {:#x}".format(self.fbp_defective_mask))
        Datalogger.PrintLog("... OPT_FBIO_DEFECTIVE  {:#x}".format(self.fbio_defective_mask))
        Datalogger.PrintLog("... OPT_ROP_L2_DEFECTIVE {}".format(["{:#x}".format(mask) for mask in self.ropl2_defective_masks]))
        Datalogger.PrintLog("... OPT_GPC_DEFECTIVE   {:#x}".format(self.gpc_disable_mask))
        Datalogger.PrintLog("... OPT_TPC_DEFECTIVE   {}".format(["{:#x}".format(mask) for mask in self.tpc_disable_masks]))
        Datalogger.PrintLog("... OPT_PES_DEFECTIVE   {}".format(["{:#x}".format(mask) for mask in self.pes_disable_masks]))

    def LogFuseInfo(self):
        super(GA100FsInfo,self).LogFuseInfo()
        Datalogger.LogInfo(OPT_SYS_PIPE_DISABLE = '{:#x}'.format(self.syspipe_disable_mask))
        Datalogger.LogInfo(OPT_CE_DISABLE       = '{:#x}'.format(self.ce_disable_mask))
        Datalogger.LogInfo(OPT_LWDEC_DISABLE    = '{:#x}'.format(self.lwdec_disable_mask))
        Datalogger.LogInfo(OPT_OFA_DISABLE      = '{:#x}'.format(self.ofa_disable_mask))
        Datalogger.LogInfo(OPT_LWJPG_DISABLE    = '{:#x}'.format(self.lwjpg_disable_mask))

    def LogDefectiveFuseInfo(self):
        super(GA100FsInfo,self).LogDefectiveFuseInfo()
        Datalogger.LogInfo(OPT_FBP_DEFECTIVE    = '{:#x}'.format(self.fbp_defective_mask))
        Datalogger.LogInfo(OPT_FBIO_DEFECTIVE   = '{:#x}'.format(self.fbio_defective_mask))
        Datalogger.LogInfo(OPT_ROP_L2_DEFECTIVE = '{}'.format(["{:#x}".format(mask) for mask in self.ropl2_defective_masks]))
        Datalogger.LogInfo(OPT_GPC_DEFECTIVE    = '{:#x}'.format(self.gpc_disable_mask))
        Datalogger.LogInfo(OPT_TPC_DEFECTIVE    = '{}'.format(["{:#x}".format(mask) for mask in self.tpc_disable_masks]))
        Datalogger.LogInfo(OPT_PES_DEFECTIVE    = '{}'.format(["{:#x}".format(mask) for mask in self.pes_disable_masks]))

    def CloneOverrides(self):
        clone_ = super(GA100FsInfo,self).CloneOverrides()
        return self.__class__(clone = clone_,
                              override_l2noc = self.override_l2noc)

    def ClearOverride(self):
        clear_ = super(GA100FsInfo,self).ClearOverride()
        return self.__class__(clone = clear_,
                              override_l2noc = False)

    @property
    def override_l2noc(self):
        return self._override_l2noc
    def SetOverrideL2Noc(self):
        return self.__class__(clone = self, override_l2noc = True)

    def SetUnitTestOverrides(self):
        return super(GA100FsInfo,self).SetUnitTestOverrides().SetOverrideL2Noc()

    def IlwertRop(self):
        return self.__class__(fbp_defective_mask = ((self.fbp_defective_mask ^ self.fbp_mask) & self.fbp_mask))

    def __ilwert__(self):
        ilwert = super(GA100FsInfo,self).__ilwert__()
        return ilwert | self.IlwertSyspipe() | self.IlwertCe() | self.IlwertLwdec() | self.IlwertOfa() | self.IlwertLwjpg()

    def __or__(self, other):
        Exceptions.ASSERT(self.__class__ == other.__class__, "Cannot or {} with {}".format(self.__class__, other.__class__))
        or_ = super(GA100FsInfo,self).__or__(other)
        return self.__class__(clone = or_,
                              fbp_defective_mask = (self.fbp_defective_mask | other.fbp_defective_mask),
                              fbio_defective_mask = (self.fbio_defective_mask | other.fbio_defective_mask),
                              ropl2_defective_masks = [(x|y) for x,y in zip(self.ropl2_defective_masks, other.ropl2_defective_masks)],
                              syspipe_disable_mask = (self.syspipe_disable_mask | other.syspipe_disable_mask),
                              ce_disable_mask = (self.ce_disable_mask | other.ce_disable_mask),
                              lwdec_disable_mask = (self.lwdec_disable_mask | other.lwdec_disable_mask),
                              ofa_disable_mask = (self.ofa_disable_mask | other.ofa_disable_mask),
                              lwjpg_disable_mask = (self.lwjpg_disable_mask | other.lwjpg_disable_mask))

    def __xor__(self, other):
        Exceptions.ASSERT(self.__class__ == other.__class__, "Cannot or {} with {}".format(self.__class__, other.__class__))
        xor_ = super(GA100FsInfo,self).__xor__(other)
        return self.__class__(clone = xor_,
                              fbp_defective_mask = (self.fbp_defective_mask ^ other.fbp_defective_mask),
                              fbio_defective_mask = (self.fbio_defective_mask ^ other.fbio_defective_mask),
                              ropl2_defective_masks = [(x^y) for x,y in zip(self.ropl2_defective_masks, other.ropl2_defective_masks)],
                              syspipe_disable_mask = (self.syspipe_disable_mask ^ other.syspipe_disable_mask),
                              ce_disable_mask = (self.ce_disable_mask ^ other.ce_disable_mask),
                              lwdec_disable_mask = (self.lwdec_disable_mask ^ other.lwdec_disable_mask),
                              ofa_disable_mask = (self.ofa_disable_mask ^ other.ofa_disable_mask),
                              lwjpg_disable_mask = (self.lwjpg_disable_mask ^ other.lwjpg_disable_mask))

    def __and__(self, other):
        Exceptions.ASSERT(self.__class__ == other.__class__, "Cannot and {} with {}".format(self.__class__, other.__class__))
        and_ = super(GA100FsInfo,self).__and__(other)
        return self.__class__(clone = and_,
                              fbp_defective_mask = (self.fbp_defective_mask & other.fbp_defective_mask),
                              fbio_defective_mask = (self.fbio_defective_mask & other.fbio_defective_mask),
                              ropl2_defective_masks = [(x&y) for x,y in zip(self.ropl2_defective_masks, other.ropl2_defective_masks)],
                              syspipe_disable_mask = (self.syspipe_disable_mask & other.syspipe_disable_mask),
                              ce_disable_mask = (self.ce_disable_mask & other.ce_disable_mask),
                              lwdec_disable_mask = (self.lwdec_disable_mask & other.lwdec_disable_mask),
                              ofa_disable_mask = (self.ofa_disable_mask & other.ofa_disable_mask),
                              lwjpg_disable_mask = (self.lwjpg_disable_mask & other.lwjpg_disable_mask))

    def __eq__(self, other):
        if self.__class__ != other.__class__:
            return False
        return (super(GA100FsInfo,self).__eq__(other) and
                self.fbp_defective_mask == other.fbp_defective_mask and
                self.fbio_defective_mask == other.fbio_defective_mask and
                self.ropl2_defective_masks == other.ropl2_defective_masks and
                self.syspipe_disable_mask == other.syspipe_disable_mask and
                self.ce_disable_mask == other.ce_disable_mask and
                self.lwdec_disable_mask == other.lwdec_disable_mask and
                self.ofa_disable_mask == other.ofa_disable_mask and
                self.lwjpg_disable_mask == other.lwjpg_disable_mask)

    def IterateTpcBreadthFirst(self):
        def _IterateBits(mask):
            while mask:
                bit = mask & (~mask+1)
                yield bit
                mask ^= bit
        for tpc in range(self.tpc_per_gpc):
            # Alternate between the uGPUs for each GPC iteration
            for gpcidx in range(bin(self.ugpu_gpc_mask(0)).count("1")):
                for ugpu in range(self.MAX_UGPU):
                    gpc = int(math.log2([*_IterateBits(self.ugpu_gpc_mask(ugpu))][gpcidx]))
                    tpc_mask = (1 << tpc)
                    if tpc_mask & self.gpc_tpc_disable_mask(gpc):
                        continue
                    tpc_enable_masks = [0x0 for x in range(self.MAX_GPC)]
                    tpc_enable_masks[gpc] = tpc_mask
                    yield self.__class__(clone = self.ClearTpc(), tpc_enable_masks = tpc_enable_masks)

    def IterateGpc(self):
        for gpc in range(self.MAX_GPC):
            gpc_mask = (1 << gpc)
            yield self.__class__(clone = self.ClearTpc(), gpc_enable_mask = gpc_mask)

    def SmcPartitions(self, num_partitions):
        if num_partitions not in [1, 2, 4, 8]:
            # Other partition combinations are theoretically possible,
            # but for the sake of simplicity we'll only support these configs for now.
            raise Exceptions.SoftwareError("Unsupported num smc partitions = {}".format(num_partitions))

        partitions = []

        if num_partitions == 1:
            partitions = [self]

        if num_partitions >= 2:
            # Create two new object, one holding this object's enabled TPCs for uGPU0 and the other holding the TPCs for uGPU1
            partitions.append(self.__class__(clone = self.ClearTpc(), gpc_enable_mask = self.ugpu_gpc_mask(0),
                                                                      tpc_enable_masks = self.tpc_enable_masks))
            partitions.append(self.__class__(clone = self.ClearTpc(), gpc_enable_mask = self.ugpu_gpc_mask(1),
                                                                      tpc_enable_masks = self.tpc_enable_masks))

        if num_partitions >= 4:
            # Split the half partitions from above into 4 quarter partitions so each object has 2 GPCs
            partitions = [x for part in partitions for x in reversed(part.SplitTpc())]

        if num_partitions >= 8:
            # Split the quarter partitions from above into 8 eighth partitions so each object has 1 GPC
            partitions = [x for part in partitions for x in reversed(part.SplitTpc())]

        yield from partitions

    ###############
    # Syspipe
    ###############

    @property
    def syspipe_disable_mask(self):
        return self._syspipe_disable_mask
    @property
    def syspipe_enable_mask(self):
        return (self._syspipe_disable_mask ^ self.syspipe_mask) & self.syspipe_mask
    @property
    def syspipe_mask(self):
        return (1 << self.MAX_SYSPIPE) - 1
    @property
    def syspipe_selected(self):
        return self._syspipe_selected

    def DisableSelectedSyspipe(self):
        return self.__class__(syspipe_disable_mask = (1 << self.syspipe_selected))

    def CloneSyspipe(self):
        return self.__class__(syspipe_disable_mask = self.syspipe_disable_mask,
                              syspipe_selected = self.syspipe_selected)

    def ClearSyspipe(self):
        return self.__class__(clone = self, syspipe_disable_mask = 0x0,
                                            syspipe_selected = -1)

    def IlwertSyspipe(self):
        return self.__class__(syspipe_disable_mask = self.syspipe_enable_mask)

    def IterateSyspipe(self):
        for syspipe in range(self.MAX_SYSPIPE):
            syspipe_mask = (1 << syspipe)
            if syspipe_mask & self.syspipe_disable_mask:
                continue
            #syspipe0 cannot be disabled
            syspipe_mask |= 0x1
            yield self.__class__(clone = self, syspipe_enable_mask = syspipe_mask,
                                               syspipe_selected = syspipe)

    ###############
    # FB
    ###############
    # GA100 is split into FF sites corresponding to each HBM, but the pairs aren't intuitive
    #
    # HBM  FBPs
    # A    F0, F2
    # B    F1, F4
    # C    F3, F5
    # D    F7, F9
    # E    F6, F11
    # F    F8, F10
    #
    # Certain FBPs are paired to one another due to L2-NOC, so if one FBP is disabled, then the
    # paired FBP must also be disabled. The exception to this is if all
    # of the GPCs in a particular uGPU are disabled.
    #
    # F1 - F4
    # F0 - F3
    # F2 - F5
    # F6 - F11
    # F7 - F8
    # F9 - F10
    #
    # uGPU0 = HBM A F
    # uGPU0 = GPC 0, 1, 6, 7
    # uGPU1 = HBM C D
    # uGPU1 = GPC 2, 3, 4, 5

    def CloneFB(self):
        super_ = super(GA100FsInfo,self).CloneFB()
        return self.__class__(clone = super_,
                              fbp_defective_mask = self.fbp_defective_mask,
                              fbio_defective_mask = self.fbio_defective_mask,
                              ropl2_defective_masks = self.ropl2_defective_masks)

    @property
    def fbp_defective_mask(self):
        return self._fbp_defective_mask
    @property
    def fbio_defective_mask(self):
        return self._fbio_defective_mask
    @property
    def ropl2_defective_masks(self):
        return self._ropl2_defective_masks[:]
    def fbp_ropl2_defective_mask(self, fbp):
        return self._ropl2_defective_masks[fbp]

    @property
    def ugpu_mask(self):
        return (1 << self.MAX_UGPU) - 1
    def ugpu_gpc_mask(self, idx):
        if idx == 0:
            return 0xC3
        elif idx == 1:
            return 0x3C
        else:
            raise Exceptions.SoftwareError("Invalid uGPU index")
    def ugpu_fbp_mask(self, idx):
        if idx == 0:
            return 0x555
        elif idx == 1:
            return 0xAAA
        else:
            raise Exceptions.SoftwareError("Invalid uGPU index")
    @property
    def ugpu_enable_mask(self):
        mask = 0x0
        mask |= 0x1 if ((self.gpc_disable_mask & self.ugpu_gpc_mask(0)) != self.ugpu_gpc_mask(0)) else 0x0
        mask |= 0x2 if ((self.gpc_disable_mask & self.ugpu_gpc_mask(1)) != self.ugpu_gpc_mask(1)) else 0x0
        return mask
    @property
    def ugpu_disable_mask(self):
        mask = 0x0
        mask |= 0x1 if ((self.gpc_disable_mask & self.ugpu_gpc_mask(0)) == self.ugpu_gpc_mask(0)) else 0x0
        mask |= 0x2 if ((self.gpc_disable_mask & self.ugpu_gpc_mask(1)) == self.ugpu_gpc_mask(1)) else 0x0
        return mask

    def hbm_fbp_pair_idx(self, idx):
        '''
        Each HBM is made up of two FBPs, but these FBPs' indices aren't obvious
        '''
        return {0 : 2,
                1 : 4,
                2 : 0,
                3 : 5,
                4 : 1,
                5 : 3,
                6 : 11,
                7 : 9,
                8 : 10,
                9 : 7,
                10 : 8,
                11 : 6}[idx]

    def fbp_l2noc_idx(self, idx):
        '''
        Each FBP has a paired FBP that must also be disabled when both uGPUs are enabled
        '''
        return {0 : 3,
                1 : 4,
                2 : 5,
                3 : 0,
                4 : 1,
                5 : 2,
                6 : 11,
                7 : 8,
                8 : 7,
                9 : 10,
                10 : 9,
                11 : 6}[idx]

    def fbp_single_site_support(self, fbp_idx):
        '''
        Only HBMs A, C, D, & F support half HBM site
        '''
        fbp_disable = (1 << fbp_idx)
        return (0x7AD & fbp_disable)

    def MakeFBConsistent(self):
        for fbp in range(self.MAX_FBP):
            fbp_disable = (1 << fbp)
            fbio_disable = (self.fbp_fbio_mask << (fbp * self.fbio_per_fbp))
            if (fbp_disable & self.fbp_defective_mask):
                self._ropl2_defective_masks[fbp] = self.fbp_ropl2_mask
                self._fbio_defective_mask |= fbio_disable
            if (fbio_disable & self.fbio_defective_mask):
                self._ropl2_defective_masks[fbp] = self.fbp_ropl2_mask
                self._fbp_defective_mask |= fbp_disable
                self._fbio_defective_mask |= fbio_disable
            if ((self.fbp_ropl2_defective_mask(fbp) & self.fbp_ropl2_mask) != 0):
                self._ropl2_defective_masks[fbp] = self.fbp_ropl2_mask
                self._fbp_defective_mask |= fbp_disable
                self._fbio_defective_mask |= fbio_disable

        for fbp in range(self.MAX_FBP):
            fbp_disable = (1 << fbp)
            if (fbp_disable & self.fbp_defective_mask) and not self.fbp_single_site_support(fbp):
                pair_disable = (1 << self.hbm_fbp_pair_idx(fbp))
                pair_fbio_disable = (self.fbp_fbio_mask << (self.hbm_fbp_pair_idx(fbp) * self.fbio_per_fbp))
                self._fbp_defective_mask |= pair_disable
                self._fbio_defective_mask |= pair_fbio_disable
                self._ropl2_defective_masks[self.hbm_fbp_pair_idx(fbp)] = self.fbp_ropl2_mask

        # Reset FB to only defective components
        self._fbp_disable_mask = self.fbp_defective_mask
        self._fbio_disable_mask = self.fbio_defective_mask
        self._ropl2_disable_masks = self.ropl2_defective_masks

        # Disable the whole uGPU if all its HBMs are defective
        if (self.fbp_disable_mask & self.ugpu_fbp_mask(0)) == self.ugpu_fbp_mask(0):
            self._gpc_disable_mask |= self.ugpu_gpc_mask(0)
            self.MakeGpcConsistent() # Rerun to propogate disable
        if (self.fbp_disable_mask & self.ugpu_fbp_mask(1)) == self.ugpu_fbp_mask(1):
            self._gpc_disable_mask |= self.ugpu_gpc_mask(1)
            self.MakeGpcConsistent()

        # Check if HBMs in both uGPUs are enabled
        fbp_xor = self.ugpu_fbp_mask(0) ^ self.ugpu_fbp_mask(1)
        ugpu0 = fbp_xor & self.ugpu_fbp_mask(0)
        ugpu1 = fbp_xor & self.ugpu_fbp_mask(1)
        l2nocs_enabled = ((self.fbp_enable_mask & ugpu0) and
                          (self.fbp_enable_mask & ugpu1))
        # Check if there are enabled GPCs in both uGPUs
        ugpu_gpcs_enabled = ((self.gpc_enable_mask & self.ugpu_gpc_mask(0)) and
                             (self.gpc_enable_mask & self.ugpu_gpc_mask(1)))
        if not self.override_l2noc and (l2nocs_enabled or ugpu_gpcs_enabled):
            # Both uGPUs are enabled, so must disable l2noc pairs
            for fbp in range(self.MAX_FBP):
                fbp_disable = (1 << fbp)
                if (fbp_disable & self.fbp_disable_mask):
                    l2noc_disable = (1 << self.fbp_l2noc_idx(fbp))
                    l2noc_fbio_disable = (self.fbp_fbio_mask << (self.fbp_l2noc_idx(fbp) * self.fbio_per_fbp))
                    self._fbp_disable_mask |= l2noc_disable
                    self._fbio_disable_mask |= l2noc_fbio_disable
                    self._ropl2_disable_masks[self.fbp_l2noc_idx(fbp)] = self.fbp_ropl2_mask

        if (self.fbp_disable_mask == self.fbp_mask and
            bin(self.fbp_defective_mask & self.ugpu_fbp_mask(0)).count("1") == 6 and
            bin(self.fbp_defective_mask & self.ugpu_fbp_mask(1)).count("1") == 6):
            # Only one HBM enabled in each uGPU, but they're not L2NOC pairs, which means
            # we are only allowed to have one of them enabled
            # Arbitrarily disable uGPU1 so that at least uGPU0 can be usable with 1 HBM
            self._fbp_defective_mask |= self.ugpu_fbp_mask(1)
            self.MakeFBConsistent() # Rerun to propogate disables

    def IterateRop(self):
        # We cannot iterate individual ROPs in GA100
        yield from self.IterateFbp()

    def IterateColdFbp(self):
        def YieldColdHbm(mask):
            if mask & self.fbp_defective_mask:
                return
            if (mask | self.fbp_defective_mask) == self.fbp_mask:
                return
            yield self.__class__(clone = self.ClearFB(), fbp_defective_mask = mask)

        if (((self.gpc_disable_mask & self.ugpu_gpc_mask(0)) == self.ugpu_gpc_mask(0)) or
            ((self.gpc_disable_mask & self.ugpu_gpc_mask(1)) == self.ugpu_gpc_mask(1))):
            yield from YieldColdHbm(0x1)   # HBM A - F0
            yield from YieldColdHbm(0x4)   # HBM A - F2
            yield from YieldColdHbm(0x12)  # HBM B - F1 F4
            yield from YieldColdHbm(0x8)   # HBM C - F3
            yield from YieldColdHbm(0x20)  # HBM C - F5
            yield from YieldColdHbm(0x80)  # HBM D - F7
            yield from YieldColdHbm(0x200) # HBM D - F9
            yield from YieldColdHbm(0x840) # HBM E - F6 F11
            yield from YieldColdHbm(0x100) # HBM F - F8
            yield from YieldColdHbm(0x400) # HBM F - F10
        else:
            yield from YieldColdHbm(0x12)  # HBM B - F1 F4
            yield from YieldColdHbm(0x840) # HBM E - F6 F11
            yield from YieldColdHbm(0x9)   # HBM A, HBM C - F0 F3
            yield from YieldColdHbm(0x24)  # HBM A, HBM C - F2 F5
            yield from YieldColdHbm(0x180) # HBM D, HBM F - F7 F8
            yield from YieldColdHbm(0x600) # HBM D, HBM F - F9 F10

    def IterateFbp(self):
        def YieldHbm(mask):
            if mask & self.fbp_defective_mask:
                return
            mask = (mask ^ self.fbp_mask) & self.fbp_mask #ilwert into a defective mask
            yield self.__class__(clone = self.ClearFB(), fbp_defective_mask = mask)

        if ((self.gpc_disable_mask & self.ugpu_gpc_mask(0)) == self.ugpu_gpc_mask(0)):
            # uGPU0 is disabled
            yield from YieldHbm(0x12)  # HBM B - F1 F4
            yield from YieldHbm(0x8)   # HBM C - F3
            yield from YieldHbm(0x20)  # HBM C - F5
            yield from YieldHbm(0x80)  # HBM D - F7
            yield from YieldHbm(0x200) # HBM D - F9
            yield from YieldHbm(0x840) # HBM E - F6 F11
        elif ((self.gpc_disable_mask & self.ugpu_gpc_mask(1)) == self.ugpu_gpc_mask(1)):
            # uGPU1 is disabled
            yield from YieldHbm(0x1)   # HBM A - F0
            yield from YieldHbm(0x4)   # HBM A - F2
            yield from YieldHbm(0x12)  # HBM B - F1 F4
            yield from YieldHbm(0x840) # HBM E - F6 F11
            yield from YieldHbm(0x100) # HBM F - F8
            yield from YieldHbm(0x400) # HBM F - F10
        else:
            yield from YieldHbm(0x12)  # HBM B - F1 F4
            yield from YieldHbm(0x840) # HBM E - F6 F11
            yield from YieldHbm(0x9)   # HBM A, HBM C - F0 F3
            yield from YieldHbm(0x24)  # HBM A, HBM C - F2 F5
            yield from YieldHbm(0x180) # HBM D, HBM F - F7 F8
            yield from YieldHbm(0x600) # HBM D, HBM F - F9 F10

    ###############
    # CopyEngines
    ###############

    @property
    def ce_disable_mask(self):
        return self._ce_disable_mask
    @property
    def ce_enable_mask(self):
        return (self._ce_disable_mask ^ self.ce_mask) & self.ce_mask
    @property
    def ce_mask(self):
        return (1 << self.MAX_PCE) - 1

    def CloneCe(self):
        return self.__class__(ce_disable_mask = self.ce_disable_mask)

    def ClearCe(self):
        return self.__class__(clone = self, ce_disable_mask = 0x0)

    def IlwertCe(self):
        return self.__class__(ce_disable_mask = self.ce_enable_mask)

    def IterateCopyEngines(self):
        for ce in range(0, self.MAX_PCE, 2):
            ce_mask = (0x3 << ce)
            if ce_mask & self.ce_disable_mask:
                continue
            yield self.__class__(clone = self.ClearCe(), ce_enable_mask = ce_mask)

    ###############
    # Lwdec
    ###############

    @property
    def lwdec_disable_mask(self):
        return self._lwdec_disable_mask
    @property
    def lwdec_enable_mask(self):
        return (self._lwdec_disable_mask ^ self.lwdec_mask) & self.lwdec_mask
    @property
    def lwdec_mask(self):
        return (1 << self.MAX_LWDEC) - 1

    def CloneLwdec(self):
        return self.__class__(lwdec_disable_mask = self.lwdec_disable_mask)

    def ClearLwdec(self):
        return self.__class__(clone = self, lwdec_disable_mask = 0x0)

    def IlwertLwdec(self):
        return self.__class__(lwdec_disable_mask = self.lwdec_enable_mask)

    def IterateLwdec(self):
        for lwdec in range(self.MAX_LWDEC):
            lwdec_mask = (1 << lwdec)
            if lwdec_mask & self.lwdec_disable_mask:
                continue
            yield self.__class__(clone = self.ClearLwdec(), lwdec_enable_mask = lwdec_mask)

    ###############
    # OFA
    ###############

    @property
    def ofa_disable_mask(self):
        return self._ofa_disable_mask
    @property
    def ofa_enable_mask(self):
        return (self._ofa_disable_mask ^ self.ofa_mask) & self.ofa_mask
    @property
    def ofa_mask(self):
        return (1 << self.MAX_OFA) - 1

    def CloneOfa(self):
        return self.__class__(ofa_disable_mask = self.ofa_disable_mask)

    def ClearOfa(self):
        return self.__class__(clone = self, ofa_disable_mask = 0x0)

    def IlwertOfa(self):
        return self.__class__(ofa_disable_mask = self.ofa_enable_mask)

    def IterateOfa(self):
        for ofa in range(self.MAX_OFA):
            ofa_mask = (1 << ofa)
            if ofa_mask & self.ofa_disable_mask:
                continue
            yield self.__class__(clone = self.ClearOfa(), ofa_enable_mask = ofa_mask)

    ###############
    # Lwjpg
    ###############

    @property
    def lwjpg_disable_mask(self):
        return self._lwjpg_disable_mask
    @property
    def lwjpg_enable_mask(self):
        return (self._lwjpg_disable_mask ^ self.lwjpg_mask) & self.lwjpg_mask
    @property
    def lwjpg_mask(self):
        return (1 << self.MAX_LWJPG) - 1

    def CloneLwjpg(self):
        return self.__class__(lwjpg_disable_mask = self.lwjpg_disable_mask)

    def ClearLwjpg(self):
        return self.__class__(clone = self, lwjpg_disable_mask = 0x0)

    def IlwertLwjpg(self):
        return self.__class__(lwjpg_disable_mask = self.lwjpg_enable_mask)

    def IterateLwjpg(self):
        for lwjpg in range(self.MAX_LWJPG):
            lwjpg_mask = (1 << lwjpg)
            if lwjpg_mask & self.lwjpg_disable_mask:
                continue
            yield self.__class__(clone = self.ClearLwjpg(), lwjpg_enable_mask = lwjpg_mask)
