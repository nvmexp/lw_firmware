# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import unittest
from . import testGA10x
from . import testGH100

_test_cases = [
    testGA10x,
    testGH100
]

def load_tests(loader, tests, pattern):
    suite = unittest.TestSuite()
    for test in _test_cases:
        subtests = loader.loadTestsFromModule(test)
        suite.addTests(subtests)
    return suite
