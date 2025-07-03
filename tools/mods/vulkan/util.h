/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#pragma once
#ifndef UTIL_H
#define UTIL_H

#include "vkmain.h"
#include "core/include/rc.h"

#define MATH_PI 3.14159265358979323846f

//--------------------------------------------------------------------
class Time
{
public:
    Time();
    ~Time() {}
    void    StartTimeRecording();
    void    StopTimeRecording();
    UINT64  GetTimeSinceLastRecordingUS() const;

private:
#ifdef VULKAN_STANDALONE
    UINT64  m_Frequency = 1;
#endif
    UINT64  m_StartTime = 0;
    UINT64  m_EndTime = 0;
    bool    m_IsTimerActive = false;
};

//--------------------------------------------------------------------
class GLMatrix
{
public:
    enum axis
    {
        xAxis = 0
        ,yAxis = 1
        ,zAxis = 2
    };

    float mat[4][4];
    GLMatrix()
    {
        for (UINT32 i = 0; i < 4; i++)
        {
            for (UINT32 j = 0; j < 4; j++)
            {
                mat[i][j] = 0.0f;
            }
        }
    }

    GLMatrix (const GLMatrix &rhs);
    // Construct a matrix with only 3 rows, & set default perspective divide to 1.0
    GLMatrix (const float mtx[12]);
    GLMatrix (vector<float>& v);

    GLMatrix & operator = (const float m[12]);
    GLMatrix operator * (const GLMatrix &rhs);
    GLMatrix & operator = (const GLMatrix &rhs);
    GLMatrix & operator = (const vector<float> &rhs);

    void SetValues(const float m[12]);
    void SetValues(const GLMatrix &rhs);
    void SetValues(const vector<float> &rhs);

    void MakeIdentity();
    void Transpose();
    void PrintMatrix();

    RC ApplyFrustum(float left, float right, float bottom, float top, float zNear, float zFar);
    void ApplyTranslation(float x, float y, float z);
    void ApplyRotation(int axis, float angle);
    void ApplyRotation(float angleInDegrees, float axisX, float axisY, float axisZ);
    void ApplyScaling(float x, float y, float z);

    RC MakeFrustum(float left, float right, float bottom, float top, float zNear, float zFar);
    static void Normalize(float vout[3], float v[3]);

};

//--------------------------------------------------------------------
class GLCamera
{
public:
    GLCamera();

    RC SetProjectionMatrix(float FOVDegrees, float aspectRatio, float zNear, float zFar);
    RC SetFrustum(float left, float right, float bottom, float top, float zNear, float zFar);
    void SetCameraPosition(float posx, float posy, float posz);

    static GLMatrix GetModelMatrix
    (
        float posx, float posy, float posz,
        float angleXdeg, float angleYdeg, float angleZdeg,
        float scaleX, float scaleY, float scaleZ
    );

    GLMatrix GetMVPMatrix(GLMatrix &modelMatrix);
    GLMatrix GetViewMatrix() { return m_ViewMatrix; }
    GLMatrix GetProjectionMatrix() { return m_ProjectionMatrix; }

private:
    GLMatrix m_ViewMatrix;
    GLMatrix m_ProjectionMatrix;

    GLMatrix m_VulkanCorrectionMtx;
    bool m_CallwlateViewProjection;
    GLMatrix m_ViewProjectionMatrix;
};

//--------------------------------------------------------------------
//! \brief Generate Geometry (Positions/Normals/Color/Tex Coordinates/Texture Data)
//!
class VkGeometry
{
public:
    enum GeometryType
    {
        GEOMETRY_TRIANGLE,
        GEOMETRY_TORUS,
        GEOMETRY_PYRAMID
    };

    VkGeometry() = default;
    virtual ~VkGeometry() {}

    SETGET_PROP(GeometryType, UINT32);

    static constexpr UINT32 GetPositionSize() { return sizeof(Position) / sizeof(float); }
    static constexpr UINT32 GetNormalSize() { return sizeof(Normal) / sizeof(float); }
    static constexpr UINT32 GetColorSize() { return sizeof(Color) / sizeof(float); }
    static constexpr UINT32 GetTxCoordSize() { return sizeof(TexCoordinates) / sizeof(float); }

    void GetVertexData
    (
        vector<UINT32> &indices,
        vector<float> &positions,
        vector<float> &normals,
        vector<float> &colors,
        vector<float> &txCoords
    ) const;

    static void GetTriangleData
    (
        vector<UINT32> &indices,
        vector<float> &positions,
        vector<float> &normals,
        vector<float> &colors,
        vector<float> &txCoords
    );

    static void GetPyramidData
    (
        vector<UINT32> &indices,
        vector<float> &positions,
        vector<float> &normals,
        vector<float> &colors,
        vector<float> &txCoords
    );

    static void GetTorusData
    (
        vector<UINT32> &indices,
        vector<float> &positions,
        vector<float> &normals,
        vector<float> &colors,
        vector<float> &txCoords
    );

    static void GetTextureData
    (
        vector<UINT08> &data,
        UINT32 width,
        UINT32 height,
        UINT32 depth,
        UINT64 pitch
    );

private:
    struct Position
    {
        float x, y, z, w;
        Position()
        {
            x = 0; y = 0; z = 0; w = 1;
        }
        Position(float x1, float y1, float z1, float w1)
        {
            x = x1;
            y = y1;
            z = z1;
            w = w1;
        }
    };

    struct Normal
    {
        float x, y, z;
        Normal()
        {
            x = 0; y = 0; z = 0;
        }
        Normal(float x1, float y1, float z1)
        {
            x = x1;
            y = y1;
            z = z1;
        }
    };

    struct Color
    {
        float red, green, blue, alpha;

        Color()
        {
            red = 0; green = 0; blue = 0; alpha = 1;
        }
        Color(float r, float g, float b, float a)
        {
            red = r;
            green = g;
            blue = b;
            alpha = a;
        }
    };

    struct TexCoordinates
    {
        float u, v;

        TexCoordinates()
        {
            u = 0; v = 0;
        }
        TexCoordinates(float U, float V)
        {
            u = U;
            v = V;
        }
    };

    struct Vertex
    {
        Position position;
        Color    color;
        Normal   normal;
        TexCoordinates txCoords;

        Vertex(Position pos, Normal norm, Color col, TexCoordinates txc)
        {
            position = pos;
            color = col;
            normal = norm;
            txCoords = txc;
        }
    };

    UINT32 m_GeometryType = GEOMETRY_TRIANGLE;
};

#endif
