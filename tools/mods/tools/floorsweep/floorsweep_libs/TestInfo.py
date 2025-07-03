# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2015-2016,2019-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import os
import re

from . import Utilities
from . import Exceptions

class TestInfo(object):
    SCRIPT = "gputest.js"
    MARGS = "{} -disable_passfail_msg -fslib_mode_disable -no_stack_dump"
    SET_SW_FUSES = []
    def __init__(self, test_id, test_args, timeout):
        self.test_id = test_id
        self.test_args = test_args
        try:
            int(test_id)
            self.test_command = "-test {} {}".format(test_id, test_args)
        except ValueError:
            # test_id is actually a virtual testid string
            self.test_command = "-test_virtid '{}' {}".format(test_id, test_args)
        self.timeout = timeout
        self.smid_failures = None
        self.ecc_failures = None

    def __str__(self):
        return "Test {}".format(self.test_id)

    @classmethod
    def SetSwFuses(cls, set_fuses):
        cls.SET_SW_FUSES = set_fuses

    def SetFuses(self, fsinfo):
        set_fuse_args = ' '.join(["-set_one_sw_fuse {} {}".format(fuse,val) for (fuse,val) in self.SET_SW_FUSES])
        fuseblow_args = ' '.join([set_fuse_args,
                                  "-floorsweep {}".format(fsinfo.FsStr()),
                                  "-disable_passfail_msg -skip_down_bin -skip_rm_state_init -no_fs_restore"])
        rc = Utilities.ModsCall('fuseblow.js', fuseblow_args, self.timeout)
        if rc != 0:
            raise Exceptions.FusingException(rc, "Failed set_one_sw_fuse")

    def Run(self, fsinfo, extra_args=""):
        if self.SET_SW_FUSES:
            self.SetFuses(fsinfo)

        script_args = ' '.join([self.MARGS.format(self.test_command),
                                "-floorsweep {}".format(fsinfo.FsStr()),
                                extra_args])
        return_code = Utilities.ModsCall(self.SCRIPT, script_args, self.timeout)

        self.smid_failures,self.ecc_failures = self.FindFailingInfo(fsinfo, Utilities.GetModsLogLocation())

        return return_code

    def RunSriov(self, num_parts, partition_arg, fsinfo, vf_args=[]):
        if self.SET_SW_FUSES:
            self.SetFuses(fsinfo)

        pf_args = ' '.join(["-null_display -rmkey RmDisableDisplay 1 -lwlink_force_disable -rmkey RMDebugOverrideSMCHwGpcReorder 1 -notest",
                            "-floorsweep {}".format(fsinfo.FsStr())])
        if len(vf_args) == 0:
            vf_args = [""] * num_parts
        else:
            Exceptions.ASSERT(num_parts == len(vf_args))

        vf_args = [' '.join([self.MARGS.format(self.test_command), vf_args[i]]) for i in range(num_parts)]

        return_codes = Utilities.SriovModsCall(self.SCRIPT, partition_arg, pf_args, vf_args, self.timeout)

        self.smid_failures,self.ecc_failures = self.FindFailingSmidInfoSriov(fsinfo)

        return return_codes


    def FindFailingInfo(self, fsinfo, filename):
        smid_failures = None
        def _AddSmidFailure(**kwargs):
            nonlocal smid_failures
            if smid_failures is None:
                smid_failures = fsinfo.__class__(override_consistent=True, **kwargs)
            else:
                smid_failures |= fsinfo.__class__(override_consistent=True, **kwargs)

        ecc_failures = None
        def _AddEccFailure(**kwargs):
            nonlocal ecc_failures
            if ecc_failures is None:
                ecc_failures = fsinfo.__class__(override_consistent=True, **kwargs)
            else:
                ecc_failures |= fsinfo.__class__(override_consistent=True, **kwargs)

        failing_smid_match = re.compile('ERROR: Failing SMID: \d+ GPC: (\d+) TPC: (\d+)').match
        failing_ecc_search = re.compile('ECC errors exceeded the threshold:').search
        with open(filename) as log_file:
            file_iter = iter(log_file)
            for line in file_iter:
                m = failing_smid_match(line)
                if m:
                    tpc_disable_masks = [0x0 for x in range(fsinfo.MAX_GPC)]
                    tpc_disable_masks[int(m.group(1))] = (1 << int(m.group(2)))
                    _AddSmidFailure(tpc_disable_masks=tpc_disable_masks)
                    continue

                m = failing_ecc_search(line)
                if m:
                    while True:
                        error_msg = [next(file_iter,None), next(file_iter,None)]
                        if error_msg[0] is None or error_msg[1] is None:
                            break
                        if not re.match("^\s*[-]",error_msg[0]) or not re.match("^\s*[*]", error_msg[1]):
                            break

                        m = re.match("^\s*[*].*partition (\S),", error_msg[1])
                        if m:
                            fbp_idx = ord(m.group(1)) - ord('A')
                            fbp_disable = (1 << fbp_idx)
                            _AddEccFailure(fbp_disable_mask=fbp_disable)
                            continue

                        m = re.match("^\s*[*].*ltc (\S+), slice (\S+)", error_msg[1])
                        if m:
                            ltc_idx = int(m.group(1))
                            fbp_idx = int(ltc_idx / fsinfo.ltc_per_fbp)
                            fbp_ltc_idx = int(ltc_idx % fsinfo.ltc_per_fbp)
                            slice_disable = (1 << int(m.group(2)))
                            slice_disable_masks = [0x0 for x in range(fsinfo.MAX_FBP)]
                            slice_disable_masks[fbp_idx] = (slice_disable << (fbp_ltc_idx * fsinfo.slice_per_ltc))
                            _AddEccFailure(slice_disable_masks=slice_disable_masks)
                            continue

                        m = re.match("^\s*[*].*GPC (\S+), MMU", error_msg[1])
                        if m:
                            gpc_disable = (1 << int(m.group(1)))
                            _AddEccFailure(gpc_disable_mask=gpc_disable)
                            continue

                        m = re.match("^\s*[*].*GPC (\S+), TPC (\S+)", error_msg[1])
                        if m:
                            tpc_disable_masks = [0x0 for x in range(fsinfo.MAX_GPC)]
                            tpc_disable_masks[int(m.group(1))] = (1 << int(m.group(2)))
                            _AddEccFailure(tpc_disable_masks=tpc_disable_masks)
                            continue
        return smid_failures,ecc_failures

    def FindFailingSmidInfoSriov(self, fsinfo):
        smidMasks = [None for x in range(4)]
        eccMasks = [None for x in range(4)]
        vf_logs = [os.path.join("subtest_vf{}".format(i),"mods.log") for i in range(4)]
        for vf_idx,vf_log in enumerate(vf_logs):
            if os.path.exists(vf_log):
                smid,ecc = self.FindFailingInfo(fsinfo, vf_log)
                if smid is not None:
                    if smidMasks[vf_idx] is None:
                        smidMasks[vf_idx] = smid
                    else:
                        smidMasks[vf_idx] |= smid
                if ecc is not None:
                    if eccMasks[vf_idx] is None:
                        eccMasks[vf_idx] = ecc
                    else:
                        eccMasks[vf_idx] |= ecc
        return smidMasks,eccMasks

class TraceTestInfo(TestInfo):
    def __init__(self, trace_name, trace_args, timeout):
        self.trace_name = os.path.expanduser(trace_name)
        super(TraceTestInfo, self).__init__(20, "-testargstr 20 Trace {} {}".format(self.trace_name, trace_args), timeout)

    def __str__(self):
        return "Trace {}".format(os.path.basename(self.trace_name))

    def Run(self, fsinfo):
        trace_path = os.path.join(self.trace_name, 'test.hdr')
        if not os.path.exists(trace_path):
            raise Exceptions.FileNotFound("File does not exist: {}".format(trace_path))

        return super(TraceTestInfo,self).Run(fsinfo)
