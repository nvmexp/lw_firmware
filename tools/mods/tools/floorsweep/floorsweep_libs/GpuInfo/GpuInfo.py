# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import itertools
import json
import os
import re
import math
from functools import reduce

from floorsweep_libs import Datalogger
from floorsweep_libs import Utilities
from floorsweep_libs import Exceptions
from floorsweep_libs.TestInfo import TestInfo
from floorsweep_libs.SkuConfig import SkuConfig

class GpuInfo(object):
    '''
    Object used to store chip and sku information
    '''
    def __init__(self, fsinfo, chip, pci_location, gpu_index, sku, skuinfo):
        self.chip = chip
        self.pci_location = pci_location
        self.gpu_index = gpu_index
        self.sku = sku

        self.only_gpc = False
        self.only_fbp = False
        self.only_ltc = False

        # Store the initial masks from ATE separately
        self._ate_fsinfo = fsinfo
        # Stores the current bad components found by the script
        self._fsinfo = fsinfo.Clear()
        # Stores a stack of pending fsinfo objects
        self._fs_stack = []

        self._sku_gpc_config   = None
        self._sku_tpc_config   = None
        self._sku_fbp_config   = None
        self._sku_ropl2_config = None
        self.ParseSkuInfo(skuinfo)

    # Manipulate FsInfo Stack
    def PushFsInfo(self, fsinfo):
        fsinfo.PrintDisableInfo()
        self._fs_stack.append(fsinfo)
        self.GetFsInfo().CheckSanity()

    def PushFsInfoTpc(self, fsinfo):
        tpcinfo = fsinfo.CloneTpc()
        tpcinfo.PrintDisableInfo()
        self._fs_stack.append(tpcinfo)
        self.GetFsInfo().CheckSanity()

    def PushFsInfoFb(self, fsinfo):
        fbinfo = fsinfo.CloneFB()
        fbinfo.PrintDisableInfo()
        self._fs_stack.append(fbinfo)
        self.GetFsInfo().CheckSanity()

    def PopFsInfo(self):
        self._fs_stack.pop()

    def GetAteFsInfo(self):
        ateinfo = self._ate_fsinfo.Clone()

        if self.only_gpc:
            ateinfo = ateinfo.SetOnlyGpc()
        if self.only_fbp:
            ateinfo = ateinfo.SetOnlyFbp()
        if self.only_ltc:
            ateinfo = ateinfo.SetOnlyLtc()

        return ateinfo

    def GetFsInfo(self):
        return reduce(lambda x,y: x|y, self._fs_stack, self._fsinfo) | self.GetAteFsInfo()

    def ClearFsStack(self):
        self._fs_stack = []

    def ApplyFsStack(self):
        self._fsinfo = self.GetFsInfo()
        self.ClearFsStack()

    # Check for SKU matches
    def CheckSkuTpc(self):
        if not self.sku:
            return -1
        fsinfo = self.GetFsInfo()
        gpc = self._sku_gpc_config.CheckConfig(fsinfo.gpc_disable_mask)
        if gpc > 0:
            raise Exceptions.IncorrectGpc()
        tpc = self._sku_tpc_config.CheckConfig(fsinfo.tpc_disable_masks)
        if tpc > 0:
            raise Exceptions.IncorrectTpc()
        return min(gpc,tpc)

    def CheckSkuFB(self):
        if not self.sku:
            return -1
        fsinfo = self.GetFsInfo()
        fbp = self._sku_fbp_config.CheckConfig(fsinfo.fbp_disable_mask)
        if fbp > 0:
            raise Exceptions.IncorrectFB()
        ropl2 = self._sku_ropl2_config.CheckConfig(fsinfo.ropl2_disable_masks)
        if ropl2 > 0:
            raise Exceptions.IncorrectRop()
        return min(fbp,ropl2)

    def ParseSkuInfo(self, skuinfo):
        if skuinfo:
            try:
                fsinfo = self.GetFsInfo()
                self._sku_gpc_config   = SkuConfig(skuinfo['GPC_CONFIG'])
                self._sku_tpc_config   = SkuConfig(skuinfo['TPC_CONFIG'],fsinfo.tpc_per_gpc,fsinfo.MAX_TPC)
                self._sku_fbp_config   = SkuConfig(skuinfo['FBP_CONFIG'])
                self._sku_ropl2_config = SkuConfig(skuinfo['ROP_L2_CONFIG'],fsinfo.ropl2_per_fbp,fsinfo.MAX_ROP)
            except Exceptions.FloorsweepException:
                raise
            except Exception as e:
                raise Exceptions.SoftwareError("Malformed skuinfo ({!s})".format(e))

    def Print(self):
        Datalogger.PrintLog("GPU INFO : {}".format(self.chip))
        if self.sku:
            Datalogger.PrintLog("SKU INFO : {}".format(self.sku))
            self.PrintSkuInfo()
        self.GetFsInfo().Print()

    def PrintSkuInfo(self):
        Datalogger.PrintLog("... SKU_FBP_DISABLE_CONFIG    : {!s}".format(self._sku_fbp_config))
        Datalogger.PrintLog("... SKU_ROP_L2_DISABLE_CONFIG : {!s}".format(self._sku_ropl2_config))
        Datalogger.PrintLog("... SKU_GPC_DISABLE_CONFIG    : {!s}".format(self._sku_gpc_config))
        Datalogger.PrintLog("... SKU_TPC_DISABLE_CONFIG    : {!s}".format(self._sku_tpc_config))

    def RunTestList(self, fsinfo, testlist):
        '''
        Run test list and return whichever test failed, if any
        '''
        for test in testlist:
            Exceptions.ASSERT(isinstance(test, TestInfo))
            try:
                return_code = test.Run(fsinfo)
            except (Exceptions.ModsTimeout, Exceptions.UnknownModsError, Exceptions.CommandError):
                return test
            else:
                if return_code != 0:
                    return test
            finally:
                self.ResetGpu()
        return None

    # The Actual Floorsweep Algorithms
    @Datalogger.LogFunction
    def FloorsweepFB(self, testlist, run_connectivity, only_fbp, iterate_cold_fbp, fb_max_tpc, *args):
        Exceptions.ASSERT(isinstance(testlist, list))

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
                if (run_connectivity or iterate_cold_fbp):
                    # iterate_cold_fbp needs to run connectivity regardless in order to have a baseline to compare to
                    Datalogger.PrintLog("FB Iteration : Connectivity : {}".format(fsinfo))
                    failure = self.RunTestList(tpc_config, testlist)
                    if (failure is None):
                        break

                try:
                    passed_once = False
                    if iterate_cold_fbp:
                        IterateFB = tpc_config.IterateColdFbp()
                    elif only_fbp:
                        IterateFB = tpc_config.IterateFbp()
                    else:
                        IterateFB = tpc_config.IterateRop()
                    for fbpath in IterateFB:
                        Datalogger.PrintLog("FB Iteration : {}".format(fbpath))
                        failure = self.RunTestList(fbpath, testlist)
                        passed_all_tests = (failure is None)
                        if not passed_all_tests and not iterate_cold_fbp:
                            self.PushFsInfoFb(fbpath.IlwertFB())
                            try:
                                if self.CheckSkuFB() == 0 and passed_once:
                                    # If we haven't passed at least once yet, then this could actually be a bad TPC
                                    # If we have passed at least once, then the TPC is fine, and we have a valid configuration
                                    Datalogger.PrintLog("Chip matches SKU FB. Exiting FB test.")
                                    raise Exceptions.SkuMatchEarlyExit()
                            except (Exceptions.IncorrectFB, Exceptions.IncorrectRop):
                                if passed_once:
                                    raise
                        passed_once = passed_once or passed_all_tests
                        if iterate_cold_fbp and passed_all_tests:
                            # Because this configuration passed all the tests, but connectivity failed, the FBP that is lwrrently disabled must be bad
                            self.PushFsInfoFb(fbpath)
                            try:
                                if self.CheckSkuFB() == 0:
                                    Datalogger.PrintLog("Chip matches SKU FB. Exiting FB test.")
                                    raise Exceptions.SkuMatchEarlyExit()
                            except (Exceptions.IncorrectFB, Exceptions.IncorrectRop):
                                raise
                    if not passed_once and iterate_cold_fbp:
                        raise Exceptions.FsSanityFailed("More than one FB configuration is bad or there is a bad TPC.")
                except Exceptions.FsSanityFailed:
                    if fb_max_tpc:
                        raise
                    # This tpc must (hopefully) be bad instead
                    gpc_idx = int(math.log(tpc_config.gpc_enable_mask,2))
                    Datalogger.PrintLog("Disabling GPC_TPC {}:{:#x}".format(gpc_idx, tpc_config.gpc_tpc_enable_mask(gpc_idx)))
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
                    failure = self.RunTestList(tpc_config, testlist)
                    passed_all_tests = (failure is None)
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

    @Datalogger.LogFunction
    def SmidShmoo(self, fsinfo, test, interval_size = 90, offset_max = 0, offset_min = -360):
        '''
        Helper function that is called when there are multiple SMID failures detected.
        The goal is to iteratively decrease the gpc clock in order to potentially narrow down
        the SMID failures to fewer TPCs and ideally just a single bad TPC.

        The general algorithm works by decreasing the gpc clock by large steps at first. This tries
        to find the range within the test goes from failing with multiple SMIDs to instead only failing
        with one SMID or passing altogether. If it finds the one bad TPC, then it will just return it.
        Otherwise it will relwrse with a smaller interval to iterate within that range to try to find
        a single bad SMID. If while iterating the smaller interval the test goes from multiple bad SMIDs
        to completely passing, or if it goes all the way to the minimunm offset and still fails with multiple
        bad SMIDs, then just return those SMIDs.
        '''
        smid_failures = test.smid_failures

        for offset in range(offset_max - interval_size, offset_min - interval_size, -interval_size):
            Datalogger.PrintLog("SmidShmoo Iteration: {} Mhz".format(offset))
            test.smid_failures = None
            try:
                return_code = test.Run(fsinfo, "-freq_clk_domain_offset_khz gpc {}".format(offset * 1000))
            except (Exceptions.ModsTimeout, Exceptions.UnknownModsError, Exceptions.CommandError):
                pass
            else:
                if return_code != 0 and test.smid_failures:
                    Datalogger.PrintLog("Found Failing SMID Masks {}".format(["{:#x}".format(mask) for mask in test.smid_failures.tpc_disable_masks]))
                    if Utilities.CountBits(test.smid_failures.tpc_disable_masks) == 1:
                        return test.smid_failures
                    else:
                        smid_failures = test.smid_failures
                        continue
            finally:
                self.ResetGpu()

            fine_interval_size = 15
            if interval_size == fine_interval_size:
                return smid_failures
            else:
                test.smid_failures = smid_failures
                return self.SmidShmoo(fsinfo, test, fine_interval_size, offset + interval_size, offset)

        return smid_failures

    @Datalogger.LogFunction
    def FloorsweepTpc(self, testlist, only_gpc, iterate_cold_gpc, iterate_tpc, iterate_cold_tpc, smid_shmoo, tpc_sriov):
        def _FloorsweepTpcRelwrse(fsinfo, testlist):
            '''
            Relwrsive helper function. Return True if a component was disabled
            '''
            if not fsinfo:
                return False

            Datalogger.PrintLog("TPC Iteration: {}".format(fsinfo))
            failed_test = self.RunTestList(fsinfo, testlist)
            if failed_test is not None:
                if failed_test.smid_failures:
                    if smid_shmoo and Utilities.CountBits(failed_test.smid_failures.tpc_disable_masks) > 1:
                        Datalogger.PrintLog("Found Failing SMID Masks {}".format(["{:#x}".format(mask) for mask in failed_test.smid_failures.tpc_disable_masks]))
                        smid_failures = self.SmidShmoo(fsinfo, failed_test)
                    else:
                        smid_failures = failed_test.smid_failures
                    self.PushFsInfoTpc(smid_failures)
                    if self.CheckSkuTpc() == 0:
                        Datalogger.PrintLog("Chip matches SKU TPC. Exiting TPC test.")
                        raise Exceptions.SkuMatchEarlyExit()
                    # Retry this stage with these TPCs disabled. Other, non-smid tests might catch more failures.
                    _FloorsweepTpcRelwrse(fsinfo | smid_failures, testlist)
                    return True

                if failed_test.ecc_failures:
                    failures = failed_test.ecc_failures
                    Datalogger.PrintLog("Found Failing ECC Masks")
                    self.PushFsInfo(failures)
                    if self.CheckSkuTpc() == 0:
                        Datalogger.PrintLog("Chip matches SKU TPC. Exiting TPC test.")
                        raise Exceptions.SkuMatchEarlyExit()
                    _FloorsweepTpcRelwrse(fsinfo | failures, testlist)
                    return True

                if fsinfo.IsSingleGpc():
                    if only_gpc:
                        self.PushFsInfoTpc(fsinfo.IlwertTpc())
                        if self.CheckSkuTpc() == 0:
                            Datalogger.PrintLog("Chip matches SKU TPC. Exiting TPC test.")
                            raise Exceptions.SkuMatchEarlyExit()
                        return True
                    elif self.chip == "GH100" and not fsinfo.IsSingleCpc():
                        # GH100 CPC2 WAR
                        # We cannot disable CPC0 and CPC1 at the same time, so we cannot test CPC2 by itself
                        # Test CPC0 and CP1 first, so then their results can be incorporated while test CPC2
                        return reduce(lambda x,y: x or y, [_FloorsweepTpcRelwrse(cpc_config, testlist) for cpc_config in fsinfo.IterateCpc()], False)

                if fsinfo.IsSingleTpc():
                    self.PushFsInfoTpc(fsinfo.IlwertTpc())
                    if self.CheckSkuTpc() == 0:
                        Datalogger.PrintLog("Chip matches SKU TPC. Exiting TPC test.")
                        raise Exceptions.SkuMatchEarlyExit()
                    return True

                left,right = fsinfo.SplitTpc()

                left_disable = _FloorsweepTpcRelwrse(left, testlist)
                right_disable = _FloorsweepTpcRelwrse(right, testlist)
                if not left_disable and not right_disable:
                    raise Exceptions.FsSanityFailed("{} failed but both {} and {} passed".format(fsinfo, left, right))

                return left_disable or right_disable
            return False

        def _FloorsweepTpcIterate(fsinfo, testlist, iterater):
            iterate_cold = iterate_cold_gpc or iterate_cold_tpc
            passed_once = False
            for tpcinfo in iterater:
                Datalogger.PrintLog("TPC Iteration : {}".format(tpcinfo))
                failure = self.RunTestList(tpcinfo, testlist)
                passed_all_tests = (failure is None)
                if not passed_all_tests and not iterate_cold:
                    self.PushFsInfoTpc(tpcinfo.IlwertTpc())
                    if self.CheckSkuTpc() == 0:
                        Datalogger.PrintLog("Chip matches SKU TPC. Exiting TPC test.")
                        raise Exceptions.SkuMatchEarlyExit()
                passed_once = passed_once or passed_all_tests
                if iterate_cold and passed_all_tests:
                    disinfo = (tpcinfo.XorTpc(fsinfo)).IlwertTpc()
                    gpc_idx = int(math.log(disinfo.gpc_enable_mask,2))
                    if not disinfo.IsSingleTpc() and iterate_cold_tpc:
                        # If this was testing cold GPC and cold TPC is requested, drill down to test the TPCs in this GPC
                        return _FloorsweepTpcIterate(fsinfo, testlist, fsinfo.IterateColdTpc(gpc_idx))
                    else:
                        self.PushFsInfoTpc(tpcinfo)
                        if self.CheckSkuTpc() == 0:
                            Datalogger.PrintLog("Chip matches SKU TPC. Exiting TPC test.")
                            raise Exceptions.SkuMatchEarlyExit()
                        return
            if not passed_once and iterate_cold:
                raise Exceptions.FsSanityFailed("More than one GPC/TPC configuration is bad.")

        def _RunConnectivity(fsinfo, testlist):
            '''
            Run one iteration of each test with everything enabled. Return whether it passed or not.
            '''
            Datalogger.PrintLog("TPC Iteration : Connectivity : {}".format(fsinfo))
            failed_test = self.RunTestList(fsinfo, testlist)
            if failed_test is not None:
                if failed_test.smid_failures:
                    if smid_shmoo and Utilities.CountBits(failed_test.smid_failures.tpc_disable_masks) > 1:
                        Datalogger.PrintLog("Found Failing SMID Masks {}".format(["{:#x}".format(mask) for mask in failed_test.smid_failures.tpc_disable_masks]))
                        smid_failures = self.SmidShmoo(fsinfo, failed_test)
                    else:
                        smid_failures = failed_test.smid_failures
                    self.PushFsInfoTpc(smid_failures)
                    if self.CheckSkuTpc() == 0:
                        Datalogger.PrintLog("Chip matches SKU TPC. Exiting TPC test.")
                        raise Exceptions.SkuMatchEarlyExit()
                    # Retry this stage with these TPCs disabled. Other, non-smid tests might catch more failures.
                    return _RunConnectivity(fsinfo | smid_failures, testlist)

                if failed_test.ecc_failures:
                    ecc_failures = failed_test.ecc_failures
                    Datalogger.PrintLog("Found Failing ECC Masks")
                    self.PushFsInfo(ecc_failures)
                    if self.CheckSkuTpc() == 0:
                        Datalogger.PrintLog("Chip matches SKU TPC. Exiting TPC test.")
                        raise Exceptions.SkuMatchEarlyExit()
                    return _RunConnectivity(fsinfo | ecc_failures, testlist)

                return False
            return True

        if self.CheckSkuTpc() == 0:
            Datalogger.PrintLog("Chip matches SKU TPC. Nothing to do.")
            return

        self.ClearFsStack()
        Datalogger.PrintLog("OLD FUSE INFO :")
        self.GetFsInfo().PrintFuseInfo()

        self.ResetGpu()

        try:
            if not iterate_tpc and not iterate_cold_gpc and not iterate_cold_tpc:
                _FloorsweepTpcRelwrse(self.GetFsInfo(), testlist)
            else:
                if iterate_tpc:
                    Iterate = self.GetFsInfo().IterateTpc()
                elif iterate_cold_gpc:
                    Iterate = self.GetFsInfo().IterateColdGpc()
                else: #iterate_cold_tpc
                    Iterate = self.GetFsInfo().IterateColdTpc()
                if iterate_tpc or ((iterate_cold_gpc or iterate_cold_tpc) and not _RunConnectivity(self.GetFsInfo(), testlist)):
                    _FloorsweepTpcIterate(self.GetFsInfo(), testlist, Iterate)
        except Exceptions.SkuMatchEarlyExit:
            # Make sure there aren't any additional bad components outside of the SKU
            Datalogger.PrintLog("TPC Iteration : SkuMatchVerify : {}".format(self.GetFsInfo()))
            failure = self.RunTestList(self.GetFsInfo(), testlist)
            if failure is None:
                self.ApplyFsStack()
                raise Exceptions.CannotMeetFsRequirements("Failed to verify SKU match. Some other component is faulty.")
        except:
            self.ClearFsStack()
            raise

        self.ApplyFsStack()

        Datalogger.PrintLog("NEW FUSE INFO :")
        fsinfo = self.GetFsInfo()
        fsinfo.PrintFuseInfo()
        fsinfo.LogFuseInfo()

    @Datalogger.LogFunction
    def FloorsweepLwlink(self, testlist):
        raise Exceptions.UnsupportedFunction("This chip doesn't support lwlink floorsweeping")

    @Datalogger.LogFunction
    def KappaBin(self, testlist):
        raise Exceptions.UnsupportedFunction("This chip doesn't support kappa binning")

    @Datalogger.LogFunction
    def FloorsweepSyspipe(self, testlist):
        raise Exceptions.UnsupportedFunction("This chip doesn't support syspipe floorsweeping")

    @Datalogger.LogFunction
    def FloorsweepCopyEngines(self, testlist):
        raise Exceptions.UnsupportedFunction("This chip doesn't support CE floorsweeping")

    @Datalogger.LogFunction
    def FloorsweepLwdec(self, testlist):
        raise Exceptions.UnsupportedFunction("This chip doesn't support Lwdec floorsweeping")

    @Datalogger.LogFunction
    def FloorsweepOfa(self, testlist):
        raise Exceptions.UnsupportedFunction("This chip doesn't support OFA floorsweeping")

    @Datalogger.LogFunction
    def FloorsweepLwjpg(self, testlist):
        raise Exceptions.UnsupportedFunction("This chip doesn't support LWJPG floorsweeping")

    def ResetGpu(self):
        if self.gpu_index == 'no_lwpex':
            return 0

        return_code = 1
        if os.path.exists('lwpex2'):
            Exceptions.ASSERT(self.pci_location, "pci_location is not set")
            return_code = Utilities.RunSubprocess('./lwpex2 -b {:x}:{:x}:{:x}.{:x} --reset'.format(*self.pci_location), 10)
            if return_code != 0:
                Datalogger.PrintLog("... failed")
        if return_code != 0:
            Exceptions.ASSERT(self.gpu_index, "gpu_index is not set")
            return_code = Utilities.RunSubprocess('./lwpex hot_reset {}'.format(self.gpu_index), 10)
        if return_code != 0:
            Datalogger.PrintLog("... failed")
        return return_code

    def Final(self):
        Datalogger.PrintLog("FINAL FUSE INFO :")
        fsinfo = self.GetFsInfo()
        fsinfo.PrintFuseInfo()
        fsinfo.LogFuseInfo()
        fsinfo.WriteFbFb()
        fsinfo.WriteFsArg()

        # Defective fuses should only include what floorsweep.egg itself found as bad
        ate_fsinfo = self.GetAteFsInfo()
        defective_fsinfo = fsinfo.SetOverrideConsistent() ^ ate_fsinfo
        defective_fsinfo.PrintDefectiveFuseInfo()
        defective_fsinfo.LogDefectiveFuseInfo()
