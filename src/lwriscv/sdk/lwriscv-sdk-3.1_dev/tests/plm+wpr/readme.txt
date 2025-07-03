===============================================================================
============================= PLM and WPR Tests ===============================
===============================================================================

INTRODUCTION

    The plm+wpr test application is a standalone binary designed to verify
    certain key security controls during FNL and BNB hardware checkouts. More
    information regarding the specific controls verified by these standalone
    tests can be found on the Confluence pages linked in each test profile's
    environment file (i.e. profile-<name>.elw).

BUILD ENVIRONMENT

    The plm+wpr test application has no environment constraints beyond what is
    required of the LWRISCV SDK on which it relies: any user that is able to
    build the liblwriscv sample applications should also be able to build the
    plm+wpr test application. Detailed LWRISCV build requirements can be found
    at https://confluence.lwpu.com/x/LKn3HQ.

BUILDING THE TESTS

    The test application is built using the provided build-tests.sh shell
    script. By default, the script will build all available profiles, but users
    can pass the name of a specific profile to target as well (see the TEST
    PROFILES section below for more information):

        ./build-tests.sh        # Build all available profiles.
        ./build-tests.sh gh100  # Build only the GH100 profile.

    Build output is logged to build-tests.log while build products are
    installed to bin/plm+wpr-<chip>-<engine>.out.

    Users can clear the build products for any particular profile by providing
    the profile name to the accompanying clean-tests.sh script, or can clear
    all build products by ilwoking without arguments:

        ./clean-tests.sh        # Clear output files from all profiles.
        ./clean-tests.sh gh100  # Clear output files from the GH100 profile only.

