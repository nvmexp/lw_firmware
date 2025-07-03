#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

def utl_module_decorator(module):
    import utl_global
    from functools import wraps
    from inspect import isclass, getmembers, isroutine
    from sys import argv
    def function_decorator(cls, func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            if len(kwargs) > 0:
                utl_global.UtlCheckUserKwargs(f'{cls.__name__}.{func.__name__}', **kwargs)
            return func(*args, **kwargs)
        return wrapper
    def class_decorator(cls):
        for attr, obj in getmembers(cls):
            # Ignore default object attributes since none of them will use
            # the UTL kwargs interface.
            if isroutine(obj) and not attr.startswith('__'):
                setattr(cls, attr, function_decorator(cls, getattr(cls, attr)))
        return cls
    for attr, obj in getmembers(module):
        if isclass(obj):
            setattr(module, attr, class_decorator(getattr(module, attr)))
        elif isroutine(obj):
            setattr(module, attr, function_decorator(module, getattr(module, attr)))
    return module

def utl_table_printing():
    import utl
    import sys
    import texttable

    def table2Str(table):
        tt = texttable.Texttable()
        tt.add_rows(table)
        tt.set_cols_width(getColWidth(table))
        return '\n' + tt.draw()

    def getColWidth(table):
        colWidth = []
        for i in range(len(table[0])):
            width = max([len(row[i]) for row in table])
            colWidth.append(width)
        return colWidth

    def OnTablePrintEvent(event):
        if (event.isDebug):
            utl.LogDebug(text = event.header)
            utl.LogDebug(text = table2Str(event.data))
        else:
            utl.LogInfo(text = event.header)
            utl.LogInfo(text = table2Str(event.data))

    utl.TablePrintEvent.AddCallback(func = OnTablePrintEvent)

def utl_unittest_kwargs_check(funcName):
    import utl_global
    def Test(**kwargs):
        utl_global.UtlCheckUserKwargs(funcName, **kwargs)
    return Test

def extract_packaged_script(packagePath, extractPath):
    from pathlib import Path
    import os
    import utl
    Path(extractPath).mkdir(exist_ok=True)
    packagePath = packagePath.replace('.hdr','.zip')
    errorHint = []
    if packagePath.endswith('.zip'):
        import zipfile
        try:
            with zipfile.ZipFile(packagePath, 'r') as package:
                hasPyScript = False
                for member in package.infolist():
                    if member.filename.endswith('.py'):
                        hasPyScript = True
                        try:
                            package.extract(member, extractPath)
                        except IOError:
                            os.remove(os.path.join(extractPath, member.filename))
                            package.extract(member, extractPath)
                if not hasPyScript:
                    return 12 # RC::FILE_READ_ERROR
                return 0
        except Exception as e:
            errorHint.append(e)

    packagePath = packagePath.replace('.zip','.tgz')
    if packagePath.endswith(('.gz', '.tgz', '.bz2', '.xz')):
        import tarfile
        try:
            with tarfile.open(packagePath, 'r') as package:
                hasPyScript = False
                for member in package.getmembers():
                    if member.name.endswith('.py'):
                        hasPyScript = True
                        try:
                            package.extract(member, extractPath)
                        except IOError:
                            os.remove(os.path.join(extractPath, member.name))
                            package.extract(member, extractPath)
                if not hasPyScript:
                    return 12 # RC::FILE_READ_ERROR
                return 0
        except Exception as e:
            errorHint.append(e)
    
    utl.LogError(text = f"Please check path: {packagePath}, only supoort hdr/zip/tar format, or check {errorHint} for possible reasons.\n")
    return 12 # RC::FILE_READ_ERROR
   
