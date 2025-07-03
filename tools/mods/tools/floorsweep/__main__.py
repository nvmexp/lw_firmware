# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import argparse
import sys
import traceback

if sys.version_info[0] < 3 or sys.version_info[1] < 5:
    sys.stdout.write('Floorsweep.egg requires at least Python 3.5\n')
    exit(1)

from floorsweep_libs import Datalogger
from floorsweep_libs import FsInfo
from floorsweep_libs import GpuInfo
from floorsweep_libs import Utilities
from floorsweep_libs import Exceptions
from floorsweep_libs import TestInfo
from floorsweep_libs import FsLib

import unittest
from floorsweep_libs import UnitTest

VERSION = "3.7.3"

class Floorsweep(object):
    def __init__(self, args):
        Datalogger.InitLog(args.all_log)
        Utilities.SetModsPrePostArgs(''.join(args.pre), ''.join(args.post))
        Utilities.SetModsLogLocation(args.mods_log)
        Utilities.SetVerboseStdout(args.verbose_stdout)

        self.gpuinfo = None
        self.sku = args.sku.strip()
        self.set_fuses = args.set_fuses
        self.fuse_post = ''.join(args.fuse_post)
        self.connectivity = args.connectivity
        self.only_gpc = args.only_gpc
        self.only_fbp = args.only_fbp
        self.only_ltc = args.only_ltc
        self.iterate_tpc = args.iterate_tpc
        self.iterate_cold_gpc = args.iterate_cold_gpc
        self.iterate_cold_tpc = args.iterate_cold_tpc
        self.tpc_sriov = args.tpc_sriov
        self.smid_shmoo = args.smid_shmoo
        self.iterate_cold_fbp = args.iterate_cold_fbp
        self.fb_max_tpc = args.fb_max_tpc
        self.no_lwpex = args.no_lwpex
        self.skip_dump_sweep = args.skip_dump_sweep

        TestInfo.TestInfo.SetSwFuses(self.set_fuses)

        def _ParseTests(testlist, arg_tests, arg_traces = None):
            if arg_tests:
                for test_args in arg_tests:
                    testlist.append(TestInfo.TestInfo(test_id=test_args[0],
                                                      test_args=test_args[1],
                                                      timeout=test_args[2]))
            if arg_traces:
                for trace_args in arg_traces:
                    testlist.append(TestInfo.TraceTestInfo(trace_name=trace_args[0],
                                                           trace_args=trace_args[1],
                                                           timeout=trace_args[2]))

        self.tpcpretests = []
        _ParseTests(self.tpcpretests, args.tpcpretests)

        self.fbtests = []
        _ParseTests(self.fbtests, args.fbtests, args.fbtraces)

        self.tpctests = []
        _ParseTests(self.tpctests, args.tpctests, args.tpctraces)

        self.lwlinktests = []
        _ParseTests(self.lwlinktests, args.lwlinktests)

        self.kappatests = []
        _ParseTests(self.kappatests, args.kappatests)

        self.syspipetests = []
        _ParseTests(self.syspipetests, args.syspipetests)

        self.cetests = []
        _ParseTests(self.cetests, args.cetests)

        self.lwdectests = []
        _ParseTests(self.lwdectests, args.lwdectests)

        self.ofatests = []
        _ParseTests(self.ofatests, args.ofatests)

        self.lwjpgtests = []
        _ParseTests(self.lwjpgtests, args.lwjpgtests)

    def Run(self):
        '''
        Run the main program exelwtion
        '''
        Datalogger.PrintLog('Running floorsweep.egg version: {}'.format(VERSION))
        Datalogger.PrintLog('Running with {}'.format(' '.join(sys.argv)))

        try:
            self.gpuinfo = GpuInfo.CreateGpuInfo(self.sku, self.fuse_post, self.no_lwpex)
            self.gpuinfo.only_gpc = self.only_gpc
            self.gpuinfo.only_fbp = self.only_fbp
            self.gpuinfo.only_ltc = self.only_ltc
            self.gpuinfo.Print()

            try:
                if self.tpcpretests:
                    self.gpuinfo.FloorsweepTpc(self.tpcpretests, self.only_gpc, self.iterate_cold_gpc, self.iterate_tpc, self.iterate_cold_tpc, self.smid_shmoo, self.tpc_sriov)
            except Exceptions.FloorsweepException as e:
                e.TEST = 971
                raise

            try:
                if self.fbtests:
                    self.gpuinfo.FloorsweepFB(self.fbtests, self.connectivity, self.only_fbp, self.iterate_cold_fbp, self.fb_max_tpc, self.only_ltc)
            except Exceptions.FloorsweepException as e:
                e.TEST = 970
                raise

            try:
                if self.tpctests:
                    self.gpuinfo.FloorsweepTpc(self.tpctests, self.only_gpc, self.iterate_cold_gpc, self.iterate_tpc, self.iterate_cold_tpc, self.smid_shmoo, self.tpc_sriov)
            except Exceptions.FloorsweepException as e:
                e.TEST = 971
                raise

            try:
                if self.lwlinktests:
                    self.gpuinfo.FloorsweepLwlink(self.lwlinktests)
            except Exceptions.FloorsweepException as e:
                e.TEST = 972
                raise

            try:
                if self.kappatests:
                    self.gpuinfo.KappaBin(self.kappatests)
            except Exceptions.FloorsweepException as e:
                e.TEST = 973
                raise

            try:
                if self.syspipetests:
                    self.gpuinfo.FloorsweepSyspipe(self.syspipetests)
            except Exceptions.FloorsweepException as e:
                e.TEST = 974
                raise

            try:
                if self.cetests:
                    self.gpuinfo.FloorsweepCopyEngines(self.cetests)
            except Exceptions.FloorsweepException as e:
                e.TEST = 975
                raise

            try:
                if self.lwdectests:
                    self.gpuinfo.FloorsweepLwdec(self.lwdectests)
            except Exceptions.FloorsweepException as e:
                e.TEST = 976
                raise

            try:
                if self.ofatests:
                    self.gpuinfo.FloorsweepOfa(self.ofatests)
            except Exceptions.FloorsweepException as e:
                e.TEST = 977
                raise

            try:
                if self.lwjpgtests:
                    self.gpuinfo.FloorsweepLwjpg(self.lwjpgtests)
            except Exceptions.FloorsweepException as e:
                e.TEST = 978
                raise

            self.LogFinal(0)
            Datalogger.PrintLog('Error Code = 000000000000 (OK)')

        except Exceptions.FloorsweepException as e:
            self.FailExit(e)
        except Exception as e:
            Datalogger.PrintLog(traceback.format_exc(), truncate=False)
            self.FailExit(Exceptions.SoftwareError(str(e)))

    def LogFinal(self, exit_code):
        self.Final(exit_code)

        if not self.skip_dump_sweep:
            # Use fuseblow.js to dump the final data into mle file
            # Must be called after Final finishes so the data will be present in the json
            Utilities.ModsCall('fuseblow.js', '-disable_passfail_msg -skip_rm_state_init -dump_sweep -fb_broken -devid_fs_war -json', 60)

    @Datalogger.LogFunction
    def Final(self, exit_code):
        '''
        Collect the results of the program
        '''
        if not self.gpuinfo:
            if exit_code == 0:
                exit_code = 1
            Datalogger.LogInfo({'Exit Code' : exit_code})
            return
        else:
            Datalogger.LogInfo({'Exit Code' : exit_code})

        self.gpuinfo.Final()

    def FailExit(self, exception):
        '''
        The program failed somewhere. Print out the error code and a fail banner.
        '''
        Exceptions.ASSERT(isinstance(exception,Exceptions.FloorsweepException))
        Datalogger.PrintLog("Exception Raised During Floorsweeping, Aborting...")
        self.LogFinal(exception.GetEC())
        Datalogger.PrintLog("Error Code = {:012d} ({!s})".format(exception.GetEC(), exception))

        Datalogger.PrintLog("\033[1;39;41m                                        \033[0m")
        Datalogger.PrintLog("\033[1;39;41m #######     ####    ########  ###      \033[0m")
        Datalogger.PrintLog("\033[1;39;41m #######    ######   ########  ###      \033[0m")
        Datalogger.PrintLog("\033[1;39;41m ##        ##    ##     ##     ###      \033[0m")
        Datalogger.PrintLog("\033[1;39;41m ##        ##    ##     ##     ###      \033[0m")
        Datalogger.PrintLog("\033[1;39;41m #######   ########     ##     ###      \033[0m")
        Datalogger.PrintLog("\033[1;39;41m #######   ########     ##     ###      \033[0m")
        Datalogger.PrintLog("\033[1;39;41m ##        ##    ##     ##     ###      \033[0m")
        Datalogger.PrintLog("\033[1;39;41m ##        ##    ##  ########  ######## \033[0m")
        Datalogger.PrintLog("\033[1;39;41m ##        ##    ##  ########  ######## \033[0m")
        Datalogger.PrintLog("\033[1;39;41m                                        \033[0m")
        exit(1)