RUNNING THE TESTS

    On all platforms, the first step in running the plm+wpr test application is
    obtaining an LwWatch prompt. This can accomplished trivially on emulation
    and silicon by simply starting a stadalone LwWatch instance on the target
    machine. On fmodel, MODS's in-built LwWatch facilities must be loaded
    instead, instructions for which can be found here:
    
        https://confluence.lwpu.com/x/8ZJkK

    Once at the LwWatch prompt, switch to the desired host engine by issuing
    "rv <engine>" (e.g. "rv fsp"), then load the correct plm+wpr test binary
    via the "rv brb <text> <data> <manifest>" command. For example:

        lw> rv fsp
        Build Jan 28 2021 20:18:25 on GSP RISC-V. Args: fsp
        Switching to FSP

        lw> rv brb plm+wpr-gh100-fsp.text.encrypt.bin                   \
                   plm+wpr-gh100-fsp.data.encrypt.bin                   \
                   plm+wpr-gh100-fsp.manifest.encrypt.bin.out.bin
        Build Jan 28 2021 20:18:25 on FSP RISC-V. Args: brb plm+wpr-gh100-fs...
        Resetting core *AND* SE...
        Issuing reset to FSP... Done.
        Issuing reset to SE...  Done.
        Configuring bcr...
        Loading images...
        .text image: plm+wpr-gh100-fsp.text.encrypt.bin
        Reading 18176 bytes from file.
        Writing 18176 bytes at 0x0 using PMB.
        .data image: plm+wpr-gh100-fsp.data.encrypt.bin
        manifest image: plm+wpr-gh100-fsp.manifest.encrypt.bin.out.bin
        Reading 2560 bytes from file.
        Writing 2560 bytes at 0x27600 using PMB.
        Starting core...
        Starting RISC-V in BROM mode...
        Waiting for BROM to finish...
        BROM finished with result: PASS
        Core started... status:
        Bootrom Configuration: 0x00000011 (VALID) core: RISC-V brfetch: DISABLED
        Bootrom DMA configuration: 0x00000000 target: 0x0(localFB) UNLOCKED
        RISCV priv lockdown is DISABLED
        Bootrom DMA SEC configuration: 0x00000000 wprid: 0x0
        RETCODE: 0x00000003 Result: PASS Phase: ENTRY Syndrome: INIT Info:INIT
        FMC Code addr: 0x0000000000100000
        FMC Data addr: 0x0000000000000000
        PKC addr     : 0x0000000000000000
        PUBKEY addr  : 0x0000000000000000

    After successful exelwtion of the test binary, the test results can be
    viewed by dumping the debug buffer via the "rv dmesg" command:

        lw> rv dmesg
        Build Jan 28 2021 20:18:25 on FSP RISC-V. Args: dmesg
        Found debug buffer at 0x27000, size 0xff0, w 1540, r 0, magic f007ba11
        DMESG buffer
        --------
        Running PLM and WPR tests on emulation.
        This binary was built on Jun 28 2021 at 11:39:36 for engine FSP.

        Beginning tests...

        === Test Suite: SETUP ===
        INIT:   PASS.
        ENVIRONMENT:    PASS.
        === Finished: 2 / 2 ===

        === Test Suite: DIRECT REGISTER ACCESS ===
        SEC-TC-A1:      PASS.
        SEC-TC-A2:      PASS.
        SEC-TC-A3:      PASS.
        SEC-TC-A4:      PASS.
        SEC-TC-A5:      PASS.
        SEC-TC-A6:      PASS.
        SEC-TC-A7:      PASS.
        SEC-TC-A8:      PASS.
        SEC-TC-A9:      PASS.
        SEC-TC-A10:     PASS.
        SEC-TC-A11:     PASS.
        SEC-TC-A12:     PASS.
        === Finished: 12 / 12 ===

        === Test Suite: GDMA REGISTER ACCESS ===
        SEC-TC-B1:      PASS.
        SEC-TC-B2:      PASS.
        SEC-TC-B3:      PASS.
        SEC-TC-B4:      PASS.
        SEC-TC-B5:      PASS.
        SEC-TC-B6:      PASS.
        === Finished: 6 / 6 ===

        === Test Suite: WPR INSTRUCTION FETCH ===
        SEC-TC-C1:      PASS.
        SEC-TC-C2:      PASS.
        SEC-TC-C3:      PASS.
        SEC-TC-C4:      PASS.
        SEC-TC-C5:      PASS.
        SEC-TC-C6:      PASS.
        SEC-TC-C7:      PASS.
        SEC-TC-C8:      PASS.
        SEC-TC-C9:      PASS.
        === Finished: 9 / 9 ===

        === Test Suite: WPR LOAD/STORE ===
        SEC-TC-D1:      PASS.
        SEC-TC-D2:      PASS.
        SEC-TC-D3:      PASS.
        SEC-TC-D4:      PASS.
        SEC-TC-D5:      PASS.
        SEC-TC-D6:      PASS.
        SEC-TC-D7:      PASS.
        SEC-TC-D8:      PASS.
        SEC-TC-D9:      PASS.
        === Finished: 9 / 9 ===

        === Test Suite: WPR DMA ===
        SEC-TC-E1:      PASS.
        SEC-TC-E2:      PASS.
        SEC-TC-E3:      PASS.
        SEC-TC-E4:      PASS.
        SEC-TC-E5:      PASS.
        SEC-TC-E6:      PASS.
        SEC-TC-E7:      PASS.
        === Finished: 7 / 7 ===

        === Test Suite: WPR GDMA ===
        SEC-TC-F1:      PASS.
        SEC-TC-F2:      PASS.
        SEC-TC-F3:      PASS.
        SEC-TC-F4:      PASS.
        SEC-TC-F5:      PASS.
        SEC-TC-F6:      PASS.
        SEC-TC-F7:      PASS.
        === Finished: 7 / 7 ===

        === Test Suite: TEARDOWN ===
        TEARDOWN:       PASS.
        === Finished: 1 / 1 ===

        Tests complete. See above for details.

        ------

    NOTE: If using fmodel, it may be necessary to issue "rv spin 1000" first to
    give the tests time to finish. Similarly, multiple ilwocations of
    "rv dmesg" may be needed in order for the full output buffer to be
    retrieved.

    In the event that no debug output is found, users can check MAILBOX0 of the
    the host engine for a value of 0x5EC7E575, which indicates that the plm+wpr
    test application booted successfully, and can proceed with troubleshooting
    efforts from there accordingly.

