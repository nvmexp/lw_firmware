# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2015,2017-2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import shlex
import subprocess
import time
import re
import os
import stat
import glob
import shutil
import select
import struct
import fcntl
import termios
import sys
import signal
from functools import reduce

from . import Datalogger
from . import Exceptions

_mods_pre_args = ''
_mods_post_args = ''
_mods_log_location = ''
_mods_mle_location = ''
_verbose_stdout = False

def SetModsPrePostArgs(pre_args, post_args):
    global _mods_pre_args, _mods_post_args
    _mods_pre_args = pre_args
    _mods_post_args = post_args

def SetModsLogLocation(location):
    global _mods_log_location, _mods_mle_location
    _mods_log_location = location
    _mods_mle_location = location[:-3] + 'mle'

def GetModsLogLocation():
    global _mods_log_location
    return _mods_log_location

def GetModsMleLocation():
    global _mods_mle_location
    return _mods_mle_location

def SetVerboseStdout(val):
    global _verbose_stdout
    _verbose_stdout = val

def GetVerboseStdout():
    global _verbose_stdout
    return _verbose_stdout

def CheckSubprocessOutput(command_string):
    Datalogger.PrintLog('... Running "{}"'.format(command_string))
    command = shlex.split(command_string)
    try:
        return subprocess.check_output(command).decode("utf-8")
    except subprocess.CalledProcessError:
        raise Exceptions.CommandError('"{}" returned non-zero'.format(command_string))

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

def RunSubprocess(command_string, timeout, elw=None):
    Datalogger.PrintLog('... Running "{}"'.format(command_string))
    command = shlex.split(command_string)
    
    def _GetProcessChildren(pid):
        try:
            return [int(x) for x in subprocess.check_output("pgrep -P {}".format(pid), shell=True).decode("utf-8").splitlines()]
        except:
            return []

    def _KillProcessFamily(pid):
        for child in _GetProcessChildren(pid):
            _KillProcessFamily(child)
        os.kill(pid, signal.SIGKILL)

    time_start = time.time()
    process = None
    try:
        with SubprocessStdout(Datalogger.GetLogName()) as data_log:
            process = subprocess.Popen(command, stdout=data_log, stderr=subprocess.STDOUT, elw=elw)
            return_code = process.poll()
            while return_code is None:
                delta = time.time() - time_start
                if timeout and delta > timeout:
                    Datalogger.PrintLog("... TIMEOUT! Killing MODS process")
                    _KillProcessFamily(process.pid)
                    return_code = 124
                    break
                time.sleep(1)
                return_code = process.poll()
    except KeyboardInterrupt:
        if process:
            _KillProcessFamily(process.pid)
        raise
    finally:
        if process and process.poll() is None:
            Datalogger.PrintLog("... Waiting for MODS process to exit...")
            try:
                process.wait(10)
            except subprocess.TimeoutExpired:
                raise Exceptions.TimeoutExpired("Timeout waiting for MODS process to exit")
    return return_code

def _FindTestId(args):
    test_str = "MODS"
    test_search = re.search("-test (\S+)", args)
    if test_search:
        trace_search = re.search("-testargstr 20 Trace (\S+)", args)
        if trace_search:
            test_str = "{} (test 20)".format(os.path.basename(trace_search.group(1)))
        else:
            test_str = "ModsTest (test {})".format(test_search.group(1))
    virtid_search = re.search("-test_virtid '(.+)'.*-?", args)
    if virtid_search:
        test_str = "ModsTest (test_virtid {})".format(virtid_search.group(1))
    return test_str

