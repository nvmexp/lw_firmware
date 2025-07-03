#include "test.h"
#include <lwmisc.h>
#include <lwriscv/sbi.h>
#include <lwriscv/csr.h>
#include <liblwriscv/print.h>
#include <liblwriscv/io.h>

char req_tfbif_regioncfg_0[] = "Must provide 3 parameter for region, vpr, and apert";
char req_tfbif_regioncfg_1[] = "REQ: Return SBI_SUCCESS upon success";
char req_tfbif_regioncfg_2[] = "REQ: Vpr for specified region shall be updated upon success";
char req_tfbif_regioncfg_3[] = "REQ: Apert for specified region shall be updated upon success";
char req_tfbif_regioncfg_4[] = "REQ: Return SBI_ERR_ILWALID_PARAM and value 1 if region is not 0-7";
char req_tfbif_regioncfg_5[] = "REQ: Return SBI_ERR_ILWALID_PARAM and value 2 if vpr is not a valid 1bit value";
char req_tfbif_regioncfg_6[] = "REQ: Return SBI_ERR_ILWALID_PARAM and value 3 if apert is not a valid 5bit value";

static SBI_RETURN_VALUE get_condition_tfbif_regioncfg(char** msg, uint64_t region, uint64_t vpr, uint64_t apert)
{
    SBI_RETURN_VALUE rc;
    rc.error = SBI_SUCCESS;
    rc.value = 0;
    *msg = req_tfbif_regioncfg_1;

    if (region > 7)
    {
        rc.error = SBI_ERR_ILWALID_PARAM;
        rc.value = 1;
        *msg = req_tfbif_regioncfg_4;
        return rc;
    }

    if (vpr > 0x1UL)
    {
        rc.error = SBI_ERR_ILWALID_PARAM;
        rc.value = 2;
        *msg = req_tfbif_regioncfg_5;
        return rc;
    }

    if (apert > 0x1FUL)
    {
        rc.error = SBI_ERR_ILWALID_PARAM;
        rc.value = 3;
        *msg = req_tfbif_regioncfg_6;
        return rc;
    }

    return rc;
}

int test_tfbif_regioncfg(int32_t funcid, uint64_t num_args, uint64_t* param)
{
    TEST_EXPECT(PASS);

    if (num_args < 3)
    {
        TEST_FAILED("%s\n", req_tfbif_regioncfg_0);
        return -1;
    }

    char* req_msg;
    uint64_t region = *param;
    uint64_t vpr = *(param + 1);
    uint64_t apert = *(param + 2);

    // Get expected conditions and corresponding requirements
    SBI_RETURN_VALUE expected_rc = get_condition_tfbif_regioncfg(&req_msg, region, vpr, apert);

    SBI_RETURN_VALUE rc = sbicall3(SBI_EXTENSION_LWIDIA, funcid, region, vpr, apert);

    // Make sure we get expected return values
    EXPECT_TRUE((expected_rc.error == rc.error) && (expected_rc.value == rc.value), "%s, exp_err: %ld, exp_value: 0x%lx, test_err: %ld, test_value: 0x%lx\n", req_msg, expected_rc.error, expected_rc.value, rc.error, rc.value);

    // NOTE: due to lack of tfbif engine on fsp, actual register write is not tested

    // notify pass
    TEST_PASSED;
    return 0;
}
