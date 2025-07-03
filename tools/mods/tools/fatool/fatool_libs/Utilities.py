# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

# TODO:
# A lot of this file is a copy of Utilities.py from the floorsweeping script library
# There are changes which involve monitoring the status file, but most of
# the rest is just a copy. Should probably combine common functionality from
# both the scripts into a single library file.

import shlex
import subprocess
import time
import re
import os
import select
import struct
import fcntl
import termios
import sys
import errno

from . import Datalogger
from . import Exceptions

_mods_log_location = ''
_verbose_stdout = False
_status_file_location = ''
_fs_obj = None

def SetStatusFileLocation(location):
    global _status_file_location
    _status_file_location = location

def GetStatusFileLocation():
    global _status_file_location
    return _status_file_location

def SetModsLogLocation(location):
    global _mods_log_location
    _mods_log_location = location

def GetModsLogLocation():
    global _mods_log_location
    return _mods_log_location

def SetVerboseStdout(val):
    global _verbose_stdout
    _verbose_stdout = val

def GetVerboseStdout():
    global _verbose_stdout
    return _verbose_stdout

def SetFailureStrategyObj(fs):
    global _fs_obj
    _fs_obj = fs

def CheckSubprocessOutput(command_string):
    Datalogger.PrintLog('... Running "{}"'.format(command_string))
    command = shlex.split(command_string)
    try:
        return subprocess.check_output(command)
    except subprocess.CalledProcessError:
        Datalogger.PrintLog('... Received Command error')
        raise Exceptions.CommandError()

class ErrorInfo():
    def __init__(self, error_code, error_msg):
        self.error_code = error_code
        self.error_msg  = error_msg
    # Get the last three digits of the Complete error code.
    def GetErrorCode(self):
        return self.error_code % 1000
    # Only set the last three digits of the Complete error code
    def SetErrorCode(self, error_code):
        self.error_code = (self.error_code // 1000 * 1000) + (error_code % 1000)

class SubprocessStdout():
    def __init__(self, file_name):
        self.file_name = file_name
        self.file = None
    def __enter__(self):
        if not GetVerboseStdout():
            self.file = open(self.file_name, 'a')
        # else return None, which when passed to the Popen stdout argument
        # will cause it to use the default behavior, ie. printing to stdout
        return self.file
    def __exit__(self, type, value, traceback):
        if self.file:
            self.file.close()

def RunSubprocess(command_string, default_timeout):
    global _fs_obj

    Datalogger.PrintLog('... Running "{}"'.format(command_string))
    command = shlex.split(command_string)

    # While running the subprocess, we are also monitoring the process with 
    # a timeout - either the default timeout or the one obtained from the 
    # mods status file
    initial_modified_time = 0
    if os.path.isfile(_status_file_location):
        initial_modified_time = os.stat(_status_file_location).st_mtime
    process = None
    
    # Set timeout for running no_test
    setup_timeout = default_timeout
    try:
        with SubprocessStdout(Datalogger.GetLogName()) as data_log:
            process = subprocess.Popen(command, stdin=open(os.devnull), stdout=data_log, stderr=subprocess.STDOUT)
            start_time = time.time()
            return_code = process.poll()
            # While no test number is present or the status file is stale
            while return_code is None:
                status_file_exists = os.path.isfile(_status_file_location)
                if status_file_exists and\
                   (os.stat(_status_file_location).st_mtime != initial_modified_time):
                   break
                lwr_time = time.time()
                delta = lwr_time - start_time
                if delta > setup_timeout:
                    process.terminate()
                    return_code = 77
                    break
                time.sleep(1)
                return_code = process.poll()
            # MODS returned already (condition should be true only if no test was run)
            if return_code is not None:
                return return_code
            # Once we are here, MODS has started running a test (since the status file
            # has been modified)
            last_modified_time = 0
            test_start_time = time.time()
            while return_code is None:
                lwr_modified_time = os.stat(_status_file_location).st_mtime
                # Every time that the status file is modified, it means MODS started a new test
                # Will always evaluate to false the first time the loop is entered
                if last_modified_time != lwr_modified_time:
                    test_start_time = time.time()
                    last_modified_time = lwr_modified_time
                    lwr_testno = _fs_obj.GetTestFromStatusFile()
                    timeout = _fs_obj.GetTimeoutSec(lwr_testno)
                    test_idx, test_len = _fs_obj.GetTestIdxFromStatusFile()
                    Datalogger.PrintLog('... MODS is running test {}/{}. Timeout = {}s'.format(test_idx + 1, test_len, timeout))
                delta = time.time() - test_start_time
                if delta > timeout:
                    process.terminate()
                    Datalogger.PrintLog('... Timeout! [{}s] Process terminated. '.format(timeout))
                    return_code = 77
                    break
                time.sleep(1)
                return_code = process.poll()
    except KeyboardInterrupt:
        if process:
            process.terminate()
        raise
    return return_code

def ModsCall(command_string, timeout, print_fail_status = True):
    global _verbose_stdout
    return_code = 0
    exit_msg = ""
    error_info = ErrorInfo(0, 'ok')

    try:
        # Launch the MODS run as a subprocess
        return_code = RunSubprocess(command_string, timeout)

        if return_code == 77:
            # The MODS run was prematurely terminated because of a timeout
            exit_msg = '<TIMEOUT>'
        elif return_code == 1:
            # Go through the MODS log and search for the error code and error string
            error_code_match = re.compile('Error Code = (\S+) \(([^)]+)\)').match
            error_gpu_init_match = re.compile('Error (\S+) : Gpu.Initialize (.+)').match
            with open(GetModsLogLocation()) as mods_log:
                for line in mods_log:
                    for match_line in [error_code_match, error_gpu_init_match]:
                        m = match_line(line)
                        if m:
                            return_code = int(m.group(1))
                            exit_msg = m.group(2)
                            # PciDeviceNotFound (220)
                            # This isn't generally recoverable, so throw an exception
                            if return_code == 220:
                                raise Exceptions.FAException(return_code)
            if return_code == 1:
                exit_msg = 'Unknown Error'
        elif return_code != 0:
            exit_msg = 'Command Error = {}'.format(return_code)
            raise Exceptions.FAException(return_code)
        else:
            exit_msg = 'ok'
    except OSError as e:
        if e.errno == errno.ENOENT:
            Datalogger.PrintLog('... File not found error. Either mods/mods log not found')
            raise Exceptions.FileNotFoundError()
        else:
            raise
    finally:
        if exit_msg == 'ok':
            Datalogger.PrintLog('... PASSED')
        else:
            Datalogger.PrintLog('... FAILED. {}'.format(exit_msg))
            _fs_obj.StoreFailedTestInfo()
            if print_fail_status:
                _fs_obj.PrintFailedTestDetails()

    error_info.error_code = return_code
    error_info.error_msg = exit_msg

    return error_info