def ModsCall(script_name, args, timeout):
    global _mods_pre_args, _mods_post_args, _verbose_stdout
    command_string = "./mods -l {} {} {} {} {}".format(GetModsLogLocation(), _mods_pre_args, script_name, args, _mods_post_args)
    return_code = 0
    exit_msg = ""
    test_str = _FindTestId(args)
    
    try:
        Datalogger.PrintLog('Enter {}'.format(test_str), indent=False)
        return_code = RunSubprocess(command_string, timeout)
        if GetVerboseStdout():
            Datalogger.ConcatLog(GetModsLogLocation())
        if os.path.exists(GetModsMleLocation()):
            Datalogger.ConcatMle(GetModsMleLocation())
        
        if return_code == 124:
            exit_msg = '<TIMEOUT>'
            raise Exceptions.ModsTimeout(command_string)
        elif return_code == 1:
            error_code_match = re.compile('Error Code = (\S+) \(([^)]+)\)').match
            error_gpu_init_match = re.compile('Error (\S+) : Gpu.Initialize (.+)').match
            with open(GetModsLogLocation()) as mods_log:
                for line in mods_log:
                    for match_line in [error_code_match, error_gpu_init_match]:
                        m = match_line(line)
                        if m:
                            return_code = int(m.group(1))
                            exit_msg = m.group(2)
                            Exceptions.CheckFatalErrorCode(return_code)
            if return_code != 1:                
                return return_code
            exit_msg = 'Unknown Error'
            raise Exceptions.UnknownModsError(command_string)
        elif return_code != 0:
            exit_msg = 'Command Error = {}'.format(return_code)
            raise Exceptions.CommandError('"{}" returned {}'.format(command_string, return_code))
        else:
            exit_msg = 'ok'
    finally:
        Datalogger.PrintLog('... MODS passed' if exit_msg == 'ok' else '... MODS failed')
        # The script must print Exit 0 regardless, otherwise the handler will kill the script prematurely
        Datalogger.PrintLog('Exit 000000000000 : {} {}'.format(test_str, exit_msg), indent=False)
            
    return return_code

