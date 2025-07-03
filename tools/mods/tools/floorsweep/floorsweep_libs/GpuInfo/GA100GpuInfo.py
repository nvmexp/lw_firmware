# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
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
from .GP10xGpuInfo import GP100GpuInfo

import math
import itertools
from functools import reduce

class GA100GpuInfo(GP100GpuInfo):
    def __init__(self, *args, **kwargs):
        super(GA100GpuInfo,self).__init__(*args, **kwargs)

    def GetAteFsInfo(self):
        return super(GA100GpuInfo,self).GetAteFsInfo().SetOverrideL2Noc()

    @Datalogger.LogFunction
    def FloorsweepFB(self, testlist, run_connectivity, only_fbp, iterate_cold_fbp, fb_max_tpc, *args):
        Exceptions.ASSERT(isinstance(testlist, list))
        if iterate_cold_fbp:
            raise Exceptions.UnsupportedFunction("iterate_cold_fbp not supported")

        if fb_max_tpc:
            raise Exceptions.UnsupportedFunction("fb_max_tpc not supported")

        if only_fbp:
            Datalogger.PrintLog("only_fbp does not change the FloorsweepFB behavior of this chip")

        def _RunTests(fsinfo):
            '''
            Return True if all tests passed
            '''
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

        if self.CheckSkuFB() == 0:
            Datalogger.PrintLog("Chip matches SKU FB. Nothing to do.")
            return

        self.ClearFsStack()
        Datalogger.PrintLog("OLD FUSE INFO :")
        self.GetFsInfo().PrintFuseInfo()

        self.ResetGpu()

        try:
            # Indicate whether a particular uGPU has been fully verified yet
            # Initialize it with whichever uGPUs are already fully disabled from ATE
            ugpu_check_mask = self.GetFsInfo().ugpu_disable_mask

            for tpc_config in self.GetFsInfo().IterateTpcBreadthFirst():
                if ugpu_check_mask == tpc_config.ugpu_mask:
                    # Done. We've checked both uGPUs
                    break
                if ugpu_check_mask & tpc_config.ugpu_enable_mask:
                    # Already verified this uGPU
                    continue

                # Update tpc_config with potential floorswept HBMs from other uGPU
                try:
                    tpc_config = tpc_config | self.GetFsInfo().CloneFB()
                except Exceptions.FsSanityFailed:
                    # The other uGPU disabled all the remaining HBMs in this uGPU
                    ugpu_check_mask |= tpc_config.ugpu_enable_mask
                    continue

                if run_connectivity:
                    # Run one iteration of each test with everything enabled.
                    ugpu_fbpath = reduce(lambda x,y: x&y, tpc_config.IterateFbp())
                    Datalogger.PrintLog("FB Iteration : Connectivity : {}".format(ugpu_fbpath))
                    if _RunTests(ugpu_fbpath):
                        ugpu_check_mask |= ugpu_fbpath.ugpu_enable_mask
                        continue

                passed_once = False
                for fbpath in tpc_config.IterateFbp():
                    Datalogger.PrintLog("FB Iteration : {}".format(fbpath))
                    if _RunTests(fbpath):
                        passed_once = True
                    else:
                        try:
                            self.PushFsInfoFb(fbpath.IlwertRop())
                        except Exceptions.FsSanityFailed:
                            pass

                if passed_once:
                    if run_connectivity and not self._fs_stack:
                        # If fs_stack is empty, then nothing is being disabled, ie. everything passed
                        raise Exceptions.FsSanityFailed("Failed connectivity {}, but passed all FB configurations".format(ugpu_fbpath))

                    self.ApplyFsStack()
                    self.GetFsInfo().CheckSanity()
                    ugpu_check_mask |= tpc_config.ugpu_enable_mask
                    if self.CheckSkuFB() == 0:
                        Datalogger.PrintLog("Chip matches SKU FB. Exiting FB test.")
                        raise Exceptions.SkuMatchEarlyExit()
                else:
                    # This TPC didn't pass with any of the HBMs connected to its uGPU
                    # This could mean that there is something wrong with this TPC instead, so it should be disabled
                    # This could instead mean that all 4 of these HBMs are bad, but if that's the case then this whole
                    # uGPU would need to be disabled anyway so that the two remaining HBMs can be enabled (otherwise they
                    # would also need to be disabled due to L2NOC restrictions).
                    gpc_idx = int(math.log(tpc_config.gpc_enable_mask,2))
                    Datalogger.PrintLog("Disabling GPC_TPC {}:{:#x}".format(gpc_idx, tpc_config.gpc_tpc_enable_mask(gpc_idx)))
                    stack_backup = self._fs_stack # Preserve HBM disables
                    self.ClearFsStack()
                    self.PushFsInfoTpc(tpc_config.IlwertTpc())
                    self.ApplyFsStack()
                    self.CheckSkuTpc()
                    if (self.GetFsInfo().ugpu_enable_mask & tpc_config.ugpu_enable_mask) == 0:
                        # That was the last TPC in this uGPU. Disable all the HBMs as well.
                        self._fs_stack = stack_backup
                        self.ApplyFsStack()
                        self.GetFsInfo().CheckSanity()
                        ugpu_check_mask |= tpc_config.ugpu_enable_mask
                        self.CheckSkuFB()
        except Exceptions.SkuMatchEarlyExit:
            # Make sure there aren't any additional bad components outside of the SKU
            # Keep the same tpc enabled but enable all the valid HBMs
            tpc_config = tpc_config.CloneTpc() | self.GetFsInfo().CloneFB()
            Datalogger.PrintLog("FB Iteration : SkuMatchVerify : {}".format(tpc_config))
            if not _RunTests(tpc_config):
                self.ApplyFsStack()
                raise Exceptions.CannotMeetFsRequirements("Failed to verify SKU match. Some other component is faulty.")
        except:
            self.ClearFsStack()
            raise
        else:
            if ugpu_check_mask != self.GetFsInfo().ugpu_mask:
                # To get here we checked every single HBM+TPC combination in at least one uGPU, but
                raise Exceptions.FsSanityFailed("Cound not verify both uGPUs")

        self.ApplyFsStack()

        Datalogger.PrintLog("NEW FUSE INFO :")
        fsinfo = self.GetFsInfo()
        fsinfo.PrintFuseInfo()
        fsinfo.LogFuseInfo()

    @Datalogger.LogFunction
    def FloorsweepTpc(self, testlist, only_gpc, iterate_cold_gpc, iterate_tpc, iterate_cold_tpc, smid_shmoo, tpc_sriov):
        if not tpc_sriov:
            return super(GA100GpuInfo,self).FloorsweepTpc(testlist, only_gpc, iterate_cold_gpc, iterate_tpc, iterate_cold_tpc, smid_shmoo, tpc_sriov)

        if smid_shmoo:
            raise Exceptions.UnsupportedFunction("smid_shmoo not supported with tpc_sriov")

        class _FloorsweepTpcSmc():
            def __init__(self, idx, gpuinfo):
                self.vf_idx = idx
                self.gpuinfo = gpuinfo
                self.done = False

                self.return_code = 0
                self.smid_failures = None
            def IsDone(self):
                return self.done
            def GetIteration(self, fsinfo, retArray=[]):

                yield fsinfo

                if self.return_code == 0:
                    retArray.append(False)
                    return

                if self.smid_failures:
                    smid_failures = self.smid_failures
                    Datalogger.PrintLog("VF{} Disabling GPC_TPC Masks {}".format(self.vf_idx, ["{:#x}".format(mask) for mask in smid_failures.tpc_disable_masks]))
                    self.gpuinfo.PushFsInfoTpc(smid_failures)
                    if self.gpuinfo.CheckSkuTpc() == 0:
                        Datalogger.PrintLog("Chip matches SKU TPC. Exiting TPC test.")
                        raise Exceptions.SkuMatchEarlyExit()
                    # Retry this stage with these TPCs disabled. Other, non-smid tests might catch more failures.
                    yield from self.GetIteration(fsinfo | smid_failures)
                    retArray.append(True)
                    return

                if only_gpc and fsinfo.IsSingleGpc():
                    Datalogger.PrintLog("VF{} Disabling GPC {:#x}".format(self.vf_idx, fsinfo.gpc_enable_mask))
                    self.gpuinfo.PushFsInfoTpc(fsinfo.IlwertTpc())
                    if self.gpuinfo.CheckSkuTpc() == 0:
                        Datalogger.PrintLog("Chip matches SKU TPC. Exiting TPC test.")
                        raise Exceptions.SkuMatchEarlyExit()
                    retArray.append(True)
                    return

                if fsinfo.IsSingleTpc():
                    gpc_idx = int(math.log(fsinfo.gpc_enable_mask,2))
                    Datalogger.PrintLog("VF{} Disabling GPC_TPC {}:{:#x}".format(self.vf_idx, gpc_idx, fsinfo.gpc_tpc_enable_mask(gpc_idx)))
                    self.gpuinfo.PushFsInfoTpc(fsinfo.IlwertTpc())
                    if self.gpuinfo.CheckSkuTpc() == 0:
                        Datalogger.PrintLog("Chip matches SKU TPC. Exiting TPC test.")
                        raise Exceptions.SkuMatchEarlyExit()
                    retArray.append(True)
                    return

                left,right = fsinfo.SplitTpc()

                left_disable = []
                right_disable = []
                yield from self.GetIteration(left, left_disable)
                yield from self.GetIteration(right, right_disable)
                if not left_disable[0] and not right_disable[0]:
                    raise Exceptions.FsSanityFailed("{} failed but both {} and {} passed".format(fsinfo, left, right))

                retArray.append(left_disable[0] or right_disable[0])
                return

        try:
            vf_list = [_FloorsweepTpcSmc(i, self) for i in range(4)]
            smc_partitions = list(self.GetFsInfo().SmcPartitions(4))
            iter_list = [iter(x.GetIteration(part)) for x,part in zip(vf_list,smc_partitions)]
            vf_fsinfo_list = [next(it) for it in iter_list]
            while not reduce(lambda x,y: x and y.IsDone(), vf_list, True):
                for vf in vf_list:
                    vf.return_code = 0
                    vf.smid_failures = None

                partition_list = []
                vf_args = []
                for i,fsi in enumerate(vf_fsinfo_list):
                    if fsi.IsSingleGpc():
                        # Need to enable the other GPC in this pair so it can still be allocated as a quarter
                        # The other GPC will just go unused because only the specified GPC will be assigned a syspipe
                        gpc_idx = int(math.log(fsi.gpc_enable_mask,2))
                        gpc_pair = gpc_idx ^ 0x1

                        tpc_enable = fsi.tpc_enable_masks[gpc_idx]
                        tpc_pair_enable = self.GetFsInfo().tpc_enable_masks[gpc_pair]

                        tpc_count = "{:b}".format(tpc_enable).count("1") # count the tpcs that are enabled
                        tpc_pair_count = "{:b}".format(tpc_pair_enable).count("1")
                        if tpc_count > tpc_pair_count:
                            engine_idx = 1
                        elif tpc_count < tpc_pair_count:
                            engine_idx = 0
                        else:
                            engine_idx = 1 if (gpc_idx > gpc_pair) else 0

                        tpc_enable_masks = fsi.tpc_enable_masks
                        tpc_enable_masks[gpc_pair] = tpc_pair_enable
                        vf_fsinfo_list[i] = fsi.__class__(clone=fsi.ClearTpc(), tpc_enable_masks = tpc_enable_masks)
                        partition_list.append("part:quarter:1-1")
                        vf_args.append("-smc_engine_idx {}".format(engine_idx))
                    else:
                        partition_list.append("part:quarter:2-0")
                        vf_args.append("-smc_engine_idx 0")

                    Datalogger.PrintLog("VF{} TPC Iteration: {}".format(i, fsi))
                partition_str = ':'.join(partition_list)

                for test in testlist:
                    Exceptions.ASSERT(isinstance(test, TestInfo))
                    return_codes = [1] * len(vf_fsinfo_list)
                    try:
                        return_codes = test.RunSriov(len(vf_fsinfo_list), partition_str, reduce(lambda x,y:x&y, vf_fsinfo_list), vf_args)
                    except (Exceptions.ModsTimeout, Exceptions.UnknownModsError, Exceptions.CommandError):
                        pass
                    else:
                        if len([x for x in return_codes if x != 0]) == 0:
                            # Test passed on all SMCs
                            continue
                    finally:
                        self.ResetGpu()

                    for vf,return_code,smid_failures in zip(vf_list,return_codes,test.smid_failures):
                        vf.return_code = return_code
                        vf.smid_failures = smid_failures
                    break

                for i,(it,vf) in enumerate(zip(iter_list, vf_list)):
                    try:
                        if vf.IsDone() and vf.return_code != 0:
                            # It thought it was done but it failed again
                            vf.done = False
                            iter_list[i] = iter(vf.GetIteration(part))
                            next(iter_list[i]) # move past the initial mask
                            vf_fsinfo_list[i] = next(iter_list[i])
                        elif not vf.IsDone():
                            vf_fsinfo_list[i] = next(it)
                    except StopIteration:
                        vf.done = True
                    finally:
                        if vf.IsDone():
                            new_parts = list(self.GetFsInfo().SmcPartitions(len(vf_fsinfo_list)))
                            vf_fsinfo_list[i] = new_parts[i]

        except Exceptions.SkuMatchEarlyExit:
            # Make sure there aren't any additional bad components outside of the SKU
            Datalogger.PrintLog("TPC Iteration : SkuMatchVerify : {}".format(self.GetFsInfo()))
            passed_all_tests = True
            for test in testlist:
                Exceptions.ASSERT(isinstance(test, TestInfo))
                try:
                    return_code = test.Run(self.GetFsInfo())
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
        except:
            self.ClearFsStack()
            raise

        self.ApplyFsStack()

        Datalogger.PrintLog("NEW FUSE INFO :")
        fsinfo = self.GetFsInfo()
        fsinfo.PrintFuseInfo()
        fsinfo.LogFuseInfo()

    def PushFsInfoSyspipe(self, fsinfo):
        self._fs_stack.append(fsinfo.CloneSyspipe())
        self.GetFsInfo().CheckSanity()

    def PushFsInfoCe(self, fsinfo):
        self._fs_stack.append(fsinfo.CloneCe())
        self.GetFsInfo().CheckSanity()

    def PushFsInfoLwdec(self, fsinfo):
        self._fs_stack.append(fsinfo.CloneLwdec())
        self.GetFsInfo().CheckSanity()

    def PushFsInfoOfa(self, fsinfo):
        self._fs_stack.append(fsinfo.CloneOfa())
        self.GetFsInfo().CheckSanity()

    def PushFsInfoLwjpg(self, fsinfo):
        self._fs_stack.append(fsinfo.CloneLwjpg())
        self.GetFsInfo().CheckSanity()

    @Datalogger.LogFunction
    def FloorsweepSyspipe(self, testlist):
        def _RunTests(fsinfo):
            '''
            Return True if all tests passed
            '''
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

        self.ClearFsStack()
        Datalogger.PrintLog("OLD FUSE INFO :")
        self.GetFsInfo().PrintFuseInfo()

        self.ResetGpu()

        try:
            syspipe0 = next(self.GetFsInfo().IterateSyspipe())
            Datalogger.PrintLog("Syspipe Iteration : {}".format(syspipe0))
            if not _RunTests(syspipe0):
                # syspipe0 cannot be disabled
                # need to find if a particular GPC is more lwlpable
                for single_gpc in syspipe0.IterateGpc():
                    Datalogger.PrintLog("Syspipe Iteration : Syspipe0 Verify : {}".format(single_gpc))
                    if not _RunTests(single_gpc):
                        gpc_idx = int(math.log(single_gpc.gpc_enable_mask,2))
                        Datalogger.PrintLog("Disabling GPC_TPC {}:{:#x}".format(gpc_idx, single_gpc.gpc_tpc_enable_mask(gpc_idx)))
                        self.PushFsInfoTpc(single_gpc.IlwertTpc())
                        self.CheckSkuTpc()
                if not self._fs_stack:
                    # No GPCs were disabled
                    raise Exceptions.FsSanityFailed("Syspipe0 failed with all GPCs but passed with each individual GPC")
                else:
                    self.ApplyFsStack()

            # Restart the iteration so it picks up any GPCs that might have been disabled, but skip syspipe0
            for single_syspipe in itertools.islice(self.GetFsInfo().IterateSyspipe(), 1, None):
                Datalogger.PrintLog("Syspipe Iteration : {}".format(single_syspipe))
                if not _RunTests(single_syspipe):
                        self.PushFsInfoSyspipe(single_syspipe.DisableSelectedSyspipe())
        except:
            self.ClearFsStack()
            raise

        self.ApplyFsStack()

        Datalogger.PrintLog("NEW FUSE INFO :")
        fsinfo = self.GetFsInfo()
        fsinfo.PrintFuseInfo()
        fsinfo.LogFuseInfo()

    @Datalogger.LogFunction
    def FloorsweepCopyEngines(self, testlist):
        def _RunTests(fsinfo):
            '''
            Return True if all tests passed
            '''
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

        self.ClearFsStack()
        Datalogger.PrintLog("OLD FUSE INFO :")
        self.GetFsInfo().PrintFuseInfo()

        self.ResetGpu()

        try:
            for ce_config in self.GetFsInfo().IterateCopyEngines():
                Datalogger.PrintLog("CopyEngine Iteration : {}".format(ce_config))
                if not _RunTests(ce_config):
                    Datalogger.PrintLog("Disabling CE {:#x}".format(ce_config.ce_enable_mask))
                    self.PushFsInfoCe(ce_config.IlwertCe())
        except:
            self.ClearFsStack()
            raise

        self.ApplyFsStack()

        Datalogger.PrintLog("NEW FUSE INFO :")
        fsinfo = self.GetFsInfo()
        fsinfo.PrintFuseInfo()
        fsinfo.LogFuseInfo()

    @Datalogger.LogFunction
    def FloorsweepLwdec(self, testlist):
        def _RunTests(fsinfo):
            '''
            Return True if all tests passed
            '''
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

        self.ClearFsStack()
        Datalogger.PrintLog("OLD FUSE INFO :")
        self.GetFsInfo().PrintFuseInfo()

        self.ResetGpu()

        try:
            for lwdec_config in self.GetFsInfo().IterateLwdec():
                Datalogger.PrintLog("Lwdec Iteration : {}".format(lwdec_config))
                if not _RunTests(lwdec_config):
                    lwdec_idx = int(math.log(lwdec_config.lwdec_enable_mask,2))
                    Datalogger.PrintLog("Disabling Lwdec {}".format(lwdec_idx))
                    self.PushFsInfoLwdec(lwdec_config.IlwertLwdec())
        except:
            self.ClearFsStack()
            raise

        self.ApplyFsStack()

        Datalogger.PrintLog("NEW FUSE INFO :")
        fsinfo = self.GetFsInfo()
        fsinfo.PrintFuseInfo()
        fsinfo.LogFuseInfo()

    @Datalogger.LogFunction
    def FloorsweepOfa(self, testlist):
        def _RunTests(fsinfo):
            '''
            Return True if all tests passed
            '''
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

        self.ClearFsStack()
        Datalogger.PrintLog("OLD FUSE INFO :")
        self.GetFsInfo().PrintFuseInfo()

        self.ResetGpu()

        try:
            for ofa_config in self.GetFsInfo().IterateOfa():
                Datalogger.PrintLog("OFA Iteration : {}".format(ofa_config))
                if not _RunTests(ofa_config):
                    ofa_idx = int(math.log(ofa_config.ofa_enable_mask,2))
                    Datalogger.PrintLog("Disabling OFA {}".format(ofa_idx))
                    self.PushFsInfoOfa(ofa_config.IlwertOfa())
        except:
            self.ClearFsStack()
            raise

        self.ApplyFsStack()

        Datalogger.PrintLog("NEW FUSE INFO :")
        fsinfo = self.GetFsInfo()
        fsinfo.PrintFuseInfo()
        fsinfo.LogFuseInfo()

    @Datalogger.LogFunction
    def FloorsweepLwjpg(self, testlist):
        def _RunTests(fsinfo):
            '''
            Return True if all tests passed
            '''
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

        self.ClearFsStack()
        Datalogger.PrintLog("OLD FUSE INFO :")
        self.GetFsInfo().PrintFuseInfo()

        self.ResetGpu()

        try:
            for lwjpg_config in self.GetFsInfo().IterateLwjpg():
                Datalogger.PrintLog("LWJPG Iteration : {}".format(lwjpg_config))
                if not _RunTests(lwjpg_config):
                    lwjpg_idx = int(math.log(lwjpg_config.lwjpg_enable_mask,2))
                    Datalogger.PrintLog("Disabling LWJPG {}".format(lwjpg_idx))
                    self.PushFsInfoLwjpg(lwjpg_config.IlwertLwjpg())
        except:
            self.ClearFsStack()
            raise

        self.ApplyFsStack()

        Datalogger.PrintLog("NEW FUSE INFO :")
        fsinfo = self.GetFsInfo()
        fsinfo.PrintFuseInfo()
        fsinfo.LogFuseInfo()
