/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _VERIF2D_NULL_H_
#define _VERIF2D_NULL_H_

//
// null class
//

class V2dNullClass : public V2dClassObj
{
public:
    V2dNullClass( Verif2d *v2d ) : V2dClassObj( v2d ) {}
    virtual ~V2dNullClass( void ) { }
    virtual void Init( ClassId classId )
        {
            V2dClassObj::Init( classId );
        }
};

#endif // _VERIF2D_NULL_H_