def SriovModsCall(script_name, partition_arg, pf_args, vf_args, timeout):
    global _mods_pre_args, _mods_post_args, _verbose_stdout
    
    post_args = _mods_post_args
    for reset_arg in ["-hot_reset", "-fundamental_reset", "-reset_device"]:
        if post_args.find(reset_arg) >= 0:
            post_args = post_args.replace(reset_arg,"")
            pf_args = " ".join([pf_args, reset_arg]) 
            
    ymlname = "sriov_config.yml"
    
    pf_args = " ".join([post_args, "-smc_partitions", partition_arg, pf_args, "-sriov_config_file", ymlname])
    vf_args = [" ".join([post_args, vf_arg]) for vf_arg in vf_args]
    
    num_parts = len(vf_args)
    subtest_dirs = ["subtest_vf{}".format(idx) for idx in range(num_parts)]
        
    return_codes = [0] * num_parts
    exit_msgs = [""] * num_parts
    test_strs = [_FindTestId(vf_arg) for vf_arg in vf_args] 
    access_token = None
    
    try:
        for i in range(num_parts):
            Datalogger.PrintLog('Enter {}'.format(test_strs[i]), indent=False)
        
        for line in CheckSubprocessOutput("./mods -acquire_access_token").splitlines():
            m = re.match("Access Token : (\S+)", line)
            if m:
                access_token = m.group(1)
        if access_token is None:
            raise Exceptions.UnknownModsError("Failed to acquire access token")
        
        with open(ymlname, "w") as yml:
            print("---", file=yml)
            
            for subtest_dir,vf_arg in zip(subtest_dirs,vf_args):
                os.makedirs(subtest_dir, exist_ok=True)
                
                if os.path.exists(os.path.join(subtest_dir, "mods.log")):
                    os.remove(os.path.join(subtest_dir, "mods.log"))
                
                if not os.path.exists(os.path.join(subtest_dir, "liblwidia-vgpu.so")):
                    shutil.copyfile("liblwidia-vgpu.so", os.path.join(subtest_dir, "liblwidia-vgpu.so"))
                
                for gld in glob.iglob("gld*.bin"):
                    if not os.path.exists(os.path.join(subtest_dir, gld)):
                        shutil.copyfile(gld, os.path.join(subtest_dir, gld))
                    
                subtest_script = os.path.join(subtest_dir, "run.sh")
                print("- test_exelwtable: {}".format(subtest_script), file=yml)
                mods_path = os.path.realpath(os.path.join(os.path.dirname(sys.argv[0]), "mods"))
                with open(subtest_script, "w") as sub_script:
                    print("#!/bin/bash", file=sub_script)
                    print("cd \"$(dirname \"$0\")\"", file=sub_script)
                    print("{} -access_token {} {} {} {}".format(mods_path, access_token, _mods_pre_args, script_name, vf_arg), file=sub_script)
                st = os.stat(subtest_script)
                os.chmod(subtest_script, st.st_mode | stat.S_IEXEC)               
        
        command_string = "./mods -access_token {} -l {} {} {} {}".format(access_token, GetModsLogLocation(), _mods_pre_args, script_name, pf_args)
        
        return_code = RunSubprocess(command_string, timeout, elw={"LD_LIBRARY_PATH":".:{}".format(os.path.realpath(os.path.dirname(sys.argv[0])))})
        
        if GetVerboseStdout():
            Datalogger.ConcatLog(GetModsLogLocation())
            for subtest_dir in subtest_dirs:
                Datalogger.ConcatLog(os.path.join(subtest_dir, "mods.log"))
        if os.path.exists(GetModsMleLocation()):
            Datalogger.ConcatMle(GetModsMleLocation())
            for subtest_dir in subtest_dirs:
                Datalogger.ConcatMle(os.path.join(subtest_dir, "mods.mle"))
        
        error_code_match = re.compile('Error Code = (\S+) \(([^)]+)\)').match
        error_gpu_init_match = re.compile('Error (\S+) : Gpu.Initialize (.+)').match
        logs = [os.path.join(dir, "mods.log") for dir in subtest_dirs]
        for idx,log in enumerate(logs):
            if os.path.exists(log):            
                with open(log) as mods_log:
                    for line in mods_log:
                        for match_line in [error_code_match, error_gpu_init_match]:
                            m = match_line(line)
                            if m:
                                return_codes[idx] = int(m.group(1))
                                exit_msgs[idx] = m.group(2)
                                Exceptions.CheckFatalErrorCode(return_code)
            else:
                return_codes[idx] = 11
                exit_msgs[idx] = 'File Not Found'
            if not exit_msgs[idx]:
                return_codes[idx] = return_code
                if return_code == 124:
                    exit_msgs[idx] = '<TIMEOUT>'        
                elif return_code == 1:
                    exit_msgs[idx] = 'Unknown Error'
                else:
                    exit_msgs[idx] = 'Command Error = {}'.format(return_code)
    finally:
        if access_token is not None:
            CheckSubprocessOutput("./mods -release_access_token -access_token {}".format(access_token))
        
        for code in return_codes:
            Datalogger.PrintLog('... passed' if code == 0 else '... failed')
        
        # The script must print Exit 0 regardless, otherwise the handler will kill the script prematurely
        for test_str,exit_msg in zip(test_strs, exit_msgs):
            Datalogger.PrintLog('Exit 000000000000 : {} {}'.format(test_str, exit_msg), indent=False)
            
    return return_codes

def GetTerminalSize():
    if not sys.stdin.isatty():
        return (25, 80)

    def ioctl_GWINSZ(fd):
        return struct.unpack("hh", fcntl.ioctl(fd, termios.TIOCGWINSZ, "1234"))
    # try stdin, stdout, stderr
    for fd in (0, 1, 2):
        try:
            return ioctl_GWINSZ(fd)
        except:
            pass
    # try os.ctermid()
    try:
        fd = os.open(os.ctermid(), os.O_RDONLY)
        try:
            return ioctl_GWINSZ(fd)
        finally:
            os.close(fd)
    except:
        pass
    # try `stty size`
    try:
        return tuple(int(x) for x in subprocess.check_output(shlex.split('stty size')).split())
    except:
        pass
    # try environment variables
    try:
        return tuple(int(os.getelw(var)) for var in ("LINES", "COLUMNS"))
    except:
        pass
    # I give up. return default.
    return (25, 80)

def CountBits(masks):
    return reduce(lambda x,y:x+y, ["{:b}".format(mask).count("1") for mask in masks])
