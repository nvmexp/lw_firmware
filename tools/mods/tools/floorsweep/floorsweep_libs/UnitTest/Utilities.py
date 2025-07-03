# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

from floorsweep_libs.TestInfo import TestInfo
from floorsweep_libs import Datalogger
from floorsweep_libs import GpuList
from floorsweep_libs import FsLib
from floorsweep_libs import Exceptions

import unittest

class TestGPU(unittest.TestCase):
    def setUp(self, gpu_name=None):
        self.gpu_name = gpu_name

        fsinfo_class,gpuinfo_class = GpuList.GetGpuClasses(self.gpu_name)
        self.fsinfo_class = fsinfo_class
        self.gpuinfo_class = gpuinfo_class

    def CreateDefectFsInfo(self, **kwargs):
        overrides = self.fsinfo_class().SetUnitTestOverrides()
        return self.fsinfo_class(clone=overrides, **kwargs)

    def CreateAteFsInfo(self, **kwargs):
        return self.fsinfo_class(**kwargs)

    def CreateGpuInfo(self, ate_fsinfo, **kwargs):
        pci_location = (0,0,0,1)
        gpu_index = "no_lwpex"
        sku = None
        skuinfo = {}

        gpuinfo = self.gpuinfo_class(ate_fsinfo, self.gpu_name, pci_location, gpu_index, sku, skuinfo)
        gpuinfo.only_gpc = kwargs["only_gpc"] if "only_gpc" in kwargs else False
        gpuinfo.only_fbp = kwargs["only_fbp"] if "only_fbp" in kwargs else False
        gpuinfo.only_ltc = kwargs["only_ltc"] if "only_ltc" in kwargs else False
        return gpuinfo

    def GetFsInfo(self, ate_fsinfo, **kwargs):
        gpuinfo = self.CreateGpuInfo(ate_fsinfo, **kwargs)
        return gpuinfo.GetFsInfo()

    def FloorsweepFB(self, ate_fsinfo, defect_fsinfo, ecc_fsinfo=None, **kwargs):
        gpuinfo = self.CreateGpuInfo(ate_fsinfo, **kwargs)


        testlist = [FakeTestInfo(defect_fsinfo, ecc_fsinfo)]
        gpuinfo.FloorsweepFB(testlist=testlist, **kwargs)

        return gpuinfo.GetFsInfo()

    def FloorsweepTpc(self, ate_fsinfo, defect_fsinfo, ecc_fsinfo=None, **kwargs):
        gpuinfo = self.CreateGpuInfo(ate_fsinfo, **kwargs)

        testlist = [FakeTestInfo(defect_fsinfo, ecc_fsinfo)]
        gpuinfo.FloorsweepTpc(testlist=testlist, **kwargs)

        return gpuinfo.GetFsInfo()

    def FloorsweepLwlink(self, ate_fsinfo, defect_fsinfo, ecc_fsinfo=None, **kwargs):
        gpuinfo = self.CreateGpuInfo(ate_fsinfo, **kwargs)

        testlist = [FakeTestInfo(defect_fsinfo, ecc_fsinfo)]
        gpuinfo.FloorsweepLwlink(testlist=testlist, **kwargs)

        return gpuinfo.GetFsInfo()

class FakeTestInfo(TestInfo):
    def __init__(self, defect_fsinfo, ecc_fsinfo):
        super(FakeTestInfo, self).__init__(999, "", 999)
        self.defect_fsinfo = defect_fsinfo
        self.ecc_fsinfo = ecc_fsinfo

    def SetFuses(self, fsinfo):
        pass

    def Run(self, fsinfo, extra_args=""):
        struct = fsinfo.CreateFsLibStruct()
        if not struct.IsFSValid():
            errmsg = "FsLib: " + struct.GetErrMsg()
            Datalogger.Enable(Datalogger.PrintLog)(errmsg)
            Datalogger.Enable(fsinfo.PrintFuseInfo)()
            raise Exceptions.SoftwareError(errmsg)

        return_code = 0

        defects = self.defect_fsinfo.Clone()
        if self.ecc_fsinfo is not None:
            defects |= self.ecc_fsinfo

        passing = ((fsinfo.SetUnitTestOverrides() & defects) == defects)

        if passing:
            Datalogger.PrintLog('... UnitTest passed')
        else:
            return_code = 1
            Datalogger.PrintLog('... UnitTest failed')

        self.smid_failures = None
        self.ecc_failures = None

        if self.ecc_fsinfo is not None:
            self.ecc_failures = (fsinfo.SetUnitTestOverrides() ^ self.ecc_fsinfo) & self.ecc_fsinfo

        return return_code

    def RunSriov(self, num_parts, partition_arg, fsinfo, vf_args=[]):
        raise Exceptions.UnsupportedFunction("UnitTest does not support sriov")

    def FindFailingSmidInfoSriov(self, fsinfo):
        raise Exceptions.UnsupportedFunction("UnitTest does not support sriov")
