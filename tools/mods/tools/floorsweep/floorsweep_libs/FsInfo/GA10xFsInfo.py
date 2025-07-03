# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

from floorsweep_libs import Exceptions
from floorsweep_libs import Datalogger
from floorsweep_libs import FsLib
from .TuringFsInfo import TU10xFsInfo

import itertools
from functools import reduce

class GA10xFsInfo(TU10xFsInfo):
    MAX_LTC = 0
    MAX_SLICE = 0
    def __init__(self, **kwargs):
        if (self.MAX_LTC == 0 or self.MAX_SLICE == 0):
            raise NotImplementedError("FsInfo MAX parameters unset")

        self._ltc_disable_masks = [0x0 for x in range(self.MAX_FBP)]
        self._rop_disable_masks = [0x0 for x in range(self.MAX_GPC)]
        # An equal number of slices must be disabled from each LTC for functionality
        # This means there is a difference between those which are disabled and those which are actuall defective
        self._slice_disable_masks = [0x0 for x in range(self.MAX_FBP)]
        self._slice_defective_masks = [0x0 for x in range(self.MAX_FBP)]

        self._override_equal_slices = False
        super(GA10xFsInfo,self).__init__(**kwargs)

    def ParseOverrideArgs(self, kwargs):
        if "override_equal_slices" in kwargs:
            self._override_equal_slices = kwargs.pop("override_equal_slices")
        super(GA10xFsInfo,self).ParseOverrideArgs(kwargs)

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
    def rop_per_gpc(self):
        return int(self.MAX_ROP / self.MAX_GPC)
    @property
    def gpc_rop_mask(self):
        return (1 << self.rop_per_gpc) - 1
    @property
    def rop_disable_masks(self):
        return self._rop_disable_masks[:] # return a copy
    @property
    def rop_enable_masks(self):
        return [((mask ^ self.gpc_rop_mask) & self.gpc_rop_mask) for mask in self._rop_disable_masks]
    def gpc_rop_disable_mask(self, gpc):
        return self._rop_disable_masks[gpc]
    def gpc_rop_enable_mask(self, gpc):
        return (self.gpc_rop_disable_mask(gpc) ^ self.gpc_rop_mask) & self.gpc_rop_mask

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
    @property
    def slice_defective_masks(self):
        return self._slice_defective_masks[:]
    def fbp_slice_disable_mask(self, fbp):
        return self._slice_disable_masks[fbp]
    def fbp_slice_enable_mask(self, fbp):
        return (self.fbp_slice_disable_mask(fbp) ^ self.fbp_slice_mask) & self.fbp_slice_mask
    def fbp_ltc_slice_disable_mask(self, fbp, local_ltc):
        return (self.fbp_slice_disable_mask(fbp) >> (local_ltc * self.slice_per_ltc)) & self.ltc_slice_mask
    def fbp_ltc_slice_enable_mask(self, fbp, local_ltc):
         return (self.fbp_ltc_slice_disable_mask(fbp, local_ltc) ^ self.ltc_slice_mask) & self.ltc_slice_mask

    @property
    def half_fbpa_mask(self):
        return (1 << self.MAX_FBP) - 1
    @property
    def half_fbpa_disable_mask(self):
        # half_fbpa_disable = 1 means the half fbpa behavior is disabled,
        # ie. both halves of the fbpa are enabled
        mask = 0x0
        for fbp,ltc_mask in enumerate(self.ltc_disable_masks):
            # The half fbpa behavior should not be active
            # if the FBPA is disabled (i.e. both LTCs are disabled)
            if ltc_mask == 0x0 or ltc_mask == self.fbp_ltc_mask:
                mask |= (0x1 << fbp)
        return mask

    def Copy(self, copy):
        self._gpc_disable_mask      = copy.gpc_disable_mask
        self._tpc_disable_masks     = copy.tpc_disable_masks
        self._pes_disable_masks     = copy.pes_disable_masks
        self._rop_disable_masks     = copy.rop_disable_masks
        self._fbp_disable_mask      = copy.fbp_disable_mask
        self._fbio_disable_mask     = copy.fbio_disable_mask
        self._ltc_disable_masks     = copy.ltc_disable_masks
        self._slice_disable_masks   = copy.slice_disable_masks
        self._slice_defective_masks = copy.slice_defective_masks

        self._override_consistent   = copy.override_consistent
        self._override_equal_slices = copy.override_equal_slices

    def ParseFuseInfo(self, fuseinfo):
        Exceptions.ASSERT(isinstance(fuseinfo, dict))

        self._fbp_disable_mask = int(fuseinfo['OPT_FBP_DISABLE'],16)
        self._fbio_disable_mask = int(fuseinfo['OPT_FBIO_DISABLE'],16)

        ltc_disable_mask = int(fuseinfo['OPT_LTC_DISABLE'],16)
        for fbp in range(self.MAX_FBP):
            self._ltc_disable_masks[fbp] = (ltc_disable_mask >> (fbp * self.ltc_per_fbp)) & self.fbp_ltc_mask

            self._slice_disable_masks[fbp] = int(fuseinfo['OPT_L2SLICE_FBP{:d}_DISABLE'.format(fbp)], 16)
            self._slice_defective_masks[fbp] = self._slice_disable_masks[fbp]

        self._gpc_disable_mask = int(fuseinfo['OPT_GPC_DISABLE'],16)

        pes_disable_mask = int(fuseinfo['OPT_PES_DISABLE'],16)
        rop_disable_mask = int(fuseinfo['OPT_ROP_DISABLE'],16)
        for gpc in range(self.MAX_GPC):
            # GA10x has a bug/feature where every chip has 3 bits per GPC in OPT_PES_DISABLE
            # regardless of how many PES's per GPC the chip actually has
            self._pes_disable_masks[gpc] = (pes_disable_mask >> (gpc * 3)) & self.gpc_pes_mask

            self._tpc_disable_masks[gpc] = int(fuseinfo['OPT_TPC_GPC{:d}_DISABLE'.format(gpc)], 16)

            self._rop_disable_masks[gpc] = (rop_disable_mask >> (gpc * self.rop_per_gpc)) & self.gpc_rop_mask

    def PrintChipInfo(self):
        Datalogger.PrintLog("... MAX_FBP_MASK         {:b}".format(self.fbp_mask))
        Datalogger.PrintLog("... MAX_FBIO_MASK        {:b}".format(self.fbio_mask))
        Datalogger.PrintLog("... MAX_FBP_LTC_MASK     {:b}".format(self.fbp_ltc_mask))
        Datalogger.PrintLog("... MAX_FBP_L2SLICE_MASK {:b}".format(self.fbp_slice_mask))
        Datalogger.PrintLog("... MAX_GPC_MASK         {:b}".format(self.gpc_mask))
        Datalogger.PrintLog("... MAX_GPC_PES_MASK     {:b}".format(self.gpc_pes_mask))
        Datalogger.PrintLog("... MAX_GPC_TPC_MASK     {:b}".format(self.gpc_tpc_mask))
        Datalogger.PrintLog("... MAX_GPC_ROP_MASK     {:b}".format(self.gpc_rop_mask))
    def PrintFuseInfo(self):
        Datalogger.PrintLog("... OPT_FBP_DISABLE       {:#x}".format(self.fbp_disable_mask))
        Datalogger.PrintLog("... OPT_FBIO_DISABLE      {:#x}".format(self.fbio_disable_mask))
        Datalogger.PrintLog("... OPT_HALF_FBPA_ENABLE  {:#x}".format(self.half_fbpa_enable_mask)) # An enable mask instead of disable
        Datalogger.PrintLog("... OPT_LTC_DISABLE       {}".format(["{:#x}".format(mask) for mask in self.ltc_disable_masks]))
        Datalogger.PrintLog("... OPT_L2SLICE_DISABLE   {}".format(["{:#x}".format(mask) for mask in self.slice_disable_masks]))
        Datalogger.PrintLog("... OPT_GPC_DISABLE       {:#x}".format(self.gpc_disable_mask))
        Datalogger.PrintLog("... OPT_PES_DISABLE       {}".format(["{:#x}".format(mask) for mask in self.pes_disable_masks]))
        Datalogger.PrintLog("... OPT_TPC_DISABLE       {}".format(["{:#x}".format(mask) for mask in self.tpc_disable_masks]))
        Datalogger.PrintLog("... OPT_ROP_DISABLE       {}".format(["{:#x}".format(mask) for mask in self.rop_disable_masks]))

    def PrintDefectiveFuseInfo(self):
        Datalogger.PrintLog("... OPT_FBP_DEFECTIVE       {:#x}".format(self.fbp_disable_mask))
        Datalogger.PrintLog("... OPT_FBIO_DEFECTIVE      {:#x}".format(self.fbio_disable_mask))
        Datalogger.PrintLog("... OPT_LTC_DEFECTIVE       {}".format(["{:#x}".format(mask) for mask in self.ltc_disable_masks]))
        Datalogger.PrintLog("... OPT_L2SLICE_DEFECTIVE   {}".format(["{:#x}".format(mask) for mask in self.slice_defective_masks]))
        Datalogger.PrintLog("... OPT_GPC_DEFECTIVE       {:#x}".format(self.gpc_disable_mask))
        Datalogger.PrintLog("... OPT_PES_DEFECTIVE       {}".format(["{:#x}".format(mask) for mask in self.pes_disable_masks]))
        Datalogger.PrintLog("... OPT_TPC_DEFECTIVE       {}".format(["{:#x}".format(mask) for mask in self.tpc_disable_masks]))
        Datalogger.PrintLog("... OPT_ROP_DEFECTIVE       {}".format(["{:#x}".format(mask) for mask in self.rop_disable_masks]))

    def PrintDisableInfo(self):
        self._PrintDisableMask("FBP", self.fbp_disable_mask)
        self._PrintDisableMask("FBIO", self.fbio_disable_mask)
        self._PrintDisableMasks("LTC", self.ltc_disable_masks)
        self._PrintDisableMasks("L2SLICE", self.slice_disable_masks)
        self._PrintDisableMask("GPC", self.gpc_disable_mask)
        self._PrintDisableMasks("PES", self.pes_disable_masks)
        self._PrintDisableMasks("TPC", self.tpc_disable_masks)
        self._PrintDisableMasks("ROP", self.rop_disable_masks)


    def LogFuseInfo(self):
        fbp_disable      = '{:#x}'.format(self.fbp_disable_mask)
        fbio_disable     = '{:#x}'.format(self.fbio_disable_mask)
        half_fbpa        = '{:#x}'.format(self.half_fbpa_enable_mask)
        ltc_disable      =    '{}'.format(["{:#x}".format(mask) for mask in self.ltc_disable_masks])
        slice_disable    =    '{}'.format(["{:#x}".format(mask) for mask in self.slice_disable_masks])
        gpc_disable      = '{:#x}'.format(self.gpc_disable_mask)
        pes_disable      =    '{}'.format(["{:#x}".format(mask) for mask in self.pes_disable_masks])
        tpc_disable      =    '{}'.format(["{:#x}".format(mask) for mask in self.tpc_disable_masks])
        rop_disable      =    '{}'.format(["{:#x}".format(mask) for mask in self.rop_disable_masks])

        Datalogger.LogInfo(OPT_FBP_DISABLE       = fbp_disable)
        Datalogger.LogInfo(OPT_FBIO_DISABLE      = fbio_disable)
        Datalogger.LogInfo(OPT_HALF_FBPA_ENABLE  = half_fbpa)
        Datalogger.LogInfo(OPT_LTC_DISABLE       = ltc_disable)
        Datalogger.LogInfo(OPT_L2SLICE_DISABLE   = slice_disable)
        Datalogger.LogInfo(OPT_GPC_DISABLE       = gpc_disable)
        Datalogger.LogInfo(OPT_PES_DISABLE       = pes_disable)
        Datalogger.LogInfo(OPT_TPC_DISABLE       = tpc_disable)
        Datalogger.LogInfo(OPT_ROP_DISABLE       = rop_disable)

    def LogDefectiveFuseInfo(self):
        fbp_disable      = '{:#x}'.format(self.fbp_disable_mask)
        fbio_disable     = '{:#x}'.format(self.fbio_disable_mask)
        ltc_disable      =    '{}'.format(["{:#x}".format(mask) for mask in self.ltc_disable_masks])
        slice_defective  =    '{}'.format(["{:#x}".format(mask) for mask in self.slice_defective_masks])
        gpc_disable      = '{:#x}'.format(self.gpc_disable_mask)
        pes_disable      =    '{}'.format(["{:#x}".format(mask) for mask in self.pes_disable_masks])
        tpc_disable      =    '{}'.format(["{:#x}".format(mask) for mask in self.tpc_disable_masks])
        rop_disable      =    '{}'.format(["{:#x}".format(mask) for mask in self.rop_disable_masks])

        Datalogger.LogInfo(OPT_FBP_DEFECTIVE     = fbp_disable)
        Datalogger.LogInfo(OPT_FBIO_DEFECTIVE    = fbio_disable)
        Datalogger.LogInfo(OPT_LTC_DEFECTIVE     = ltc_disable)
        Datalogger.LogInfo(OPT_L2SLICE_DEFECTIVE = slice_defective)
        Datalogger.LogInfo(OPT_GPC_DEFECTIVE     = gpc_disable)
        Datalogger.LogInfo(OPT_PES_DEFECTIVE     = pes_disable)
        Datalogger.LogInfo(OPT_TPC_DEFECTIVE     = tpc_disable)
        Datalogger.LogInfo(OPT_ROP_DEFECTIVE     = rop_disable)

    def CreateFsLibStruct(self):
        struct = FsLib.FsLibStruct(self.CHIP)

        struct.SetFBPMask(self.fbp_disable_mask)
        for fbp in range(self.MAX_FBP):
            struct.SetFBIOPerFBPMask(    fbp, ((self.fbp_disable_mask >> fbp) & 0x1))
            struct.SetFBPAPerFBPMask(    fbp, ((self.fbp_disable_mask >> fbp) & 0x1))
            struct.SetHalfFBPAPerFBPMask(fbp, ((self.half_fbpa_enable_mask >> fbp) & 0x1)) #enable mask
            struct.SetLTCPerFBPMask(     fbp, self.ltc_disable_masks[fbp])
            struct.SetL2SlicePerFBPMask( fbp, self.slice_disable_masks[fbp])

        struct.SetGPCMask(self.gpc_disable_mask)
        for gpc in range(self.MAX_GPC):
            struct.SetPesPerGpcMask(gpc, self.pes_disable_masks[gpc])
            struct.SetTpcPerGpcMask(gpc, self.tpc_disable_masks[gpc])

        return struct

    def ParseInitMasks(self, kwargs):
        Exceptions.ASSERT(isinstance(kwargs, dict))
        self._gpc_disable_mask    = self._GetMask(kwargs, self._gpc_disable_mask, "gpc", self.gpc_mask)
        self._pes_disable_masks   = self._GetMasks(kwargs, self._pes_disable_masks, "pes", self.gpc_pes_mask)
        self._tpc_disable_masks   = self._GetMasks(kwargs, self._tpc_disable_masks, "tpc", self.gpc_tpc_mask)
        self._rop_disable_masks   = self._GetMasks(kwargs, self._rop_disable_masks, "rop", self.gpc_rop_mask)
        self._fbp_disable_mask    = self._GetMask(kwargs, self._fbp_disable_mask, "fbp", self.fbp_mask)
        self._fbio_disable_mask   = self._GetMask(kwargs, self._fbio_disable_mask, "fbio", self.fbio_mask)
        self._ltc_disable_masks   = self._GetMasks(kwargs, self._ltc_disable_masks, "ltc", self.fbp_ltc_mask)
        self._slice_disable_masks = self._GetMasks(kwargs, self._slice_disable_masks, "slice", self.fbp_slice_mask)
        if "slice_defective_masks" in kwargs:
            Exceptions.ASSERT(isinstance(kwargs["slice_defective_masks"], list), "slice_defective_masks is not a list")
            self._slice_defective_masks = kwargs.pop("slice_defective_masks")

    def WriteFbFb(self):
        fs = ["fbp_enable:{:#x}:fbio_enable:{:#x}".format(self.fbp_enable_mask, self.fbio_enable_mask)]
        fs.append("half_fbpa_enable:{:#x}".format(self.half_fbpa_enable_mask))
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
        fs.append("half_fbpa_enable:{:#x}".format(self.half_fbpa_enable_mask))
        for fbp_idx,ltc_mask in enumerate(self.ltc_enable_masks):
            if ltc_mask and ltc_mask != self.fbp_ltc_mask:
                fs.append("fbp_ltc_enable:{:d}:{:#x}".format(fbp_idx, ltc_mask))
        for fbp_idx,slice_mask in enumerate(self.slice_enable_masks):
            if slice_mask and slice_mask != self.fbp_slice_mask:
                fs.append("fbp_l2slice_enable:{:d}:{:#x}".format(fbp_idx, slice_mask))

        fs.append("gpc_enable:{:#x}".format(self.gpc_enable_mask))
        for gpc_idx,pes_mask in enumerate(self.pes_enable_masks):
            if pes_mask and pes_mask != self.gpc_pes_mask:
                fs.append("gpc_pes_enable:{:d}:{:#x}".format(gpc_idx, pes_mask))
        for gpc_idx,tpc_mask in enumerate(self.tpc_enable_masks):
            if tpc_mask and tpc_mask != self.gpc_tpc_mask:
                fs.append("gpc_tpc_enable:{:d}:{:#x}".format(gpc_idx, tpc_mask))
        for gpc_idx,rop_mask in enumerate(self.rop_enable_masks):
            if rop_mask and rop_mask != self.gpc_rop_mask:
                fs.append("gpc_rop_enable:{:d}:{:#x}".format(gpc_idx, rop_mask))

        fs = ':'.join(fs)
        return fs

    def CloneTpc(self):
        return self.__class__(gpc_disable_mask  = self.gpc_disable_mask,
                              pes_disable_masks = self.pes_disable_masks,
                              tpc_disable_masks = self.tpc_disable_masks,
                              rop_disable_masks = self.rop_disable_masks)

    def ClearTpc(self):
        return self.__class__(clone = self, gpc_disable_mask = 0x0,
                                            pes_disable_masks = [0x0 for x in range(self.MAX_GPC)],
                                            tpc_disable_masks = [0x0 for x in range(self.MAX_GPC)],
                                            rop_disable_masks = [0x0 for x in range(self.MAX_GPC)])

    def CloneFB(self):
        return self.__class__(fbp_disable_mask    = self.fbp_disable_mask,
                              fbio_disable_mask   = self.fbio_disable_mask,
                              ltc_disable_masks   = self.ltc_disable_masks,
                              slice_defective_masks = self.slice_defective_masks)

    def ClearFB(self):
        return self.__class__(clone = self, fbp_disable_mask    = 0x0,
                                            fbio_disable_mask   = 0x0,
                                            ltc_disable_masks = [0x0 for x in range(self.MAX_FBP)],
                                            slice_disable_masks = [0x0 for x in range(self.MAX_FBP)],
                                            slice_defective_masks = [0x0 for x in range(self.MAX_FBP)])

    def CloneOverrides(self):
        clone_ = super(GA10xFsInfo,self).CloneOverrides()
        return self.__class__(clone = clone_,
                              override_equal_slices = self.override_equal_slices)

    def ClearOverride(self):
        clear_ = super(GA10xFsInfo,self).ClearOverride()
        return self.__class__(clone = clear_,
                              override_equal_slices = False)

    @property
    def override_equal_slices(self):
        return self._override_equal_slices
    def SetOverrideEqualSlices(self):
        return self.__class__(clone = self, override_equal_slices = True)

    def MakeGpcConsistent(self):
        super(GA10xFsInfo,self).MakeGpcConsistent()
        for gpc in range(self.MAX_GPC):
            gpc_disable = (1 << gpc)
            if ((self.gpc_disable_mask & gpc_disable) or
                    ((self.gpc_rop_disable_mask(gpc) & self.gpc_rop_mask) == self.gpc_rop_mask)):
                self._gpc_disable_mask |= gpc_disable
                self._pes_disable_masks[gpc] = self.gpc_pes_mask
                self._tpc_disable_masks[gpc] = self.gpc_tpc_mask
                self._rop_disable_masks[gpc] = self.gpc_rop_mask

    def MakeFBConsistent(self):
        max_slice_disable = 0
        for fbp in range(self.MAX_FBP):
            for ltc in range(self.ltc_per_fbp):
                ltc_disable = (1 << ltc)
                slice_disable_count = bin((self.slice_defective_masks[fbp] >> (ltc * self.slice_per_ltc)) & self.ltc_slice_mask).count('1')
                if ((self.fbp_ltc_disable_mask(fbp) & ltc_disable) or
                         slice_disable_count >= 3 or # Each LTC must have at least 2 slices enabled
                         (self.only_ltc and slice_disable_count > 0)):
                    self._ltc_disable_masks[fbp] |= ltc_disable
                    self._slice_defective_masks[fbp] |= (self.ltc_slice_mask << (ltc * self.slice_per_ltc))
                else:
                    max_slice_disable = max(max_slice_disable, slice_disable_count)
            fbp_disable = (1 << fbp)
            fbio_disable = (self.fbp_fbio_mask << (fbp * self.fbio_per_fbp))
            fbp_ltc_disable = (self.fbp_ltc_disable_mask(fbp) & self.fbp_ltc_mask)
            fbp_slice_disable = (self.slice_defective_masks[fbp] & self.fbp_slice_mask)
            if ((self.fbp_disable_mask & fbp_disable) or
                    (self.fbio_disable_mask & fbio_disable) or
                    (fbp_ltc_disable == self.fbp_ltc_mask) or
                    (self.only_fbp and (fbp_ltc_disable != 0 or fbp_slice_disable != 0))):
                self._fbp_disable_mask |= fbp_disable
                self._fbio_disable_mask |= fbio_disable
                self._ltc_disable_masks[fbp] = self.fbp_ltc_mask
                self._slice_defective_masks[fbp] = self.fbp_slice_mask

        self._slice_disable_masks = self.slice_defective_masks

        # All LTCs must have an equal number of slices enabled (unless the LTC is entirely disabled)
        if not self.override_equal_slices:
            if max_slice_disable == 1:
                for fbp in range(self.MAX_FBP):
                    for ltc in range(self.ltc_per_fbp):
                        mask = self.fbp_ltc_slice_disable_mask(fbp, ltc)
                        slice_disable = (1 << 0)
                        while bin(mask).count('1') < max_slice_disable:
                            mask |= slice_disable
                            slice_disable <<= 1
                        self._slice_disable_masks[fbp] |= (mask << (ltc * self.slice_per_ltc))
            elif max_slice_disable == 2:
                fbp_slice_patterns = [0x3c, 0xc3] # These are the only valid 2-slice per LTC patterns for an FBP
                pattern_failures = [0,0]
                def _FailsPattern(fbp, slice_mask, pattern):
                    for ltc in range(self.ltc_per_fbp):
                        # If an LTC is fully disabled, don't count that as a pattern failure
                        ltc_slice_dis = self.fbp_ltc_slice_disable_mask(fbp, ltc)
                        if ltc_slice_dis == self.ltc_slice_mask:
                            pattern |= (ltc_slice_dis << (ltc * self.slice_per_ltc))
                    return ((pattern & slice_mask) != slice_mask)
                for fbp in range(self.MAX_FBP):
                    if (1 << fbp) & self.fbp_disable_mask:
                        continue
                    mask = self.fbp_slice_disable_mask(fbp)
                    for i,pattern in enumerate(fbp_slice_patterns):
                        if _FailsPattern(fbp, mask, pattern):
                            pattern_failures[i] += 1
                best_pattern_idx = 0 if pattern_failures[0] <= pattern_failures[1] else 1
                for fbp in range(self.MAX_FBP):
                    fbp_disable = (1 << fbp)
                    if fbp_disable & self.fbp_disable_mask:
                        continue
                    mask = self.fbp_slice_disable_mask(fbp)
                    pattern = fbp_slice_patterns[best_pattern_idx]
                    if _FailsPattern(fbp, mask, pattern):
                        # If an FBP cannot fit its slices to the pattern, then just disable the whole thing
                        self._fbp_disable_mask |= fbp_disable
                        self._fbio_disable_mask |= (self.fbp_fbio_mask << (fbp * self.fbio_per_fbp))
                        self._ltc_disable_masks[fbp] = self.fbp_ltc_mask
                        self._slice_defective_masks[fbp] = self.fbp_slice_mask
                        self._slice_disable_masks[fbp] = self.fbp_slice_mask
                    else:
                        self._slice_disable_masks[fbp] |= pattern

    # Bitwise operators
    def IlwertTpc(self):
        return self.__class__(tpc_disable_masks = self.tpc_enable_masks)
    def IlwertLtc(self):
        return self.__class__(slice_defective_masks = [(mask ^ self.fbp_slice_mask) & self.fbp_slice_mask for mask in self.slice_defective_masks])
    def __ilwert__(self):
        # Only ilwert the smallest components, eg. tpc and ltc, because an aggregate might
        # still be enabled when its component mask is ilwerted. The results will trickle up
        # to the aggregates when the masks are made consistent.
        return self.CloneOverrides() | self.IlwertTpc() | self.IlwertLtc()

    def __or__(self, other):
        Exceptions.ASSERT(self.__class__ == other.__class__, "Cannot or {} with {}".format(self.__class__, other.__class__))
        return self.__class__(clone = self.CloneOverrides(),
                              gpc_disable_mask      = (self.gpc_disable_mask | other.gpc_disable_mask),
                              pes_disable_masks     = [(x|y) for x,y in zip(self.pes_disable_masks,other.pes_disable_masks)],
                              tpc_disable_masks     = [(x|y) for x,y in zip(self.tpc_disable_masks,other.tpc_disable_masks)],
                              rop_disable_masks     = [(x|y) for x,y in zip(self.rop_disable_masks,other.rop_disable_masks)],
                              fbp_disable_mask      = (self.fbp_disable_mask | other.fbp_disable_mask),
                              fbio_disable_mask     = (self.fbio_disable_mask | other.fbio_disable_mask),
                              ltc_disable_masks     = [(x|y) for x,y in zip(self.ltc_disable_masks,other.ltc_disable_masks)],
                              slice_defective_masks = [(x|y) for x,y in zip(self.slice_defective_masks,other.slice_defective_masks)])

    def __xor__(self, other):
        Exceptions.ASSERT(self.__class__ == other.__class__, "Cannot xor {} with {}".format(self.__class__, other.__class__))
        return self.__class__(clone = self.CloneOverrides(),
                              gpc_disable_mask      = (self.gpc_disable_mask ^ other.gpc_disable_mask),
                              pes_disable_masks     = [(x^y) for x,y in zip(self.pes_disable_masks,other.pes_disable_masks)],
                              tpc_disable_masks     = [(x^y) for x,y in zip(self.tpc_disable_masks,other.tpc_disable_masks)],
                              rop_disable_masks     = [(x^y) for x,y in zip(self.rop_disable_masks,other.rop_disable_masks)],
                              fbp_disable_mask      = (self.fbp_disable_mask ^ other.fbp_disable_mask),
                              fbio_disable_mask     = (self.fbio_disable_mask ^ other.fbio_disable_mask),
                              ltc_disable_masks     = [(x^y) for x,y in zip(self.ltc_disable_masks,other.ltc_disable_masks)],
                              slice_defective_masks = [(x^y) for x,y in zip(self.slice_defective_masks,other.slice_defective_masks)])

    def __and__(self, other):
        Exceptions.ASSERT(self.__class__ == other.__class__, "Cannot and {} with {}".format(self.__class__, other.__class__))
        return self.__class__(clone = self.CloneOverrides(),
                              gpc_disable_mask      = (self.gpc_disable_mask & other.gpc_disable_mask),
                              pes_disable_masks     = [(x&y) for x,y in zip(self.pes_disable_masks,other.pes_disable_masks)],
                              tpc_disable_masks     = [(x&y) for x,y in zip(self.tpc_disable_masks,other.tpc_disable_masks)],
                              rop_disable_masks     = [(x&y) for x,y in zip(self.rop_disable_masks,other.rop_disable_masks)],
                              fbp_disable_mask      = (self.fbp_disable_mask & other.fbp_disable_mask),
                              fbio_disable_mask     = (self.fbio_disable_mask & other.fbio_disable_mask),
                              ltc_disable_masks     = [(x&y) for x,y in zip(self.ltc_disable_masks,other.ltc_disable_masks)],
                              slice_defective_masks = [(x&y) for x,y in zip(self.slice_defective_masks,other.slice_defective_masks)])

    def __eq__(self, other):
        if self.__class__ != other.__class__:
            return False
        return (self.gpc_disable_mask  == other.gpc_disable_mask and
                self.pes_disable_masks == other.pes_disable_masks and
                self.tpc_disable_masks == other.tpc_disable_masks and
                self.rop_disable_masks == other.rop_disable_masks and
                self.fbp_disable_mask  == other.fbp_disable_mask and
                self.fbio_disable_mask == other.fbio_disable_mask and
                self.ltc_disable_masks == other.ltc_disable_masks and
                self.slice_defective_masks == other.slice_defective_masks)

    def IterateColdGpc(self):
        for gpc in range(self.MAX_GPC):
            gpc_mask = (1 << gpc)
            if gpc_mask & self.gpc_disable_mask:
                continue
            gpc_disable_mask = self.gpc_disable_mask | gpc_mask
            yield self.__class__(clone = self.ClearTpc(), gpc_disable_mask = gpc_disable_mask,
                                                          pes_disable_masks = self.pes_disable_masks,
                                                          tpc_disable_masks = self.tpc_disable_masks,
                                                          rop_disable_masks = self.rop_disable_masks)

    def IterateColdFbp(self):
        for fbp in range(self.MAX_FBP):
            fbp_mask = (1 << fbp)
            if fbp_mask & self.fbp_disable_mask:
                continue
            if (fbp_mask | self.fbp_disable_mask) == self.fbp_mask:
                continue
            yield self.__class__(clone = self.ClearFB(), fbp_disable_mask = fbp_mask,
                                                         ltc_disable_masks = self.ltc_disable_masks,
                                                         slice_defective_masks = self.slice_defective_masks)

    def IterateLtc(self):
        for fbp in range(self.MAX_FBP):
            for ltc in range(self.ltc_per_fbp):
                ltc_mask = (1 << ltc)
                if ltc_mask & self.fbp_ltc_disable_mask(fbp):
                    continue
                ltc_enable_masks = [0x0 for x in range(self.MAX_FBP)]
                ltc_enable_masks[fbp] = ltc_mask
                yield self.__class__(clone = self.ClearFB(), ltc_enable_masks = ltc_enable_masks)

    def IterateColdSlice(self):
        for fbp in range(self.MAX_FBP):
            for ltc in range(self.ltc_per_fbp):
                one_slice_masks = [(1 << bit) for bit in range(self.slice_per_ltc)]
                two_slice_masks = [0x3, 0xc]
                disable_masks = one_slice_masks + two_slice_masks
                for disable_mask in disable_masks:
                    if bin(disable_mask & self.fbp_ltc_slice_disable_mask(fbp, ltc)).count('1') >= 3:
                        # Can't disable more than 3 slices total in a particular LTC
                        continue
                    slice_disable = (disable_mask << (ltc * self.slice_per_ltc))
                    if slice_disable & self.fbp_slice_disable_mask(fbp):
                        continue
                    slice_disable_masks = self.slice_disable_masks
                    slice_disable_masks[fbp] |= slice_disable
                    yield self.__class__(clone = self.ClearFB(), fbp_disable_mask = self.fbp_disable_mask,
                                                                 ltc_disable_masks = self.ltc_disable_masks,
                                                                 slice_defective_masks = slice_disable_masks)

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
                                                      slice_defective_masks = slice_disable_masks)


