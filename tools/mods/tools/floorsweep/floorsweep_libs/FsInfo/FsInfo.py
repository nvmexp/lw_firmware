# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import re
import math

from floorsweep_libs import Datalogger
from floorsweep_libs import Utilities
from floorsweep_libs import Exceptions

class FsInfo(object):
    '''
    Object used to store floorsweeping information
    '''
    MAX_GPC = 0
    MAX_TPC = 0
    MAX_FBP = 0
    MAX_FBIO = 0
    MAX_ROP = 0
    def __init__(self, **kwargs):
        if (self.MAX_GPC == 0 or self.MAX_TPC == 0 or self.MAX_FBP == 0 or self.MAX_FBIO == 0 or self.MAX_ROP == 0):
            raise NotImplementedError("FsInfo MAX parameters unset")

        # Disable Masks
        self._gpc_disable_mask    = 0x0
        self._tpc_disable_masks   = [0x0 for x in range(self.MAX_GPC)]
        self._fbp_disable_mask    = 0x0
        self._fbio_disable_mask   = 0x0
        self._ropl2_disable_masks = [0x0 for x in range(self.MAX_FBP)]

        # Override Args
        self._override_consistent = False
        self._only_gpc = False
        self._only_fbp = False
        self._only_ltc = False

        if "clone" in kwargs:
            self.Copy(kwargs.pop("clone"))

        if "fuseinfo" in kwargs:
            self.ParseFuseInfo(kwargs.pop("fuseinfo"))

        self.ParseInitMasks(kwargs)

        self.ParseOverrideArgs(kwargs)

        Exceptions.ASSERT(len(kwargs) == 0, "Extraneous FsInfo init args: " + str(kwargs))

        if not self.override_consistent:
            self.MakeGpcConsistent()
            self.MakeFBConsistent()

    def Copy(self, copy):
        Exceptions.ASSERT(isinstance(copy, FsInfo))
        self._gpc_disable_mask    = copy.gpc_disable_mask
        self._tpc_disable_masks   = copy.tpc_disable_masks
        self._fbp_disable_mask    = copy.fbp_disable_mask
        self._fbio_disable_mask   = copy.fbio_disable_mask
        self._ropl2_disable_masks = copy.ropl2_disable_masks

        self._override_consistent = copy.override_consistent
        self._only_gpc = copy.only_gpc
        self._only_fbp = copy.only_fbp
        self._only_ltc = copy.only_ltc

    def ParseFuseInfo(self, fuseinfo):
        Exceptions.ASSERT(isinstance(fuseinfo, dict))

        self._fbp_disable_mask = int(fuseinfo['OPT_FBP_DISABLE'],16)
        self._fbio_disable_mask = int(fuseinfo['OPT_FBIO_DISABLE'],16)
        ropl2_disable_mask = int(fuseinfo['OPT_ROP_L2_DISABLE'],16)
        for fbp in range(self.MAX_FBP):
            self._ropl2_disable_masks[fbp] = (ropl2_disable_mask >> (fbp * self.ropl2_per_fbp)) & self.fbp_ropl2_mask

        self._gpc_disable_mask = int(fuseinfo['OPT_GPC_DISABLE'],16)
        tpc_disable_mask = int(fuseinfo['OPT_TPC_DISABLE'],16)
        for gpc in range(self.MAX_GPC):
            self._tpc_disable_masks[gpc] = (tpc_disable_mask >> (gpc * self.tpc_per_gpc)) & self.gpc_tpc_mask

    def ParseInitMasks(self, kwargs):
        Exceptions.ASSERT(isinstance(kwargs, dict))
        self._gpc_disable_mask    = self._GetMask(kwargs, self._gpc_disable_mask, "gpc", self.gpc_mask)
        self._tpc_disable_masks   = self._GetMasks(kwargs, self._tpc_disable_masks, "tpc", self.gpc_tpc_mask)
        self._fbp_disable_mask    = self._GetMask(kwargs, self._fbp_disable_mask, "fbp", self.fbp_mask)
        self._fbio_disable_mask   = self._GetMask(kwargs, self._fbio_disable_mask, "fbio", self.fbio_mask)
        self._ropl2_disable_masks = self._GetMasks(kwargs, self._ropl2_disable_masks, "ropl2", self.fbp_ropl2_mask)

    def ParseOverrideArgs(self, kwargs):
        if "override_consistent" in kwargs:
            self._override_consistent = kwargs.pop("override_consistent")
        if "only_gpc" in kwargs:
            self._only_gpc = kwargs.pop("only_gpc")
        if "only_fbp" in kwargs:
            self._only_fbp = kwargs.pop("only_fbp")
        if "only_ltc" in kwargs:
            self._only_ltc = kwargs.pop("only_ltc")

    def _GetMask(self, kwargs, mask, name, ilwert_mask):
        '''Helper function for ParseInitMasks'''
        if (name + "_disable_mask") in kwargs:
            return kwargs.pop(name + "_disable_mask")
        if (name + "_enable_mask") in kwargs:
            return (kwargs.pop(name + "_enable_mask") ^ ilwert_mask) & ilwert_mask
        return mask
    def _GetMasks(self, kwargs, masks, name, ilwert_mask):
        '''Helper function for ParseInitMasks'''
        if (name + "_disable_masks") in kwargs:
            Exceptions.ASSERT(isinstance(kwargs[name + "_disable_masks"], list), name + "_disable_masks is not a list")
            return kwargs.pop(name + "_disable_masks")[:] #copy
        if (name + "_enable_masks") in kwargs:
            Exceptions.ASSERT(isinstance(kwargs[name + "_enable_masks"], list), name + "_enable_masks is not a list")
            return [((mask ^ ilwert_mask) & ilwert_mask) for mask in kwargs.pop(name + "_enable_masks")]
        return masks

    def FsStr(self):
        fs = ["fbp_enable:{:#x}:fbio_enable:{:#x}".format(self.fbp_enable_mask, self.fbio_enable_mask)]
        for fbp_idx,ropl2_mask in enumerate(self.ropl2_enable_masks):
            if ropl2_mask and ropl2_mask != self.fbp_ropl2_mask:
                fs.append("fbp_rop_l2_enable:{:d}:{:#x}".format(fbp_idx, ropl2_mask))

        fs.append("gpc_enable:{:#x}".format(self.gpc_enable_mask))
        for gpc_idx,tpc_mask in enumerate(self.tpc_enable_masks):
            if tpc_mask and tpc_mask != self.gpc_tpc_mask:
                fs.append("gpc_tpc_enable:{:d}:{:#x}".format(gpc_idx, tpc_mask))

        return ':'.join(fs)

    def __str__(self):
        fs = self.FsStr()
        return fs.replace("_enable", "")

    def PrintChipInfo(self):
        Datalogger.PrintLog("... MAX_FBP_MASK        {:b}".format(self.fbp_mask))
        Datalogger.PrintLog("... MAX_FBIO_MASK       {:b}".format(self.fbio_mask))
        Datalogger.PrintLog("... MAX_FBP_ROP_L2_MASK {:b}".format(self.fbp_ropl2_mask))
        Datalogger.PrintLog("... MAX_GPC_MASK        {:b}".format(self.gpc_mask))
        Datalogger.PrintLog("... MAX_GPC_TPC_MASK    {:b}".format(self.gpc_tpc_mask))
    def PrintFuseInfo(self):
        Datalogger.PrintLog("... OPT_FBP_DISABLE    {:#x}".format(self.fbp_disable_mask))
        Datalogger.PrintLog("... OPT_FBIO_DISABLE   {:#x}".format(self.fbio_disable_mask))
        Datalogger.PrintLog("... OPT_ROP_L2_DISABLE {}".format(["{:#x}".format(mask) for mask in self.ropl2_disable_masks]))
        Datalogger.PrintLog("... OPT_GPC_DISABLE    {:#x}".format(self.gpc_disable_mask))
        Datalogger.PrintLog("... OPT_TPC_DISABLE    {}".format(["{:#x}".format(mask) for mask in self.tpc_disable_masks]))
    def PrintDefectiveFuseInfo(self):
        # No defective fuses by default
        pass

    def _PrintDisableMask(self, str, mask):
        if bin(mask).count('1') > 0:
            Datalogger.PrintLog("Disabling {} {:#x}".format(str, mask))
    def _PrintDisableMasks(self, str, masks):
        if sum([bin(mask).count('1') for mask in masks]) > 0:
            Datalogger.PrintLog("Disabling {} {}".format(str, ["{:#x}".format(mask) for mask in masks]))
    def PrintDisableInfo(self):
        self._PrintDisableMask("FBP", self.fbp_disable_mask)
        self._PrintDisableMask("FBIO", self.fbio_disable_mask)
        self._PrintDisableMasks("ROP_L2", self.ropl2_disable_masks)
        self._PrintDisableMask("GPC", self.gpc_disable_mask)
        self._PrintDisableMasks("TPC", self.tpc_disable_masks)

    def Print(self):
        Datalogger.PrintLog("CHIP INFO :")
        self.PrintChipInfo()
        Datalogger.PrintLog("FUSE INFO :")
        self.PrintFuseInfo()

    def LogFuseInfo(self):
        gpc_disable   = '{:#x}'.format(self.gpc_disable_mask)
        tpc_disable   = '{:#x}'.format(int(''.join(['{:0{width}b}'.format(mask,width=self.tpc_per_gpc) for mask in reversed(self.tpc_disable_masks)]),2))
        fbp_disable   = '{:#x}'.format(self.fbp_disable_mask)
        fbio_disable  = '{:#x}'.format(self.fbio_disable_mask)
        ropl2_disable = '{:#x}'.format(int(''.join(['{:0{width}b}'.format(mask,width=self.ropl2_per_fbp) for mask in reversed(self.ropl2_disable_masks)]),2))

        Datalogger.LogInfo(OPT_GPC_DISABLE    = gpc_disable)
        Datalogger.LogInfo(OPT_TPC_DISABLE    = tpc_disable)
        Datalogger.LogInfo(OPT_FBP_DISABLE    = fbp_disable)
        Datalogger.LogInfo(OPT_FBIO_DISABLE   = fbio_disable)
        Datalogger.LogInfo(OPT_ROP_L2_DISABLE = ropl2_disable)

    def CreateFsLibStruct(self):
        raise Exceptions.UnsupportedFunction("This chip doesn't support FsLib")

    def LogDefectiveFuseInfo(self):
        # No defective fuses by default
        pass

    def WriteFbFb(self):
        fs = ["fbp_enable:{:#x}:fbio_enable:{:#x}".format(self.fbp_enable_mask, self.fbio_enable_mask)]
        for fbp_idx,ropl2_mask in enumerate(self.ropl2_enable_masks):
            if ropl2_mask and ropl2_mask != self.fbp_ropl2_mask:
                fs.append("fbp_rop_l2_enable:{:d}:{:#x}".format(fbp_idx, ropl2_mask))
        fb_str = ':'.join(fs)

        with open("fb_fb.arg", "w+") as fb_fb:
            fb_fb.write("-floorsweep {}".format(fb_str))

    def WriteFsArg(self):
        with open("fs.arg", "w+") as fs_arg:
            fs_arg.write("-floorsweep {}".format(self.FsStr()))

    def Clone(self):
        return self.__class__(clone = self)

    def Clear(self):
        return self.__class__()

    def CloneTpc(self):
        return self.__class__(gpc_disable_mask  = self.gpc_disable_mask,
                              tpc_disable_masks = self.tpc_disable_masks)

    def ClearTpc(self):
        return self.__class__(clone = self, gpc_disable_mask = 0x0,
                                            tpc_disable_masks = [0x0 for x in range(self.MAX_GPC)])

    def CloneFB(self):
        return self.__class__(fbp_disable_mask    = self.fbp_disable_mask,
                              fbio_disable_mask   = self.fbio_disable_mask,
                              ropl2_disable_masks = self.ropl2_disable_masks)

    def ClearFB(self):
        return self.__class__(clone = self, fbp_disable_mask    = 0x0,
                                            fbio_disable_mask   = 0x0,
                                            ropl2_disable_masks = [0x0 for x in range(self.MAX_FBP)])

    def CloneOverrides(self):
        return self.__class__(override_consistent = self.override_consistent,
                              only_gpc = self.only_gpc,
                              only_fbp = self.only_fbp,
                              only_ltc = self.only_ltc)

    def ClearOverride(self):
        return self.__class__(clone = self, override_consistent = False,
                                            only_gpc = False,
                                            only_fbp = False,
                                            only_ltc = False)

    # Read-only property accessors
    @property
    def gpc_disable_mask(self):
        return self._gpc_disable_mask
    @property
    def gpc_enable_mask(self):
        return (self.gpc_disable_mask ^ self.gpc_mask) & self.gpc_mask
    @property
    def tpc_disable_masks(self):
        return self._tpc_disable_masks[:] # return a copy, not a reference
    @property
    def tpc_enable_masks(self):
        return [((mask ^ self.gpc_tpc_mask) & self.gpc_tpc_mask) for mask in self._tpc_disable_masks]
    def gpc_tpc_disable_mask(self, gpc):
        return self._tpc_disable_masks[gpc]
    def gpc_tpc_enable_mask(self, gpc):
        return (self.gpc_tpc_disable_mask(gpc) ^ self.gpc_tpc_mask) & self.gpc_tpc_mask
    @property
    def fbp_disable_mask(self):
        return self._fbp_disable_mask
    @property
    def fbp_enable_mask(self):
        return (self.fbp_disable_mask ^ self.fbp_mask) & self.fbp_mask
    @property
    def fbio_disable_mask(self):
        return self._fbio_disable_mask
    @property
    def fbio_enable_mask(self):
        return (self.fbio_disable_mask ^ self.fbio_mask) & self.fbio_mask
    @property
    def ropl2_disable_masks(self):
        return self._ropl2_disable_masks[:] # return a copy, not a reference
    @property
    def ropl2_enable_masks(self):
        return [((mask ^ self.fbp_ropl2_mask) & self.fbp_ropl2_mask) for mask in self._ropl2_disable_masks]
    def fbp_ropl2_disable_mask(self, fbp):
        return self._ropl2_disable_masks[fbp]
    def fbp_ropl2_enable_mask(self, fbp):
        return (self.fbp_ropl2_disable_mask(fbp) ^ self.fbp_ropl2_mask) & self.fbp_ropl2_mask

    # Read-only helper attributes
    @property
    def ropl2_per_fbp(self):
        return int(self.MAX_ROP / self.MAX_FBP)
    @property
    def tpc_per_gpc(self):
        return int(self.MAX_TPC / self.MAX_GPC)
    @property
    def fbp_mask(self):
        return (1 << self.MAX_FBP) - 1
    @property
    def fbio_mask(self):
        return (1 << self.MAX_FBIO) - 1
    @property
    def fbp_ropl2_mask(self):
        return (1 << self.ropl2_per_fbp) - 1
    @property
    def gpc_mask(self):
        return (1 << self.MAX_GPC) - 1
    @property
    def gpc_tpc_mask(self):
        return (1 << self.tpc_per_gpc) - 1

    def MakeGpcConsistent(self):
        for gpc in range(self.MAX_GPC):
            gpc_disable = (1 << gpc)
            gpc_tpc_disable = (self.gpc_tpc_disable_mask(gpc) & self.gpc_tpc_mask)
            if ((gpc_disable & self.gpc_disable_mask) or
                    (gpc_tpc_disable == self.gpc_tpc_mask) or
                    (self.only_gpc and gpc_tpc_disable != 0)):
                self._gpc_disable_mask |= gpc_disable
                self._tpc_disable_masks[gpc] = self.gpc_tpc_mask

    def MakeFBConsistent(self):
        for fbp in range(self.MAX_FBP):
            fbp_disable = (1 << fbp)
            fbp_ropl2_disable = (self.fbp_ropl2_disable_mask(fbp) & self.fbp_ropl2_mask)
            if ((fbp_disable & self.fbp_disable_mask) or
                    (fbp_disable & self.fbio_disable_mask) or
                    (fbp_ropl2_disable == self.fbp_ropl2_mask) or
                    (self.only_fbp and fbp_ropl2_disable != 0)):
                self._fbp_disable_mask |= fbp_disable
                self._fbio_disable_mask |= fbp_disable
                self._ropl2_disable_masks[fbp] = self.fbp_ropl2_mask

    # Override Arg Methods
    @property
    def override_consistent(self):
        return self._override_consistent
    def SetOverrideConsistent(self):
        return self.__class__(clone = self, override_consistent = True)
    @property
    def only_gpc(self):
        return self._only_gpc
    def SetOnlyGpc(self):
        return self.__class__(clone = self, only_gpc = True)
    @property
    def only_fbp(self):
        return self._only_fbp
    def SetOnlyFbp(self):
        return self.__class__(clone = self, only_fbp = True)
    @property
    def only_ltc(self):
        return self._only_ltc
    def SetOnlyLtc(self):
        return self.__class__(clone = self, only_ltc = True)

    def SetUnitTestOverrides(self):
        # Overrides that are necessary to properly check fsinfo in UnitTests
        return self.SetOverrideConsistent()

    def CheckSanity(self):
        if (self.gpc_disable_mask == self.gpc_mask):
            raise Exceptions.FsSanityFailed("Sanity Failed. Tried to disable every GPC.")
        if (self.fbp_disable_mask == self.fbp_mask):
            raise Exceptions.FsSanityFailed("Sanity Failed. Tried to disable every FBP.")

    # Bitwise operators
    def IlwertTpc(self):
        return self.__class__(tpc_disable_masks = self.tpc_enable_masks)
    def IlwertRop(self):
        return self.__class__(ropl2_disable_masks = self.ropl2_enable_masks)
    def __ilwert__(self):
        # Only ilwert the smallest components, eg. tpc and ropl2, because an aggregate might
        # still be enabled when its component mask is ilwerted. The results will trickle up
        # to the aggregates when the masks are made consistent.
        return self.CloneOverrides() | self.IlwertTpc() | self.IlwertRop()

    def __or__(self, other):
        Exceptions.ASSERT(self.__class__ == other.__class__, "Cannot or {} with {}".format(self.__class__, other.__class__))
        return self.__class__(clone = self.CloneOverrides(),
                              gpc_disable_mask    = (self.gpc_disable_mask   | other.gpc_disable_mask),
                              tpc_disable_masks   = [(x|y) for x,y in zip(self.tpc_disable_masks, other.tpc_disable_masks)],
                              fbp_disable_mask    = (self.fbp_disable_mask   | other.fbp_disable_mask),
                              fbio_disable_mask   = (self.fbio_disable_mask   | other.fbio_disable_mask),
                              ropl2_disable_masks = [(x|y) for x,y in zip(self.ropl2_disable_masks, other.ropl2_disable_masks)])

    def XorTpc(self, other):
        Exceptions.ASSERT(self.__class__ == other.__class__, "Cannot and {} with {}".format(self.__class__, other.__class__))
        return self.__class__(clone = (self.CloneOverrides() | self.CloneFB()),
                              tpc_disable_masks   = [(x^y) for x,y in zip(self.tpc_disable_masks, other.tpc_disable_masks)])
    def __xor__(self, other):
        Exceptions.ASSERT(self.__class__ == other.__class__, "Cannot and {} with {}".format(self.__class__, other.__class__))
        return self.__class__(clone = self.CloneOverrides(),
                              gpc_disable_mask    = (self.gpc_disable_mask   ^ other.gpc_disable_mask),
                              tpc_disable_masks   = [(x^y) for x,y in zip(self.tpc_disable_masks, other.tpc_disable_masks)],
                              fbp_disable_mask    = (self.fbp_disable_mask   ^ other.fbp_disable_mask),
                              fbio_disable_mask   = (self.fbio_disable_mask  ^ other.fbio_disable_mask),
                              ropl2_disable_masks = [(x^y) for x,y in zip(self.ropl2_disable_masks, other.ropl2_disable_masks)])

    def __and__(self, other):
        Exceptions.ASSERT(self.__class__ == other.__class__, "Cannot and {} with {}".format(self.__class__, other.__class__))
        return self.__class__(clone = self.CloneOverrides(),
                              gpc_disable_mask    = (self.gpc_disable_mask   & other.gpc_disable_mask),
                              tpc_disable_masks   = [(x&y) for x,y in zip(self.tpc_disable_masks, other.tpc_disable_masks)],
                              fbp_disable_mask    = (self.fbp_disable_mask   & other.fbp_disable_mask),
                              fbio_disable_mask   = (self.fbio_disable_mask  & other.fbio_disable_mask),
                              ropl2_disable_masks = [(x&y) for x,y in zip(self.ropl2_disable_masks, other.ropl2_disable_masks)])

    def __eq__(self, other):
        if self.__class__ != other.__class__:
            return False
        return (self.gpc_disable_mask == other.gpc_disable_mask and
                self.tpc_disable_masks == other.tpc_disable_masks and
                self.fbp_disable_mask == other.fbp_disable_mask and
                self.fbio_disable_mask == other.fbio_disable_mask and
                self.ropl2_disable_masks == other.ropl2_disable_masks)

    def IsSingleTpc(self):
        return (1 == sum([int(i) for mask in self.tpc_enable_masks for i in '{:b}'.format(mask)]))

    def IsSingleGpc(self):
        return (1 == sum([int(i) for i in '{:b}'.format(self.gpc_enable_mask)]))

    def IterateGpc(self):
        for gpc in range(self.MAX_GPC):
            gpc_mask = (1 << gpc)
            yield self.__class__(clone = self.ClearTpc(), gpc_enable_mask = gpc_mask)

    def IterateColdGpc(self):
        for gpc in range(self.MAX_GPC):
            gpc_mask = (1 << gpc)
            if gpc_mask & self.gpc_disable_mask:
                continue
            gpc_disable_mask = self.gpc_disable_mask
            gpc_disable_mask |= gpc_mask
            yield self.__class__(clone = self.ClearTpc(), gpc_disable_mask = gpc_disable_mask,
                                                          tpc_disable_masks = self.tpc_disable_masks)

    def IterateTpc(self):
        for gpc in range(self.MAX_GPC):
            for tpc in range(self.tpc_per_gpc):
                tpc_mask = (1 << tpc)
                if tpc_mask & self.gpc_tpc_disable_mask(gpc):
                    continue
                tpc_enable_masks = [0x0 for x in range(self.MAX_GPC)]
                tpc_enable_masks[gpc] = tpc_mask
                yield self.__class__(clone = self.ClearTpc(), tpc_enable_masks = tpc_enable_masks)

    def IterateTpcBreadthFirst(self):
        for tpc in range(self.tpc_per_gpc):
            for gpc in range(self.MAX_GPC):
                tpc_mask = (1 << tpc)
                if tpc_mask & self.gpc_tpc_disable_mask(gpc):
                    continue
                tpc_enable_masks = [0x0 for x in range(self.MAX_GPC)]
                tpc_enable_masks[gpc] = tpc_mask
                yield self.__class__(clone = self.ClearTpc(), tpc_enable_masks = tpc_enable_masks)

    def IterateColdTpc(self, in_gpc=None):
        if in_gpc == None:
            gpcs = range(self.MAX_GPC)
        else:
            gpcs = [in_gpc]
        for gpc in gpcs:
            for tpc in range(self.tpc_per_gpc):
                tpc_mask = (1 << tpc)
                if tpc_mask & self.gpc_tpc_disable_mask(gpc):
                    continue
                tpc_disable_masks = self.tpc_disable_masks
                tpc_disable_masks[gpc] |= tpc_mask
                yield self.__class__(clone = self.ClearTpc(), tpc_disable_masks = tpc_disable_masks)

    def IterateRop(self):
        for fbp in range(self.MAX_FBP):
            for ropl2 in range(self.ropl2_per_fbp):
                ropl2_mask = (1 << ropl2)
                if ropl2_mask & self.fbp_ropl2_disable_mask(fbp):
                    continue
                ropl2_enable_masks = [0x0 for x in range(self.MAX_FBP)]
                ropl2_enable_masks[fbp] = ropl2_mask
                yield self.__class__(clone = self.ClearFB(), ropl2_enable_masks = ropl2_enable_masks)

    def IterateColdFbp(self):
        for fbp in range(self.MAX_FBP):
            fbp_mask = (1 << fbp)
            if fbp_mask & self.fbp_disable_mask:
                continue
            if (fbp_mask | self.fbp_disable_mask) == self.fbp_mask:
                continue
            yield self.__class__(clone = self.ClearFB(), fbp_disable_mask = fbp_mask,
                                                          ropl2_disable_masks = self.ropl2_disable_masks)

    def IterateFbp(self):
        for fbp in range(self.MAX_FBP):
            fbp_mask = (1 << fbp)
            if fbp_mask & self.fbp_disable_mask:
                continue
            yield self.__class__(clone = self.ClearFB(), fbp_enable_mask = fbp_mask)

    def SplitTpc(self):
        if self.IsSingleGpc():
            gpc_idx = int(math.log(self.gpc_enable_mask,2))
            left_tpc_mask,right_tpc_mask = self._SplitMask(self.gpc_tpc_enable_mask(gpc_idx))

            left_tpc_masks = [0x0 for x in range(self.MAX_GPC)]
            right_tpc_masks = [0x0 for x in range(self.MAX_GPC)]
            left_tpc_masks[gpc_idx] = left_tpc_mask
            right_tpc_masks[gpc_idx] = right_tpc_mask

            left_tpc = self.__class__(clone = self.ClearTpc(), tpc_enable_masks = left_tpc_masks)
            right_tpc = self.__class__(clone = self.ClearTpc(), tpc_enable_masks = right_tpc_masks)
            return left_tpc,right_tpc
        else:
            left_gpc_mask,right_gpc_mask = self._SplitMask(self.gpc_enable_mask)
            left_gpc = self.__class__(clone = self.ClearTpc(), gpc_enable_mask = left_gpc_mask)
            right_gpc = self.__class__(clone = self.ClearTpc(), gpc_enable_mask = right_gpc_mask)
            return left_gpc,right_gpc

    def _SplitMask(self, mask):
        if (1 == sum([int(i) for i in '{:b}'.format(mask)])):
            raise Exceptions.SoftwareError("Cannot split mask with 1 enabled bit")

        def _Enabled_bit_range(mask):
            '''
            Return [start,end) indexed range of enabled bits in mask
            E.g. 0b111000 returns 3,6
            '''
            start = 0
            while not (mask & 0x1):
                mask >>= 1
                start += 1
            end = start
            while mask:
                mask >>= 1
                end += 1
            return start,end
        def _Bit_range_mask(start, end):
            '''
            Effectively the ilwerse of _Enabled_bit_range
            '''
            return int('1'*(end-start), 2) << start

        mask_start,mask_end = _Enabled_bit_range(mask)
        mask_split = int((mask_end-mask_start)/2)
        right_mask = _Bit_range_mask(mask_start, mask_start + mask_split)
        left_mask  = _Bit_range_mask(mask_start + mask_split, mask_end)

        return left_mask,right_mask

    def SmcPartitions(self, num_partitions):
        raise Exceptions.UnsupportedFunction("This chip doesn't support SMC")

    def IterateLwlink(self):
        raise Exceptions.UnsupportedFunction("This chip doesn't support lwlink")

    def IterateKappa(self):
        raise Exceptions.UnsupportedFunction("This chip doesn't support kappa binning")

    def IterateSyspipe(self):
        raise Exceptions.UnsupportedFunction("This chip doesn't support syspipe floorsweeping")

    def IterateCopyEngines(self):
        raise Exceptions.UnsupportedFunction("This chip doesn't support CE floorsweeping")

    def IterateLwdec(self):
        raise Exceptions.UnsupportedFunction("This chip doesn't support Lwdec floorsweeping")

    def IterateOfa(self):
        raise Exceptions.UnsupportedFunction("This chip doesn't support OFA floorsweeping")

    def IterateLwjpg(self):
        raise Exceptions.UnsupportedFunction("This chip doesn't support LWJPG floorsweeping")
