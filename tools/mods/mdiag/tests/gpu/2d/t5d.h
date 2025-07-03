/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007, 2019, 2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _SUBCHSW_H_
#define _SUBCHSW_H_

#include "mdiag/utils/types.h"

#include "mdiag/tests/gpu/lwgpu_single.h"
#include "mdiag/tests/gpu/trace_3d/trace_3d.h"
#include "mdiag/utils/buf.h"
#include "mdiag/tests/gpu/trace_3d/trace_and_2d.h"

class GrRandom;

class Trace5dTest : public TraceAnd2dTest<Trace3DTest> {
 public:
    Trace5dTest(ArgReader *params);

    virtual ~Trace5dTest(void);

    static Test *Factory(ArgDatabase *args);

    virtual void CleanUp(void);

 protected:
    int WriteMethod(int subchannel, UINT32 address, UINT32 data);
    virtual bool BeforeExelwteMethods(void);
    virtual bool RunBeforeExelwteMethods(void);
    virtual bool BeforeEachMethod(void);
    virtual bool BeforeEachMethodGroup(unsigned num_methods);
    virtual bool AfterExelwteMethods(void);
    virtual bool RunAfterExelwteMethods(void);
    void Rulw2dTest( void );
    bool RunSetup2d(void);

    virtual void CheckValidationBuffer(void);

    bool do2dBefore, do2dAfter;
    int num2dBefore, num2dAfter;
    int singleMethod;
    bool interleaveWithRect, interleaveWithBlit, interleaveWithScaled, interleaveWithDvd10;
    unsigned subchswGran, subchswInterval, subchswPoint, subchswNum;
    unsigned tcSubchswPoint, tcSubchswCount, tcLoopCount, tcMethodCount;
    int height2d, width2d, max_height2d, max_width2d;

    virtual Test::TestStatus CheckPassFail(void);
    Test::TestStatus testStatus2d;

    ArgReader *m_pParams;

    // temporarily added here
    GrRandom *grRandom;
    Buf* buf;

  private:
    int Standard2DSetup(void);
};

#ifdef MAKE_TEST_LIST
CREATE_TEST_LIST_ENTRY(t5d, Trace5dTest, "trace_3d with various 2d tests interleaved with the trace");
#undef TEST_LIST_HEAD
#define TEST_LIST_HEAD &t5d_testentry
#endif

#endif
