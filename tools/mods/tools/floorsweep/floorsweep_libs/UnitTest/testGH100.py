# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

from floorsweep_libs import Datalogger
from floorsweep_libs import Exceptions
from . import Utilities

import unittest
from tempfile import NamedTemporaryFile

class TestGH100(Utilities.TestGPU):
    def setUp(self):
        super(TestGH100,self).setUp(gpu_name="GH100")

    ###########################################################################
    # 1 FBP
    def test_1FBP(self):
        fbp_disable_mask = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(fbp_disable_mask = fbp_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1FBP_1TPC(self):
        fbp_disable_mask = 0x1
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[0] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(fbp_disable_mask = fbp_disable_mask,
                                                tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1FBP_CPC0(self):
        fbp_disable_mask = 0x1
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[0] = 0x7
        defect_fsinfo = self.CreateDefectFsInfo(fbp_disable_mask = fbp_disable_mask,
                                                tpc_disable_masks = tpc_disable_masks)

        tpc_disable_masks[0] = 0x6
        ate_fsinfo = self.CreateAteFsInfo(tpc_disable_masks = tpc_disable_masks)

        with self.assertRaises(Exceptions.FsSanityFailed):
            fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

    def test_1FBP_connectivity(self):
        fbp_disable_mask = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(fbp_disable_mask = fbp_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=True, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1FBP_connectivity_ecc(self):
        fbp_disable_mask = 0x8
        defect_fsinfo = self.CreateDefectFsInfo()
        ecc_fsinfo = self.CreateDefectFsInfo(fbp_disable_mask = fbp_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, ecc_fsinfo, run_connectivity=True, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1FBP_only_fbp(self):
        fbp_disable_mask = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(fbp_disable_mask = fbp_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=True, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1FBP_iterate_cold_fbp(self):
        fbp_disable_mask = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(fbp_disable_mask = fbp_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=True, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1FBP_fb_max_tpc(self):
        fbp_disable_mask = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(fbp_disable_mask = fbp_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=True, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    ###########################################################################
    # 2 FBP
    def test_2FBP(self):
        fbp_disable_mask = 0x11
        defect_fsinfo = self.CreateDefectFsInfo(fbp_disable_mask = fbp_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x1B)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x3cf)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x3, 0x0, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0xff, 0x0, 0xff, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_2FBP_ate(self):
        fbp_disable_mask = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(fbp_disable_mask = fbp_disable_mask)

        fbp_disable_mask = 0x10
        ate_fsinfo = self.CreateAteFsInfo(fbp_disable_mask = fbp_disable_mask)

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x1B)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x3cf)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x3, 0x0, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0xff, 0x0, 0xff, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    ###########################################################################
    # 1 FBIO
    def test_1FBIO(self):
        fbio_disable_mask = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(fbio_disable_mask = fbio_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1FBIO_ate(self):
        fbio_disable_mask = 0x1
        defect_fsinfo = self.CreateDefectFsInfo()

        ate_fsinfo = self.CreateAteFsInfo(fbio_disable_mask = fbio_disable_mask)

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1FBIO_ate_notest(self):
        fbio_disable_mask = 0x1
        defect_fsinfo = self.CreateDefectFsInfo()

        ate_fsinfo = self.CreateAteFsInfo(fbio_disable_mask = fbio_disable_mask)

        fsinfo = self.GetFsInfo(ate_fsinfo)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

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
        self.assertEqual(fsinfo.ltc_disable_masks, [0x2, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xf0, 0x0, 0x0, 0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1LTC_only_ltc(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(ltc_disable_masks = ltc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=True)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x2, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xf0, 0x0, 0x0, 0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1LTC_only_ltc_ate_notest(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x2

        ate_fsinfo = self.CreateAteFsInfo(ltc_disable_masks = ltc_disable_masks)

        fsinfo = self.GetFsInfo(ate_fsinfo, only_ltc=True)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x2, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xf0, 0x0, 0x0, 0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1LTC_only_fbp(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(ltc_disable_masks = ltc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=True, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1LTC_only_fbp_ate_notest(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x2

        ate_fsinfo = self.CreateAteFsInfo(ltc_disable_masks = ltc_disable_masks)

        fsinfo = self.GetFsInfo(ate_fsinfo, only_fbp=True)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1LTC_iterate_cold_fbp(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(ltc_disable_masks = ltc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=True, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    ###########################################################################
    # 2 LTC
    def test_2LTC(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x3
        defect_fsinfo = self.CreateDefectFsInfo(ltc_disable_masks = ltc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_2LTC_l2noc(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x1
        ltc_disable_masks[3] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(ltc_disable_masks = ltc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x1, 0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xf, 0x0, 0x0, 0xf0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_2LTC_split_l2noc(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x1
        ltc_disable_masks[3] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(ltc_disable_masks = ltc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_2LTC_split_l2noc_ate_notest(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x1
        ltc_disable_masks[3] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo()

        ate_fsinfo = self.CreateAteFsInfo(ltc_disable_masks = ltc_disable_masks)

        fsinfo = self.GetFsInfo(ate_fsinfo)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_2LTC_ate(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(ltc_disable_masks = ltc_disable_masks)

        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x1
        ate_fsinfo = self.CreateAteFsInfo(ltc_disable_masks = ltc_disable_masks)

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_2LTC_split_FBP(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x2
        ltc_disable_masks[1] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(ltc_disable_masks = ltc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x2, 0x1, 0x0, 0x1, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xf0, 0xf, 0x0, 0xf, 0xf0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_2LTC_split_FBP_ate(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(ltc_disable_masks = ltc_disable_masks)

        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[1] = 0x1
        ate_fsinfo = self.CreateAteFsInfo(ltc_disable_masks = ltc_disable_masks)

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x2, 0x1, 0x0, 0x1, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xf0, 0xf, 0x0, 0xf, 0xf0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_2LTC_split_FBP_only_fbp(self):
        ltc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        ltc_disable_masks[0] = 0x2
        ltc_disable_masks[1] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(ltc_disable_masks = ltc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=True, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x1B)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x3cf)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x3, 0x0, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0xff, 0x0, 0xff, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    ###########################################################################
    # 1 SLICE
    def test_1SLICE(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(slice_disable_masks = slice_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0x2, 0x0, 0x0, 0x20, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1SLICE_connectivity_ecc(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0x2

        defect_fsinfo = self.CreateDefectFsInfo()
        ecc_fsinfo = self.CreateDefectFsInfo(slice_disable_masks = slice_disable_masks)
        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, ecc_fsinfo, run_connectivity=True, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0x2, 0x0, 0x0, 0x20, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1SLICE_only_fbp(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(slice_disable_masks = slice_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=True, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1SLICE_only_fbp_ate_notest(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0x2

        ate_fsinfo = self.CreateAteFsInfo(slice_disable_masks = slice_disable_masks)

        fsinfo = self.GetFsInfo(ate_fsinfo, only_fbp=True)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1SLICE_only_ltc_ate_notest(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0x2

        ate_fsinfo = self.CreateAteFsInfo(slice_disable_masks = slice_disable_masks)

        fsinfo = self.GetFsInfo(ate_fsinfo, only_ltc=True)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x1, 0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xf, 0x0, 0x0, 0xf0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1SLICE_iterate_cold_fbp(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(slice_disable_masks = slice_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=True, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1SLICE_only_ltc(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(slice_disable_masks = slice_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=True)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x1, 0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xf, 0x0, 0x0, 0xf0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1SLICE_2LTC(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0x18
        defect_fsinfo = self.CreateDefectFsInfo(slice_disable_masks = slice_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    ###########################################################################
    # 2 SLICE
    def test_2SLICE(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0xc
        defect_fsinfo = self.CreateDefectFsInfo(slice_disable_masks = slice_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x1, 0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xf, 0x0, 0x0, 0xf0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_2SLICE_only_fbp(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0xc
        defect_fsinfo = self.CreateDefectFsInfo(slice_disable_masks = slice_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=True, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_2SLICE_2LTC(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0xc
        slice_disable_masks[1] = 0x30
        defect_fsinfo = self.CreateDefectFsInfo(slice_disable_masks = slice_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x1, 0x2, 0x0, 0x2, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xf, 0xf0, 0x0, 0xf0, 0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    ###########################################################################
    # 3 SLICE
    def test_3SLICE(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0x13
        defect_fsinfo = self.CreateDefectFsInfo(slice_disable_masks = slice_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, run_connectivity=False, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x9)
        self.assertEqual(fsinfo.fbio_disable_mask, 0xc3)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_3SLICE_connectivity_ecc(self):
        slice_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_FBP)]
        slice_disable_masks[0] = 0x1
        slice_disable_masks[1] = 0x2
        slice_disable_masks[3] = 0x10

        defect_fsinfo = self.CreateDefectFsInfo()
        ecc_fsinfo = self.CreateDefectFsInfo(slice_disable_masks = slice_disable_masks)
        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepFB(ate_fsinfo, defect_fsinfo, ecc_fsinfo, run_connectivity=True, only_fbp=False, iterate_cold_fbp=False, fb_max_tpc=False, only_ltc=False)

        self.assertEqual(fsinfo.fbp_disable_mask, 0x0)
        self.assertEqual(fsinfo.fbio_disable_mask, 0x0)
        self.assertEqual(fsinfo.ltc_disable_masks, [0x1, 0x1, 0x0, 0x2, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.slice_disable_masks, [0xf, 0xf, 0x0, 0xf0, 0xf0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    ###########################################################################
    # 1 GPC
    def test_1GPC(self):
        gpc_disable_mask = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(gpc_disable_mask = gpc_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x2)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x7, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x1ff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1GPC_CPC0(self):
        gpc_disable_mask = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(gpc_disable_mask = gpc_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        with self.assertRaises(Exceptions.FsSanityFailed):
            self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

    ###########################################################################
    # 1 CPC
    def test_1CPC(self):
        cpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        cpc_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(cpc_disable_masks = cpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x38, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1CPC_CPC2(self):
        cpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        cpc_disable_masks[1] = 0x4
        defect_fsinfo = self.CreateDefectFsInfo(cpc_disable_masks = cpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x1c0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1CPC_CPC2_iterate_tpc(self):
        cpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        cpc_disable_masks[1] = 0x4
        defect_fsinfo = self.CreateDefectFsInfo(cpc_disable_masks = cpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=True, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x1c0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1CPC_CPC2_iterate_cold_gpc(self):
        cpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        cpc_disable_masks[1] = 0x4
        defect_fsinfo = self.CreateDefectFsInfo(cpc_disable_masks = cpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=True, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x2)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x7, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x1ff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1CPC_CPC0(self):
        cpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        cpc_disable_masks[0] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(cpc_disable_masks = cpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        with self.assertRaises(Exceptions.FsSanityFailed):
            self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

    ###########################################################################
    # 2 CPC
    def test_2CPC_CPC0CPC1(self):
        cpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        cpc_disable_masks[1] = 0x3
        defect_fsinfo = self.CreateDefectFsInfo(cpc_disable_masks = cpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x2)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x7, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x1ff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_2CPC_CPC0CPC2(self):
        cpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        cpc_disable_masks[1] = 0x5
        defect_fsinfo = self.CreateDefectFsInfo(cpc_disable_masks = cpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x5, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x1c7, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_2CPC_CPC1CPC2(self):
        cpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        cpc_disable_masks[1] = 0x6
        defect_fsinfo = self.CreateDefectFsInfo(cpc_disable_masks = cpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x6, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x1f8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_2CPC_CPC0CPC1_iterate_tpc(self):
        cpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        cpc_disable_masks[1] = 0x3
        defect_fsinfo = self.CreateDefectFsInfo(cpc_disable_masks = cpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=True, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x2)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x7, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x1ff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_2CPC_CPC0CPC1_ate_notest(self):
        cpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        cpc_disable_masks[1] = 0x3
        defect_fsinfo = self.CreateDefectFsInfo()

        ate_fsinfo = self.CreateAteFsInfo(cpc_disable_masks = cpc_disable_masks)

        fsinfo = self.GetFsInfo(ate_fsinfo)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x2)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x7, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x1ff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    ###########################################################################
    # 1 TPC
    def test_1TPC(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[1] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1TPC_CPC0(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[0] = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1TPC_CPC2(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[1] = 0x80
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1TPC_CPC2_iterate_tpc(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[1] = 0x80
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=True, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1TPC_CPC2_iterate_cold_tpc(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[1] = 0x80
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=True, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1TPC_CPC2_iterate_cold_gpc_tpc(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[1] = 0x80
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=True, iterate_tpc=False, iterate_cold_tpc=True, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_1TPC_iterate_cold_gpc(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[5] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=True, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x20)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x7, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x1ff, 0x0, 0x0])

    def test_1TPC_iterate_cold_tpc(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[5] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=True, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0, 0x0])

    def test_1TPC_iterate_cold_gpctpc(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[5] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=True, iterate_tpc=False, iterate_cold_tpc=True, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0, 0x0])

    def test_1TPC_2GPC(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[0] = 0x1
        tpc_disable_masks[5] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x1, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0, 0x0])

    def test_1TPC_2GPC_iterate_tpc(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[0] = 0x1
        tpc_disable_masks[5] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=True, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x1, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0, 0x0])

    def test_1TPC_2GPC_only_gpc(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[1] = 0x1
        tpc_disable_masks[5] = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=True, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x22)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x7, 0x0, 0x0, 0x0, 0x7, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x1ff, 0x0, 0x0, 0x0, 0x1ff, 0x0, 0x0])

    def test_1TPC_only_gpc_ate_notest(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[1] = 0x1
        tpc_disable_masks[5] = 0x1

        ate_fsinfo = self.CreateAteFsInfo(tpc_disable_masks = tpc_disable_masks)

        fsinfo = self.GetFsInfo(ate_fsinfo, only_gpc=True)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x22)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x7, 0x0, 0x0, 0x0, 0x7, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x1ff, 0x0, 0x0, 0x0, 0x1ff, 0x0, 0x0])

    ###########################################################################
    # 2 TPC
    def test_2TPC_CPC0(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[0] = 0x3
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_2TPC_CPC2(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[1] = 0x81
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x81, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_2TPC_only_gpc(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[1] = 0x3
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=True, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x2)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x7, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x1ff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])

    ###########################################################################
    # 3 TPC
    def test_3TPC_CPC0_ate(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[0] = 0x7
        defect_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)

        tpc_disable_masks[0] = 0x5
        ate_fsinfo = self.CreateAteFsInfo(tpc_disable_masks = tpc_disable_masks)

        with self.assertRaises(Exceptions.FsSanityFailed):
            fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, only_gpc=False, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

    def test_3TPC_ecc(self):
        tpc_disable_masks = [0x0 for x in range(self.fsinfo_class.MAX_GPC)]
        tpc_disable_masks[1] = 0x1
        tpc_disable_masks[2] = 0x5

        defect_fsinfo = self.CreateDefectFsInfo()
        ecc_fsinfo = self.CreateDefectFsInfo(tpc_disable_masks = tpc_disable_masks)
        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepTpc(ate_fsinfo, defect_fsinfo, ecc_fsinfo, only_gpc=True, iterate_cold_gpc=False, iterate_tpc=False, iterate_cold_tpc=False, smid_shmoo=False, tpc_sriov=False)

        self.assertEqual(fsinfo.gpc_disable_mask, 0x0)
        self.assertEqual(fsinfo.cpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
        self.assertEqual(fsinfo.tpc_disable_masks, [0x0, 0x1, 0x5, 0x0, 0x0, 0x0, 0x0, 0x0])

    ############################################################################
    # 1 IOCTRL
    def test_1IOCTRL(self):
        ioctrl_disable_mask = 0x1
        defect_fsinfo = self.CreateDefectFsInfo(ioctrl_disable_mask = ioctrl_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepLwlink(ate_fsinfo, defect_fsinfo)

        self.assertEqual(fsinfo.ioctrl_disable_mask, 0x1)
        self.assertEqual(fsinfo.lwlink_disable_mask, 0x3f)

    def test_1IOCTRL_ate_notest(self):
        ioctrl_disable_mask = 0x1
        ate_fsinfo = self.CreateAteFsInfo(ioctrl_disable_mask = ioctrl_disable_mask)

        fsinfo = self.GetFsInfo(ate_fsinfo)

        self.assertEqual(fsinfo.ioctrl_disable_mask, 0x1)
        self.assertEqual(fsinfo.lwlink_disable_mask, 0x3f)

    def test_1IOCTRL_6LWLINK_ate_notest(self):
        lwlink_disable_mask = 0xfc0
        ate_fsinfo = self.CreateAteFsInfo(lwlink_disable_mask = lwlink_disable_mask)

        fsinfo = self.GetFsInfo(ate_fsinfo)

        self.assertEqual(fsinfo.ioctrl_disable_mask, 0x2)
        self.assertEqual(fsinfo.lwlink_disable_mask, 0xfc0)

    ###########################################################################
    # 1 LWLINK
    def test_1LWLINK(self):
        lwlink_disable_mask = 0x2
        defect_fsinfo = self.CreateDefectFsInfo(lwlink_disable_mask = lwlink_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepLwlink(ate_fsinfo, defect_fsinfo)

        self.assertEqual(fsinfo.ioctrl_disable_mask, 0x0)
        self.assertEqual(fsinfo.lwlink_disable_mask, 0x2)

    def test_1LWLINK_ate_notest(self):
        lwlink_disable_mask = 0x2
        ate_fsinfo = self.CreateAteFsInfo(lwlink_disable_mask = lwlink_disable_mask)

        fsinfo = self.GetFsInfo(ate_fsinfo)

        self.assertEqual(fsinfo.ioctrl_disable_mask, 0x0)
        self.assertEqual(fsinfo.lwlink_disable_mask, 0x2)

    ###########################################################################
    # 2 LWLINK
    def test_2LWLINK(self):
        lwlink_disable_mask = 0x202
        defect_fsinfo = self.CreateDefectFsInfo(lwlink_disable_mask = lwlink_disable_mask)

        ate_fsinfo = self.CreateAteFsInfo()

        fsinfo = self.FloorsweepLwlink(ate_fsinfo, defect_fsinfo)

        self.assertEqual(fsinfo.ioctrl_disable_mask, 0x0)
        self.assertEqual(fsinfo.lwlink_disable_mask, 0x202)

    ###########################################################################
    # MISC
    def test_Log_SMID(self):
        with NamedTemporaryFile(mode='w+') as testlog:
            testlog.write("ERROR: Failing SMID: 0 GPC: 1 TPC: 4\n")
            testlog.write("ERROR: Failing SMID: 0 GPC: 2 TPC: 0\n")
            testlog.seek(0)

            test = Utilities.FakeTestInfo(None, None)
            fsinfo = self.CreateAteFsInfo()
            smid_errors,ecc_errors = test.FindFailingInfo(fsinfo, testlog.name)

            self.assertEqual(ecc_errors, None)
            self.assertEqual(smid_errors.gpc_disable_mask, 0x0)
            self.assertEqual(smid_errors.cpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
            self.assertEqual(smid_errors.tpc_disable_masks, [0x0, 0x10, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0])

    def test_Log_ECC(self):
        with NamedTemporaryFile(mode='w+') as testlog:
            testlog.write("ECC errors exceeded the threshold:\n")
            testlog.write("- L1DATA CORR: errs = 1, thresh = 0\n")
            testlog.write("  * 1 in GPC 4, TPC 4\n")
            testlog.write("- L1DATA UNCORR: errs = 1, thresh = 0\n")
            testlog.write("  * 1 in GPC 5, TPC 2\n")
            testlog.write("- FB CORR: errs = 14, thresh = 0\n")
            testlog.write("  * 14 in partition B, subpartition 1\n")
            testlog.write("- FB CORR: errs = 14, thresh = 0\n")
            testlog.write("  * 1 in ltc 5, slice 2\n")
            testlog.seek(0)

            test = Utilities.FakeTestInfo(None, None)
            fsinfo = self.CreateAteFsInfo()
            smid_errors,ecc_errors = test.FindFailingInfo(fsinfo, testlog.name)

            self.assertEqual(smid_errors, None)
            self.assertEqual(ecc_errors.gpc_disable_mask, 0x0)
            self.assertEqual(ecc_errors.cpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
            self.assertEqual(ecc_errors.tpc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x10, 0x4, 0x0, 0x0])
            self.assertEqual(ecc_errors.fbp_disable_mask, 0x2)
            self.assertEqual(ecc_errors.fbio_disable_mask, 0x0)
            self.assertEqual(ecc_errors.ltc_disable_masks, [0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
            self.assertEqual(ecc_errors.slice_disable_masks, [0x0, 0x0, 0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0])
