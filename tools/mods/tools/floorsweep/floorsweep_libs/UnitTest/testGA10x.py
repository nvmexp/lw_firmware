# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

from floorsweep_libs import Datalogger
from . import Utilities

import unittest

class TestGA104(Utilities.TestGPU):
    def setUp(self):
        super(TestGA104,self).setUp(gpu_name="GA104")

    ###########################################################################
    # 1 FBP
    def test_1FBP(self):
        fbp_disable_mask = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(fbp_disable_mask = fbp_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x1)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x1)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0x0])

    def test_1FBP_connectivity(self):
        fbp_disable_mask = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(fbp_disable_mask = fbp_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=True, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x1)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x1)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0x0])

    def test_1FBP_only_fbp(self):
        fbp_disable_mask = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(fbp_disable_mask = fbp_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=True, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x1)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x1)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0x0])

    def test_1FBP_iterate_cold_fbp(self):
        fbp_disable_mask = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(fbp_disable_mask = fbp_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=True, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x1)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x1)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0x0])

    def test_1FBP_fb_max_tpc(self):
        fbp_disable_mask = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(fbp_disable_mask = fbp_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=True, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x1)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x1)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0x0])

    ###########################################################################
    # 2 FBP
    def test_2FBP(self):
        fbp_disable_mask = 0x5
        defect_fsinfo = self.CreateDefectFsInfo(fbp_disable_mask = fbp_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x5)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x5)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x3, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0xff, 0x0])

    def test_2FBP_ate(self):
        fbp_disable_mask = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(fbp_disable_mask = fbp_disable_mask)

        fbp_disable_mask = 0x5
        ate_fsinfo = self.CreateAteFsInfo(fbp_disable_mask = fbp_disable_mask)

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x5)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x5)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x3, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0xff, 0x0])

    ###########################################################################
    # 1 FBIO
    def test_1FBIO(self):
        fbio_disable_mask = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(fbio_disable_mask = fbio_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x1)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x1)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0x0])

    ###########################################################################
    # 1 LTC
    def test_1LTC(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(ltc_disable_masks = ltc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x2, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xf0, 0x0, 0x0, 0x0])

    def test_1LTC_only_ltc(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(ltc_disable_masks = ltc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=True)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x2, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xf0, 0x0, 0x0, 0x0])

    def test_1LTC_only_ltc_ate_notest(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x2

        ate_fsinfo = self.CreateAteFsInfo(ltc_disable_masks = ltc_disable_masks)

        fsinfo = self.GetFsInfo(ate_fsinfo, only_ltc=True)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x2, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xf0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_defective_masks, [0xf0, 0x0, 0x0, 0x0])

    def test_1LTC_only_fbp(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(ltc_disable_masks = ltc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=True, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x1)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x1)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0x0])

    def test_1LTC_only_fbp_ate_notest(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x2

        ate_fsinfo = self.CreateAteFsInfo(ltc_disable_masks = ltc_disable_masks)

        fsinfo = self.GetFsInfo(ate_fsinfo, only_fbp=True)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x1)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x1)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_defective_masks, [0xff, 0x0, 0x0, 0x0])

    def test_1LTC_iterate_cold_fbp(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(ltc_disable_masks = ltc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=True, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x1)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x1)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0x0])

    ###########################################################################
    # 2 LTC
    def test_2LTC(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x3
        defect_fsinfo = self.CreateDefectFsInfo(ltc_disable_masks = ltc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x1)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x1)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0x0])

    def test_2LTC_ate(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(ltc_disable_masks = ltc_disable_masks)

        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x1
        ate_fsinfo = self.CreateAteFsInfo(ltc_disable_masks = ltc_disable_masks)

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x1)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x1)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0x0])

    def test_2LTC_split_FBP(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x2
        ltc_disable_masks[2] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(ltc_disable_masks = ltc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x2, 0x0, 0x1, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xf0, 0x0, 0xf, 0x0])

    def test_2LTC_split_FBP_ate(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(ltc_disable_masks = ltc_disable_masks)

        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[2] = 0x1
        ate_fsinfo = self.CreateAteFsInfo(ltc_disable_masks = ltc_disable_masks)

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x2, 0x0, 0x1, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xf0, 0x0, 0xf, 0x0])

    def test_2LTC_split_FBP_only_fbp(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x2
        ltc_disable_masks[2] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(ltc_disable_masks = ltc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=True, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x5)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x5)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x3, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0xff, 0x0])

    ###########################################################################
    # 1 SLICE
    def test_1SLICE(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(slice_defective_masks = slice_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0x12, 0x11, 0x11, 0x11])
        self.assertEqual(fsinfo.slice_defective_masks, [0x2, 0x0, 0x0, 0x0])

    def test_1SLICE_only_fbp(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(slice_defective_masks = slice_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=True, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x1)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x1)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_defective_masks, [0xff, 0x0, 0x0, 0x0])


    def test_1SLICE_only_fbp_ate_notest(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0x2

        ate_fsinfo = self.CreateAteFsInfo(slice_defective_masks = slice_disable_masks)

        fsinfo = self.GetFsInfo(ate_fsinfo, only_fbp=True)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x1)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x1)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_defective_masks, [0xff, 0x0, 0x0, 0x0])

    def test_1SLICE_only_ltc_ate_notest(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0x2

        ate_fsinfo = self.CreateAteFsInfo(slice_defective_masks = slice_disable_masks)

        fsinfo = self.GetFsInfo(ate_fsinfo, only_ltc=True)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x1, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xf, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_defective_masks, [0xf, 0x0, 0x0, 0x0])

    def test_1SLICE_iterate_cold_fbp(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(slice_defective_masks = slice_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=True, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x1)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x1)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_defective_masks, [0xff, 0x0, 0x0, 0x0])

    def test_1SLICE_only_ltc(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(slice_defective_masks = slice_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=True)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x1, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xf, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_defective_masks, [0xf, 0x0, 0x0, 0x0])

    def test_1SLICE_2LTC(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0x18
        defect_fsinfo = self.CreateDefectFsInfo(slice_defective_masks = slice_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0x18, 0x11, 0x11, 0x11])
        self.assertEqual(fsinfo.slice_defective_masks, [0x18, 0x0, 0x0, 0x0])

    ###########################################################################
    # 2 SLICE
    def test_2SLICE(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0xc
        defect_fsinfo = self.CreateDefectFsInfo(slice_defective_masks = slice_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0x3c, 0x3c, 0x3c, 0x3c])
        self.assertEqual(fsinfo.slice_defective_masks, [0xc, 0x0, 0x0, 0x0])

    def test_2SLICE_2LTC(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0xc
        slice_disable_masks[1] = 0x30
        defect_fsinfo = self.CreateDefectFsInfo(slice_defective_masks = slice_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0x3c, 0x3c, 0x3c, 0x3c])
        self.assertEqual(fsinfo.slice_defective_masks, [0xc, 0x30, 0x0, 0x0])

    def test_2SLICE_ate_ilwalid_mask(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0x5
        defect_fsinfo = self.CreateDefectFsInfo(slice_defective_masks = slice_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo(slice_defective_masks = slice_disable_masks)

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x1)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x1)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_defective_masks, [0xff, 0x0, 0x0, 0x0])

    ###########################################################################
    # 3 SLICE
    def test_3SLICE(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0x7
        defect_fsinfo = self.CreateDefectFsInfo(slice_defective_masks = slice_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x1, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xf, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_defective_masks, [0xf, 0x0, 0x0, 0x0])

    def test_3SLICE_split_LTC(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0x3
        slice_disable_masks[1] = 0x40
        defect_fsinfo = self.CreateDefectFsInfo(slice_defective_masks = slice_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xc3, 0xc3, 0xc3, 0xc3])
        self.assertEqual(fsinfo.slice_defective_masks, [0x3, 0x40, 0x0, 0x0])

    def test_3SLICE_split_LTC_no_match(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0xc
        slice_disable_masks[1] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(slice_defective_masks = slice_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x2)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x2)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x0, 0x3, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0x3c, 0xff, 0x3c, 0x3c])
        self.assertEqual(fsinfo.slice_defective_masks, [0xc, 0xff, 0x0, 0x0])

    ###########################################################################
    # 1 GPC
    def test_1GPC(self):
        gpc_disable_mask = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(gpc_disable_mask = gpc_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x1)
        self.assertEqual(fsinfo.pes_disable_masks, [0x3, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0xf, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.rop_disable_masks, [0x3, 0x0, 0x0, 0x0, 0x0, 0x0])

    ###########################################################################
    # 1 PES
    def test_1PES(self):
        pes_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        pes_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(pes_disable_masks = pes_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.pes_disable_masks, [0x2, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0xc, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.rop_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    ###########################################################################
    # 1 TPC
    def test_1TPC(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[0] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.pes_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x1, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.rop_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1TPC_iterate_cold_gpc(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[5] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=True, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x20)
        self.assertEqual(fsinfo.pes_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x3])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0xf])
        self.assertEqual(fsinfo.rop_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x3])

    def test_1TPC_iterate_cold_tpc(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[5] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=True, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.pes_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x1])
        self.assertEqual(fsinfo.rop_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1TPC_iterate_cold_gpctpc(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[5] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=True, iterate_tpc=False, iterate_cold_tpc=True, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.pes_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x1])
        self.assertEqual(fsinfo.rop_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1TPC_2GPC(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[0] = 0x1
        tpc_disable_masks[5] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.pes_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x1, 0x0, 0x0, 0x0, 0x0, 0x1])
        self.assertEqual(fsinfo.rop_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1TPC_2GPC_iterate_tpc(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[0] = 0x1
        tpc_disable_masks[5] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=True, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.pes_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x1, 0x0, 0x0, 0x0, 0x0, 0x1])
        self.assertEqual(fsinfo.rop_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1TPC_2GPC_only_gpc(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[0] = 0x1
        tpc_disable_masks[5] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=True, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x21)
        self.assertEqual(fsinfo.pes_disable_masks, [0x3, 0x0, 0x0, 0x0, 0x0, 0x3])
        self.assertEqual(fsinfo.tpc_disable_masks, [0xf, 0x0, 0x0, 0x0, 0x0, 0xf])
        self.assertEqual(fsinfo.rop_disable_masks, [0x3, 0x0, 0x0, 0x0, 0x0, 0x3])

    def test_1TPC_only_gpc_ate_notest(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[0] = 0x1
        tpc_disable_masks[5] = 0x1

        ate_fsinfo = self.CreateAteFsInfo(tpc_disable_masks = tpc_disable_masks)

        fsinfo = self.GetFsInfo(ate_fsinfo, only_gpc=True)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x21)
        self.assertEqual(fsinfo.pes_disable_masks, [0x3, 0x0, 0x0, 0x0, 0x0, 0x3])
        self.assertEqual(fsinfo.tpc_disable_masks, [0xf, 0x0, 0x0, 0x0, 0x0, 0xf])
        self.assertEqual(fsinfo.rop_disable_masks, [0x3, 0x0, 0x0, 0x0, 0x0, 0x3])

    ###########################################################################
    # 2 TPC
    def test_2TPC(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[0] = 0x1
        tpc_disable_masks[5] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.pes_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x1, 0x0, 0x0, 0x0, 0x0, 0x2])
        self.assertEqual(fsinfo.rop_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_2TPC_only_gpc(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[0] = 0x1
        tpc_disable_masks[5] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=True, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x21)
        self.assertEqual(fsinfo.pes_disable_masks, [0x3, 0x0, 0x0, 0x0, 0x0, 0x3])
        self.assertEqual(fsinfo.tpc_disable_masks, [0xf, 0x0, 0x0, 0x0, 0x0, 0xf])
        self.assertEqual(fsinfo.rop_disable_masks, [0x3, 0x0, 0x0, 0x0, 0x0, 0x3])

    ###########################################################################
    # 1 ROP
    def test_1ROP(self):
        rop_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        rop_disable_masks[0] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(rop_disable_masks = rop_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x1)
        self.assertEqual(fsinfo.pes_disable_masks, [0x3, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0xf, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.rop_disable_masks, [0x3, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1ROP_ate(self):
        rop_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        rop_disable_masks[0] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(rop_disable_masks = rop_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo(rop_disable_masks = rop_disable_masks)

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.pes_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.rop_disable_masks, [0x1, 0x0, 0x0, 0x0, 0x0, 0x0])
