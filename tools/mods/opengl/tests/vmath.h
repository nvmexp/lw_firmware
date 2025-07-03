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
// IJK defines a vector in 3D space.
typedef struct
{
  float i, j, k;
} IJK;

inline float deg(float r)
{
  return 57.29577951*r;
}

inline float rad(float d)
{
  return 0.017453292*d;
}

inline void aclwm_vector(IJK *dst, IJK *src)
{
  dst->i += src->i;
  dst->j += src->j;
  dst->k += src->k;
}

inline void cross_product(IJK *v1, IJK *v2, IJK *v3)
{
  v3->i=(v1->j*v2->k-v1->k*v2->j);
  v3->j=(v1->k*v2->i-v1->i*v2->k);
  v3->k=(v1->i*v2->j-v1->j*v2->i);
}

inline float normalize_vector(IJK *v)
{
  float length;

  length=sqrt(v->i*v->i+v->j*v->j+v->k*v->k);
  if (length>0)
  {
    v->i/=length;
    v->j/=length;
    v->k/=length;
  }

  return length;
}

inline float dot_product(IJK *v1,  IJK *v2)
{
  return v1->i*v2->i+v1->j*v2->j+v1->k*v2->k;
}

inline float give_vector_length(IJK *v)
{
  return sqrt(dot_product(v,v));
}

inline float give_vector_angle(IJK *v1, IJK *v2)
{
  return acos(dot_product(v1,v2)/(give_vector_length(v1)*give_vector_length(v2)));
}
