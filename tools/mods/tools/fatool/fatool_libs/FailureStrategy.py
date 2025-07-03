# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import json
import os
import re
import shutil
import sys

from . import Datalogger
from . import Exceptions
from . import Utilities
from . import GpuResetHelper

# _bad_fs_unit_info stores the info of the fs unit that has been already found as bad
# the format is like:
# _bad_fs_unit_info = [{
#   'testno': testno,
#   'component': component,
#   'groupIdx': groupIdx,  
#   'elementIdx': elementIdx, 
#   'element_enable_mask': element_enable_mask
# }]
_bad_fs_unit_info = []

#_failed_test_info is a set storing the fail test info. 
# The element of it is a string: "<testno> <pstate>".
# No duplicated element allowed.
_fail_test_info = set()

# Action to take on failure
class ActionOnFailure:
    Stop = 0
    IgnoreAndContinue = 1
    RerunFailed = 2
    ConditionalRerunAll = 3
    RerunAll = 4

class FailureStrategy(object):

    # Lwrrently setting it to one of the reserved RCs
    MarginalPassErrInfo = Utilities.ErrorInfo(905, "Marginal Pass after applying failure strategy")
    # This rc is used when the user wants to bypass the first run and directly apply the failure strategy, but still fail after applying all strategies
    # Set it to one of the reserved RCs.
    DirectApplyStrategyErrInfo = Utilities.ErrorInfo(904, "Bypass first run and directly apply failure strategy but still failed")
    IlwalidErrInfo = Utilities.ErrorInfo(-1, "Invalid Error Code")
    def __init__(self, strategy_file, args, mods_log, status_file, timeout):
        self.strategy_file      = strategy_file
        self.mods_args          = '-a -l {} {}'.format(mods_log, args)
        self.new_mods_args      = self.mods_args
        self.mods_status_file   = status_file
        self.timeout            = timeout
        self.strategy_data      = None
        self.sw_fusing_cmdline  = None

        if self.strategy_file and os.path.isfile(self.strategy_file):
            # Preprocess some of the FA strategy
            with open(strategy_file) as f:
                self.strategy_data = json.load(f)
            if 'ActionOnFailure' in self.strategy_data:
                self.on_failure = self.strategy_data['ActionOnFailure']
            else:
                self.on_failure = ActionOnFailure.Stop
            if 'SwFusingArgs' in self.strategy_data:
                if not self.InitSwFusing(mods_log, self.strategy_data['SwFusingArgs']):
                    sys.exit(2)
        else:
            self.on_failure = ActionOnFailure.Stop

    def SetActionOnFailure(self, action):
        # Only Actions allowed without a strategy file are [Stop, IgnoreAndContinue]
        if not self.strategy_file and action >= ActionOnFailure.RerunFailed:
            Datalogger.PrintLog('ERROR: Need strategy file for action on fail {}'.format(action))
            return False
        self.on_failure = action
        return True

    def GetActionOnFailure(self):
        return self.on_failure

    def InitSwFusing(self, mods_log, sw_fusing_args):
        if '-l ' in sw_fusing_args:
            Datalogger.PrintLog('ERROR: -l <path> is not supported for SW fusing args. Log is appended to the mods log')
            return False
        if 'fuseblow.js' not in sw_fusing_args:
            Datalogger.PrintLog('ERROR: fuseblow.js needs to be specified in SW fusing args')
            return False
        self.sw_fusing_cmdline = './mods -a -l {} {}'.format(mods_log, sw_fusing_args)
        return True

    def GetMarginalPassRc(self):
        return self.MarginalPassErrInfo.error_code

    def GetDirectApplyStrategyRc(self):
        return self.DirectApplyStrategyErrInfo.error_code

    def GetValueFromStatusFile(self, key):
        for numTries in range(0, 5):
            try:
                with open(self.mods_status_file) as f:
                    status = json.load(f)
                    if key in status:
                        return status[key]
                    break
            # In case file is in an intermediate state
            except ValueError:
                pass
        return ''

    def GetTestFromStatusFile(self):
        testno = self.GetValueFromStatusFile('testNum')
        if testno:
            return int(testno)
        return -1

    def GetPStateFromStatusFile(self):
        return self.GetValueFromStatusFile('pstate')

    def PrintFailedTestDetails(self):
        if os.path.isfile(self.mods_status_file):
            testno = self.GetTestFromStatusFile()
            pstate = self.GetPStateFromStatusFile()
            Datalogger.PrintLog('... Failure Status - TestNo:{}, PState:{}'.format(testno, pstate))

    def StoreFailedTestInfo(self):
        if os.path.isfile(self.mods_status_file):
            testno = self.GetTestFromStatusFile()
            pstate = self.GetPStateFromStatusFile()
            _fail_test_info.add("{} {}".format(testno, pstate))

    def GetTestIdxFromStatusFile(self):
        test_idx = self.GetValueFromStatusFile('testIndex')
        test_len = self.GetValueFromStatusFile('numTests')
        return test_idx, test_len

    def IsTestingComplete(self, last_exit_code):
        if last_exit_code == 150: # No tests runs
            return True
        test_idx, test_len = self.GetTestIdxFromStatusFile()
        if test_idx and test_len:
            return int(test_idx) == int(test_len)
        return False

    def IsModsOnLastTest(self):
        test_idx, test_len = self.GetTestIdxFromStatusFile()
        if test_idx and test_len:
            return int(test_idx) == (int(test_len) - 1)
        return False

    def GetTimeoutSec(self, testno):
        if not self.strategy_data or 'TimeoutSec' not in self.strategy_data:
            return self.timeout
        timeouts = self.strategy_data['TimeoutSec']
        return timeouts[str(testno)] if str(testno) in timeouts else self.timeout

    def GetStrategyIndex(self, testno):
        idx = -1
        default_idx = -1
        if 'Strategies' not in self.strategy_data:
            return -1
        for strategy in self.strategy_data['Strategies']:
            idx += 1
            # Record the index of the default for later
            if 'TestFilter' not in strategy or strategy['TestFilter'] == "default":
                default_idx = idx
                continue
            # If test is found in TestFilter, return the index
            if testno in list(map(int, strategy['TestFilter'])):
                return idx
        return default_idx

    def ApplySwFusing(self):
        if self.sw_fusing_cmdline == None:
            return
        Datalogger.PrintLog('SW fusing the chip')
        sw_fusing_error_info = Utilities.ModsCall(self.sw_fusing_cmdline, self.timeout)
        sw_fusing_error_code = sw_fusing_error_info.error_code
        if sw_fusing_error_code != 0:
            # There is no point continuing if we can't get to the correct fusing state
            Datalogger.PrintLog('SW fusing failed')
            raise Exceptions.FAException(sw_fusing_error_code)

    # Wrapper function
    def ApplyFailureStrategy(self):
        if not os.path.isfile(self.mods_status_file):
            return self.IlwalidErrInfo
        # Save a copy of the status file
        backup_status_file = self.mods_status_file + '.bkup'
        shutil.copy2(self.mods_status_file, backup_status_file)
        # Apply strategies
        error_info = self.ApplyFailureStrategyInner()
        # Restore the copy
        shutil.copy2(backup_status_file, self.mods_status_file)
        os.remove(backup_status_file)
        return error_info

    # TODO: Resume Offset is still pending
    # TODO: This function seems too long. Maybe will move different options to different functions.
    def ApplyFailureStrategyInner(self):
        testno = self.GetTestFromStatusFile()
        if testno <= 0:
            Datalogger.PrintLog('Warning: Invalid testno {} from status file. Ignoring', testno)
            return self.IlwalidErrInfo
        strat_idx = self.GetStrategyIndex(testno)
        if strat_idx == -1:
            Datalogger.PrintLog('Could not find failure strategy for testno {}'.format(testno))
            return self.IlwalidErrInfo
        test_strategy = self.strategy_data['Strategies'][strat_idx]
        if 'Ignore' in test_strategy and test_strategy['Ignore']:
            Datalogger.PrintLog('Test {} specified to be ignored. Ignoring'.format(testno))
            return self.IlwalidErrInfo
        resume_offset = 0
        mods_args = "{} -rerun_last".format(self.new_mods_args)
        addn_args = ''
        run_on_success = False
        strategy_success = False
        last_success_error_info =  Utilities.ErrorInfo(0, 'ok')

        if 'RunOnStrategySuccess' in test_strategy and test_strategy['RunOnStrategySuccess']:
            Datalogger.PrintLog('RunOnStrategySuccess enabled for test {}'.format(testno))
            run_on_success = True

        valid_clk_domains = ['dram', 'gpc', 'host', 'ltc', 'xbar', 'sys', 'hub',\
                             'leg', 'pwr', 'msd', 'lwd', 'disp', 'pex']

        for s in test_strategy['Strategy']:
            if 'AdditionalArgs' in s:
                addn_args = s['AdditionalArgs']
            # Rerun with additional arguments only
            if 'Param' not in s or s['Param'] == 'none':
                GpuResetHelper.ResetGpu()
                self.ApplySwFusing()
                command_string    = './mods {} {}'.format(mods_args, addn_args)
                new_error_info = Utilities.ModsCall(command_string, self.timeout)
                if new_error_info.error_code == 0:
                    strategy_success = True
                    Datalogger.PrintLog('... Strategy is Successful for test {}. With additional args: {}.'\
                                .format(testno, addn_args))
                    if (run_on_success):
                        Datalogger.PrintLog('... Continue to run next strategy for test {}'.format(testno))
                        last_success_error_info = new_error_info
                        continue
                    else:
                        return new_error_info
            # Rerun with floorswpeeing
            elif s['Param'] == 'floorsweep':
                dom = ''
                fs_loop = 1
                if 'Domain' in s:
                    dom = s['Domain']
                    # TODO: Now we only support FS TPC(i.e. disable one TPC each time)
                    # Will support more FS options later
                    if dom not in ['tpc', 'fbp', 'gpc']:
                        Datalogger.PrintLog('Invalid Domain {}'.format(dom))
                        Datalogger.PrintLog('Ignoring strategy')
                        continue
                if 'Loop' in s:
                    fs_loop = s['Loop']
                gpc_max_count = GpuResetHelper._gpc_max_count
                gpc_mask = GpuResetHelper._gpc_mask
                tpc_mask_str_lst = GpuResetHelper._tpc_mask_str_lst
                fbp_max_count = GpuResetHelper._fbp_max_count
                fbp_mask = GpuResetHelper._fbp_mask
                l2_mask_str_lst = GpuResetHelper._l2_mask_str_lst
                if gpc_max_count == 0 or gpc_mask == -1 or tpc_mask_str_lst == [] or fbp_max_count == 0 or fbp_mask == -1 or l2_mask_str_lst == []:
                    Datalogger.PrintLog('Can not get the default GPC/TPC/FBP mask')
                    Datalogger.PrintLog('Ignoring strategy')
                    continue
                if not dom:
                    Datalogger.PrintLog('FS domain not specified')
                    Datalogger.PrintLog('Ignoring strategy')
                    continue
                global _bad_fs_unit_info
                if dom == 'tpc':
                    weak_tpc_found = False
                    for gpc in range(gpc_max_count):
                        if ((1 << gpc) & gpc_mask):
                            tpc_mask = "{:b}".format(int(tpc_mask_str_lst[gpc_max_count - 1 - gpc], 16))
                            for i in range(len(tpc_mask)):
                                if (tpc_mask[i] == '1'):
                                    tpc_index = len(tpc_mask) - 1 - i
                                    fs_tpc_mask = tpc_mask[:i] + '0' + tpc_mask[i+1:]
                                    fs_tpc_mask = int(fs_tpc_mask, 2)
                                    command_string    = './mods {} {} -loops {}'.format(mods_args, addn_args, fs_loop)
                                    command_string += " -adjust_fs_args" if "-adjust_fs_args" not in command_string else ""
                                    # if the current tpc has been found as bad in previous test, skip it
                                    if CheckIfExistBadFsUnitInfo('tpc', gpc, tpc_index):
                                        continue
                                    command_string = GenerateNewFsCmd('tpc', command_string, gpc, fs_tpc_mask)
                                    if command_string == -1:
                                        continue
                                    GpuResetHelper.ResetGpu()
                                    self.ApplySwFusing()
                                    Datalogger.PrintLog("... Perform TPC floorsweeping shmoo, disable GPC{}TPC{}".format(gpc, tpc_index))
                                    new_error_info = Utilities.ModsCall(command_string, self.timeout)
                                    if new_error_info.error_code == 0:
                                        weak_tpc_found = True
                                        strategy_success = True
                                        Datalogger.PrintLog('... Strategy is Successful for test {}. Weak TPC found. It is in GPC {}, TPC {}. The enabled TPC mask for that GPC is {:#x}'\
                                        .format(testno, gpc, tpc_index, fs_tpc_mask))
                                        AddToBadFsUnitInfo(testno, 'tpc', gpc, tpc_index, fs_tpc_mask)
                                        if (run_on_success):
                                            Datalogger.PrintLog('... Continue to run next strategy for test {}'.format(testno))
                                            last_success_error_info = new_error_info
                                            break
                                        else:
                                            return new_error_info
                        # if already found a weak TPC, need to break the for loop to apply next strategy
                        if (weak_tpc_found and run_on_success):
                            break
                    if (not weak_tpc_found):
                        Datalogger.PrintLog('... Strategy is unsuccessful for test {}. Can not find weak TPC.'\
                            .format(testno))
                if dom == 'gpc':
                    weak_gpc_found = False
                    gpc_mask_str = "{:b}".format(gpc_mask)
                    for i in range(len(gpc_mask_str)):
                        if (gpc_mask_str[i] == '1'):
                            gpc_index = len(gpc_mask_str) - 1 - i
                            fs_gpc_mask = gpc_mask_str[:i] + '0' + gpc_mask_str[i+1:]
                            fs_gpc_mask = int(fs_gpc_mask, 2)
                            command_string    = './mods {} {} -loops {}'.format(mods_args, addn_args, fs_loop)
                            command_string += " -adjust_fs_args" if "-adjust_fs_args" not in command_string else ""
                            # if the current gpc has been found as bad in previous test, skip it
                            if CheckIfExistBadFsUnitInfo('gpc', gpc_index, gpc_index):
                                continue
                            command_string = GenerateNewFsCmd('gpc', command_string, 0, fs_gpc_mask)
                            if command_string == -1:
                                continue
                            GpuResetHelper.ResetGpu()
                            self.ApplySwFusing()
                            Datalogger.PrintLog("... Perform GPC floorsweeping shmoo, disable GPC{}".format(gpc_index))
                            new_error_info = Utilities.ModsCall(command_string, self.timeout)
                            if new_error_info.error_code == 0:
                                weak_tpc_found = True
                                strategy_success = True
                                Datalogger.PrintLog('... Strategy is Successful for test {}. Weak GPC found. It is GPC {}. The enabled GPC Mask is {:#x}'\
                                .format(testno, gpc_index, fs_gpc_mask))
                                AddToBadFsUnitInfo(testno, 'gpc', gpc_index, gpc_index, fs_gpc_mask)
                                if (run_on_success):
                                    Datalogger.PrintLog('... Continue to run next strategy for test {}'.format(testno))
                                    last_success_error_info = new_error_info
                                    break
                                else:
                                    return new_error_info
                    if (not weak_gpc_found):
                        Datalogger.PrintLog('... Strategy is unsuccessful for test {}. Can not find weak GPC.'\
                            .format(testno))
                if dom == 'fbp':
                    weak_fbp_found = False
                    fbp_mask_str = "{:b}".format(fbp_mask)
                    for i in range(len(fbp_mask_str)):
                        if (fbp_mask_str[i] == '1'):
                            fbp_index = len(fbp_mask_str) - 1 - i
                            fs_fbp_mask = fbp_mask_str[:i] + '0' + fbp_mask_str[i+1:]
                            fs_fbp_mask = int(fs_fbp_mask, 2)
                            command_string    = './mods {} {} -loops {}'.format(mods_args, addn_args, fs_loop)
                            command_string += " -adjust_fs_args" if "-adjust_fs_args" not in command_string else ""
                            # if the current fbp has been found as bad in previous test, skip it
                            if CheckIfExistBadFsUnitInfo('fbp', fbp_index, fbp_index):
                                continue
                            command_string = GenerateNewFsCmd('fbp', command_string, 0, fs_fbp_mask)
                            if command_string == -1:
                                continue
                            GpuResetHelper.ResetGpu()
                            self.ApplySwFusing()
                            Datalogger.PrintLog("... Perform FBP floorsweeping shmoo, disable FBP{}".format(fbp_index))
                            new_error_info = Utilities.ModsCall(command_string, self.timeout)
                            if new_error_info.error_code == 0:
                                weak_tpc_found = True
                                strategy_success = True
                                Datalogger.PrintLog('... Strategy is Successful for test {}. Weak FBP found. It is FBP {}. The enabled FBP Mask is {:#x}'\
                                .format(testno, fbp_index, fs_fbp_mask))
                                AddToBadFsUnitInfo(testno, 'fbp', fbp_index, fbp_index, fs_fbp_mask)
                                if (run_on_success):
                                    Datalogger.PrintLog('... Continue to run next strategy for test {}'.format(testno))
                                    last_success_error_info = new_error_info
                                    break
                                else:
                                    return new_error_info
                    if (not weak_fbp_found):
                        Datalogger.PrintLog('... Strategy is unsuccessful for test {}. Can not find weak FBP.'\
                            .format(testno))
            # Rerun with changes to perf point
            else:
                dom = ''
                if 'Domain' in s:
                    dom = s['Domain']
                    if dom not in valid_clk_domains:
                        Datalogger.PrintLog('Invalid Domain {}'.format(dom))
                        Datalogger.PrintLog('Ignoring strategy')
                        continue
                if s['Param'] == 'freq':
                    if not dom:
                        Datalogger.PrintLog('Clk domain not specified, required for Param freq')
                        Datalogger.PrintLog('Ignoring strategy')
                        continue
                    if 'UsePct' in s and s['UsePct']:
                        option = ' -freq_clk_domain_offset_pct ' + dom
                    else:
                        option = ' -freq_clk_domain_offset_khz ' + dom
                elif s['Param'] == 'volt':
                    rail = 'lwvdd'
                    if 'Rail' in s:
                        if s['Rail'] == 'msvdd':
                            Datalogger.PrintLog('MSVDD rail not yet supported by MODS')
                            Datalogger.PrintLog('Ignoring strategy')
                            continue
                        elif s['Rail'] != 'lwvdd':
                            Datalogger.PrintLog('Invalid Rail {}'.format(s['Rail']))
                            Datalogger.PrintLog('Ignoring strategy')
                            continue
                    if dom:
                        option = ' -rail_clk_domain_offset_mv {} {}'.format(rail, dom)
                    else:
                        option = ' -rail_offset_mv {}'.format(rail)
                else:
                    Datalogger.PrintLog('Invalid Strategy Param {} for testno {}'.format(s['Param'], testno))
                    Datalogger.PrintLog('Ignoring strategy');
                    continue
                if 'StartingOffset' not in s:
                    Datalogger.PrintLog('No starting offset specified for strategy {} (test {})'.format(s['Param'], testno));
                    Datalogger.PrintLog('Ignoring strategy');
                    continue
                offset        = int(s['StartingOffset'])
                ending_offset = offset
                step_size     = ending_offset
                if 'EndingOffset' in s:
                    ending_offset = int(s['EndingOffset'])
                if 'StepSize' in s:
                    step_size = int(s['StepSize'])
                if (step_size > 0 and offset > ending_offset) or\
                   (step_size < 0 and offset < ending_offset):
                    Datalogger.PrintLog('Offset set [{}, {}, {}] invalid'.format(offset,\
                                                                                 step_size,\
                                                                                 ending_offset));
                    Datalogger.PrintLog('Ignoring strategy');
                    continue
                shmoo_success = False
                while (step_size > 0 and offset <= ending_offset) or\
                      (step_size < 0 and offset >= ending_offset):
                    new_args = mods_args + option + ' {}'.format(offset)
                    GpuResetHelper.ResetGpu()
                    self.ApplySwFusing()
                    command_string    = './mods {} {}'.format(new_args, addn_args)
                    new_error_info = Utilities.ModsCall(command_string, self.timeout)
                    if new_error_info.error_code == 0:
                        strategy_success = True
                        shmoo_success = True
                        Datalogger.PrintLog('... Strategy is Successful for test {}. Shmoo Option is{}. Offset is {}.'\
                                .format(testno, option, offset))
                        if run_on_success:
                            Datalogger.PrintLog('... Continue to run next strategy for test {}'.format(testno))
                            last_success_error_info = new_error_info
                            break
                        else:
                            return new_error_info
                    offset += step_size
                if (not shmoo_success):
                    Datalogger.PrintLog('... Strategy is unsuccessful for test {}. Shmoo Option is{}.'\
                            .format(testno, option))

        if strategy_success:
            Datalogger.PrintLog('... RunOnStrategySuccess enabled. One or more strategies is successful for test {}.'.format(testno))
            return last_success_error_info

        Datalogger.PrintLog('... Used all failure strategies for test {}, still failed'.format(testno))
        return self.IlwalidErrInfo

    # Return the new RC in case of pass and old rc in case of fail by default
    def ReportErrInfo(self, old_error_info, new_error_info):
        # when the failure strategy succeeds, replace the old rc with the marginalPassRc
        if new_error_info.error_code == 0:
            new_error_info.SetErrorCode(self.MarginalPassErrInfo.error_code)
            new_error_info.error_msg = self.MarginalPassErrInfo.error_msg
            return new_error_info
        else:
            return old_error_info

    def ActOnFailure(self, first_error_info):

        # STOP: Stop at a failure and just return
        if self.on_failure == ActionOnFailure.Stop:
            return first_error_info

        # IGNORE_AND_CONTINUE: Ignore the failed test and continue with the rest
        elif self.on_failure == ActionOnFailure.IgnoreAndContinue:
            error_code      = first_error_info.error_code
            new_args       = self.mods_args + ' -resume 1'
            command_string = './mods {}'.format(new_args)
            while not self.IsTestingComplete(error_code):
                # Failed on the last test, no other test to run after this
                if self.IsModsOnLastTest():
                    break
                GpuResetHelper.ResetGpu()
                error_info = Utilities.ModsCall(command_string, self.timeout)
            return first_error_info

        # APPLY_STRATEGY_AND_RERUN_FAILED: Apply failure strategy until the test passes and return
        elif self.on_failure == ActionOnFailure.RerunFailed:
            new_error_info = self.ApplyFailureStrategy()
            return self.ReportErrInfo(first_error_info, new_error_info)

        # APPLY_STRATEGY_AND_RERUN_ALL: Apply failure strategy and rerun everything
        elif self.on_failure == ActionOnFailure.RerunAll or \
             self.on_failure == ActionOnFailure.ConditionalRerunAll:
            new_args = self.mods_args + ' -resume 1'
            command_string    = './mods {}'.format(new_args)
            new_error_info = first_error_info
            failure_strat_failed = False
            while True:
                new_error_info = self.ApplyFailureStrategy()
                if new_error_info.error_code != 0:
                    failure_strat_failed = True
                    if self.on_failure == ActionOnFailure.ConditionalRerunAll:
                        Datalogger.PrintLog('Returning before running all tests')
                        break
                # Failed on the last test, no other test to run after this
                if self.IsModsOnLastTest():
                    break
                GpuResetHelper.ResetGpu()
                global _bad_fs_unit_info
                # add the FS masks of the existing FS info stored in _bad_fs_unit_info
                for fs_unit_info in _bad_fs_unit_info:
                    lwr_component = fs_unit_info.get('component', '')
                    lwr_groupIdx = fs_unit_info.get('groupIdx', 0)
                    lwr_element_enable_mask = fs_unit_info.get('element_enable_mask', 0)
                    command_string = GenerateNewFsCmd(lwr_component, command_string, lwr_groupIdx, lwr_element_enable_mask)
                    self.new_mods_args = GenerateNewFsCmd(lwr_component, self.new_mods_args, lwr_groupIdx, lwr_element_enable_mask)
                new_error_info = Utilities.ModsCall(command_string, self.timeout)
                if self.IsTestingComplete(new_error_info.error_code):
                    break

            if failure_strat_failed:
                return first_error_info
            else:
                return self.ReportErrInfo(first_error_info, new_error_info)

        else:
            Datalogger.PrintLog('Invalid action on failure')
            return first_error_info

