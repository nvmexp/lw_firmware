#!/home/utils/Python/builds/3.9.6-20210823/bin/python3


import unittest
from unittest.case import doModuleCleanups
from fs_lib import *

class TestFSGH100IsValid(unittest.TestCase):
    
    def test_isvalid1(self):
        gh100 = IsFSValidCallStruct("gh100")
        for i in [0, 1, 2]: gh100.setDisableFBP(i)
        self.assertFalse(gh100.isFSValid())

    def test_isvalid_FBPFS(self):
        gh100 = IsFSValidCallStruct("gh100")
        for i in [0, 3]: gh100.setDisableFBP(i)
        for i in [(0,0), (0,1), (3,0), (3,1)]: gh100.setDisableFBIO(*i)
        for i in [(0,0), (0,1), (3,0), (3,1)]: gh100.setDisableLTC(*i)
        for i in range(0, 8):
            gh100.setDisableL2Slice(0, i)
            gh100.setDisableL2Slice(3, i)
        self.assertTrue(gh100.isFSValid(), msg=gh100.getErrMsg())

    def test_isvalid_GPC0FS(self):
        gh100 = IsFSValidCallStruct("gh100")
        gh100.setDisableGPC(0)
        self.assertFalse(gh100.isFSValid())

    def test_isvalid_GPC1FS(self):
        gh100 = IsFSValidCallStruct("gh100")
        gh100.setDisableGPC(1)
        self.assertFalse(gh100.isFSValid())


class TestFSGH100IsInSKU(unittest.TestCase):

    def test_IsInSKUValid(self):
        sku = SkuConfig("example_sku_name", 42, 
            {
                "tpc": FSConfig(62, 62, 6, -1, 0, 0, [1], [9]),
                "gpc": FSConfig(7, 8, -1, -1, 0, 0, [], []),
                "l2slice": FSConfig(94, 94, -1, -1, 0, 0, [], [])
            }
        )

        gh100 = IsFSValidCallStruct("gh100")
        gh100.setDisableGPC(1)
        for i in range(0, 9) : gh100.setDisableTPC(1, i)
        for i in range(0, 3) : gh100.setDisableCPC(1, i)
        gh100.setDisableTPC(2, 3)
        gh100.setDisableL2Slice(0, 0)
        gh100.setDisableL2Slice(3, 4)

        self.assertTrue(gh100.isInSKU(sku), msg=gh100.getErrMsg())

    def test_IsInSKUNotValidMustEnableTPC(self):
        sku = SkuConfig("example_sku_name", 42, 
            {
                "tpc": FSConfig(62, 62, 6, -1, 0, 0, [9], []),
                "gpc": FSConfig(7, 8, -1, -1, 0, 0, [], []),
                "l2slice": FSConfig(94, 94, -1, -1, 0, 0, [], [])
            }
        )

        gh100 = IsFSValidCallStruct("gh100")
        gh100.setDisableGPC(1)
        for i in range(0, 9) : gh100.setDisableTPC(1, i)
        for i in range(0, 3) : gh100.setDisableCPC(1, i)
        gh100.setDisableTPC(2, 3)
        gh100.setDisableL2Slice(0, 0)
        gh100.setDisableL2Slice(3, 4)

        self.assertFalse(gh100.isInSKU(sku))

    def test_IsInSKUNotValidMustDisableTPC(self):
        sku = SkuConfig("example_sku_name", 42, 
            {
                "tpc": FSConfig(62, 62, 6, -1, 0, 0, [], [3]),
                "gpc": FSConfig(7, 8, -1, -1, 0, 0, [], []),
                "l2slice": FSConfig(94, 94, -1, -1, 0, 0, [], [])
            }
        )

        gh100 = IsFSValidCallStruct("gh100")
        gh100.setDisableGPC(1)
        for i in range(0, 9) : gh100.setDisableTPC(1, i)
        for i in range(0, 3) : gh100.setDisableCPC(1, i)
        gh100.setDisableTPC(2, 3)
        gh100.setDisableL2Slice(0, 0)
        gh100.setDisableL2Slice(3, 4)

        self.assertFalse(gh100.isInSKU(sku))


class TestFSGH100CanFitSKU(unittest.TestCase):
    def test_CanFitSKU1(self):
        sku = SkuConfig("example_sku_name", 42, 
            {
                "tpc": FSConfig(62, 62, 6, -1, 0, 0, [1], [9]),
                "gpc": FSConfig(7, 8, -1, -1, 0, 0, [], []),
                "l2slice": FSConfig(94, 94, -1, -1, 0, 0, [], [])
            }
        )

        gh100 = IsFSValidCallStruct("gh100")
        gh100.setDisableGPC(1)
        for i in range(0, 9) : gh100.setDisableTPC(1, i)
        for i in range(0, 3) : gh100.setDisableCPC(1, i)
        gh100.setDisableL2Slice(0, 0)
        gh100.setDisableL2Slice(3, 4)

        self.assertTrue(gh100.canFitSKU(sku), msg=gh100.getErrMsg())


