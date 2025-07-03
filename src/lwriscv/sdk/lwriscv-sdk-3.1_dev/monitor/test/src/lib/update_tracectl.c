#include "test.h"
#include <lwmisc.h>
#include <lwriscv/sbi.h>
#include <lwriscv/csr.h>
#include <liblwriscv/print.h>
#include <liblwriscv/io.h>

char req_update_tracectl_0[] = "Must provide parameter for new tracectl";
char req_update_tracectl_1[] = "REQ: Return SBI_SUCCESS upon success";
char req_update_tracectl_2[] = "REQ: Riscv tracectl shall be updated upon success";
char req_update_tracectl_3[] = "REQ: Return SBI_ERR_ILWALID_PARAM and value 1 if param is not a valid 32bit value";
char req_update_tracectl_4[] = "REQ: Return SBI_ERR_ILWALID_PARAM and value 2 if mode field is not valid";

static SBI_RETURN_VALUE get_condition_update_tracectl(char** msg, uint64_t param)
{
    SBI_RETURN_VALUE rc;
    rc.error = SBI_SUCCESS;
    rc.value = 0;
    *msg = req_update_tracectl_1;

    if (param > 0xFFFFFFFFUL)
    {
        rc.error = SBI_ERR_ILWALID_PARAM;
        rc.value = 1;
        *msg = req_update_tracectl_3;
        return rc;
    }

    if (DRF_VAL(_PRGNLCL, _RISCV_TRACECTL, _MODE, (uint32_t)param) == 0x3)
    {
        rc.error = SBI_ERR_ILWALID_PARAM;
        rc.value = 2;
        *msg = req_update_tracectl_4;
        return rc;
    }
    return rc;
}

int test_update_tracectl(int32_t funcid, uint64_t num_args, uint64_t* param)
{
    TEST_EXPECT(PASS);

    if (num_args < 1)
    {
        TEST_FAILED("%s\n", req_update_tracectl_0);
        return -1;
    }

    char* req_msg;
    uint64_t value = *param;

    // Get expected conditions and corresponding requirements
    SBI_RETURN_VALUE expected_rc = get_condition_update_tracectl(&req_msg, value);

    // set precondition, clear the value to begin with
    localWrite(LW_PRGNLCL_RISCV_TRACECTL, 0);

    SBI_RETURN_VALUE rc = sbicall1(SBI_EXTENSION_LWIDIA, funcid, value);

    // Make sure we get expected return values
    EXPECT_TRUE((expected_rc.error == rc.error) && (expected_rc.value == rc.value), "%s, exp_err: %ld, exp_value: 0x%lx, test_err: %ld, test_value: 0x%lx\n", req_msg, expected_rc.error, expected_rc.value, rc.error, rc.value);

    // Make sure the tracectl is successfully updated
    if (expected_rc.error == SBI_SUCCESS)
    {
        uint32_t tracectl = localRead(LW_PRGNLCL_RISCV_TRACECTL);
        EXPECT_TRUE(tracectl == (uint32_t)value, "%s, expected: 0x%lx, real: 0x%x\n", req_update_tracectl_2, value, tracectl);
    }

    // notify pass
    TEST_PASSED;
    return 0;
}