POTENTIAL PITFALLS

    The plm+wpr test application requires FB access in order to verify WPR
    functionality. It may therefore be necessary to perform a MODS "notest"
    run before attempting to execute the test binary on emulation or silicon
    - this will ensure that devinit has completed and that FB is properly
    configured for the tests.

    Similarly, when testing on fmodel, it may be prudent to select a MODS
    breakpoint that is located later in the boot process in order to reduce
    the risk of an improper FB configuration leading to failure.

    Note also that engines without NACK_AS_ACK capabilities may trigger an FBIF
    hang when exelwting negative tests of WPR. For this reason, negative tests
    on such engines must be exelwted one at a time by appropriate commenting of
    suites.c, with additional MODS "notest" runs being exelwted between each
    pair of test cases.

TEST PROFILES

    In order to support hardware-checkout efforts across multiple chips, the
    plm+wpr test application can be lwstomized via the use of test profiles.
    These profiles are defined by environment files of the form
    "profile-<name>.elw" where "<name>" can be any valid filename string (but
    is usually a chip name). Passing the "<name>" portion of a profile's
    environment file to the build-tests.sh script then causes the script to
    build that specific profile.

    In order to create a new profile, users can simply copy an existing profile
    (e.g. profile-gh100.elw) and modify its fields as needed. For more
    information, refer to the included inline documentation.

    One important profile caveat to note is that, while it is possible to have
    multiple profiles targeting the same chip, only one profile per chip can be
    safely built at any one time as the output files are named on a per-chip,
    per-engine basis.

ADDING A CHIP

    To add support for a new chip:

        1) Create a profile for the chip (see TEST PROFILES above).
        2) Create a chip-specific manifest (src/manifest-<chip>.c).
           
    Information on populating the new manifest can be found here:
    
        https://confluence.lwpu.com/x/KUOtLg

    If your new profile targets an engine not previously used by any prior
    profile, please also see the section on ADDING AN ENGINE.

ADDING AN ENGINE

    To add support for a new engine:

        1) Add the engine to the ENGINES list of the desired test profile(s).
        2) Update engine.h with the appropriate engine-specific settings.

    Refer to the inline comments in engine.h for more information regarding the
    available configuration options.

    WARNING: The plm+wpr test application lwrrently assumes an architecture of
    rv64imf for all engines. Attempting to run the test application on an
    engine with an incompatible architecture may produce unpredictable results!

ADDING A TEST CASE / TEST SUITE

    To add a new test case:

        1) Create a function in tests.c that carries out the desired test.
        2) Update tests.h with a prototype for the new test function.
        3) Add a call to the test function to one or more test suites.

    To add a new test suite:

        1) Create a function in suites.c that runs the desired test cases.
        2) Insert a pointer to the new function into the g_suiteList[] array.

    In both cases, existing functions should serve as clear examples of how the
    basic provided test harness can be used to properly track test exelwtion
    and results.

ADDING A TEST TARGET / ACCESS TYPE

    To add a new test target or access type:

        1) Create a new enum value in TARGET_TYPE or TARGET_ACCESS_TYPE.
        2) Update target[Attempt/Clear/Prepare]Access() and/or targetSetPLM().
        3) If needed, add any required setup steps to targetInitializeAll().

    Note that adding a new test target or access type may also entail
    additional modifications, such as:

        1) Augmenting testInit(), testElwironment(), and/or testTeardown() to
           account for additional resource requirements.

        2) Creating auxiliary I/O utilities (io.c/io.h) to facilitate low-level
           access to the new test target / via the new access type.

        3) Adding new configuration constants to config.h, along with
           applicable compile-time checks.

        4) Extending the engine-specific configuration settings to allow for
           selective enablement of the new test target / access type.

ADDING AN EXCEPTION TYPE

    To add support for a new exception type:

        1) Create a new enum value in HAL_EXCEPTION_TYPE.
        2) Update halGetExceptionType() to return the new value when appropriate.
        3) Update harnessExceptionHandler() to process the new exception type.
        4) If needed, add a corresponding enum value to HARNESS_EXCEPTION_MASK.

    Please refer the harnessExceptionHandler() documentation for important
    notes regarding the function's return value when adding support for a new
    exception type.

MODIFYING OTHER TEST PARAMETERS

    Target settings, sentinel values, and all other parameters not specific to
    any one engine or test profile can be found in config.c/config.h. Care
    should be taken when modifying these values as they are shared globally by
    all test configurations.
