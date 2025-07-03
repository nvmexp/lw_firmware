#include "test.h"
#include <lwmisc.h>
#include <lwriscv/sbi.h>
#include <lwriscv/csr.h>
#include <liblwriscv/print.h>
#include <liblwriscv/io.h>

char req_fbif_transcfg_0[] = "Must provide 2 parameter for region & value";
char req_fbif_transcfg_1[] = "REQ: Return SBI_SUCCESS upon success";
char req_fbif_transcfg_2[] = "REQ: Transcfg for specified region shall be updated upon success";
char req_fbif_transcfg_3[] = "REQ: Return SBI_ERR_ILWALID_PARAM and value 1 if region is not 0-7";
char req_fbif_transcfg_4[] = "REQ: Return SBI_ERR_ILWALID_PARAM and value 2 if transcfg is not a valid 32bit value";

static SBI_RETURN_VALUE get_condition_fbif_transcfg(char** msg, uint64_t region, uint64_t transcfg)
{
    SBI_RETURN_VALUE rc;
    rc.error = SBI_SUCCESS;
    rc.value = 0;
    *msg = req_fbif_transcfg_1;

    if (region > 7)
    {
        rc.error = SBI_ERR_ILWALID_PARAM;
        rc.value = 1;
        *msg = req_fbif_transcfg_3;
        return rc;
    }

    if (transcfg > 0xFFFFFFFFUL)
    {
        rc.error = SBI_ERR_ILWALID_PARAM;
        rc.value = 2;
        *msg = req_fbif_transcfg_4;
        return rc;
    }

    return rc;
}

static void clear_all_transcfg(void)
{
    localWrite(LW_PRGNLCL_FBIF_TRANSCFG(0), 0);
    localWrite(LW_PRGNLCL_FBIF_TRANSCFG(1), 0);
    localWrite(LW_PRGNLCL_FBIF_TRANSCFG(2), 0);
    localWrite(LW_PRGNLCL_FBIF_TRANSCFG(3), 0);
    localWrite(LW_PRGNLCL_FBIF_TRANSCFG(4), 0);
    localWrite(LW_PRGNLCL_FBIF_TRANSCFG(5), 0);
    localWrite(LW_PRGNLCL_FBIF_TRANSCFG(6), 0);
    localWrite(LW_PRGNLCL_FBIF_TRANSCFG(7), 0);
    
}

int test_fbif_transcfg(int32_t funcid, uint64_t num_args, uint64_t* param)
{
    TEST_EXPECT(PASS);

    if (num_args < 2)
    {
        TEST_FAILED("%s\n", req_fbif_transcfg_0);
        return -1;
    }

    char* req_msg;
    uint64_t region = *param;
    uint64_t transcfg = *(param + 1);

    // Get expected conditions and corresponding requirements
    SBI_RETURN_VALUE expected_rc = get_condition_fbif_transcfg(&req_msg, region, transcfg);

    clear_all_transcfg();

    SBI_RETURN_VALUE rc = sbicall2(SBI_EXTENSION_LWIDIA, funcid, region, transcfg);

    // Make sure we get expected return values
    EXPECT_TRUE((expected_rc.error == rc.error) && (expected_rc.value == rc.value), "%s, exp_err: %ld, exp_value: 0x%lx, test_err: %ld, test_value: 0x%lx\n", req_msg, expected_rc.error, expected_rc.value, rc.error, rc.value);

    // Make sure the tracectl is successfully updated
    if (expected_rc.error == SBI_SUCCESS)
    {
        uint32_t transcfg_r = localRead(LW_PRGNLCL_FBIF_TRANSCFG((uint32_t)region));
        EXPECT_TRUE(transcfg_r == (uint32_t)transcfg, "%s, expected: 0x%lx, real: 0x%x\n", req_fbif_transcfg_2, transcfg, transcfg_r);
    }

    // notify pass
    TEST_PASSED;
    return 0;
}