# Extract FsMask from old command and remove all FS args in old command
# Return value is (fs_arg_str, old_command_without_fs)
def ExtractFsMaskFromOldCommand(old_command):
    old_command_lst = old_command.split(" ")
    # Find the indexes of "-floorsweep"
    fs_arg_indexes_lst = [index for (index, value) in enumerate(old_command_lst) if value == "-floorsweep"]

    # Extract all fs mask and combine them together
    fs_arg_str = ""
    for index in fs_arg_indexes_lst:
        if (index + 1) < len(old_command_lst):
            fs_arg_str += old_command_lst[index + 1]
            fs_arg_str += ":"
    fs_arg_str = fs_arg_str.rstrip(':')

    # Remove all fs args in the original command
    old_command_without_fs = ""
    for i in range(len(old_command_lst)):
        if (i in fs_arg_indexes_lst or (i - 1) in fs_arg_indexes_lst):
            continue
        else:
            old_command_without_fs += old_command_lst[i]
            old_command_without_fs += ' '
    old_command_without_fs = old_command_without_fs.rstrip(' ')

    return (fs_arg_str, old_command_without_fs)

# Use the required floorsweeping info to create new floorsweep mask and combine with existing mask
def GenerateNewFsCmd(fsunit, old_command, req_group_idx, req_element_enable_mask):
    (fs_arg_str, old_command_without_fs) = ExtractFsMaskFromOldCommand(old_command)
    fs_arg_str_lst = fs_arg_str.split(":")
    if fsunit == "tpc":
        replace_mask = False
        if "gpc_tpc_disable" in fs_arg_str_lst:
            Datalogger.PrintLog('[Warning] gpc_tpc_disable appears in mods command. FATOOL only support enable FS Mask. Ignore strategy')
            return -1
        if "gpc_tpc_enable" in fs_arg_str_lst:
            # find the indexes of "gpc_tpc_enable"
            gpc_tpc_enable_mask_idx_lst = [index for (index, value) in enumerate(fs_arg_str_lst) if value == "gpc_tpc_enable"]
            for index in gpc_tpc_enable_mask_idx_lst:
                if index + 2 < len(fs_arg_str_lst):
                    lwr_group_idx = int(fs_arg_str_lst[index + 1], 10)
                    lwr_element_enable_mask = int(fs_arg_str_lst[index + 2], 16)
                    # combine the current disable mask and the required mask
                    if lwr_group_idx == req_group_idx:
                        final_element_enable_mask = lwr_element_enable_mask & req_element_enable_mask
                        fs_arg_str_lst[index + 2] = "{:#x}".format(final_element_enable_mask)
                        replace_mask = True
        # if no existing mask of the current tpc, add the required enabled mask
        if not replace_mask:
            fs_arg_str_lst.append("gpc_tpc_enable")
            fs_arg_str_lst.append("{}".format(req_group_idx))
            fs_arg_str_lst.append("{:#x}".format(req_element_enable_mask))

  
    elif fsunit == "gpc" or fsunit == "fbp":
        unit_enable_arg = fsunit + "_enable"
        unit_disable_arg = fsunit + "_disable"
        replace_mask = False
        if unit_disable_arg in fs_arg_str_lst:
            Datalogger.PrintLog('[Warning] {} appears in mods command. FATOOL only support enable FS Mask. Ignore strategy'.format(unit_disable_arg))
            return -1
        if unit_enable_arg in fs_arg_str_lst:
            unit_enable_mask_idx_lst = [index for (index, value) in enumerate(fs_arg_str_lst) if value == unit_enable_arg]
            for index in unit_enable_mask_idx_lst:
                if index + 1 < len(fs_arg_str_lst):
                    lwr_element_enable_mask = int(fs_arg_str_lst[index + 1], 16)
                    final_element_enable_mask = lwr_element_enable_mask & req_element_enable_mask
                    fs_arg_str_lst[index + 1] = "{:#x}".format(final_element_enable_mask)
                    replace_mask = True
        # if no existing mask of the current gpc/fbp, add the required enabled mask
        if not replace_mask:
            fs_arg_str_lst.append("{}".format(unit_enable_arg))
            fs_arg_str_lst.append("{:#x}".format(req_element_enable_mask))
    
    else:
            Datalogger.PrintLog('[Warning] FATOOL does not support {} floorsweep. Ignore strategy'.format(fsunit))
            return -1

    new_fs_arg_str = ":".join(fs_arg_str_lst).lstrip(":")
    new_command = old_command_without_fs + " -floorsweep " + new_fs_arg_str
    return new_command

# Check whether the current unit already exists in _bad_fs_unit_info
def CheckIfExistBadFsUnitInfo(checked_component, checked_groupIdx, checked_elementIdx):
    global _bad_fs_unit_info
    for fs_unit_info in _bad_fs_unit_info:
        lwr_component = fs_unit_info.get('component', '')
        lwr_groupIdx = fs_unit_info.get('groupIdx', 0)
        lwr_elementIdx = fs_unit_info.get('elementIdx', 0)
        if (lwr_component == checked_component and lwr_groupIdx == checked_groupIdx and lwr_elementIdx == checked_elementIdx):
            return True
    return False

def AddToBadFsUnitInfo(testno, component, groupIdx, elementIdx, element_enable_mask):
    global _bad_fs_unit_info
    _bad_fs_unit_info.append({'testno': testno, 'component': component, 'groupIdx': groupIdx, 'elementIdx': elementIdx, 'element_enable_mask': element_enable_mask})

