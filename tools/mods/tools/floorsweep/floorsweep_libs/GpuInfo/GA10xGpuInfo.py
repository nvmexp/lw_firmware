# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

from floorsweep_libs import Datalogger
from floorsweep_libs import Exceptions
from floorsweep_libs import FsInfo
from floorsweep_libs.TestInfo import TestInfo
from floorsweep_libs.SkuConfig import SkuConfig
from .GP10xGpuInfo import GP10xGpuInfo

import math

class GA10xGpuInfo(GP10xGpuInfo):
    def __init__(self, *args, **kwargs):
        self._sku_rop_config   = None
        self._sku_ltc_config   = None
        self._sku_slice_config = None
        super(GA10xGpuInfo,self).__init__(*args, **kwargs)

    def GetAteFsInfo(self):
        return super(GA10xGpuInfo,self).GetAteFsInfo().SetOverrideEqualSlices()

    # Check for SKU matches
    def CheckSkuTpc(self):
        if not self.sku:
            return -1
        sup = super(GA10xGpuInfo,self).CheckSkuTpc()

        fsinfo = self.GetFsInfo()
        rop = self._sku_rop_config.CheckConfig(fsinfo.rop_disable_masks)
        if rop > 0:
            raise Exceptions.IncorrectRop()
        return min(sup, rop)

    def CheckSkuFB(self):
        if not self.sku:
            return -1
        fsinfo = self.GetFsInfo()
        fbp = self._sku_fbp_config.CheckConfig(fsinfo.fbp_disable_mask)
        if fbp > 0:
            raise Exceptions.IncorrectFB()
        ltc = self._sku_ltc_config.CheckConfig(fsinfo.ltc_disable_masks)
        if ltc > 0:
            raise Exceptions.IncorrectLtc()
        slice = self._sku_slice_config.CheckConfig(fsinfo.slice_disable_masks)
        if slice > 0:
            raise Exceptions.IncorrectL2Slices()
        return min(fbp,ltc,slice)

    def ParseSkuInfo(self, skuinfo):
        if skuinfo:
            try:
                fsinfo = self.GetFsInfo()
                self._sku_gpc_config   = SkuConfig(skuinfo['GPC_CONFIG'])
                self._sku_pes_config   = SkuConfig(skuinfo['PES_CONFIG'],fsinfo.pes_per_gpc,fsinfo.MAX_PES)
                self._sku_tpc_config   = SkuConfig(skuinfo['TPC_CONFIG'],fsinfo.tpc_per_gpc,fsinfo.MAX_TPC)
                self._sku_rop_config   = SkuConfig(skuinfo['ROP_CONFIG'],fsinfo.rop_per_gpc,fsinfo.MAX_ROP)
                self._sku_fbp_config   = SkuConfig(skuinfo['FBP_CONFIG'])
                self._sku_ltc_config   = SkuConfig(skuinfo['LTC_CONFIG'],fsinfo.ltc_per_fbp,fsinfo.MAX_LTC)
                self._sku_slice_config = SkuConfig(skuinfo['L2SLICE_CONFIG'],fsinfo.slice_per_fbp,fsinfo.MAX_SLICE)
            except Exceptions.FloorsweepException:
                raise
            except Exception as e:
                raise Exceptions.SoftwareError("Malformed skuinfo ({!s})".format(e))

    def PrintSkuInfo(self):
        Datalogger.PrintLog("... SKU_FBP_DISABLE_CONFIG     : {!s}".format(self._sku_fbp_config))
        Datalogger.PrintLog("... SKU_LTC_DISABLE_CONFIG     : {!s}".format(self._sku_ltc_config))
        Datalogger.PrintLog("... SKU_L2SLICE_DISABLE_CONFIG : {!s}".format(self._sku_slice_config))
        Datalogger.PrintLog("... SKU_GPC_DISABLE_CONFIG     : {!s}".format(self._sku_gpc_config))
        Datalogger.PrintLog("... SKU_PES_DISABLE_CONFIG     : {!s}".format(self._sku_pes_config))
        Datalogger.PrintLog("... SKU_TPC_DISABLE_CONFIG     : {!s}".format(self._sku_tpc_config))
        Datalogger.PrintLog("... SKU_ROP_DISABLE_CONFIG     : {!s}".format(self._sku_rop_config))

    @Datalogger.LogFunction
    def FloorsweepFB(self, testlist, run_connectivity, only_fbp, iterate_cold_fbp, fb_max_tpc, only_ltc, *args):
        Exceptions.ASSERT(isinstance(testlist, list))
        def _RunConnectivity(fsinfo, testlist):
            '''
            Run one iteration of each test with everything enabled. Return whether it passed or not.
            '''
            Datalogger.PrintLog("FB Iteration : Connectivity : {}".format(fsinfo))
            for test in testlist:
                Exceptions.ASSERT(isinstance(test, TestInfo))
                try:
                    return_code = test.Run(fsinfo)
                except (Exceptions.ModsTimeout, Exceptions.UnknownModsError, Exceptions.CommandError):
                    return False
                else:
                    if return_code != 0:
                        return False
                finally:
                    self.ResetGpu()

            return True

        def _IterateSlices(fsinfo, testlist):
            for fbpath in fsinfo.IterateColdSlice():
                Datalogger.PrintLog("FB Iteration : {}".format(fbpath))
                passed_all_tests = True
                for test in testlist:
                    Exceptions.ASSERT(isinstance(test, TestInfo))
                    try:
                        return_code = test.Run(fbpath)
                    except (Exceptions.ModsTimeout, Exceptions.UnknownModsError, Exceptions.CommandError):
                        pass
                    else:
                        if return_code == 0:
                            continue
                    finally:
                        self.ResetGpu()

                    passed_all_tests = False
                    break
                if passed_all_tests:
                    # Because this configuration passed all the tests, but connectivity failed, the slice that is lwrrently disabled must be bad
                    self.PushFsInfoFb(fbpath.ExpandColdSlice())
                    try:
                        if self.CheckSkuFB() == 0:
                            Datalogger.PrintLog("Chip matches SKU FB. Exiting FB test.")
                            raise Exceptions.SkuMatchEarlyExit()
                    except (Exceptions.IncorrectFB, Exceptions.IncorrectLtc):
                        raise
                    return
            # If it reached here, all the slice combinations failed, so just disable the whole LTC
            self.PushFsInfoFb(fsinfo.IlwertLtc())

        def _IterateLtc(fsinfo, testlist):
            for fbpath in fsinfo.IterateLtc():
                _IterateSlices(fbpath, testlist)

        if self.CheckSkuFB() == 0:
            Datalogger.PrintLog("Chip matches SKU FB. Nothing to do.")
            return

        self.ClearFsStack()
        Datalogger.PrintLog("OLD FUSE INFO :")
        self.GetFsInfo().PrintFuseInfo()

        self.ResetGpu()

        try:
            if fb_max_tpc:
                IterateTPC = [self.GetFsInfo()]
            else:
                IterateTPC = self.GetFsInfo().IterateTpcBreadthFirst()
            for tpc_config in IterateTPC:
                if ((run_connectivity or iterate_cold_fbp)
                        and _RunConnectivity(tpc_config, testlist)):
                    # iterate_cold_fbp needs to run connectivity regardless in order to have a baseline to compare to
                    break
                else:
                    try:
                        passed_once = False
                        if iterate_cold_fbp:
                            IterateFB = tpc_config.IterateColdFbp()
                        elif only_fbp:
                            IterateFB = tpc_config.IterateFbp()
                        else:
                            IterateFB = tpc_config.IterateLtc()
                        for fbpath in IterateFB:
                            Datalogger.PrintLog("FB Iteration : {}".format(fbpath))
                            passed_all_tests = True
                            for test in testlist:
                                Exceptions.ASSERT(isinstance(test, TestInfo))
                                try:
                                    return_code = test.Run(fbpath)
                                except (Exceptions.ModsTimeout, Exceptions.UnknownModsError, Exceptions.CommandError):
                                    pass
                                else:
                                    if return_code == 0:
                                        continue
                                finally:
                                    self.ResetGpu()

                                passed_all_tests = False
                                if not iterate_cold_fbp:
                                    if only_fbp or only_ltc:
                                        self.PushFsInfoFb(fbpath.IlwertLtc())
                                    else:
                                        _IterateLtc(fbpath, testlist) # drill down into the LTC/L2Slices to find failures
                                    try:
                                        if self.CheckSkuFB() == 0 and passed_once:
                                            # If we haven't passed at least once yet, then this could actually be a bad TPC
                                            # If we have passed at least once, then the TPC is fine, and we have a valid configuration
                                            Datalogger.PrintLog("Chip matches SKU FB. Exiting FB test.")
                                            raise Exceptions.SkuMatchEarlyExit()
                                    except (Exceptions.IncorrectFB, Exceptions.IncorrectLtc):
                                        if passed_once:
                                            raise
                                break
                            passed_once = passed_once or passed_all_tests
                            if iterate_cold_fbp and passed_all_tests:
                                # Because this configuration passed all the tests, but connectivity failed, the FBP that is lwrrently disabled (or something in it) must be bad
                                self.PushFsInfoFb(fbpath)
                                try:
                                    if self.CheckSkuFB() == 0:
                                        Datalogger.PrintLog("Chip matches SKU FB. Exiting FB test.")
                                        raise Exceptions.SkuMatchEarlyExit()
                                except (Exceptions.IncorrectFB, Exceptions.IncorrectLtc):
                                    raise
                        if not passed_once and iterate_cold_fbp:
                            raise Exceptions.FsSanityFailed("More than one FB configuration is bad or there is a bad TPC.")
                    except Exceptions.FsSanityFailed:
                        if fb_max_tpc:
                            raise
                        # This tpc must (hopefully) be bad instead
                        Datalogger.PrintLog("Resetting FB masks")
                        self.ClearFsStack()
                        self.PushFsInfoTpc(tpc_config.IlwertTpc())
                        self.ApplyFsStack()
                        self.CheckSkuTpc()
                        continue
                    else:
                        if run_connectivity and not self._fs_stack:
                            # If fs_stack is empty, then nothing is being disabled, ie. everything passed
                            raise Exceptions.FsSanityFailed("Failed connectivity {}, but passed all FB configurations".format(tpc_config))
                        # We have found a valid FB configuration.
                        break
        except Exceptions.SkuMatchEarlyExit:
            if not iterate_cold_fbp:
                # Make sure there aren't any additional bad components outside of the SKU
                if fb_max_tpc:
                    IterateTPC = [self.GetFsInfo()]
                else:
                    IterateTPC = self.GetFsInfo().IterateTpcBreadthFirst()
                for tpc_config in IterateTPC:
                    Datalogger.PrintLog("FB Iteration : SkuMatchVerify : {}".format(tpc_config))
                    passed_all_tests = True
                    for test in testlist:
                        Exceptions.ASSERT(isinstance(test, TestInfo))
                        try:
                            return_code = test.Run(tpc_config)
                        except (Exceptions.ModsTimeout, Exceptions.UnknownModsError, Exceptions.CommandError):
                            pass
                        else:
                            if return_code == 0:
                                continue
                        finally:
                            self.ResetGpu()
                        passed_all_tests = False
                        break
                    if not passed_all_tests:
                        self.ApplyFsStack()
                        raise Exceptions.CannotMeetFsRequirements("Failed to verify SKU match. Some other component is faulty.")
                    break
        except:
            self.ClearFsStack()
            raise

        self.ApplyFsStack()

        Datalogger.PrintLog("NEW FUSE INFO :")
        fsinfo = self.GetFsInfo()
        fsinfo.PrintFuseInfo()
        fsinfo.LogFuseInfo()
