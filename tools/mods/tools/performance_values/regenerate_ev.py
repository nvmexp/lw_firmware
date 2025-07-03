#!/usr/bin/python

# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2012,2016 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import sys, os, subprocess, shutil

# Usage:
# regenerate_ev.py path_to_branch_backup start_changelist# [optional_outputfile]
# regenerate_ev.py \\netapp-hq05\MODS_backup\performance_values_backup\gpu_drv_r310_00 14193966
# note: If you run from a dos window you will need to run as administrator.

srcdir = sys.argv[1]
startcl = sys.argv[2]
dest = sys.argv[3] if len(sys.argv) == 4 else "../../gpu/js/pvsstatexp.js"
tmp_path = "tmp_pv"

if not os.path.exists(srcdir):
    print("srcdir: " + sys.argv[1] + "does not exist.")
    sys.exit()

if os.path.exists(tmp_path):
    shutil.rmtree(tmp_path)

os.mkdir(tmp_path)

clpathstoprocess = []
for root, subdirs, files in os.walk(srcdir):
    for subdir in subdirs:
       if (subdir >= startcl):
           clpathstoprocess.append(subdir)
           print("adding " + subdir + "to be scanned.")


if os.path.exists(dest):
    os.remove(dest)

for clpath in clpathstoprocess:
    fullclpath = os.path.join(srcdir, clpath)
    for root, subdirs, files in os.walk(fullclpath):
        for file in files:
            fullclfile = os.path.join(fullclpath, file)
            print("copy " + fullpath + "to " + tmp_path)
            shutil.copy(fullclfile, tmp_path)
    
make_ev_cmd = "make_ev_split.py " + tmp_path + " " + dest

print("calling " + make_ev_cmd)
subprocess.call(make_ev_cmd, shell=True);

shutil.rmtree(tmp_path)
