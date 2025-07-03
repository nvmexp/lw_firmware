# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import math
import re

from floorsweep_libs import Exceptions
from floorsweep_libs import Datalogger
from floorsweep_libs import FsLib
from .GA100FsInfo import GA100FsInfo
from .GA10xFsInfo import GA10xFsInfo

class GH100FsInfo(GA100FsInfo):
    CHIP = "gh100"
    MAX_GPC = 8
    MAX_CPC = 24
    MAX_TPC = 72
    MAX_FBP = 12
    MAX_FBIO = 24
    MAX_LTC = 24
    MAX_SLICE = 96
    MAX_ROP = 8
    MAX_PES = [9]
    MAX_IOCTRL = 3
    MAX_LWLINK = 18
    MAX_UGPU = 2
    MAX_SYSPIPE = 8
    MAX_PCE = 16
    MAX_LCE = 10
    MAX_LWDEC = 8
    MAX_OFA = 1
    MAX_LWJPG = 8
    def __init__(self, **kwargs):
        self._ltc_disable_masks = [0x0 for x in range(self.MAX_FBP)]
        self._slice_disable_masks = [0x0 for x in range(self.MAX_FBP)]
        self._cpc_disable_masks = [0x0 for x in range(self.MAX_GPC)]

        self._ioctrl_disable_mask = 0x0
        self._lwlink_disable_mask = 0x0

        self._override_cpc2 = False

        super(GH100FsInfo,self).__init__(**kwargs)

        if not self.override_consistent:
            self.MakeLwLinkConsistent()

    def Copy(self, copy):
        self._gpc_disable_mask      = copy.gpc_disable_mask
        self._cpc_disable_masks     = copy.cpc_disable_masks
        self._tpc_disable_masks     = copy.tpc_disable_masks

        self._fbp_disable_mask      = copy.fbp_disable_mask
        self._fbio_disable_mask     = copy.fbio_disable_mask
        self._ltc_disable_masks     = copy.ltc_disable_masks
        self._slice_disable_masks   = copy.slice_disable_masks

        self._syspipe_disable_mask = copy.syspipe_disable_mask
        self._syspipe_selected = copy.syspipe_selected

        self._ioctrl_disable_mask = copy.ioctrl_disable_mask
        self._lwlink_disable_mask = copy.lwlink_disable_mask

        self._override_l2noc = copy.override_l2noc
        self._override_consistent = copy.override_consistent
        self._override_cpc2 = copy.override_cpc2
        self._only_gpc = copy.only_gpc
        self._only_fbp = copy.only_fbp
        self._only_ltc = copy.only_ltc

    def ParseOverrideArgs(self, kwargs):
        if "override_cpc2" in kwargs:
            self._override_cpc2 = kwargs.pop("override_cpc2")
        super(GH100FsInfo,self).ParseOverrideArgs(kwargs)

    def ParseFuseInfo(self, fuseinfo):
        def _GetFuseMask(name):
            return int(fuseinfo[name],16) | int(fuseinfo[name + '_CP'],16)

        self._fbp_disable_mask = _GetFuseMask('OPT_FBP_DISABLE')
        self._fbio_disable_mask = _GetFuseMask('OPT_FBIO_DISABLE')

        ltc_disable_mask = _GetFuseMask('OPT_LTC_DISABLE')
        for fbp in range(self.MAX_FBP):
            self._ltc_disable_masks[fbp] = (ltc_disable_mask >> (fbp * self.ltc_per_fbp)) & self.fbp_ltc_mask
            self._slice_disable_masks[fbp] = _GetFuseMask('OPT_L2SLICE_FBP{:d}_DISABLE'.format(fbp))

        self._gpc_disable_mask = int(fuseinfo['OPT_GPC_DISABLE'],16)

        cpc_disable_mask = int(fuseinfo['OPT_CPC_DISABLE'],16)
        for gpc in range(self.MAX_GPC):
            self._cpc_disable_masks[gpc] = (cpc_disable_mask >> (gpc * self.cpc_per_gpc)) & self.gpc_cpc_mask
            self._tpc_disable_masks[gpc] = int(fuseinfo['OPT_TPC_GPC{:d}_DISABLE'.format(gpc)], 16)

        self._ioctrl_disable_mask = int(fuseinfo['OPT_LWLINK_DISABLE'],16)
        self._lwlink_disable_mask = int(fuseinfo['OPT_PERLINK_DISABLE'],16)

    def ParseInitMasks(self, kwargs):
        self._fbp_disable_mask    = self._GetMask(kwargs, self._fbp_disable_mask, "fbp", self.fbp_mask)
        self._fbio_disable_mask   = self._GetMask(kwargs, self._fbio_disable_mask, "fbio", self.fbio_mask)
        self._ltc_disable_masks = self._GetMasks(kwargs, self._ltc_disable_masks, "ltc", self.fbp_ltc_mask)
        self._slice_disable_masks = self._GetMasks(kwargs, self._slice_disable_masks, "slice", self.fbp_slice_mask)
        self._gpc_disable_mask    = self._GetMask(kwargs, self._gpc_disable_mask, "gpc", self.gpc_mask)
        self._tpc_disable_masks   = self._GetMasks(kwargs, self._tpc_disable_masks, "tpc", self.gpc_tpc_mask)
        self._cpc_disable_masks   = self._GetMasks(kwargs, self._cpc_disable_masks, "cpc", self.gpc_cpc_mask)

        self._ioctrl_disable_mask = self._GetMask(kwargs, self._ioctrl_disable_mask, "ioctrl", self.ioctrl_mask)
        self._lwlink_disable_mask = self._GetMask(kwargs, self._lwlink_disable_mask, "lwlink", self.lwlink_mask)

    def WriteFbFb(self):
        fs = ["fbp_enable:{:#x}:fbio_enable:{:#x}".format(self.fbp_enable_mask, self.fbio_enable_mask)]
        for fbp_idx,ltc_mask in enumerate(self.ltc_enable_masks):
            if ltc_mask and ltc_mask != self.fbp_ltc_mask:
                fs.append("fbp_ltc_enable:{:d}:{:#x}".format(fbp_idx, ltc_mask))
        for fbp_idx,slice_mask in enumerate(self.slice_enable_masks):
            if slice_mask and slice_mask != self.fbp_slice_mask:
                fs.append("fbp_l2slice_enable:{:d}:{:#x}".format(fbp_idx, slice_mask))
        fb_str = ':'.join(fs)

        with open("fb_fb.arg", "w+") as fb_fb:
            fb_fb.write("-floorsweep {}".format(fb_str))

    def FsStr(self):
        fs = ["fbp_enable:{:#x}:fbio_enable:{:#x}".format(self.fbp_enable_mask, self.fbio_enable_mask)]
        for fbp_idx,ltc_mask in enumerate(self.ltc_enable_masks):
            if ltc_mask and ltc_mask != self.fbp_ltc_mask:
                fs.append("fbp_ltc_enable:{:d}:{:#x}".format(fbp_idx, ltc_mask))
        for fbp_idx,slice_mask in enumerate(self.slice_enable_masks):
            if slice_mask and slice_mask != self.fbp_slice_mask:
                fs.append("fbp_l2slice_enable:{:d}:{:#x}".format(fbp_idx, slice_mask))

        fs.append("gpc_enable:{:#x}".format(self.gpc_enable_mask))
        for gpc_idx,cpc_mask in enumerate(self.cpc_enable_masks):
            if cpc_mask and cpc_mask != self.gpc_cpc_mask:
                fs.append("gpc_cpc_enable:{:d}:{:#x}".format(gpc_idx, cpc_mask))
        for gpc_idx,tpc_mask in enumerate(self.tpc_enable_masks):
            if tpc_mask and tpc_mask != self.gpc_tpc_mask:
                fs.append("gpc_tpc_enable:{:d}:{:#x}".format(gpc_idx, tpc_mask))

        if self.syspipe_disable_mask:
            fs.append(":syspipe_enable:{:#x}".format(self.syspipe_enable_mask))
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
            fs.append(fs_part)

        if self.ioctrl_enable_mask != self.ioctrl_mask:
            fs.append("lwlink_enable:{:#x}".format(self.ioctrl_enable_mask))
        if self.lwlink_enable_mask != self.lwlink_mask:
            fs.append("lwlink_perlink_enable:{:#x}".format(self.lwlink_enable_mask))

        return ':'.join(fs)

    def CreateFsLibStruct(self):
        struct = FsLib.FsLibStruct(self.CHIP)

        struct.SetFBPMask(self.fbp_disable_mask)
        for fbp in range(self.MAX_FBP):
            struct.SetFBIOPerFBPMask(    fbp, ((self.fbio_disable_mask >> (fbp * self.fbio_per_fbp)) & self.fbp_fbio_mask))
            struct.SetFBPAPerFBPMask(    fbp, ((self.fbp_disable_mask >> fbp) & 0x1))
            struct.SetLTCPerFBPMask(     fbp, self.ltc_disable_masks[fbp])
            struct.SetL2SlicePerFBPMask( fbp, self.slice_disable_masks[fbp])

        struct.SetGPCMask(self.gpc_disable_mask)
        for gpc in range(self.MAX_GPC):
            struct.SetCpcPerGpcMask(gpc, self.cpc_disable_masks[gpc])
            struct.SetTpcPerGpcMask(gpc, self.tpc_disable_masks[gpc])

        return struct

    def PrintChipInfo(self):
        Datalogger.PrintLog("... MAX_FBP_MASK         {:b}".format(self.fbp_mask))
        Datalogger.PrintLog("... MAX_FBIO_MASK        {:b}".format(self.fbio_mask))
        Datalogger.PrintLog("... MAX_FBP_LTC_MASK     {:b}".format(self.fbp_ltc_mask))
        Datalogger.PrintLog("... MAX_FBP_L2SLICE_MASK {:b}".format(self.fbp_slice_mask))
        Datalogger.PrintLog("... MAX_GPC_MASK         {:b}".format(self.gpc_mask))
        Datalogger.PrintLog("... MAX_GPC_CPC_MASK     {:b}".format(self.gpc_cpc_mask))
        Datalogger.PrintLog("... MAX_GPC_TPC_MASK     {:b}".format(self.gpc_tpc_mask))
        Datalogger.PrintLog("... MAX_SYSPIPE_MASK     {:b}".format(self.syspipe_mask))
        Datalogger.PrintLog("... MAX_IOCTRL_MASK      {:b}".format(self.ioctrl_mask))
        Datalogger.PrintLog("... MAX_PERLINK_MASK     {:b}".format(self.lwlink_mask))

    def PrintFuseInfo(self):
        Datalogger.PrintLog("... OPT_FBP_DISABLE       {:#x}".format(self.fbp_disable_mask))
        Datalogger.PrintLog("... OPT_FBIO_DISABLE      {:#x}".format(self.fbio_disable_mask))
        Datalogger.PrintLog("... OPT_LTC_DISABLE       {}".format(["{:#x}".format(mask) for mask in self.ltc_disable_masks]))
        Datalogger.PrintLog("... OPT_L2SLICE_DISABLE   {}".format(["{:#x}".format(mask) for mask in self.slice_disable_masks]))
        Datalogger.PrintLog("... OPT_GPC_DISABLE       {:#x}".format(self.gpc_disable_mask))
        Datalogger.PrintLog("... OPT_CPC_DISABLE       {}".format(["{:#x}".format(mask) for mask in self.cpc_disable_masks]))
        Datalogger.PrintLog("... OPT_TPC_DISABLE       {}".format(["{:#x}".format(mask) for mask in self.tpc_disable_masks]))
        Datalogger.PrintLog("... OPT_SYS_PIPE_DISABLE  {:#x}".format(self.syspipe_disable_mask))
        Datalogger.PrintLog("... OPT_LWLINK_DISABLE    {:#x}".format(self.ioctrl_disable_mask))
        Datalogger.PrintLog("... OPT_PERLINK_DISABLE   {:#x}".format(self.lwlink_disable_mask))

    def PrintDefectiveFuseInfo(self):
        Datalogger.PrintLog("... OPT_FBP_DEFECTIVE       {:#x}".format(self.fbp_disable_mask))
        Datalogger.PrintLog("... OPT_FBIO_DEFECTIVE      {:#x}".format(self.fbio_disable_mask))
        Datalogger.PrintLog("... OPT_LTC_DEFECTIVE       {}".format(["{:#x}".format(mask) for mask in self.ltc_disable_masks]))
        Datalogger.PrintLog("... OPT_L2SLICE_DEFECTIVE   {}".format(["{:#x}".format(mask) for mask in self.slice_disable_masks]))
        Datalogger.PrintLog("... OPT_GPC_DEFECTIVE       {:#x}".format(self.gpc_disable_mask))
        Datalogger.PrintLog("... OPT_CPC_DEFECTIVE       {}".format(["{:#x}".format(mask) for mask in self.cpc_disable_masks]))
        Datalogger.PrintLog("... OPT_TPC_DEFECTIVE       {}".format(["{:#x}".format(mask) for mask in self.tpc_disable_masks]))
        Datalogger.PrintLog("... OPT_LWLINK_DEFECTIVE    {:#x}".format(self.ioctrl_disable_mask))
        Datalogger.PrintLog("... OPT_PERLINK_DEFECTIVE   {:#x}".format(self.lwlink_disable_mask))

    def PrintDisableInfo(self):
        self._PrintDisableMask("FBP", self.fbp_disable_mask)
        self._PrintDisableMask("FBIO", self.fbio_disable_mask)
        self._PrintDisableMasks("LTC", self.ltc_disable_masks)
        self._PrintDisableMasks("L2SLICE", self.slice_disable_masks)
        self._PrintDisableMask("GPC", self.gpc_disable_mask)
        self._PrintDisableMasks("CPC", self.cpc_disable_masks)
        self._PrintDisableMasks("TPC", self.tpc_disable_masks)
        self._PrintDisableMask("IOCTRL", self.ioctrl_disable_mask)
        self._PrintDisableMask("LWLINK", self.lwlink_disable_mask)

    def LogFuseInfo(self):
        fbp_disable      = '{:#x}'.format(self.fbp_disable_mask)
        fbio_disable     = '{:#x}'.format(self.fbio_disable_mask)
        ltc_disable      =    '{}'.format(["{:#x}".format(mask) for mask in self.ltc_disable_masks])
        slice_disable    =    '{}'.format(["{:#x}".format(mask) for mask in self.slice_disable_masks])
        gpc_disable      = '{:#x}'.format(self.gpc_disable_mask)
        cpc_disable      =    '{}'.format(["{:#x}".format(mask) for mask in self.cpc_disable_masks])
        tpc_disable      =    '{}'.format(["{:#x}".format(mask) for mask in self.tpc_disable_masks])
        syspipe_disable  = '{:#x}'.format(self.syspipe_disable_mask)
        ioctrl_disable   = '{:#x}'.format(self.ioctrl_disable_mask)
        lwlink_disable   = '{:#x}'.format(self.lwlink_disable_mask)
        ce_disable       = '{:#x}'.format(self.ce_disable_mask)
        lwdec_disable    = '{:#x}'.format(self.lwdec_disable_mask)
        ofa_disable      = '{:#x}'.format(self.ofa_disable_mask)
        lwjpg_disable    = '{:#x}'.format(self.lwjpg_disable_mask)

        Datalogger.LogInfo(OPT_FBP_DISABLE       = fbp_disable)
        Datalogger.LogInfo(OPT_FBIO_DISABLE      = fbio_disable)
        Datalogger.LogInfo(OPT_LTC_DISABLE       = ltc_disable)
        Datalogger.LogInfo(OPT_L2SLICE_DISABLE   = slice_disable)
        Datalogger.LogInfo(OPT_GPC_DISABLE       = gpc_disable)
        Datalogger.LogInfo(OPT_CPC_DISABLE       = cpc_disable)
        Datalogger.LogInfo(OPT_TPC_DISABLE       = tpc_disable)
        Datalogger.LogInfo(OPT_SYS_PIPE_DISABLE  = syspipe_disable)
        Datalogger.LogInfo(OPT_LWLINK_DISABLE    = ioctrl_disable)
        Datalogger.LogInfo(OPT_PERLINK_DISABLE   = lwlink_disable)
        Datalogger.LogInfo(OPT_CE_DISABLE        = ce_disable)
        Datalogger.LogInfo(OPT_LWDEC_DISABLE     = lwdec_disable)
        Datalogger.LogInfo(OPT_OFA_DISABLE       = ofa_disable)
        Datalogger.LogInfo(OPT_LWJPG_DISABLE     = lwjpg_disable)

    def LogDefectiveFuseInfo(self):
        fbp_disable      = '{:#x}'.format(self.fbp_disable_mask)
        fbio_disable     = '{:#x}'.format(self.fbio_disable_mask)
        ltc_disable      =    '{}'.format(["{:#x}".format(mask) for mask in self.ltc_disable_masks])
        slice_disable    =    '{}'.format(["{:#x}".format(mask) for mask in self.slice_disable_masks])
        gpc_disable      = '{:#x}'.format(self.gpc_disable_mask)
        cpc_disable      =    '{}'.format(["{:#x}".format(mask) for mask in self.cpc_disable_masks])
        tpc_disable      =    '{}'.format(["{:#x}".format(mask) for mask in self.tpc_disable_masks])
        ioctrl_disable   = '{:#x}'.format(self.ioctrl_disable_mask)
        lwlink_disable   = '{:#x}'.format(self.lwlink_disable_mask)

        Datalogger.LogInfo(OPT_FBP_DEFECTIVE     = fbp_disable)
        Datalogger.LogInfo(OPT_FBIO_DEFECTIVE    = fbio_disable)
        Datalogger.LogInfo(OPT_LTC_DEFECTIVE     = ltc_disable)
        Datalogger.LogInfo(OPT_L2SLICE_DEFECTIVE = slice_disable)
        Datalogger.LogInfo(OPT_GPC_DEFECTIVE     = gpc_disable)
        Datalogger.LogInfo(OPT_CPC_DEFECTIVE     = cpc_disable)
        Datalogger.LogInfo(OPT_TPC_DEFECTIVE     = tpc_disable)
        Datalogger.LogInfo(OPT_LWLINK_DEFECTIVE  = ioctrl_disable)
        Datalogger.LogInfo(OPT_PERLINK_DEFECTIVE = lwlink_disable)

    def CheckSanity(self):
        if (self.cpc_disable_masks[0] & 0x1):
            raise Exceptions.FsSanityFailed("Sanity Failed. No functional TPCs in GPC0/CPC0")
        return super(GH100FsInfo,self).CheckSanity()

    @property
    def override_cpc2(self):
        return self._override_cpc2
    def SetOverrideCPC2(self):
        return self.__class__(clone = self, override_cpc2 = True)

    def CloneOverrides(self):
        clone_ = super(GH100FsInfo,self).CloneOverrides()
        return self.__class__(clone = clone_,
                              override_cpc2 = self.override_cpc2)

    # Bitwise operators
    def IlwertTpc(self):
        return self.__class__(tpc_disable_masks = self.tpc_enable_masks)
    def IlwertFB(self):
        return self.__class__(slice_disable_masks = [(mask ^ self.fbp_slice_mask) & self.fbp_slice_mask for mask in self.slice_disable_masks])
    def IlwertLwLink(self):
        return self.__class__(lwlink_disable_mask = self.lwlink_enable_mask)
    def __ilwert__(self):
        # Only ilwert the smallest components, eg. tpc and ltc, because an aggregate might
        # still be enabled when its component mask is ilwerted. The results will trickle up
        # to the aggregates when the masks are made consistent.
        return self.CloneOverrides() | self.IlwertTpc() | self.IlwertFB() | self.IlwertLwLink()

    def __or__(self, other):
        Exceptions.ASSERT(self.__class__ == other.__class__, "Cannot or {} with {}".format(self.__class__, other.__class__))
        return self.__class__(clone = self.CloneOverrides(),
                              gpc_disable_mask      = (self.gpc_disable_mask   | other.gpc_disable_mask),
                              cpc_disable_masks     = [(x|y) for x,y in zip(self.cpc_disable_masks,other.cpc_disable_masks)],
                              tpc_disable_masks     = [(x|y) for x,y in zip(self.tpc_disable_masks, other.tpc_disable_masks)],
                              fbp_disable_mask      = (self.fbp_disable_mask   | other.fbp_disable_mask),
                              fbio_disable_mask     = (self.fbio_disable_mask   | other.fbio_disable_mask),
                              ltc_disable_masks     = [(x|y) for x,y in zip(self.ltc_disable_masks,other.ltc_disable_masks)],
                              slice_disable_masks   = [(x|y) for x,y in zip(self.slice_disable_masks,other.slice_disable_masks)],
                              ioctrl_disable_mask   = (self.ioctrl_disable_mask | other.ioctrl_disable_mask),
                              lwlink_disable_mask   = (self.lwlink_disable_mask | other.lwlink_disable_mask))

    def __xor__(self, other):
        Exceptions.ASSERT(self.__class__ == other.__class__, "Cannot or {} with {}".format(self.__class__, other.__class__))
        return self.__class__(clone = self.CloneOverrides(),
                              gpc_disable_mask      = (self.gpc_disable_mask   ^ other.gpc_disable_mask),
                              cpc_disable_masks     = [(x^y) for x,y in zip(self.cpc_disable_masks,other.cpc_disable_masks)],
                              tpc_disable_masks     = [(x^y) for x,y in zip(self.tpc_disable_masks, other.tpc_disable_masks)],
                              fbp_disable_mask      = (self.fbp_disable_mask   ^ other.fbp_disable_mask),
                              fbio_disable_mask     = (self.fbio_disable_mask   ^ other.fbio_disable_mask),
                              ltc_disable_masks     = [(x^y) for x,y in zip(self.ltc_disable_masks,other.ltc_disable_masks)],
                              slice_disable_masks   = [(x^y) for x,y in zip(self.slice_disable_masks,other.slice_disable_masks)],
                              ioctrl_disable_mask   = (self.ioctrl_disable_mask ^ other.ioctrl_disable_mask),
                              lwlink_disable_mask   = (self.lwlink_disable_mask ^ other.lwlink_disable_mask))

    def __and__(self, other):
        Exceptions.ASSERT(self.__class__ == other.__class__, "Cannot and {} with {}".format(self.__class__, other.__class__))
        return self.__class__(clone = self.CloneOverrides(),
                              gpc_disable_mask      = (self.gpc_disable_mask   & other.gpc_disable_mask),
                              cpc_disable_masks     = [(x&y) for x,y in zip(self.cpc_disable_masks,other.cpc_disable_masks)],
                              tpc_disable_masks     = [(x&y) for x,y in zip(self.tpc_disable_masks, other.tpc_disable_masks)],
                              fbp_disable_mask      = (self.fbp_disable_mask   & other.fbp_disable_mask),
                              fbio_disable_mask     = (self.fbio_disable_mask   & other.fbio_disable_mask),
                              ltc_disable_masks     = [(x&y) for x,y in zip(self.ltc_disable_masks,other.ltc_disable_masks)],
                              slice_disable_masks   = [(x&y) for x,y in zip(self.slice_disable_masks,other.slice_disable_masks)],
                              ioctrl_disable_mask   = (self.ioctrl_disable_mask & other.ioctrl_disable_mask),
                              lwlink_disable_mask   = (self.lwlink_disable_mask & other.lwlink_disable_mask))

    def __eq__(self, other):
        if self.__class__ != other.__class__:
            return False
        return (self.gpc_disable_mask == other.gpc_disable_mask and
                self.cpc_disable_masks == other.cpc_disable_masks and
                self.tpc_disable_masks == other.tpc_disable_masks and
                self.fbp_disable_mask == other.fbp_disable_mask and
                self.fbio_disable_mask == other.fbio_disable_mask and
                self.ltc_disable_masks == other.ltc_disable_masks and
                self.slice_disable_masks == other.slice_disable_masks and
                self.ioctrl_disable_mask == other.ioctrl_disable_mask and
                self.lwlink_disable_mask == other.lwlink_disable_mask)

    def CloneTpc(self):
        return self.__class__(gpc_disable_mask  = self.gpc_disable_mask,
                              cpc_disable_masks = self.cpc_disable_masks,
                              tpc_disable_masks = self.tpc_disable_masks)

    def ClearTpc(self):
        return self.__class__(clone = self, gpc_disable_mask  = 0x0,
                                            cpc_disable_masks = [0x0 for x in range(self.MAX_GPC)],
                                            tpc_disable_masks = [0x0 for x in range(self.MAX_GPC)])

    def CloneFB(self):
        return self.__class__(fbp_disable_mask      = self.fbp_disable_mask,
                              fbio_disable_mask     = self.fbio_disable_mask,
                              ltc_disable_masks     = self.ltc_disable_masks,
                              slice_disable_masks   = self.slice_disable_masks)

    def ClearFB(self):
        return self.__class__(clone = self,
                              fbp_disable_mask      = 0x0,
                              fbio_disable_mask     = 0x0,
                              ltc_disable_masks     = [0x0 for x in range(self.MAX_FBP)],
                              slice_disable_masks   = [0x0 for x in range(self.MAX_FBP)])

    def CloneLwLink(self):
        return self.__class__(ioctrl_disable_mask = self.ioctrl_disable_mask,
                              lwlink_disable_mask = self.lwlink_disable_mask)

    def ClearLwLink(self):
        return self.__class__(clone = self, ioctrl_disable_mask = 0x0,
                                            lwlink_disable_mask = 0x0)

    def MakeGpcConsistent(self):
        super(GH100FsInfo,self).MakeGpcConsistent()
        for gpc in range(self.MAX_GPC):
            gpc_disable = (1 << gpc)
            for cpc in range(self.cpc_per_gpc):
                cpc_disable = (1 << cpc)
                if (cpc_disable & self.gpc_cpc_disable_mask(gpc)):
                    self._tpc_disable_masks[gpc] |= self.gpc_cpc_tpc_mask(cpc)
                if ((self.gpc_cpc_tpc_disable_mask(gpc,cpc) & self.cpc_tpc_mask) == self.cpc_tpc_mask):
                    self._cpc_disable_masks[gpc] |= cpc_disable
            if ((self.gpc_disable_mask & gpc_disable) or
                    ((self.gpc_cpc_disable_mask(gpc) & self.gpc_cpc_mask) == self.gpc_cpc_mask) or
                    (self.override_cpc2 and ((self.gpc_cpc_disable_mask(gpc) & self.gpc_cpc_mask) == 0x3)) or # CPC2 cannot be enabled alone
                    ((self.gpc_tpc_disable_mask(gpc) & self.gpc_tpc_mask) == self.gpc_tpc_mask)):
                self._gpc_disable_mask |= gpc_disable
                self._cpc_disable_masks[gpc] = self.gpc_cpc_mask
                self._tpc_disable_masks[gpc] = self.gpc_tpc_mask

    def MakeFBConsistent(self):
        for fbp in range(self.MAX_FBP):
            fbp_disable = (1 << fbp)
            fbio_disable = (self.fbp_fbio_mask << (fbp * self.fbio_per_fbp))
            fbp_l2noc_idx = self.fbp_l2noc_idx(fbp)
            for ltc in range(self.ltc_per_fbp):
                ltc_disable = (1 << ltc)
                ltc_l2noc = 1 if ltc is 0 else 0
                ltc_l2noc_disable = (1 << ltc_l2noc)
                slice_disable_count = bin((self.slice_disable_masks[fbp] >> (ltc * self.slice_per_ltc)) & self.ltc_slice_mask).count('1')
                if ((self.fbp_ltc_disable_mask(fbp) & ltc_disable) or
                         slice_disable_count > 1 or # Each LTC can have at most 1 slice disabled
                         (self.only_ltc and slice_disable_count > 0)):
                    self._ltc_disable_masks[fbp] |= ltc_disable
                    self._slice_disable_masks[fbp] |= (self.ltc_slice_mask << (ltc * self.slice_per_ltc))
                    if not self.override_l2noc:
                        self._ltc_disable_masks[fbp_l2noc_idx] |= ltc_l2noc_disable
                        self._slice_disable_masks[fbp_l2noc_idx] |= (self.ltc_slice_mask << (ltc_l2noc * self.slice_per_ltc))
                if not self.override_l2noc:
                    for slice in range(self.slice_per_ltc):
                        slice_disable = (1 << slice)
                        if (slice_disable & self.fbp_ltc_slice_disable_mask(fbp,ltc)):
                            self._slice_disable_masks[fbp_l2noc_idx] |= (slice_disable << (ltc_l2noc * self.slice_per_ltc))
            if ((fbp_disable & self.fbp_disable_mask) or
                (fbio_disable & self.fbio_disable_mask) or
                (self.fbp_ltc_disable_mask(fbp) == self.fbp_ltc_mask) or
                (self.fbp_slice_disable_mask(fbp) == self.fbp_slice_mask) or
                (self.only_fbp and (self.fbp_ltc_disable_mask(fbp) != 0 or
                                    self.fbp_slice_disable_mask(fbp) != 0))):
                self._fbp_disable_mask |= fbp_disable
                self._ltc_disable_masks[fbp] = self.fbp_ltc_mask
                self._slice_disable_masks[fbp] = self.fbp_slice_mask
                self._fbio_disable_mask |= fbio_disable
                if not self.override_l2noc:
                    l2noc_disable = (1 << fbp_l2noc_idx)
                    l2noc_fbio_disable = (self.fbp_fbio_mask << (fbp_l2noc_idx * self.fbio_per_fbp))
                    self._fbp_disable_mask |= l2noc_disable
                    self._fbio_disable_mask |= l2noc_fbio_disable
                    self._ltc_disable_masks[fbp_l2noc_idx] = self.fbp_ltc_mask
                    self._slice_disable_masks[fbp_l2noc_idx] = self.fbp_slice_mask

        # If you have imperfect LTC Slice masks, then there can only be 2 (or 4) slices disabled
        # across the whole chip, 1 per uGPU (or 2 per uGPU in separate FBPs)
        # If there is exactly one LTC with an imperfect slice mask, then no other LTC can have any slices disabled at all.
        # Otherwise, every LTC with at least one slice disabled must be fully disabled.
        for ugpu in range(self.MAX_UGPU):
            one_slice_disable_count = 0
            slice_disable_count = 0
            for fbp in [fbp for fbp,i in enumerate(bin(self.ugpu_fbp_mask(ugpu))[:1:-1]) if int(i)]:
                for ltc in range(self.ltc_per_fbp):
                    slice_count = bin(self.fbp_ltc_slice_disable_mask(fbp,ltc)).count('1')
                    if slice_count == 1:
                        one_slice_disable_count += 1
                    slice_disable_count += slice_count
            for fbp in [fbp for fbp,i in enumerate(bin(self.ugpu_fbp_mask(ugpu))[:1:-1]) if int(i)]:
                for ltc in range(self.ltc_per_fbp):
                    slice_count = bin(self.fbp_ltc_slice_disable_mask(fbp,ltc)).count('1')
                    if ((one_slice_disable_count > 1 and slice_count > 0) or
                        (slice_disable_count > 1 and slice_count > 0)):
                        self._ltc_disable_masks[fbp] |= (1 << ltc)
                        self._slice_disable_masks[fbp] |= (self.ltc_slice_mask << (ltc * self.slice_per_ltc))
                fbp_disable = (1 << fbp)
                fbio_disable = (self.fbp_fbio_mask << (fbp * self.fbio_per_fbp))
                slice_count = bin(self.slice_disable_masks[fbp]).count('1')
                if (self.ltc_disable_masks[fbp] == self.fbp_ltc_mask):
                    self._fbp_disable_mask |= fbp_disable
                    self._fbio_disable_mask |= fbio_disable
                    self._ltc_disable_masks[fbp] = self.fbp_ltc_mask
                    self._slice_disable_masks[fbp] = self.fbp_slice_mask

    def MakeLwLinkConsistent(self):
        for ioctrl in range(self.MAX_IOCTRL):
            ioctrl_disable = (1 << ioctrl)
            if ((ioctrl_disable & self.ioctrl_disable_mask) or
                (self.ioctrl_lwlink_disable_mask(ioctrl) == self.ioctrl_lwlink_mask)):
                self._ioctrl_disable_mask |= ioctrl_disable
                self._lwlink_disable_mask |= (self.ioctrl_lwlink_mask << (ioctrl * self.lwlink_per_ioctrl))

    def IterateGpc(self):
        for gpc in range(self.MAX_GPC):
            gpc_mask = (1 << gpc)
            yield self.__class__(clone = self.ClearTpc(), gpc_enable_mask = gpc_mask,
                                                          cpc_enable_masks = self.cpc_enable_masks,
                                                          tpc_enable_masks = self.tpc_enable_masks)

    def IterateColdGpc(self):
        for gpc in range(self.MAX_GPC):
            gpc_mask = (1 << gpc)
            if gpc_mask & self.gpc_disable_mask:
                continue
            gpc_disable_mask = self.gpc_disable_mask
            gpc_disable_mask |= gpc_mask
            yield self.__class__(clone = self.ClearTpc(), gpc_disable_mask = gpc_disable_mask,
                                                          tpc_disable_masks = self.tpc_disable_masks)

    def IterateCpc(self):
        for gpc in range(self.MAX_GPC):
            gpc_mask = (1 << gpc)
            if gpc_mask & self.gpc_disable_mask:
                continue
            for cpc in range(self.cpc_per_gpc):
                cpc_mask = (1 << cpc)
                if cpc_mask & self.cpc_disable_masks[gpc]:
                    continue
                cpc_enable_masks = [0x0 for x in range(self.MAX_GPC)]
                cpc_enable_masks[gpc] = cpc_mask
                yield self.__class__(clone = self.ClearTpc(), cpc_enable_masks = cpc_enable_masks,
                                                              tpc_enable_masks = self.tpc_enable_masks)

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
        raise Exceptions.UnsupportedFunction("ROPL2 not supported on this chip")

    def IterateColdFbp(self):
        def YieldColdHbm(mask):
            if mask & self.fbp_disable_mask:
                return
            if (mask | self.fbp_disable_mask) == self.fbp_mask:
                return
            yield self.__class__(clone = self.ClearFB(), fbp_disable_mask = mask)

        yield from YieldColdHbm(0x12)  # HBM B - F1 F4
        yield from YieldColdHbm(0x840) # HBM E - F6 F11
        yield from YieldColdHbm(0x9)   # HBM A, HBM C - F0 F3
        yield from YieldColdHbm(0x24)  # HBM A, HBM C - F2 F5
        yield from YieldColdHbm(0x180) # HBM D, HBM F - F7 F8
        yield from YieldColdHbm(0x600) # HBM D, HBM F - F9 F10

    def IterateFbp(self):
        def YieldHbm(mask):
            if mask & self.fbp_disable_mask:
                return
            yield self.__class__(clone = self.ClearFB(), fbp_enable_mask = mask)

        yield from YieldHbm(0x12)  # HBM B - F1 F4
        yield from YieldHbm(0x840) # HBM E - F6 F11
        yield from YieldHbm(0x9)   # HBM A, HBM C - F0 F3
        yield from YieldHbm(0x24)  # HBM A, HBM C - F2 F5
        yield from YieldHbm(0x180) # HBM D, HBM F - F7 F8
        yield from YieldHbm(0x600) # HBM D, HBM F - F9 F10

    def IterateLtc(self):
        for fbp in [fbp for fbp,i in enumerate(bin(self.ugpu_fbp_mask(0))[:1:-1]) if int(i)]:
            fbp_l2noc_idx = self.fbp_l2noc_idx(fbp)
            for ltc in range(self.ltc_per_fbp):
                ltc_mask = (1 << ltc)
                ltc_l2noc_mask = (ltc_mask ^ self.fbp_ltc_mask) & self.fbp_ltc_mask
                if (ltc_mask & self.fbp_ltc_disable_mask(fbp) or
                    ltc_l2noc_mask & self.fbp_ltc_disable_mask(fbp_l2noc_idx)):
                    continue
                ltc_enable_masks = [0x0 for x in range(self.MAX_FBP)]
                ltc_enable_masks[fbp] = ltc_mask
                ltc_enable_masks[fbp_l2noc_idx] = ltc_l2noc_mask
                yield self.__class__(clone = self.ClearFB(), ltc_enable_masks = ltc_enable_masks)

    def IterateColdSlice(self):
        for fbp in [fbp for fbp,i in enumerate(bin(self.ugpu_fbp_mask(0))[:1:-1]) if int(i)]:
            fbp_l2noc_idx = self.fbp_l2noc_idx(fbp)
            for ltc in range(self.ltc_per_fbp):
                ltc_l2noc = 1 if ltc is 0 else 0
                if bin(self.fbp_ltc_slice_disable_mask(fbp, ltc)).count('1') > 0:
                    # Can't disable more than 1 slice in a particular LTC
                    continue
                for slice in range(self.slice_per_ltc):
                    slice_disable = ((1 << slice) << (ltc * self.slice_per_ltc))
                    slice_l2noc_disable = ((1 << slice) << (ltc_l2noc * self.slice_per_ltc))
                    if slice_disable & self.fbp_slice_disable_mask(fbp):
                        continue
                    slice_disable_masks = [0x0 for x in range(self.MAX_FBP)]
                    slice_disable_masks[fbp] |= slice_disable
                    slice_disable_masks[fbp_l2noc_idx] |= slice_l2noc_disable
                    yield self.__class__(clone = self.ClearFB(), slice_disable_masks = slice_disable_masks)

    def ExpandColdSlice(self):
        '''
        In order to add this FsInfo to the GpuInfo's list, we need to re-enabled the fully disabled FBPs
        so that the L2 slice in question is the only thing disabled, so it becomes the only new thing disabled in the GpuInfo's total
        '''
        fbp_enable_mask = self.fbp_enable_mask
        ltc_disable_masks = self.ltc_disable_masks
        slice_disable_masks = self.slice_disable_masks
        for fbp in range(self.MAX_FBP):
            fbp_mask = (1 << fbp)
            fbp_enable_mask |= fbp_mask
            ltc_disable_masks[fbp] = 0x0
            if (fbp_mask & self.fbp_disable_mask):
                slice_disable_masks[fbp] = 0x0
            else:
                for ltc in range(self.ltc_per_fbp):
                    ltc_mask = (1 << ltc)
                    if (ltc_mask & self.fbp_ltc_enable_mask(fbp)):
                        slice_disable_masks[fbp] = (self.fbp_ltc_slice_disable_mask(fbp,ltc) << (ltc * self.slice_per_ltc))
        return self.__class__(clone = self.ClearFB(), fbp_enable_mask = fbp_enable_mask,
                                                      ltc_disable_masks = ltc_disable_masks,
                                                      slice_disable_masks = slice_disable_masks)

    def IsSinglePes(self):
        raise Exceptions.UnsupportedFunction("PES floorsweeping not supported on this chip")

    def IsSingleCpc(self):
        return (1 == sum([int(i) for mask in self.cpc_enable_masks for i in '{:b}'.format(mask)]))

    def SplitTpc(self):
        if self.IsSingleCpc():
            gpc_idx = int(math.log(self.gpc_enable_mask,2))
            cpc_idx = int(math.log(self.gpc_cpc_enable_mask(gpc_idx),2))
            left_tpc_mask,right_tpc_mask = self._SplitMask(self.gpc_cpc_tpc_enable_mask(gpc_idx,cpc_idx))

            left_tpc_masks = [0x0 for x in range(self.MAX_GPC)]
            right_tpc_masks = [0x0 for x in range(self.MAX_GPC)]
            left_tpc_masks[gpc_idx] = left_tpc_mask
            right_tpc_masks[gpc_idx] = right_tpc_mask
            for i in range(0,cpc_idx):
                left_tpc_masks[gpc_idx] <<= self.tpc_per_cpc
                right_tpc_masks[gpc_idx] <<= self.tpc_per_cpc

            left_tpc = self.__class__(clone = self.ClearTpc(), tpc_enable_masks = left_tpc_masks)
            right_tpc = self.__class__(clone = self.ClearTpc(), tpc_enable_masks = right_tpc_masks)
            return left_tpc,right_tpc
        elif self.IsSingleGpc():
            gpc_idx = int(math.log(self.gpc_enable_mask,2))
            left_cpc_mask,right_cpc_mask = self._SplitMask(self.gpc_cpc_enable_mask(gpc_idx))

            left_cpc_masks = [0x0 for x in range(self.MAX_GPC)]
            right_cpc_masks = [0x0 for x in range(self.MAX_GPC)]
            left_cpc_masks[gpc_idx] = left_cpc_mask
            right_cpc_masks[gpc_idx] = right_cpc_mask

            left_cpc  = self.__class__(clone = self.ClearTpc(), cpc_enable_masks = left_cpc_masks,
                                                                tpc_enable_masks = self.tpc_enable_masks)
            right_cpc = self.__class__(clone = self.ClearTpc(), cpc_enable_masks = right_cpc_masks,
                                                                tpc_enable_masks = self.tpc_enable_masks)
            return left_cpc,right_cpc
        else:
            left_gpc_mask,right_gpc_mask = self._SplitMask(self.gpc_enable_mask)
            left_gpc  = self.__class__(clone = self.ClearTpc(), gpc_enable_mask  = left_gpc_mask,
                                                                cpc_enable_masks = self.cpc_enable_masks,
                                                                tpc_enable_masks = self.tpc_enable_masks)
            right_gpc = self.__class__(clone = self.ClearTpc(), gpc_enable_mask  = right_gpc_mask,
                                                                cpc_enable_masks = self.cpc_enable_masks,
                                                                tpc_enable_masks = self.tpc_enable_masks)
            return left_gpc,right_gpc

    def IterateLwLink(self):
        for lwlink in range(self.MAX_LWLINK):
            lwlink_enable = (1 << lwlink)
            if (lwlink_enable & self.lwlink_disable_mask):
                continue
            yield self.__class__(clone = self.ClearLwLink(), lwlink_enable_mask = lwlink_enable)

    def IsSingleIoctrl(self):
        return (1 == sum([int(i) for i in '{:b}'.format(self.ioctrl_enable_mask)]))

    def IsSingleLwLink(self):
        return (1 == sum([int(i) for i in '{:b}'.format(self.lwlink_enable_mask)]))

    def SplitLwLink(self):
        if self.IsSingleIoctrl():
            ioctrl_idx = int(math.log(self.ioctrl_enable_mask,2))
            left_lwlink_mask,right_lwlink_mask = self._SplitMask(self.lwlink_enable_mask)

            left_lwlink = self.__class__(clone = self.ClearLwLink(), lwlink_enable_mask = left_lwlink_mask)
            right_lwlink = self.__class__(clone = self.ClearLwLink(), lwlink_enable_mask = right_lwlink_mask)
            return left_lwlink,right_lwlink
        else:
            left_ioctrl_mask,right_ioctrl_mask = self._SplitMask(self.ioctrl_enable_mask)
            left_ioctrl = self.__class__(clone = self.ClearLwLink(), ioctrl_enable_mask = left_ioctrl_mask,
                                                                     lwlink_enable_mask = self.lwlink_enable_mask)
            right_ioctrl = self.__class__(clone = self.ClearLwLink(), ioctrl_enable_mask = right_ioctrl_mask,
                                                                      lwlink_enable_mask = self.lwlink_enable_mask)
            return left_ioctrl,right_ioctrl

    @property
    def cpc_per_gpc(self):
        return int(self.MAX_CPC / self.MAX_GPC)
    @property
    def gpc_cpc_mask(self):
        return (1 << self.cpc_per_gpc) - 1
    @property
    def cpc_disable_masks(self):
        return self._cpc_disable_masks[:] # return a copy, not a reference
    @property
    def cpc_enable_masks(self):
        return [((mask ^ self.gpc_cpc_mask) & self.gpc_cpc_mask) for mask in self._cpc_disable_masks]
    def gpc_cpc_disable_mask(self, gpc):
        return self._cpc_disable_masks[gpc]
    def gpc_cpc_enable_mask(self, gpc):
        return (self.gpc_cpc_disable_mask(gpc) ^ self.gpc_cpc_mask) & self.gpc_cpc_mask
    def gpc_cpc_tpc_disable_mask(self, gpc, cpc):
        mask = self._tpc_disable_masks[gpc]
        for i in range(0,cpc):
            mask >>= self.tpc_per_cpc
        return mask & self.cpc_tpc_mask
    def gpc_cpc_tpc_enable_mask(self, gpc, cpc):
        return (self.gpc_cpc_tpc_disable_mask(gpc, cpc) ^ self.cpc_tpc_mask) & self.cpc_tpc_mask
    @property
    def tpc_per_cpc(self):
        return int(self.MAX_TPC / self.MAX_CPC)
    @property
    def cpc_tpc_mask(self):
        return (1 << self.tpc_per_cpc) - 1
    def gpc_cpc_tpc_mask(self, cpc):
        return self.cpc_tpc_mask << (cpc * self.tpc_per_cpc)

    @property
    def ropl2_per_fbp(self):
        raise Exceptions.UnsupportedFunction("ROPL2 not supported on this chip")
    @property
    def fbp_ropl2_mask(self):
        raise Exceptions.UnsupportedFunction("ROPL2 not supported on this chip")
    @property
    def ropl2_disable_masks(self):
        raise Exceptions.UnsupportedFunction("ROPL2 not supported on this chip")
    @property
    def ropl2_enable_masks(self):
        raise Exceptions.UnsupportedFunction("ROPL2 not supported on this chip")
    def fbp_ropl2_disable_mask(self, fbp):
        raise Exceptions.UnsupportedFunction("ROPL2 not supported on this chip")
    def fbp_ropl2_enable_mask(self, fbp):
        raise Exceptions.UnsupportedFunction("ROPL2 not supported on this chip")

    @property
    def ltc_per_fbp(self):
        return int(self.MAX_LTC / self.MAX_FBP)
    @property
    def fbp_ltc_mask(self):
        return (1 << self.ltc_per_fbp) - 1
    @property
    def ltc_disable_masks(self):
        return self._ltc_disable_masks[:] # return a copy, not a reference
    @property
    def ltc_enable_masks(self):
        return [((mask ^ self.fbp_ltc_mask) & self.fbp_ltc_mask) for mask in self._ltc_disable_masks]
    def fbp_ltc_disable_mask(self, fbp):
        return self._ltc_disable_masks[fbp]
    def fbp_ltc_enable_mask(self, fbp):
        return (self.fbp_ltc_disable_mask(fbp) ^ self.fbp_ltc_mask) & self.fbp_ltc_mask

    @property
    def slice_per_ltc(self):
        return int(self.MAX_SLICE / self.MAX_LTC)
    @property
    def ltc_slice_mask(self):
        return (1 << self.slice_per_ltc) - 1
    @property
    def slice_per_fbp(self):
        return int(self.MAX_SLICE / self.MAX_FBP)
    @property
    def fbp_slice_mask(self):
        return (1 << self.slice_per_fbp) - 1
    @property
    def slice_disable_masks(self):
        return self._slice_disable_masks[:] # return a copy
    @property
    def slice_enable_masks(self):
        return [((mask ^ self.fbp_slice_mask) & self.fbp_slice_mask) for mask in self._slice_disable_masks]
    def fbp_slice_disable_mask(self, fbp):
        return self._slice_disable_masks[fbp]
    def fbp_slice_enable_mask(self, fbp):
        return (self.fbp_slice_disable_mask(fbp) ^ self.fbp_slice_mask) & self.fbp_slice_mask
    def fbp_ltc_slice_disable_mask(self, fbp, local_ltc):
        return (self.fbp_slice_disable_mask(fbp) >> (local_ltc * self.slice_per_ltc)) & self.ltc_slice_mask
    def fbp_ltc_slice_enable_mask(self, fbp, local_ltc):
        return (self.fbp_ltc_slice_disable_mask(fbp, local_ltc) ^ self.ltc_slice_mask) & self.ltc_slice_mask

    @property
    def ioctrl_mask(self):
        return (1 << self.MAX_IOCTRL) - 1
    @property
    def ioctrl_disable_mask(self):
        return self._ioctrl_disable_mask
    @property
    def ioctrl_enable_mask(self):
        return (self.ioctrl_disable_mask ^ self.ioctrl_mask) & self.ioctrl_mask
    @property
    def lwlink_mask(self):
        return (1 << self.MAX_LWLINK) - 1
    @property
    def lwlink_per_ioctrl(self):
        return int(self.MAX_LWLINK / self.MAX_IOCTRL)
    @property
    def ioctrl_lwlink_mask(self):
        return (1 << self.lwlink_per_ioctrl) - 1
    @property
    def lwlink_disable_mask(self):
        return self._lwlink_disable_mask
    @property
    def lwlink_enable_mask(self):
        return (self.lwlink_disable_mask ^ self.lwlink_mask) & self.lwlink_mask
    def ioctrl_lwlink_disable_mask(self, ioctrl):
        return (self.lwlink_disable_mask >> (ioctrl * self.lwlink_per_ioctrl)) & self.ioctrl_lwlink_mask
    def ioctrl_lwlink_enable_mask(self, ioctrl):
        return (self.ioctrl_lwlink_disable_mask(ioctrl) ^ self.ioctrl_lwlink_mask) & self.ioctrl_lwlink_mask
