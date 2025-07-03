# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import argparse
import os
import platform
import sys
import shutil

from fatool_libs import Datalogger
from fatool_libs import Utilities
from fatool_libs import Exceptions
from fatool_libs import GpuResetHelper
from fatool_libs import FailureStrategy

class FATool(object):

    VERSION = '2.6.6'

    def __init__(self, args):
        Datalogger.InitLog(args.fatool_log)
        self.use_lwpex      = args.use_lwpex
        self.reset_on_init  = not args.no_reset_init
        self.args           = args.args
        self.sw_fusing_args = args.sw_fusing_args
        self.mods_log       = args.mods_log
        self.timeout        = args.timeout
        self.strategy_file  = args.strategy_file
        self.fail_action    = args.fail_action
        self.status_file    = 'mods.status'
        self.direct_strategy_mode = args.direct_strategy_mode
        Utilities.SetVerboseStdout(args.verbose_stdout)
        Utilities.SetModsLogLocation(self.mods_log)
        Utilities.SetStatusFileLocation(self.status_file)

    def Run(self):
        '''
        Run the main program exelwtion
        '''
        Datalogger.PrintLog('Python version: {}'.format(platform.python_version()))
        Datalogger.PrintLog('Running FA tool version: {}'.format(self.VERSION))
        Datalogger.PrintLog('FA tool command line : {}'.format(' '.join(sys.argv)))

        if '-loops ' in self.args:
            Datalogger.PrintLog('WARNING: -loops is not supported')
        if '-l ' in self.args:
            Datalogger.PrintLog('ERROR: -l <path> is not supported. Use --mods_log <path> to set the log path instead.')
            sys.exit(2)
        self.ExpandFsArgFile()
        if self.strategy_file:
            if not os.path.isfile(self.strategy_file):
                Datalogger.PrintLog('ERROR: Strategy file {} not found'.format(self.strategy_file))
                sys.exit(2)
            Datalogger.PrintFile(self.strategy_file, "Strategy file")

        self.fail_strat = FailureStrategy.FailureStrategy(self.strategy_file,   \
                                                          self.args + (' -reset_device' if not self.use_lwpex else ''),\
                                                          self.mods_log,        \
                                                          self.status_file,     \
                                                          self.timeout)
        Utilities.SetFailureStrategyObj(self.fail_strat)
        if self.fail_action >= 0:
            if not self.fail_strat.SetActionOnFailure(self.fail_action):
               sys.exit(2)
        if self.sw_fusing_args:
            if not self.fail_strat.InitSwFusing(self.mods_log, self.sw_fusing_args):
               sys.exit(2)

        action = self.fail_strat.GetActionOnFailure()
        if '-run_on_error' in self.args and action > 0:
            Datalogger.PrintLog('ERROR: -run_on_error not compatible with action on failure {}'.format(action))
            sys.exit(2)
        command_string    = './mods -a -l {} {}'.format(self.mods_log, self.args)
        error_info = Utilities.ErrorInfo(0, 'ok')
        try:
            GpuResetHelper.InitGpuResetHelper(self.use_lwpex, self.reset_on_init, self.args, self.mods_log, self.timeout)
            self.fail_strat.ApplySwFusing()
            if (not self.direct_strategy_mode):
                error_info = Utilities.ModsCall(command_string, self.timeout)
                if error_info.error_code != 0:
                    if error_info.error_code == 5:
                        Datalogger.PrintLog('Bad MODS command line argument. Exiting !')
                    elif error_info.error_code == 150:
                        Datalogger.PrintLog('Test array empty. Exiting !')
                    else:
                        error_info = self.fail_strat.ActOnFailure(error_info)
            else:
                error_info = self.fail_strat.ActOnFailure(self.fail_strat.DirectApplyStrategyErrInfo)
        except Exceptions.FAException as ex:
            error_info.error_code = ex.exit_code
            error_info.error_msg = 'Exception caught'
            Datalogger.PrintLog('Exception caught. Exiting !')

        self.GenerateFsReport()
        self.PrintFailedTestInfo()
        self.Final(error_info)

    def PassExit(self):
        '''
        The program passed. Print out the pass banner.
        '''
        Datalogger.PrintLog('\033[1;39;42m                                        \033[0m')
        Datalogger.PrintLog('\033[1;39;42m #######     ####     ######    ######  \033[0m')
        Datalogger.PrintLog('\033[1;39;42m ########   ######   ########  ######## \033[0m')
        Datalogger.PrintLog('\033[1;39;42m ##    ##  ##    ##  ##     #  ##     # \033[0m')
        Datalogger.PrintLog('\033[1;39;42m ##    ##  ##    ##   ###       ###     \033[0m')
        Datalogger.PrintLog('\033[1;39;42m ########  ########    ####      ####   \033[0m')
        Datalogger.PrintLog('\033[1;39;42m #######   ########      ###       ###  \033[0m')
        Datalogger.PrintLog('\033[1;39;42m ##        ##    ##  #     ##  #     ## \033[0m')
        Datalogger.PrintLog('\033[1;39;42m ##        ##    ##  ########  ######## \033[0m')
        Datalogger.PrintLog('\033[1;39;42m ##        ##    ##   ######    ######  \033[0m')
        Datalogger.PrintLog('\033[1;39;42m                                        \033[0m')
        exit(0)

    def FailExit(self):
        '''
        The program failed somewhere. Print out the fail banner.
        '''
        Datalogger.PrintLog('\033[1;39;41m                                        \033[0m')
        Datalogger.PrintLog('\033[1;39;41m #######     ####    ########  ###      \033[0m')
        Datalogger.PrintLog('\033[1;39;41m #######    ######   ########  ###      \033[0m')
        Datalogger.PrintLog('\033[1;39;41m ##        ##    ##     ##     ###      \033[0m')
        Datalogger.PrintLog('\033[1;39;41m ##        ##    ##     ##     ###      \033[0m')
        Datalogger.PrintLog('\033[1;39;41m #######   ########     ##     ###      \033[0m')
        Datalogger.PrintLog('\033[1;39;41m #######   ########     ##     ###      \033[0m')
        Datalogger.PrintLog('\033[1;39;41m ##        ##    ##     ##     ###      \033[0m')
        Datalogger.PrintLog('\033[1;39;41m ##        ##    ##  ########  ######## \033[0m')
        Datalogger.PrintLog('\033[1;39;41m ##        ##    ##  ########  ######## \033[0m')
        Datalogger.PrintLog('\033[1;39;41m                                        \033[0m')
        exit(1)

    def Final(self, error_info):
        '''
        Collect the results of the program
        '''
        Datalogger.PrintLog('\nError Code = {:012d} ({})'.format(error_info.error_code, error_info.error_msg))
        if error_info.GetErrorCode() == self.fail_strat.GetMarginalPassRc():
            Datalogger.PrintLog('MARGINAL PASS\n')
        if error_info.GetErrorCode() == self.fail_strat.GetDirectApplyStrategyRc():
            Datalogger.PrintLog('Bypass the first run and directly apply the failure strategy, but still failed\n')
        if error_info.error_code == 0:
            self.PassExit()
        else:
            self.FailExit()

    # Need to expand the arg file for future text processing
    def ExpandFsArgFile(self):
        # fb_fb.arg and fs.arg are the output of floorsweep.egg
        if '@fb_fb.arg' or '@fs.arg' in self.args:
            args_lst = self.args.split(' ')
            arg_file_indexes_lst = [index for (index, value) in enumerate(args_lst) if \
                value == '@fb_fb.arg' or value == '@fs.arg']
            for index in arg_file_indexes_lst:
                arg_file_name = args_lst[index]
                arg_file_name = arg_file_name[1:]
                if not os.path.isfile(arg_file_name):
                    continue
                with open(arg_file_name, 'r') as f:
                    arg_file_content = f.read()
                    args_lst[index] = arg_file_content
            self.args = ' '.join(args_lst).strip(' ')

    # Genertate fa_fs.arg file and dump sweep.json.
    # fa_fs.arg file includes includes original masks + the ones found defective by FATOOL
    # If there's already a sweep.json in the runspace which is generated by floorsweep.egg
    # FATOOL will rename it to old_sweep.json. Then FATOOL use generate_sweep_json to generate
    # an updated sweep.json which includes original + the ones found defective by FATOOL
    def GenerateFsReport(self):
        bad_fs_unit_info = FailureStrategy._bad_fs_unit_info
        if len(bad_fs_unit_info) == 0:
            return
        if os.path.isfile('sweep.json'):
            shutil.move('sweep.json', 'old_sweep.json')

        additional_args = ' -disable_passfail_msg -notest -no_gold -generate_sweep_json' +\
                        (' -reset_device' if self.reset_on_init else '')
        reset_args = self.args.replace('gpugen.js', 'gputest.js')
        old_dump_cmd = './mods -a -l {} {} {}'.format(self.mods_log, reset_args, additional_args)
        # new_dump_command will contain masks in original cmd line + masks of defective found by FATOOL
        new_dump_cmd = old_dump_cmd
        # Add all FS Mask stored in bad_fs_unit_info to old_dump_cmd
        for fs_unit_info in bad_fs_unit_info:
            lwr_component = fs_unit_info.get('component', '')
            lwr_groupIdx = fs_unit_info.get('groupIdx', 0)
            lwr_element_enable_mask = fs_unit_info.get('element_enable_mask', 0)
            new_dump_cmd = FailureStrategy.GenerateNewFsCmd(lwr_component, new_dump_cmd, lwr_groupIdx, lwr_element_enable_mask)
        Datalogger.PrintLog('... Running notest with -generate_sweep_json to dump FS info to sweep.json')
        # This cmd will generate sweep.json in the runspace, which includes original + the ones found defective by FATOOL
        Utilities.ModsCall(new_dump_cmd, self.timeout, False)

        (new_fs_arg_str, new_dump_cmd_without_fs) = FailureStrategy.ExtractFsMaskFromOldCommand(new_dump_cmd)
        if (len(new_fs_arg_str)):
            with open("fa_fs.arg", "w+") as fs_arg:
                fs_arg.write("-floorsweep {}".format(new_fs_arg_str))

    def PrintFailedTestInfo(self):
         Datalogger.PrintLog("Failed test info is listed below:")
         for test_info_str in FailureStrategy._fail_test_info:
            test_info_str_lst = test_info_str.split(' ')
            if (len(test_info_str_lst) == 2):
                Datalogger.PrintLog("Test Number: {}, Pstate: {}".format(test_info_str_lst[0], test_info_str_lst[1]))

