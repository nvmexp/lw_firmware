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
from floorsweep_libs import FsInfo
from floorsweep_libs import Utilities
from floorsweep_libs.TestInfo import TestInfo
from floorsweep_libs.SkuConfig import SkuConfig
from .GA100GpuInfo import GA100GpuInfo

import math
from functools import reduce

class GH100GpuInfo(GA100GpuInfo):
    def __init__(self, *args, **kwargs):
        self._sku_cpc_config   = None
        self._sku_rop_config   = None
        self._sku_ltc_config   = None
        self._sku_slice_config = None

        super(GH100GpuInfo,self).__init__(*args, **kwargs)

        fsinfo = self.GetFsInfo()
        self._gpc0cpc0_fsinfo = fsinfo.ClearTpc()

        gpc_enable_mask = fsinfo.gpc_enable_mask & 0x1
        cpc_enable_masks = [0x0 for x in range(self._fsinfo.__class__.MAX_GPC)]
        cpc_enable_masks[0] = fsinfo.cpc_enable_masks[0] & 0x1
        tpc_enable_masks = [0x0 for x in range(self._fsinfo.__class__.MAX_GPC)]
        tpc_enable_masks[0] = fsinfo.tpc_enable_masks[0] & 0x7
        self._gpc0cpc0_fsinfo |= self._fsinfo.__class__(gpc_enable_mask = gpc_enable_mask,
                                                        cpc_enable_masks = cpc_enable_masks,
                                                        tpc_enable_masks = tpc_enable_masks)
        self._gpccpc_fsinfos = [self._fsinfo.__class__(tpc_enable_masks = [(fsinfo.tpc_enable_masks[x] & 0x3f) if x == gpc else 0x0 for x in range(self._fsinfo.__class__.MAX_GPC)]) for gpc in range(self._fsinfo.__class__.MAX_GPC)]
        self._gpc0cpc0_fsinfo.CheckSanity()

    def GetAteFsInfo(self):
        return super(GA100GpuInfo,self).GetAteFsInfo().SetOverrideCPC2()

    def PushFsInfoFb(self, fsinfo):
        super(GH100GpuInfo,self).PushFsInfoFb(fsinfo)
        self._gpc0cpc0_fsinfo |= fsinfo.CloneFB()

    def PushFsInfoTpc(self, fsinfo):
        super(GH100GpuInfo,self).PushFsInfoTpc(fsinfo)
        for i,gpccpc_fsinfo in enumerate(self._gpccpc_fsinfos):
            self._gpccpc_fsinfos[i] = gpccpc_fsinfo | fsinfo

    def PushFsInfoLwLink(self, fsinfo):
        lwlinkinfo = fsinfo.CloneLwlink()
        lwlinkinfo.PrintDisableInfo()
        self._fs_stack.append(lwlinkinfo)
        self.GetFsInfo().CheckSanity()

    # Check for SKU matches
    def CheckSkuTpc(self):
        self._gpc0cpc0_fsinfo.CheckSanity()

        if not self.sku:
            return -1
        fsinfo = self.GetFsInfo()

        gpc = self._sku_gpc_config.CheckConfig(fsinfo.gpc_disable_mask)
        if gpc > 0:
            raise Exceptions.IncorrectGpc()

        cpc = self._sku_cpc_config.CheckConfig(fsinfo.cpc_disable_masks)
        if cpc > 0:
            raise Exceptions.IncorrectCpc()

        tpc = self._sku_tpc_config.CheckConfig(fsinfo.tpc_disable_masks)
        if tpc > 0:
            raise Exceptions.IncorrectTpc()

        return min(gpc, cpc, tpc)

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

        return min(fbp, ltc, slice)

    def IsSliceSku(self):
        if not self.sku:
            return False
        return (self._sku_slice_config.attr != 'DONTCARE')

    def ParseSkuInfo(self, skuinfo):
        if skuinfo:
            try:
                fsinfo = self.GetFsInfo()
                self._sku_gpc_config   = SkuConfig(skuinfo['GPC_CONFIG'])
                self._sku_cpc_config   = SkuConfig(skuinfo['CPC_CONFIG'],fsinfo.cpc_per_gpc,fsinfo.MAX_CPC)
                self._sku_tpc_config   = SkuConfig(skuinfo['TPC_CONFIG'],fsinfo.tpc_per_gpc,fsinfo.MAX_TPC)
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
        Datalogger.PrintLog("... SKU_CPC_DISABLE_CONFIG     : {!s}".format(self._sku_cpc_config))
        Datalogger.PrintLog("... SKU_TPC_DISABLE_CONFIG     : {!s}".format(self._sku_tpc_config))

    def RunTestList(self, fsinfo, testlist):
        # Combine in the valid GPC0CPC0 mask to ensure it's enabled
        tpcinfo = fsinfo & self._gpc0cpc0_fsinfo.CloneTpc()

        # CPC2 cannot be tested alone. After first testing CPC0 & CPC1, add their working components while testing CPC2
        if fsinfo.IsSingleCpc():
            gpc_idx = int(math.log(fsinfo.gpc_enable_mask,2))
            cpc_idx = int(math.log(fsinfo.cpc_enable_masks[gpc_idx],2))
            if gpc_idx > 0 and cpc_idx == 2:
                if (self._gpccpc_fsinfos[gpc_idx].cpc_disable_masks[gpc_idx] & 0x3) == 0x3:
                    # CPC0 and CPC1 are already disabled, meaning we cannot test CPC2 at all. Just return failure.
                    return testlist[0]
                else:
                    tpcinfo &= self._gpccpc_fsinfos[gpc_idx]
        fsinfo = fsinfo.ClearTpc() | tpcinfo.CloneTpc()

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

    @Datalogger.LogFunction
    def FloorsweepFB(self, testlist, run_connectivity, only_fbp, iterate_cold_fbp, fb_max_tpc, only_ltc, *args):
        Exceptions.ASSERT(isinstance(testlist, list))
        def _RunConnectivity(fsinfo):
            '''
            Return False if additional testing is required
            '''
            Datalogger.PrintLog("FB Connectivity : {}".format(fsinfo))
            failure = self.RunTestList(fsinfo, testlist)
            if failure is None:
                return True
            elif failure.ecc_failures:
                failures = failure.ecc_failures
                Datalogger.PrintLog("Found Failing ECC Masks")
                self.PushFsInfo(failures)
                if self.CheckSkuFB() == 0:
                    Datalogger.PrintLog("Chip matches SKU FB. Exiting FB test.")
                    raise Exceptions.SkuMatchEarlyExit()
                return _RunConnectivity(fsinfo | failures)
            return False

        def _IterateSlices(fsinfo):
            for fbpath in fsinfo.IterateColdSlice():
                Datalogger.PrintLog("FB Iteration : {}".format(fbpath))
                failure = self.RunTestList(fbpath, testlist)
                if failure is None:
                    # Because this configuration passed all the tests, but the overall FBP failed, the slice that is lwrrently disabled must be bad
                    self.PushFsInfoFb(fbpath.ExpandColdSlice())
                    try:
                        if self.CheckSkuFB() == 0:
                            Datalogger.PrintLog("Chip matches SKU FB. Exiting FB test.")
                            raise Exceptions.SkuMatchEarlyExit()
                    except (Exceptions.IncorrectFB, Exceptions.IncorrectLtc):
                        pass
                    return True
            # If it reached here, all the slice combinations failed, so just disable the whole LTC
            self.PushFsInfoFb(fsinfo.IlwertFB())
            return False

        def _IterateLtc(fsinfo):
            passed_once = False
            fsinfo.PrintFuseInfo()
            for fbpath in fsinfo.IterateLtc():
                fbpath.PrintFuseInfo()
                passed = _IterateSlices(fbpath)
                if passed:
                    passed_once = True
            return passed_once

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
                if ((run_connectivity or iterate_cold_fbp) # iterate_cold_fbp needs to run connectivity regardless in order to have a baseline to compare to
                    and _RunConnectivity(tpc_config)):
                    break

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
                        failure = self.RunTestList(fbpath, testlist)
                        if failure is None:
                            passed_once = True
                        if failure is not None and not iterate_cold_fbp:
                            self.PushFsInfoFb(fbpath.IlwertFB())
                            try:
                                if self.CheckSkuFB() == 0 and passed_once:
                                    # If we haven't passed at least once yet, then this could actually be a bad TPC
                                    # If we have passed at least once, then the TPC is fine, and we have a valid configuration
                                    Datalogger.PrintLog("Chip matches SKU FB. Exiting FB test.")
                                    raise Exceptions.SkuMatchEarlyExit()
                            except (Exceptions.IncorrectFB, Exceptions.IncorrectLtc):
                                if passed_once:
                                    raise
                        elif failure is None and iterate_cold_fbp:
                            # Because this configuration passed all the tests, but connectivity failed, the FBP that is lwrrently disabled must be bad
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
                    self._gpc0cpc0_fsinfo |= tpc_config.IlwertTpc()
                    self.ApplyFsStack()
                    self.CheckSkuTpc()
                    continue
                else:
                    if run_connectivity and not self._fs_stack:
                        # If fs_stack is empty, then nothing is being disabled, ie. everything passed
                        raise Exceptions.FsSanityFailed("Failed connectivity {}, but passed all FB configurations".format(tpc_config))
                    # We have found a valid FB configuration.

                    fbp_dis_count = bin(self.GetFsInfo().fbp_disable_mask).count('1')
                    ltc_dis_count = reduce(lambda x,y: x+y, [bin(ltc_mask).count('1') for ltc_mask in self.GetFsInfo().ltc_disable_masks])
                    if self.IsSliceSku() and (fbp_dis_count > 2 or (fbp_dis_count == 0 and ltc_dis_count > 2)):
                        # Too many FBPs are disabled to potentially drill down to slices
                        raise Exceptions.IncorrectL2Slices()
                    if only_fbp or only_ltc or iterate_cold_fbp:
                        break
                    if fbp_dis_count == 2 or (fbp_dis_count == 0 and ltc_dis_count == 2):
                        # Try to drill down any FBP failures to particular slices instead
                        fsinfo = self.GetFsInfo()
                        fsEnable = fsinfo.IlwertFB()
                        for sliceInfo in fsEnable.IterateColdSlice():
                            fbpath = sliceInfo.CloneFB() | tpc_config.CloneTpc()
                            Datalogger.PrintLog("FB Iteration : {}".format(fbpath))
                            failure = self.RunTestList(fbpath, testlist)
                            if failure is None:
                                # Because this configuration passed all the tests, but the overall FBP failed, the slice that is lwrrently disabled must be bad
                                Datalogger.PrintLog("Resetting FB masks")
                                self.ClearFsStack()
                                self.PushFsInfoFb(fbpath.ExpandColdSlice())
                                try:
                                    if self.CheckSkuFB() == 0:
                                        Datalogger.PrintLog("Chip matches SKU FB. Exiting FB test.")
                                        raise Exceptions.SkuMatchEarlyExit()
                                except (Exceptions.IncorrectFB, Exceptions.IncorrectLtc):
                                    raise
                                break
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

    @Datalogger.LogFunction
    def FloorsweepTpc(self, testlist, only_gpc, iterate_cold_gpc, iterate_tpc, iterate_cold_tpc, smid_shmoo, tpc_sriov):
        if self.CheckSkuTpc() == 0:
            Datalogger.PrintLog("Chip matches SKU TPC. Nothing to do.")
            return

        def _RunSanity():
            # Verify that there is at least one good TPC within GPC0CPC0
            Datalogger.PrintLog("GPC0CPC0 Sanity Iteration : {}".format(self._gpc0cpc0_fsinfo))
            failed_test = self.RunTestList(self._gpc0cpc0_fsinfo, testlist)
            if failed_test is not None:
                if failed_test.smid_failures:
                    if smid_shmoo and Utilities.CountBits(failed_test.smid_failures.tpc_disable_masks) > 1:
                        Datalogger.PrintLog("Found Failing SMID Masks {}".format(["{:#x}".format(mask) for mask in failed_test.smid_failures.tpc_disable_masks]))
                        smid_failures = self.SmidShmoo(fsinfo, failed_test)
                    else:
                        Datalogger.PrintLog("Found Failing SMID Masks")
                        smid_failures = failed_test.smid_failures
                    self.PushFsInfoTpc(smid_failures)
                    self._gpc0cpc0_fsinfo |= smid_failures.CloneTpc()
                    self.CheckSkuTpc()
                    # Re-run with the updated mask
                    _RunSanity()
                    return

                if failed_test.ecc_failures:
                    ecc_failures = failed_test.ecc_failures
                    Datalogger.PrintLog("Found Failing ECC Masks")
                    self.PushFsInfo(ecc_failures)
                    self._gpc0cpc0_fsinfo |= ecc_failures
                    self.CheckSkuTpc()
                    # Re-run with the updated mask
                    _RunSanity()
                    return

                for tpc_config in self._gpc0cpc0_fsinfo.IterateTpc():
                    Datalogger.PrintLog("GPC0CPC0 Sanity Iteration : {}".format(tpc_config))
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

                        self.PushFsInfoTpc(tpc_config.IlwertTpc())
                        self._gpc0cpc0_fsinfo |= tpc_config.IlwertTpc()
                        self.CheckSkuTpc()
                        break

        self.ResetGpu()

        try:
            _RunSanity()
        except:
            self.ClearFsStack()
            raise

        self.ApplyFsStack()

        return super(GH100GpuInfo,self).FloorsweepTpc(testlist, only_gpc, iterate_cold_gpc, iterate_tpc, iterate_cold_tpc, smid_shmoo, tpc_sriov)

    @Datalogger.LogFunction
    def FloorsweepLwlink(self, testlist):
        def _FloorsweepLwLinkRelwrse(fsinfo, testlist):
            '''
            Relwrsive helper function. Return True if a component was disabled
            '''
            if not fsinfo:
                return False

            Datalogger.PrintLog("LwLink Iteration: {}".format(fsinfo))
            failed_test = self.RunTestList(fsinfo, testlist)
            if failed_test is not None:
                if fsinfo.IsSingleLwLink():
                    self.PushFsInfoLwLink(fsinfo.IlwertLwLink())
                    return True

                left,right = fsinfo.SplitLwLink()

                left_disable = _FloorsweepLwLinkRelwrse(left, testlist)
                right_disable = _FloorsweepLwLinkRelwrse(right, testlist)
                if not left_disable and not right_disable:
                    raise Exceptions.FsSanityFailed("{} failed but both {} and {} passed".format(fsinfo, left, right))

                return left_disable or right_disable
            return False

        self.ClearFsStack()
        Datalogger.PrintLog("OLD FUSE INFO :")
        self.GetFsInfo().PrintFuseInfo()

        self.ResetGpu()

        try:
            _FloorsweepLwLinkRelwrse(self.GetFsInfo(), testlist)
        except:
            self.ClearFsStack()
            raise

        self.ApplyFsStack()

        Datalogger.PrintLog("NEW FUSE INFO :")
        fsinfo = self.GetFsInfo()
        fsinfo.PrintFuseInfo()
        fsinfo.LogFuseInfo()