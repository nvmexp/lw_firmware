# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import datetime
import traceback
import json
import pprint
import sys
import os
import shutil

from . import Exceptions
from . import Utilities

_enable = True
_log_name = 'all.log'
_mle_name = 'all.mle'
_data_log = []
_indent_level = 0
_terminal_width = 80
_log_info_stack = []

def SetEnable(val):
    global _enable
    _enable = val

def IsEnabled():
    global _enable
    return _enable

def InitLog(log_name):
    global _indent_level, _terminal_width, _log_name, _mle_name
    if not IsEnabled():
        return
    _log_name = log_name
    _mle_name = log_name[:-3] + 'mle'
    open(_log_name, 'a').close()
    open(_mle_name, 'a').close()
    _indent_level = 0
    _terminal_width = Utilities.GetTerminalSize()[1]

def GetLogName():
    global _log_name
    return _log_name

def PrintLog(msg, indent=True, truncate=True):
    global _indent_level, _terminal_width, _log_name
    if not IsEnabled():
        return

    indent_str = ''
    if indent:
        indent_str = ('...->'*_indent_level) + (' ' if _indent_level>0 else '')

    with open(_log_name, 'a') as log:
        print("FS: {}".format(indent_str + msg), file=log)

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

def ConcatLog(input_log, chunk=256):
    global _log_name
    if not IsEnabled():
        return
    with open(input_log, 'rb') as input_file, open(_log_name, 'ab') as output_file:
        shutil.copyfileobj(input_file, output_file, chunk)

def ConcatMle(input_mle, chunk=256):
    global _mle_name
    if not IsEnabled():
        return
    with open(input_mle, 'rb') as input_file, open(_mle_name, 'ab') as output_file:
        shutil.copyfileobj(input_file, output_file, chunk)

def LogInfo(*args, **kwargs):
    global _log_info_stack
    if not IsEnabled():
        return
    if not _log_info_stack:
        raise Exceptions.SoftwareError("Log_info must be called within a function wrapped with LogFunction")
    if args:
        for arg in args:
            if not isinstance(arg, dict):
                raise Exceptions.SoftwareError("Must call Log_info with keyword arguments or with dictionaries")
            _log_info_stack[-1].update(arg)
    if kwargs:
        _log_info_stack[-1].update(kwargs)

def LogFunction(function):
    '''
    Wrap around a function to log when it is called, what its results are, how long it takes, and whether it returned an error
    '''
    if not IsEnabled():
        return function

    def wrapped(*args, **kwargs):
        global _indent_level, _log_name, _log_info_stack
        func_name = function.__name__
        PrintLog('-> {:s} ... '.format(func_name))
        _log_info_stack.append({})
        _indent_level += 1

        time1 = datetime.datetime.now()

        try:
            result = function(*args, **kwargs)
        except (Exceptions.FloorsweepException) as e:
            time2 = datetime.datetime.now()
            _indent_level -= 1
            PrintLog('<- {:s} : {!s}'.format(func_name, e))
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

def Enable(function):
    def wrapped(*args, **kwargs):
        enabled = IsEnabled()
        SetEnable(True)
        try:
            result = function(*args, **kwargs)
            return result
        finally:
            SetEnable(enabled)
    wrapped.__name__ = function.__name__
    return wrapped

def Disable(function):
    def wrapped(*args, **kwargs):
        enabled = IsEnabled()
        SetEnable(False)
        try:
            result = function(*args, **kwargs)
            return result
        finally:
            SetEnable(enabled)
    wrapped.__name__ = function.__name__
    return wrapped
