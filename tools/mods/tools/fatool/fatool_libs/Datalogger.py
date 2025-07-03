# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

# TODO:
# This is almost a copy of Datalogger.py from the floorsweeping script library
# Should probably couple both the scripts into one

from __future__ import print_function
import datetime
import traceback
import json
import pprint
import sys
import os

from . import Exceptions

_log_name = ''
_data_log = []
_indent_level = 0
_terminal_width = 80
_log_info_stack = []

def InitLog(log_name):
    global _indent_level, _terminal_width, _log_name
    _log_name = log_name
    open(_log_name, 'w').close()
    _indent_level = 0
    _terminal_width = GetTerminalSize()[1]

def GetLogName():
    global _log_name
    return _log_name

def PrintLog(msg, indent=True, truncate=True):
    global _indent_level, _terminal_width, _log_name

    indent_str = ''
    if indent:
        indent_str = ('...->'*_indent_level) + (' ' if _indent_level>0 else '')

    with open(_log_name, 'a') as log:
        print("FA: {}".format(indent_str + msg), file=log)

    newmsg = []
    while truncate and (len(indent_str) + len(msg)) > _terminal_width:
        if len(indent_str) > _terminal_width:
            indent_str = indent_str[:_terminal_width]
            msg = ''
            break

        trunc_len = _terminal_width - len(indent_str)
        space_idx = msg.rfind(' ', 0, trunc_len)
        if (space_idx > trunc_len or space_idx <= 0):
            space_idx = trunc_len

        newmsg.append(indent_str + msg[0:space_idx])
        msg = msg[space_idx:]

        if indent:
            indent_str = ('...->'*_indent_level) + (' ' if _indent_level>0 else '') + '... ...'
            
    newmsg.append(indent_str + msg)

    for line in newmsg:
        print(line)

def PrintFile(file_path, file_type=""):
    global _log_name
    with open(_log_name, 'a') as log:
        print("FA: ----------------{} Dump Start------------------------".format(file_type), file=log)
        print("FA: ----------------{} Dump Start------------------------".format(file_type))
        with open(file_path, 'r') as f:
            line = f.readline()
            while line:
                print("FA: {}".format(line), file=log)
                print("FA: {}".format(line))
                line = f.readline()
        print("FA: ----------------{} Dump End--------------------------".format(file_type), file=log)
        print("FA: ----------------{} Dump End------------------------".format(file_type))


def ConcatLog(input_log, chunk=256):
    global _log_name
    with open(input_log) as input_file, open(_log_name, 'a') as output_file:
        data = input_file.read(chunk)
        while data != '':
            output_file.write(data)
            data = input_file.read(chunk)

def LogInfo(*args, **kwargs):
    global _log_info_stack
    if not _log_info_stack:
        PrintLog('Log_info must be called within a function wrapped with LogFunction')
        raise Exceptions.SoftwareError()
    if args:
        for arg in args:
            if not isinstance(arg, dict):
                PrintLog('Must call Log_info with keyword arguments or with dictionaries')
                raise Exceptions.SoftwareError()
            _log_info_stack[-1].update(arg)
    if kwargs:
        _log_info_stack[-1].update(kwargs)
    
def LogFunction(function):
    '''
    Wrap around a function to log when it is called, what its results are, how long it takes, and whether it returned an error
    '''
    def wrapped(*args, **kwargs):
        global _indent_level, _log_name, _log_info_stack
        func_name = function.__name__        
        PrintLog('-> {:s} ... '.format(func_name))
        _log_info_stack.append({})
        _indent_level += 1
        
        time1 = datetime.datetime.now()

        try:
            result = function(*args, **kwargs)
        except (Exceptions.FAException) as e:
            time2 = datetime.datetime.now()
            _indent_level -= 1
            PrintLog('<- {:s} : {!s}'.format(func_name, e.exit_code))
            LogInfo(error=str(e))
            raise
        except Exception as e:
            time2 = datetime.datetime.now()
            _indent_level -= 1
            PrintLog('<- {:s} : {!s} : {!s}'.format(func_name, e.__class__.__name__, e))
            LogInfo(error=str(e))
            raise
        except KeyboardInterrupt:
            time2 = datetime.datetime.now()
            _indent_level -= 1
            PrintLog('<- {:s} : KeyboardInterrupt'.format(func_name))
            raise Exceptions.UserAbortedScript()
        else:
            time2 = datetime.datetime.now()
            _indent_level -= 1
            PrintLog('<- {:s} in {:3f} seconds'.format(func_name, (time2-time1).total_seconds()))
            return result
        finally:
            info = _log_info_stack.pop()
            event = { 'name' : func_name,
                      'time_begin' : time1.isoformat(' '),
                      'result' : info,
                      'time_end' : time2.isoformat(' ')}
            _data_log.append(event)
            with open('sweep.json', 'w+') as json_file:
                json.dump(_data_log, json_file, indent=4)
                
    wrapped.__name__ = function.__name__
    return wrapped

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
