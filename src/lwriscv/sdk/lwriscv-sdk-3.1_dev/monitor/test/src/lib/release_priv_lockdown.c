#include "test.h"
#include <lwmisc.h>
#include <lwriscv/sbi.h>
#include <lwriscv/csr.h>
#include <liblwriscv/print.h>
#include <liblwriscv/io.h>

char req_release_priv_lockdown_0[] = "REQ: Return SBI_SUCCESS";
char req_release_priv_lockdown_1[] = "REQ: Riscv br priv lockdown must be unlocked";

int test_release_priv_lockdown(int32_t funcid)
{
    TEST_EXPECT(PASS);

    // Get expected conditions and corresponding requirements
    SBI_RETURN_VALUE expected_rc;
    expected_rc.error = SBI_SUCCESS;
    expected_rc.value = 0;

    // set precondition, lock the priv lockdown
    localWrite(LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN,
               DRF_DEF(_PRGNLCL, _RISCV_BR_PRIV_LOCKDOWN, _LOCK, _LOCKED));

    SBI_RETURN_VALUE rc = sbicall0(SBI_EXTENSION_LWIDIA, funcid);

    // Make sure we get expected return values
    EXPECT_TRUE((expected_rc.error == rc.error) && (expected_rc.value == rc.value), "%s, exp_err: %ld, exp_value: 0x%lx, test_err: %ld, test_value: 0x%lx\n", req_release_priv_lockdown_0, expected_rc.error, expected_rc.value, rc.error, rc.value);

    // Make sure the priv lockdown is succesfully released
    if (expected_rc.error == SBI_SUCCESS)
    {
        uint32_t lock_state = localRead(LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN);
        EXPECT_TRUE(lock_state == LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN_LOCK_UNLOCKED, "%s\n", req_release_priv_lockdown_1);
    }

    // notify pass
    TEST_PASSED;
    return 0;
}
