TL;DR;

This code is MP-SK partitions for GSP test ucode (in chips_a/uproc/gsp).
It is built with MP-SK and released as SK in chips_a (and then pulled by GSP).

STRUCTURE

There are 2 partitions in the image:
Partition 0 is "SafeRTOS" partition. Code here is 1st stage bootloader,
because it's only responsibility is to jump to SafeRTOS bootloader.
After SafeRTOS bootloader takes over, it clobbers this partition, with partition
switch code built into SafeRTOS. That way, no TCM is wasted for unused code.
Also because of that, SafeRTOS doesn't have to be built/linked with anything
built in //sw/lwriscv.

Partition 1 is "HS" test partition. All it does is reads perf counters, writes
them back to shared memory and switches back to partition 0.

BUILD INSTRUCTIONS

It can be built with only //sw/lwriscv checked out.
To release, chips_a must be checked out and LW_SOURCE set properly.
You need to build/release on Linux (for now).

Build (1-4) and release (5-) process is as follows:
1. Separation kernel with policies is built into binary (spark)
2. Partition code and liblwriscv are built and linked together
    ** NOTE **
    SafeRTOS partition code here is "dummy", that handles part of former SK
    functionality.
    It's only responsibility is to jump into bootloader, that will then clobber
    that partition with chips_a partition switch code.
    That way we don't have unused TCM.
    ** /NOTE **
3. Manifest is generated
4. Everything is combined together and stored in bin/
5. Files are released with release-imgs-if-changed into
    ${LW_SOURCE}/uproc/lwriscv/sepkern/bin/gsp
    ** NOTE **
    This path will likely change in future
    ** /NOTE **

For both build and release, there are helper scripts - build.sh and release.sh
that will do some sanity checking beforehead.

After that, one needs to rebuild GSP image (or submit released images with AS2
to trigger auto rebuild of GSP)
