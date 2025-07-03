# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2018 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_

# To use in GDB (after loading lwwatch):
# > source rvbltest.py
# > source /home/amarshall/p4/amarshall-linux-default/sw/dev/gpu_drv/chips_a/uproc/lwriscv/bootloader_tests/rvbltest.py
# > print $rvbltest("rvbl_simple", 0)

import os
class Rvbltest(gdb.Function):
    def __init__(self):
        super (Rvbltest, self).__init__("rvbltest")

    # `expected` is a signed 64-bit value
    def ilwoke(self, testname, testidx=0, expected=-1):
        uproc_basedir = os.elwiron['SW_TOOLS_DIR']+"/dev/gpu_drv/chips_a/uproc/"
        testname = testname.string()

        if type(testidx) == gdb.Value:
            if testidx.type.code & gdb.TYPE_CODE_STRING == gdb.TYPE_CODE_STRING:
                testidx = testidx.string()
        testidx = int(testidx)

        if type(expected) == gdb.Value:
            if expected.type.code & gdb.TYPE_CODE_STRING == gdb.TYPE_CODE_STRING:
                expected = expected.string()
        expected = int(expected)

        # Hacky, but this area of memory always seems to be ok to scribble on top of...
        fb_origin = 0x800000
        fb_addr = 0x6180000000000000 + fb_origin
        spin_each = 5000
        testpath = "%slwriscv/bootloader_tests/%s/_out/gsp_riscv_tu10x/%s" % (uproc_basedir, testname, testname)
        gdb.execute("rv reset")
        gdb.execute("rv bootd 0")
        gdb.execute("rv loadfb 0x%X %s" % (fb_origin, testpath+"_image.bin"))
        gdb.execute("rv reset")
        gdb.execute("rv bootd 0x%X" % fb_addr)
        gdb.execute("rv load 0x100000000 %s.test%d_params.bin" % (testpath, testidx))
        gdb.execute("rv wcsr 0x7C0 0")
        gdb.execute("rv go")
        tohost_csr = None
        # This is obscenely long, but I want some kind of timeout...
        for i in xrange(500):
            gdb.execute("rv spin %d" % spin_each)
            gdb.execute("rv halt")
            gdb.execute("rv dmesg f")

            # Gather the CSR data
            gdb.execute("set $rvbltest_tmp = malloc(8)")
            gdb.execute("print riscvIcdRcsr(0x7C0, $rvbltest_tmp)")
            tohost_csr = int(gdb.parse_and_eval("*(LwU64*)$rvbltest_tmp"))
            gdb.execute("call free((void*)$rvbltest_tmp)")
            if tohost_csr == expected:
                return "Passed!\n"
            elif tohost_csr == 0:
                print "Still working...\n"
            else:
                gdb.execute("rv state")
                print "Error :( [0x%X]\n" % tohost_csr
                break

            gdb.execute("rv go")
        return "Could not complete the test. Most recent CSR value was [0x%X]\n" % tohost_csr

Rvbltest()
