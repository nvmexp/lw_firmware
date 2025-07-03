#!/usr/bin/python
import sys
import os
import shutil
import subprocess
import time
import multiprocessing

#
# (Temporary) list of tests to skip (i.e. need updating)
#
skip_list = [ 'mods' ]
build_cfg = "debug"
MAX_CONLWRRENT_TASKS = multiprocessing.cpu_count()

def find_tests():
    list = []
    for root, dirs, files in os.walk(os.getcwd()):
        dirs.sort()
        for file in files:
            if os.path.basename(root) in skip_list:
                continue
            if any(s in os.path.basename(root) for s in [ 'debug', 'release' ]):
                continue
            if file == "Makefile":
                if root not in list:
                    list.append(root)
    return list


class async_test:
    def __init__(self, test):
        command = "make -C %s BUILD_CFG=%s cleanrun" % (test, build_cfg)
        self.name = os.path.basename(test)
        self.output = subprocess.Popen(command, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        #self.output = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    def code(self):
        return self.output.poll()

    def log(self):
        out, err = self.output.communicate()
        return out

def print_summary(running_tasks):
    sys.stdout.write("Running: ")
    for test_task in running_tasks:
        sys.stdout.write("%s " % os.path.basename(test_task[0]))
    sys.stdout.write("\r")
    sys.stdout.flush()

def run_tests():
    tests = find_tests()
    running_tests = []

    failed = False
    while len(running_tests) or len(tests):
        while len(tests) and len(running_tests) < MAX_CONLWRRENT_TASKS:
            test = tests.pop()
            running_tests.append(async_test(test))

        for running_test in running_tests:
            code = running_test.code()
            if code is not None:
                running_tests.remove(running_test)
                if code == 0:
                    sys.stdout.write("\r%-28s : \x1b[32mPASSED   \x1b[37m\n" % (running_test.name))
                else:
                    #sys.stdout.write("\r%-28s : \x1b[31mFAILED   \x1b[37m\n" % (running_test.name))
                    sys.stdout.write("\r%-28s : \x1b[31mFAILED   \x1b[37m\n" % (running_test.name))
                    #sys.stdout.write(running_test.log().decode("utf-8"))
                    failed = True

        time.sleep(1)

    if failed:
        sys.stdout.write("""\x1b[31m
    #######    #    ### #
    #         # #    #  #
    #        #   #   #  #
    #####   #     #  #  #
    #       #######  #  #
    #       #     #  #  #
    #       #     # ### #######

    Run ./regolden.sh to update test golden logs
    \x1b[37m\n""")
        sys.exit(1)
    else:
        sys.stdout.write("""\x1b[32m
    ######     #     #####   #####
    #     #   # #   #     # #     #
    #     #  #   #  #       #
    ######  #     #  #####   #####
    #       #######       #       #
    #       #     # #     # #     #
    #       #     #  #####   #####

    Kernel Resident Sizes:
""")
        sys.stdout.flush()
        os.system("$LW_TOOLS/riscv/toolchain/linux/release/20190625/bin/riscv64-elf-readelf -s 0_simple/_out/debug/firmware.elf | grep code_paged_size")
        sys.stdout.write("""\x1b[37m\n""")
        sys.exit(0)

#
# Treat lone command line arg as build config (release or debug)
#
if len(sys.argv) == 2:
    build_cfg = sys.argv[1]
    if build_cfg not in [ 'debug', 'release' ]:
        sys.stderr.write("Invalid build config value: %s\n" % build_cfg)
        sys.exit(1)

#
# Sanity check elw vars
#
if not os.elwiron['LW_TOOLS']:
    sys.stderr.write("LW_TOOLS not defined!\n")
    sys.exit(1)

make_cmd='make 2>&1 | tee -a build_and_run.txt'
make_clean_cmd='make clean BUILD_CFG=' + build_cfg + ' 2>&1 >/dev/null'

sys.stdout.write("Running %s tests using %d cores.\n" % (build_cfg, MAX_CONLWRRENT_TASKS))
run_tests()