if __name__ == '__main__':

    parser = argparse.ArgumentParser(description='FA Tool version {} (running with Python version {})'.format(FATool.VERSION, platform.python_version()))
    parser.add_argument('--mods_args',             dest='args',                  type=str, default='',           help='Args for mods')
    parser.add_argument('--sw_fusing_args',        dest='sw_fusing_args',        type=str, default='',           help='Args for SW fusing run, if required')
    parser.add_argument('--timeout_s',             dest='timeout',               type=int, default=30,           help='Default timeout per MODS test in seconds')
    parser.add_argument('--use_lwpex',             dest='use_lwpex',             action='store_true',            help='Use lwpex or lwpex2 to reset the chip, instead of appending -reset_device to MODS command lines.')
    parser.add_argument('--no_reset_on_init',      dest='no_reset_init',         action='store_true',            help='Do not use -reset_device while initializing the GPU reset using lwpex / in the first MODS run')
    parser.add_argument('--mods_log',              dest='mods_log',              type=str, default='mods.log',   help='Change location/name of each generated mods.log file')
    parser.add_argument('--fatool_log',            dest='fatool_log',            type=str, default='fatool.log', help='Name/location of the FA tool report')
    parser.add_argument('--strategy_file',         dest='strategy_file',         type=str, default='',           help='Name/location of the json file which contains details for failure strategy')
    parser.add_argument('--action_on_fail',        dest='fail_action',           type=int, default=-1,           help='Set/override the action on failure : 0(stop), 1(ignore and continue), 2(rerun failed), 3(conditional rerun all), 4(rerun all)')
    parser.add_argument("--version",               dest="version",               action='store_true',            help="Print fatool.egg version")
    parser.add_argument("--verbose_stdout",        dest="verbose_stdout",        action='store_true',            help="Send all output to stdout")
    parser.add_argument("--direct_strategy_mode",  dest="direct_strategy_mode",  action='store_true',            help="Bypass the first MODS run and directly apply the failure strategy based on the mods.status file in the runspace")

    # Print help if no arguments are specified
    if len(sys.argv) == 1:
        parser.print_help(sys.stderr)
        sys.exit(1)
    else:
        args = parser.parse_args(sys.argv[1:])
        fa = FATool(args)
        if args.version:
            print("Python version: {}".format(platform.python_version()))
            print("FA Tool version: {}".format(FATool.VERSION))
        elif not args.args:
            print("ERROR : No MODS arguments specified")
            parser.print_help(sys.stderr)
            sys.exit(1)
        else:
            fa.Run()