class GA102FsInfo(GA10xFsInfo):
    CHIP = "ga102"
    MAX_GPC = 7
    MAX_TPC = 42
    MAX_FBP = 6
    MAX_FBIO = 6
    MAX_ROP = 14
    MAX_PES = [2,2,2]
    MAX_LTC = 12
    MAX_SLICE = 48
    def __init__(self, **kwargs):
        super(GA102FsInfo,self).__init__(**kwargs)

class GA103FsInfo(GA10xFsInfo):
    CHIP = "ga103"
    MAX_GPC = 6
    MAX_TPC = 30
    MAX_FBP = 5
    MAX_FBIO = 5
    MAX_ROP = 12
    MAX_PES = [2,2,1]
    MAX_LTC = 10
    MAX_SLICE = 40
    def __init__(self, **kwargs):
        super(GA103FsInfo,self).__init__(**kwargs)

class GA104FsInfo(GA10xFsInfo):
    CHIP = "ga104"
    MAX_GPC = 6
    MAX_TPC = 24
    MAX_FBP = 4
    MAX_FBIO = 4
    MAX_ROP = 12
    MAX_PES = [2,2]
    MAX_LTC = 8
    MAX_SLICE = 32
    def __init__(self, **kwargs):
        super(GA104FsInfo,self).__init__(**kwargs)

class GA106FsInfo(GA10xFsInfo):
    CHIP = "ga106"
    MAX_GPC = 3
    MAX_TPC = 15
    MAX_FBP = 3
    MAX_FBIO = 3
    MAX_ROP = 6
    MAX_PES = [2,2,1]
    MAX_LTC = 6
    MAX_SLICE = 24
    def __init__(self, **kwargs):
        super(GA106FsInfo,self).__init__(**kwargs)

class GA107FsInfo(GA10xFsInfo):
    CHIP = "ga107"
    MAX_GPC = 2
    MAX_TPC = 10
    MAX_FBP = 2
    MAX_FBIO = 2
    MAX_ROP = 4
    MAX_PES = [2,2,1]
    MAX_LTC = 4
    MAX_SLICE = 16
    def __init__(self, **kwargs):
        super(GA107FsInfo,self).__init__(**kwargs)