if __name__ == '__main__':
    def test_action(default_test, default_args, default_timeout):
        class TestAction(argparse.Action):
            def __call__(self, parser, args, values, option_string):
                if len(values) > 3:
                    raise argparse.ArgumentTypeError("{} only accepts as many as 3 parameters.".format(option_string))
                set_values = [default_test, default_args, default_timeout]
                if len(values) > 0:
                    set_values[0] = values[0]
                    set_values[1] = ''
                if len(values) > 1:
                    try:
                        int(values[1])
                    except ValueError:
                        # not a timeout, set as arguments
                        set_values[1] = values[1]
                    else:
                        # is an int, set as the timeout
                        set_values[2] = int(values[1])
                if len(values) > 2:
                    try:
                        set_values[2] = int(values[2])
                    except ValueError:
                        raise argparse.ArgumentTypeError("Third parameter to {} must be an int".format(option_string))
                new_attr = getattr(args, self.dest, [])
                if new_attr is None:
                    new_attr = []
                new_attr.append(set_values)
                setattr(args, self.dest, new_attr)
        return TestAction

    parser = argparse.ArgumentParser(description="floorsweep")
    parser.add_argument("--pre",           dest="pre",           type=str, default="", help="pre-script args for mods")
    parser.add_argument("--post",          dest="post",          type=str, default="", help="post-script args for mods")
    parser.add_argument("--sku",           dest="sku",           type=str, default="", help="The sku to attempt to fit the chip to. If omitted the script will test every component regardless.")
    parser.add_argument("--fuse_post",     dest="fuse_post",     type=str, default="", help="post-script args specifically for fuse dump.")
    parser.add_argument("--set_one_sw_fuse",dest="set_fuses",    action='append', nargs=2, help="Set this SW fuse on the chip before every test. Can be specified multiple times. Two arguments: fuse_name value. Example: --set_one_sw_fuse opt_ecc_en 0x1")
    parser.add_argument("--connectivity",  dest="connectivity",  action='store_true',  help="Run FB connectivity. Will run each fbtest/fbtrace with all FBs enabled before testing each ROP/L2 individually.")
    parser.add_argument("--only_gpc",      dest="only_gpc",      action='store_true',  help="Only floorsweep to the resolution of whole GPCs.")
    parser.add_argument("--only_fbp",      dest="only_fbp",      action='store_true',  help="Only floorsweep to the resolution of whole FBPs.")
    parser.add_argument("--only_ltc",      dest="only_ltc",      action='store_true',  help="Only floorsweep to the resolution of whole LTCs.")
    parser.add_argument("--iterate_tpc",   dest="iterate_tpc",   action='store_true',  help="Iterate through the TPCs individually instead of using a binary search.")
    parser.add_argument("--iterate_cold_gpc",dest="iterate_cold_gpc",action='store_true',  help="Iterate by disabling one GPC at a time, instead of enabling, and instead of using a binary search.")
    parser.add_argument("--iterate_cold_tpc",dest="iterate_cold_tpc",action='store_true',  help="Iterate by disabling one TPC at a time, instead of enabling, and instead of using a binary search.")
    parser.add_argument("--tpc_sriov",     dest="tpc_sriov",     action='store_true',  help="Use SMC/SRIOV to floorsweep the GPU's TPCs in parallel")
    parser.add_argument("--smid_shmoo",    dest="smid_shmoo",    action='store_true',  help="After finding failing SMIDs, shmoo the clock to find the worst TPC.")
    parser.add_argument("--iterate_cold_fbp",dest="iterate_cold_fbp",action='store_true',help="Instead of iterating with one hot FBP, iterate with one cold FBP. WARNING: This may fail if more than one FBP is bad.")
    parser.add_argument("--fb_max_tpc",    dest="fb_max_tpc",    action='store_true',  help="Use all available TPCs while testing the FB. WARNING: This may result in false-positives because TPCs are verified after FB.")
    parser.add_argument("--fbtrace",       dest="fbtraces",      action=test_action('unigine','',390), nargs="*", help="Add trace by name (and arguments (and timeout)) to test the FB. Example: --fbtrace unigine '' 390")
    parser.add_argument("--fbtest",        dest="fbtests",       action=test_action(20,'-testargstr 20 Trace unigine',390), nargs="*", help="Add MODS test (and arguments (and timeout seconds)) to test the FB. Example: --fbtest 20 \"-testargstr 20 Trace unigine\" 390")
    parser.add_argument("--tpctrace",      dest="tpctraces",     action=test_action('unigine','',390), nargs="*", help="Add trace by name (and arguments (and timeout)) to test the tpcs. Example: --tpctrace unigine '' 390")
    parser.add_argument("--tpctest",       dest="tpctests",      action=test_action(20,'-testargstr 20 Trace unigine',390), nargs="*", help="Add MODS test (and arguments (and timeout seconds)) to test the tpcs. Example: --tpctest 20 \"-testargstr 20 Trace unigine\" 390")
    parser.add_argument("--tpcpretest",    dest="tpcpretests",   action=test_action(20,'-testargstr 20 Trace unigine',390), nargs="*", help="Add MODS test (and arguments (and timeout seconds)) to test the tpcs. Example: --tpctest 20 \"-testargstr 20 Trace unigine\" 390")
    parser.add_argument("--lwlinktest",    dest="lwlinktests",   action=test_action(246,'',390), nargs="*", help="Add MODS test (and arguments (and timeout seconds)) to test the lwlinks. Example: --lwlinktest 20 \"-testargstr 20 Trace unigine\" 390")
    parser.add_argument("--kappatest",     dest="kappatests",    action=test_action(1,'',390), nargs="*", help="Add MODS test (and arguments (and timeout seconds)) to test the kappa bins. Example: --kappatest 20 \"-testargstr 20 Trace unigine\" 390")
    parser.add_argument("--syspipetest",   dest="syspipetests",  action=test_action(1,'',390), nargs="*", help="Add MODS test (and arguments (and timeout seconds)) to test the syspipes. Example: --syspipetest 20 \"-testargstr 20 Trace unigine\" 390")
    parser.add_argument("--cetest",        dest="cetests",       action=test_action(1,'',390), nargs="*", help="Add MODS test (and arguments (and timeout seconds)) to test the copy engines. Example: --cetest 20 \"-testargstr 20 Trace unigine\" 390")
    parser.add_argument("--lwdectest",     dest="lwdectests",    action=test_action(1,'',390), nargs="*", help="Add MODS test (and arguments (and timeout seconds)) to test the lwdec engines. Example: --lwdectest 20 \"-testargstr 20 Trace unigine\" 390")
    parser.add_argument("--ofatest",       dest="ofatests",      action=test_action(1,'',390), nargs="*", help="Add MODS test (and arguments (and timeout seconds)) to test the ofa. Example: --ofatest 20 \"-testargstr 20 Trace unigine\" 390")
    parser.add_argument("--lwjpgtest",     dest="lwjpgtests",    action=test_action(1,'',390), nargs="*", help="Add MODS test (and arguments (and timeout seconds)) to test the lwjpg. Example: --lwjpgtest 20 \"-testargstr 20 Trace unigine\" 390")
    parser.add_argument("-l","--mods_log", dest="mods_log",      type=str, default="mods.log", help="Pass -l to mods calls to change location/name of each generated mods.log file")
    parser.add_argument("--all_log",       dest="all_log",       type=str, default="all.log",  help="Change the location/name of the generated all.log file")
    parser.add_argument("--no_lwpex",      dest="no_lwpex",      action='store_true', help="Do not use lwpex or lwpex2 to reset the chip. '-hot_reset' will be appended to all MODS command lines.")
    parser.add_argument("--verbose_stdout",dest="verbose_stdout",action='store_true', help="Send all output to stdout")
    parser.add_argument("--skip_dump_sweep",dest="skip_dump_sweep",action='store_true',help="Do not run MODS with -dump_sweep at the end of the script")
    parser.add_argument("--unittest",      dest="unittest",      action='store_true', help="Run UnitTest Suite")
    parser.add_argument("--version",       dest="version",       action='store_true', help="Print floorsweep.egg version")
    args = parser.parse_args(sys.argv[1:])

    if args.version:
        print("floorsweep.egg version: {}".format(VERSION))
    elif args.unittest:
        Datalogger.SetEnable(False)
        FsLib.InitFsLib()
        unittest.main(module=UnitTest, verbosity=1, argv=[sys.argv[0]])
    else:
        fs = Floorsweep(args)
        fs.Run()