class TestFSGH100FuncDownBin(unittest.TestCase):
    def test_funcDownBinSlicePair(self):
        gh100 = IsFSValidCallStruct("gh100")
        gh100.setDisableL2Slice(0, 1)
        self.assertFalse(gh100.isFSValid())

        fixed_gh100 = gh100.funcDownBin()
        self.assertTrue(gh100.getL2SliceEnable(3, 5))
        self.assertFalse(fixed_gh100.getL2SliceEnable(3, 5))
        self.assertFalse(fixed_gh100.getL2SliceEnable(0, 1))
        self.assertTrue(fixed_gh100.isFSValid())

    def test_funcDownBinCPCTPC(self):
        gh100 = IsFSValidCallStruct("gh100")
        for i in range(0, 3) : gh100.setDisableTPC(1, i)

        self.assertFalse(gh100.isFSValid())

        fixed_gh100 = gh100.funcDownBin()
        self.assertTrue(gh100.getCPCEnable(1, 0))
        self.assertFalse(fixed_gh100.getCPCEnable(1, 0))
        self.assertTrue(fixed_gh100.isFSValid())

    def test_funcDownBinTPCCPC(self):
        gh100 = IsFSValidCallStruct("gh100")
        gh100.setDisableCPC(1, 1)

        self.assertFalse(gh100.isFSValid())

        fixed_gh100 = gh100.funcDownBin()
        self.assertTrue(gh100.getTPCEnable(1, 3))
        self.assertTrue(gh100.getTPCEnable(1, 4))
        self.assertTrue(gh100.getTPCEnable(1, 5))

        self.assertFalse(fixed_gh100.getTPCEnable(1, 3))
        self.assertFalse(fixed_gh100.getTPCEnable(1, 4))
        self.assertFalse(fixed_gh100.getTPCEnable(1, 5))

        self.assertTrue(fixed_gh100.isFSValid())

    def test_DownBinGPC(self):
        gh100 = IsFSValidCallStruct("gh100")

        gh100.setDisableTPC(1, 1)

        sku = SkuConfig("example_sku_name", 42, 
            {
                "tpc": FSConfig(1, 72, -1, -1, 0, 0, [], []),
                "gpc": FSConfig(7, 7, -1, -1, 0, 0, [], []),
            }
        )

        downbinned, spares = gh100.downBin(sku)
        self.assertFalse(downbinned.getGPCEnable(1))
        self.assertTrue(downbinned.getGPCEnable(2))

    def test_DownBinTPC(self):
        gh100 = IsFSValidCallStruct("gh100")

        # disable 1 tpc in every gpc except 7
        for i in range(0, 7) : gh100.setDisableTPC(i, 1)

        #disable 2 more tpcs in gpc3
        gh100.setDisableTPC(3, 4)
        gh100.setDisableTPC(3, 5)

        sku = SkuConfig("example_sku_name", 42, 
            {
                "tpc": FSConfig(62, 62, 1, -1, 0, 0, [], []),
                "gpc": FSConfig(8, 8, -1, -1, 0, 0, [], []),
            }
        )

        downbinned, spares = gh100.downBin(sku)
        self.assertTrue(      gh100.getTPCEnable(7,0))
        self.assertFalse(downbinned.getTPCEnable(7,0))
        self.assertTrue(downbinned.getTPCEnable(7,1))

    def test_skyline_1(self):
        gh100 = IsFSValidCallStruct("gh100")

        # disable 1 tpc in every gpc except 7
        for i in range(0, 7) : gh100.setDisableTPC(i, 1)

        #disable 2 more tpcs in gpc3
        gh100.setDisableTPC(3, 4)
        gh100.setDisableTPC(3, 5)

        sku = SkuConfig("example_sku_name", 42, 
            {
                "tpc": FSConfig(62, 62, 1, -1, 0, 0, [], [], 10, [6,6,7,7,8,8,8]),
                "gpc": FSConfig(8, 8, -1, -1, 0, 0, [], []),
            }
        )

        downbinned, spares = gh100.downBin(sku)
        self.assertTrue(gh100.canFitSKU(sku), msg=gh100.getErrMsg())

    def test_skyline_2(self):
        gh100 = IsFSValidCallStruct("gh100")

        # disable all the gpcs except 2 in GPC 1 and 2
        for i in range(0, 5) : gh100.setDisableTPC(1, i)
        for i in range(0, 5) : gh100.setDisableTPC(2, i)

        #disable 2 more tpcs in gpc3
        gh100.setDisableTPC(3, 4)
        gh100.setDisableTPC(3, 5)

        sku = SkuConfig("example_sku_name", 42, 
            {
                "tpc": FSConfig(62, 62, 1, -1, 0, 0, [], [], 10, [6,6,7,7,8,8,8]),
                "gpc": FSConfig(8, 8),
            }
        )

        downbinned, spares = gh100.downBin(sku)
        self.assertFalse(gh100.canFitSKU(sku))


    # this test is a copy of Test_gh100_downBin_repair_skyline_sanity_1 from the cpp unit tests
    # that test has more documentation
    def test_downbin_skyline_repair_1(self):
        gh100 = IsFSValidCallStruct("gh100")

        sku = SkuConfig("example_sku_name", 42, 
            {
                "tpc": FSConfig(62, 62, 1, -1, 0, 2, [], [], 10, [6,7,8,8,9,9,9]),
                "gpc": FSConfig(7,8),
            }
        )

        downbinned, spares = gh100.downBin(sku)

        expected_spares = IsFSValidCallStruct("gh100")
        expected_spares.setDisableTPC(3, 0)
        expected_spares.setDisableTPC(4, 0)

        self.assertTrue(spares.getTPCEnable(2, 0))
        self.assertFalse(spares.getTPCEnable(3, 0))
        self.assertFalse(spares.getTPCEnable(4, 0))

if __name__ == '__main__':
    

    unittest.main()
