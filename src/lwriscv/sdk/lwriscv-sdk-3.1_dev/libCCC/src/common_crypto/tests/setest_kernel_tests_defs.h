/*
 * Copyright (c) 2019-2021, LWPU CORPORATION. All rights reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef INCLUDED_SETEST_KERNEL_TESTS_DEFS_H
#define INCLUDED_SETEST_KERNEL_TESTS_DEFS_H

/* Can be defined in makefile (e.g. rules.mk) if tests are enabled */
#ifndef KERNEL_TEST_MODE
#define KERNEL_TEST_MODE 0
#endif

#ifndef HAVE_ENGINE_API_TESTS
#define HAVE_ENGINE_API_TESTS 0
#endif

#if KERNEL_TEST_MODE

/* RSA cipher nopad/pad/unpad code is tested if makefile adds file
 * tests/setest_kernel_rsa_cipher.c and defines TEST_RSA_CIPHER
 */
#define NIST_AES_TESTS
#define TEST_WORKING_CASES

#if HAVE_SE_RSA || CCC_WITH_RSA
#define TEST_RSA
#define TEST_RSA4096
#endif /* HAVE_SE_RSA || CCC_WITH_RSA */

#define TEST_DIGEST
#if HAVE_SE_AES_KDF
#define TEST_AES_KDF_TESTS
#endif
#if HAVE_SE_CMAC_KDF
#define TEST_CMAC_KDF_TESTS
#endif
#define TEST_ECC

#define TEST_ECDSA	// ECDSA verification tests
#define TEST_ECDSA_SIGN	// ECDSA signing tests

#define TEST_DH		  // => Diffie-Hellman-Merkle key agreement (DH algorithm)
#define TEST_ECDH
#if !HAVE_OP_INIT_DEFINE_CONTEXT_ALGORITHM
#define TEST_CLASS_INIT
#endif

#define TEST_SEC_SAFE_REQ_VERIFY // Verify CCC security/safety requirements
#define TEST_CCC_PLC_API_TESTS // CCC PLC related API/code coverage tests
#define TEST_SRS_VERIFY	// CCC requirements (SRS) verification

#if defined(HAVE_AES_IV_PRESET) && HAVE_AES_IV_PRESET
#define TEST_AES_IV_PRESET
#endif

#if defined(HAVE_AES_XTS) && HAVE_AES_XTS
#define TEST_AES_XTS
#endif

#define TEST_PSC_LEGACY_TESTS 0 // Run/skip selected tests from this file in PSC as well

#if CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T23X)

#define TEST_PSC	// Run ONLY PSC SE tests with PSC ROM for now!

// all tests do not fit in PSC memory...
#define HAVE_ONLY_PSC	// Only run PSC tests in T23X for now

#if CCC_SOC_FAMILY_ON(CCC_SOC_ON_VDK)
// The new system under development runs a LIMITED SET OF TESTS in VDK!!!
//
// VDK crashes or does not work with these
#define HAVE_VDK_ERROR_SKIP
#endif
#endif /* CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T23X) */

#ifndef HAVE_WYCHE_ECDSA
#define HAVE_WYCHE_ECDSA 0
#endif

#ifndef HAVE_WYCHE_ECDH
#define HAVE_WYCHE_ECDH 0
#endif

#if CCC_WITH_ED25519
#define TEST_ED25519	  // T194 Elliptic 2.0 supported EDDSA with Edwards lwrve ED25519
#define TEST_ED25519_SIGN // T194 Elliptic 2.0 supported EDDSA signing with Edwards lwrve ED25519
#endif

#if CCC_WITH_X25519
#define TEST_X25519	  // T194 Elliptic 2.0 supported C25519 ECDH

#define X25519_ITERATION	   1	// 1000 iterations OK in FPGA (1000000 takes a long while)

#define X25519_OPENSSL_COMPAT_TEST 1	// OpenSSL compat test => note openssl vector byte order!!
#endif

#ifndef LONG_DIGEST_TEST
#define LONG_DIGEST_TEST 0	// XXX for testing NC regions with SHA
#endif

#ifndef MAX_TEST_FRAGMENT_SIZE
#define MAX_TEST_FRAGMENT_SIZE 4096	/* Used for SHA update fragment space for long fragments
					 * Define this as 4096 or 262144 for now.
					 */
//#define MAX_TEST_FRAGMENT_SIZE 262144
#endif

#ifndef UPDATE_DIGEST_TEST
/* Longest test will pass total of 262144000 bytes through SHA256
 * in 1000 chunks of 262144 bytes each.
 */
#define UPDATE_DIGEST_TEST 0	// For testing multiple large updates (uses HEAP)
#endif

/* Added large sha256 digest tests mainly for speed measurements.
 * Requested by MB2.
 *
 * Define an arbitrary number of pages your system can allocate from
 * heap TEST_N_PAGE_DIGEST and if verifying the result value matters:
 * define it as a byte vector to TEST_N_PAGE_DIGEST_RESULT (NULL used
 * unless result defined).
 *
 * If speed measurement is needed, enable those in
 * crypto_system_config.h
 *
 * If your system does not allow large heap allocations you may also
 * try defining SHA_FROM_MEMORY nonzero, if which case the test will
 * attempt to digest N arbitrary memory pages starting from a stack
 * variable address.  This test may crash (if N is too large) and the
 * result can not be verified, but it may be the only way in some
 * systems to digest larger amounts of data in a single digest call
 * (as requested by MB2).  This is disabled by default.
 */
#ifndef TEST_N_PAGE_DIGEST
#define TEST_N_PAGE_DIGEST 0
#endif

#define TEST_PKA1_ECDH_KSLOT	// Test these: have support
#define TEST_PKA1_RSA_KSLOT	// Test these: have support

#ifndef HAVE_KOBLITZ_LWRVES
#define HAVE_KOBLITZ_LWRVES 0
#endif

#ifndef HAVE_RSA_PKCS_SIGN
#define HAVE_RSA_PKCS_SIGN 0
#endif

#ifndef HAVE_RSA_PSS_SIGN
#define HAVE_RSA_PSS_SIGN 0
#endif

#if defined(HAVE_SE_ASYNC) && HAVE_SE_ASYNC

#if MAX_TEST_FRAGMENT_SIZE > 4096
/* ASYNC operations do not support non-contiguous buffers.  Allocating
 * very large buffers from contiguous memory may not be possible, so
 * disable the async tests. If allocating large buffers are OK => just
 * disable this #error and run the tests!
 */
#error "Disable ASYNC_TEST with large fragment size tests"
#endif

#define ASYNC_TEST 1
#else
#define ASYNC_TEST 0
#endif /* defined(HAVE_SE_ASYNC) && HAVE_SE_ASYNC */

#if defined(HAVE_MD5) && HAVE_MD5
#define TEST_MD5
#else
#undef TEST_MD5
#endif

#if defined(HAVE_SW_WHIRLPOOL) && HAVE_SW_WHIRLPOOL
#define TEST_WHIRLPOOL
#else
#undef TEST_WHIRLPOOL
#endif

#if defined(CCC_WITH_XMSS) && CCC_WITH_XMSS
#if !defined(TEST_XMSS)
#define TEST_XMSS
#endif
#else
#undef TEST_XMSS
#endif

#endif /* KERNEL_TEST_MODE */
#endif /* INCLUDED_SETEST_KERNEL_TESTS_DEFS_H */
