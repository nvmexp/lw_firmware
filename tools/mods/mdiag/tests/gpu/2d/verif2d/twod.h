/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _VERIF2D_TWOD_H_
#define _VERIF2D_TWOD_H_

class V2dTwoD: public V2dClassObj
{
public:
    V2dTwoD(Verif2d *v2d): V2dClassObj(v2d) {}
    virtual ~V2dTwoD() {}

    void Init(ClassId classId);

    void SetSurfaces(map<string, V2dSurface *> &argMap);
};

#endif // _VERIF2D_TWOD_H_
