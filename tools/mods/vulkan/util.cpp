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

#include "util.h"
#include "lwdiagutils.h"

#ifndef VULKAN_STANDALONE
#include "core/include/patterns.h"
#endif

//--------------------------------------------------------------------
Time::Time()
{
#if defined(VULKAN_STANDALONE) && defined(_WIN32)
    QueryPerformanceFrequency((LARGE_INTEGER *)&m_Frequency);
#endif
}

//--------------------------------------------------------------------
void Time::StartTimeRecording()
{
#if defined(VULKAN_STANDALONE) && defined(_WIN32)
    UINT64 startTime;
    QueryPerformanceCounter((LARGE_INTEGER *)&startTime);
    m_StartTime = static_cast<UINT64>(((float)startTime / m_Frequency) * 1000000.0f);
#else
    m_StartTime = Xp::GetWallTimeUS();
#endif
    m_IsTimerActive = true;
}

//--------------------------------------------------------------------
void Time::StopTimeRecording()
{
    if (m_IsTimerActive)
    {
#if defined(VULKAN_STANDALONE) && defined(_WIN32)
        UINT64 endTime;
        QueryPerformanceCounter((LARGE_INTEGER *)&endTime);
        m_EndTime = static_cast<UINT64>(((float)endTime / m_Frequency) * 1000000.0f);
#else
        m_EndTime = Xp::GetWallTimeUS();
#endif
        m_IsTimerActive = false;
    }
}

//--------------------------------------------------------------------
UINT64 Time::GetTimeSinceLastRecordingUS() const
{
    return m_EndTime - m_StartTime;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
GLMatrix::GLMatrix(const GLMatrix &rhs)
{
    SetValues(rhs);
}

//------------------------------------------------------------------------------
void GLMatrix::SetValues(const GLMatrix &rhs)
{
    for (UINT32 i = 0; i < 4; i++)
    {
        for (UINT32 j = 0; j < 4; j++)
        {
            mat[i][j] = rhs.mat[i][j];
        }
    }
}

//------------------------------------------------------------------------------
// Construct a matrix using a 3 row matrix and default the perspective divide to 1.0
GLMatrix::GLMatrix(const float m[12])
{
    SetValues(m);
}
GLMatrix & GLMatrix::operator= (const float m[12])
{
    SetValues(m);
    return *this;
}
void GLMatrix::SetValues(const float m[12])
{
    UINT32 i = 0;
    for (UINT32 row = 0; row < 3; row++)
    {
        for (UINT32 col = 0; col < 4; col++)
        {
            mat[row][col] = m[i++];
        }
    }
    // default perspective divide to 1
    mat[3][0] = mat[3][1] = mat[3][2] = 0.0;
    mat[3][3] = 1.0;
}

//-------------------------------------------------------------------------------------------------
// Construct a 3x4 or 4x4 matrix from a vector of floats. If the size of the vector is 12 then
// we assume its a 3x4 matrix and the perspective divide will be set to 1.0.
GLMatrix::GLMatrix(vector<float>& v)
{
    SetValues(v);
}
GLMatrix & GLMatrix::operator= (const vector<float> &rhs)
{
    SetValues(rhs);
    return *this;
}
void GLMatrix::SetValues(const vector<float>& v)
{
    UINT32 i = 0;
    UINT32 rows = 4;
    // we only deal with 4x4 or 4x3 matrices
    MASSERT((v.size() == 12) || (v.size() == 16));
    if (v.size() == 12)
    {
        rows = 3;
    }
    for (UINT32 row = 0; row < rows; row++)
    {
        for (UINT32 col = 0; col < 4; col++)
        {
            mat[row][col] = v[i++];
        }
    }
    if (rows == 3)
    {
        mat[3][0] = mat[3][1] = mat[3][2] = 0;
        mat[3][3] = 1.0;
    }
}
//--------------------------------------------------------------------
GLMatrix & GLMatrix::operator= (const GLMatrix &rhs)
{
    if (this == &rhs)
    {
        return *this;
    }

    SetValues(rhs);
    return *this;
}

//--------------------------------------------------------------------
GLMatrix GLMatrix::operator* (const GLMatrix &rhs)
{
    GLMatrix prodMatrix;
    for (UINT32 i = 0; i < 4; i++)
    {
        for (UINT32 j = 0; j < 4; j++)
        {
            for (UINT32 k = 0; k < 4; k++)
            {
                prodMatrix.mat[i][j] += mat[i][k] * rhs.mat[k][j];
            }
        }
    }
    return prodMatrix;
}

//--------------------------------------------------------------------
void GLMatrix::MakeIdentity()
{
    for (UINT32 i = 0; i < 4; i++)
    {
        for (UINT32 j = 0; j < 4; j++)
        {
            mat[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
}

//-------------------------------------------------------------------------------------------------
// Setup a perspective matrix that produces a perspective projection.
// A frustum is a truncated section of a pyramid viewed from the narrow end to the broad end.
// left, right, top, bottom & zNear specify the points of the near clipping plane. zFar specifies
// the far clipping plane. Both zNear & zFar must be positive, right & left must not be equal, and
// top and bottom must not be equal.
RC GLMatrix::MakeFrustum
(
    float left,
    float right,
    float bottom,
    float top,
    float zNear,
    float zFar
)
{
    const float deltaX = (right - left);
    const float deltaY = (top - bottom);
    const float deltaZ = (zFar - zNear);
    if ((zNear <= 0.0) || (zFar <= 0.0) || (deltaX == 0.0) || (deltaY == 0.0) || (deltaZ == 0.0))
    {
        // zNear & zFar must be positive nonequal values, right & left must not be equal
        // and top & bottom must not be equal.
        return RC::BAD_PARAMETER;
    }

    // row major
    mat[0][0] = (zNear * 2.0f) / deltaX;
    mat[0][1] = 0.0f;
    mat[0][2] = (right + left) / deltaX;
    mat[0][3] = 0.0f;

    mat[1][0] = 0.0f;
    mat[1][1] = (zNear * 2.0f) / deltaY;
    mat[1][2] = (top + bottom) / deltaY;
    mat[1][3] = 0.0f;

    mat[2][0] = 0.0f;
    mat[2][1] = 0.0f;
    mat[2][2] = -(zFar + zNear) / deltaZ;
    mat[2][3] = -2.0f * zFar * zNear / deltaZ;

    mat[3][0] = 0.0f;
    mat[3][1] = 0.0f;
    mat[3][2] = -1.0f;
    mat[3][3] = 0.0f;

    return OK;
}

//------------------------------------------------------------------------------------------------
// see GLMatric::MakeFrustum()
RC GLMatrix::ApplyFrustum
(
    float left,
    float right,
    float bottom,
    float top,
    float zNear,
    float zFar
)
{
    RC rc;
    GLMatrix frustumMatrix;
    CHECK_RC(frustumMatrix.MakeFrustum(left, right, bottom, top, zNear, zFar));
    (*this) = frustumMatrix * (*this);
    return rc;
}

//--------------------------------------------------------------------
void GLMatrix::Transpose()
{
    const GLMatrix copy = *this;
    for (UINT32 i = 0; i < 4; i++)
    {
        for (UINT32 j = 0; j < 4; j++)
        {
            mat[i][j] = copy.mat[j][i];
        }
    }
}

//--------------------------------------------------------------------
void GLMatrix::PrintMatrix()
{
    for (UINT32 i = 0; i < 4; i++)
    {
        Printf(Tee::PriNormal, "\t");
        for (UINT32 j = 0; j < 4; j++)
        {
            Printf(Tee::PriNormal, "%.3f  ", mat[i][j]);
        }
        Printf(Tee::PriNormal, "\n");
    }
    Printf(Tee::PriNormal, "\n");
}

//--------------------------------------------------------------------
void GLMatrix::ApplyTranslation(float x, float y, float z)
{
    GLMatrix translationMatrix;
    translationMatrix.MakeIdentity();
    translationMatrix.mat[0][3] = x;
    translationMatrix.mat[1][3] = y;
    translationMatrix.mat[2][3] = z;

    (*this) = translationMatrix * (*this);
}

//-------------------------------------------------------------------------------------------------
// Normalize the vector
// This code was borrowed from OpenGL library.
void GLMatrix::Normalize
(
    float vout[3],
    float v[3]
)
{
    float len = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
    if (len <= 0.0)
    {
        vout[0] = 0.0;
        vout[1] = 0.0;
        vout[2] = 0.0;
        return;
    }
    if (len == 1.0F)
    {
        vout[0] = v[0];
        vout[1] = v[1];
        vout[2] = v[2];
        return;
    }
    else
    {
        union
        {
            unsigned int i;
            float f;
        } seed;

        /*
        ** This code callwlates a reciprocal square root accurate to well over
        ** 16 bits using Newton-Raphson approximation.
        **
        ** To callwlate the seed, the shift compresses the floating-point
        ** range just as sqrt() does, and the subtract ilwerts the range
        ** like reciprocation does.  The constant was chosen by trial-and-error
        ** to minimize the maximum error of the iterated result for all values
        ** over the range .5 to 2.
        */
        seed.f = len;
        seed.i = 0x5f375a00u - (seed.i >> 1);

        /*
        ** The Newton-Raphson iteration to approximate X = 1/sqrt(Y) is:
        **
        **      X[1] = .5*X[0]*(3 - Y*X[0]^2)
        **
        ** A double iteration is:
        **
        **      X[2] = .0625*X[0]*(3 - Y*X[0]^2)*[12 - (Y*X[0]^2)*(3 - Y*X[0]^2)^2]
        **
        */
        const float xy = len * seed.f * seed.f;
        const float subexp = 3.f - xy;
        len = .0625f * seed.f * subexp * (12.f - xy * subexp * subexp);
        vout[0] = v[0] * len;
        vout[1] = v[1] * len;
        vout[2] = v[2] * len;
        return;
    }
}

//-------------------------------------------------------------------------------------------------
// Apply a rotation matrix around a vector at x, y, z coordinates.
// Note x, y, z are normalized before applying rotation.
//
void GLMatrix::ApplyRotation
(
    float angleInDegrees,
    float axisX,
    float axisY,
    float axisZ
)
{
    GLMatrix rotMat;     // rotation matrix
    const float angleInRadians = angleInDegrees * (MATH_PI / 180.0f);
    const float cosvalue = LwDiagUtils::Cos(angleInRadians);
    const float silwalue = LwDiagUtils::Sin(angleInRadians);
    float axis[3] = { axisX, axisY, axisZ };
    Normalize(axis, axis);

    const float xy1mcos = (axis[0] * axis[1] * (1 - cosvalue));
    const float yz1mcos = (axis[1] * axis[2] * (1 - cosvalue));
    const float zx1mcos = (axis[2] * axis[0] * (1 - cosvalue));

    const float xs = (axis[0] * silwalue);
    const float ys = (axis[1] * silwalue);
    const float zs = (axis[2] * silwalue);
    const float xx = (axis[0] * axis[0]);
    const float yy = (axis[1] * axis[1]);
    const float zz = (axis[2] * axis[2]);

    rotMat.mat[0][0] = xx + cosvalue * (1 - xx);
    rotMat.mat[1][0] = xy1mcos + zs;
    rotMat.mat[2][0] = zx1mcos + ys;
    rotMat.mat[3][0] = 0.0f;

    rotMat.mat[0][1] = xy1mcos - zs;
    rotMat.mat[1][1] = yy + cosvalue * (1 - yy);
    rotMat.mat[2][1] = yz1mcos + xs;
    rotMat.mat[3][1] = 0.0f;

    rotMat.mat[0][2] = zx1mcos + ys;
    rotMat.mat[1][2] = yz1mcos - xs;
    rotMat.mat[2][2] = zz + cosvalue * (1 - zz);
    rotMat.mat[3][2] = 0.0f;

    rotMat.mat[0][3] = 0.0f;
    rotMat.mat[1][3] = 0.0f;
    rotMat.mat[2][3] = 0.0f;
    rotMat.mat[3][3] = 1.0f;

    (*this) = rotMat * (*this);
}

//--------------------------------------------------------------------
void GLMatrix::ApplyRotation
(
    int axis,
    float angleInDegrees
)
{
    const float angleInRadians = angleInDegrees * (MATH_PI / 180.0f);
    const float cosvalue = LwDiagUtils::Cos(angleInRadians);
    const float silwalue = LwDiagUtils::Sin(angleInRadians);

    GLMatrix rotationalMatrix;
    rotationalMatrix.MakeIdentity();

    switch (axis)
    {

    case xAxis:
        rotationalMatrix.mat[1][1] = cosvalue;
        rotationalMatrix.mat[1][2] = -silwalue;
        rotationalMatrix.mat[2][1] = silwalue;
        rotationalMatrix.mat[2][2] = cosvalue;
        break;

    case yAxis:
        rotationalMatrix.mat[0][0] = cosvalue;
        rotationalMatrix.mat[0][2] = silwalue;
        rotationalMatrix.mat[2][0] = -silwalue;
        rotationalMatrix.mat[2][2] = cosvalue;
        break;

    case zAxis:
        rotationalMatrix.mat[0][0] = cosvalue;
        rotationalMatrix.mat[0][1] = -silwalue;
        rotationalMatrix.mat[1][0] = silwalue;
        rotationalMatrix.mat[1][1] = cosvalue;
        break;
    default:
        MASSERT(!"Unrecognized axis");
    }

    (*this) = rotationalMatrix * (*this);
}

//--------------------------------------------------------------------
void GLMatrix::ApplyScaling
(
    float x,
    float y,
    float z
)
{
    GLMatrix scaledMatrix;
    scaledMatrix.MakeIdentity();
    scaledMatrix.mat[0][0] = x;
    scaledMatrix.mat[1][1] = y;
    scaledMatrix.mat[2][2] = z;

    (*this) = scaledMatrix * (*this);
}

//------------------------------------------------------------------------------
GLCamera::GLCamera()
{
    m_ViewMatrix.MakeIdentity();
    m_ProjectionMatrix.MakeIdentity();

    // Vulkan clip space has ilwerted Y and half Z.
    m_VulkanCorrectionMtx.MakeIdentity();
    m_VulkanCorrectionMtx.mat[1][1] *= -1.0f;
    m_VulkanCorrectionMtx.mat[2][2] *= 0.5f;
    m_VulkanCorrectionMtx.mat[3][2] *= 0.5f;

    m_CallwlateViewProjection = true;
    m_ViewProjectionMatrix.MakeIdentity();
}

//------------------------------------------------------------------------------
RC GLCamera::SetProjectionMatrix
(
    float FOVDegrees,
    float aspectRatio,
    float zNear,
    float zFar
)
{
    m_ProjectionMatrix.MakeIdentity();

    if (FOVDegrees <= 0 || aspectRatio == 0 || (zNear == zFar))
    {
        Printf(Tee::PriError, "Incorrect parameters passed to Set Projection Matrix\n");
        return RC::SOFTWARE_ERROR;
    }

    const float scaleFactor = 1.0f / tan(FOVDegrees * 0.5f * (MATH_PI / 180.0f));
    m_ProjectionMatrix.mat[0][0] = scaleFactor / aspectRatio;
    m_ProjectionMatrix.mat[1][1] = scaleFactor;
    m_ProjectionMatrix.mat[2][2] = -(zFar + zNear) / (zFar - zNear);
    m_ProjectionMatrix.mat[3][3] = 0.0;
    m_ProjectionMatrix.mat[3][2] = -1.0;
    m_ProjectionMatrix.mat[2][3] = -(2 * zFar * zNear) / (zFar - zNear);

    return RC::OK;
}

//------------------------------------------------------------------------------
// see GLMatrix::MakeFrustum()
RC GLCamera::SetFrustum
(
    float left,
    float right,
    float bottom,
    float top,
    float zNear,
    float zFar
)
{
    m_ProjectionMatrix.MakeIdentity();
    return m_ProjectionMatrix.ApplyFrustum(left, right, bottom, top, zNear, zFar);
}

//------------------------------------------------------------------------------
void GLCamera::SetCameraPosition
(
    float posx,
    float posy,
    float posz
)
{
    //By default the camera is at origin looking down -Z
    m_ViewMatrix.MakeIdentity();
    m_ViewMatrix.ApplyTranslation(posx, posy, posz);
}

//------------------------------------------------------------------------------
GLMatrix GLCamera::GetModelMatrix
(
    float posx,
    float posy,
    float posz,
    float angleXdeg,
    float angleYdeg,
    float angleZdeg,
    float scaleX,
    float scaleY,
    float scaleZ
)
{
    //By default the camera is at origin looking down -Z
    // -Z is in front of the camera, larger values of -Z appear further away
    GLMatrix modelMatrix;
    modelMatrix.MakeIdentity();
    modelMatrix.ApplyScaling(scaleX, scaleY, scaleZ);
    modelMatrix.ApplyRotation(GLMatrix::xAxis, angleXdeg);
    modelMatrix.ApplyRotation(GLMatrix::yAxis, angleYdeg);
    modelMatrix.ApplyRotation(GLMatrix::zAxis, angleZdeg);
    modelMatrix.ApplyTranslation(posx, posy, posz);

    return modelMatrix;
}

//------------------------------------------------------------------------------
GLMatrix GLCamera::GetMVPMatrix(GLMatrix &modelMatrix)
{

    if (m_CallwlateViewProjection)
    {
        m_ViewProjectionMatrix = m_VulkanCorrectionMtx * m_ProjectionMatrix * m_ViewMatrix;
    }
    m_CallwlateViewProjection = false;

    GLMatrix MVPMatrix = m_ViewProjectionMatrix * modelMatrix;

    const bool printMatrix = false;
    if (printMatrix)
    {
        Printf(Tee::PriNormal, "Model Matrix:\n");        modelMatrix.PrintMatrix();
        Printf(Tee::PriNormal, "View Matrix:\n");         m_ViewMatrix.PrintMatrix();
        Printf(Tee::PriNormal, "Projection Matrix:\n");   m_ProjectionMatrix.PrintMatrix();
        Printf(Tee::PriNormal, "MVP Matrix:\n");          MVPMatrix.PrintMatrix();
    }
    return MVPMatrix;
}

//------------------------------------------------------------------------------
void VkGeometry::GetVertexData
(
    vector<UINT32> &indices,
    vector<float> &positions,
    vector<float> &normals,
    vector<float> &colors,
    vector<float> &txCoords
) const
{
    switch (m_GeometryType)
    {
    case GEOMETRY_TRIANGLE:
        return GetTriangleData(indices, positions, normals, colors, txCoords);
    case GEOMETRY_TORUS:
        return GetTorusData(indices, positions, normals, colors, txCoords);
    case GEOMETRY_PYRAMID:
        return GetPyramidData(indices, positions, normals, colors, txCoords);
    default:
        MASSERT(0);
        break;
    }
}

//------------------------------------------------------------------------------
void VkGeometry::GetTriangleData
(
    vector<UINT32> &indices,
    vector<float> &positions,
    vector<float> &normals,
    vector<float> &colors,
    vector<float> &txCoords
)
{
    //Push indices
    indices.push_back(0);
    indices.push_back(1);
    indices.push_back(2);

    vector<Position> pos;
    pos.push_back(Position(0.0, 1.0, 0.0, 1));
    pos.push_back(Position(-1.0, -1.0, 0.0, 1));
    pos.push_back(Position(1.0, -1.0, 0.0, 1));

    vector<Position> norm;
    norm.push_back(Position(0.0, 1.0, 0.0, 1));
    norm.push_back(Position(-1.0, -1.0, 0.0, 1));
    norm.push_back(Position(1.0, -1.0, 0.0, 1));

    vector<Color> col;
    col.push_back(Color(1.0, 0.0, 0.0, 1.0));
    col.push_back(Color(0.0, 1.0, 0.0, 1.0));
    col.push_back(Color(0.0, 0.0, 1.0, 1.0));

    vector<TexCoordinates> tex;
    tex.push_back(TexCoordinates(0.5f, 1.0));
    tex.push_back(TexCoordinates(0.0, 0.0));
    tex.push_back(TexCoordinates(1.0, 0.0));

    float *t = reinterpret_cast<float *>(pos.data());
    positions = vector<float>(t, t + (pos.size() * sizeof(Position) / sizeof(float)));
    t = reinterpret_cast<float *>(norm.data());
    normals = vector<float>(t, t + norm.size() * sizeof(Normal) / sizeof(float));
    t = reinterpret_cast<float *>(col.data());
    colors = vector<float>(t, t + col.size() * sizeof(Color) / sizeof(float));
    t = reinterpret_cast<float *>(tex.data());
    txCoords = vector<float>(t, t + tex.size() * sizeof(TexCoordinates) / sizeof(float));

    MASSERT(positions.size() / GetPositionSize() == colors.size()  / GetColorSize());
    MASSERT(positions.size() / GetPositionSize() == normals.size() / GetNormalSize());
    MASSERT(positions.size() / GetPositionSize() == txCoords.size()/ GetTxCoordSize());
}

//------------------------------------------------------------------------------
void VkGeometry::GetPyramidData
(
    vector<UINT32> &indices,
    vector<float> &positions,
    vector<float> &normals,
    vector<float> &colors,
    vector<float> &txCoords
)
{
    //Push indices
    for (int i = 0; i < 12; i++)
    {
        indices.push_back(i);
    }

    vector<Position> pos;
    pos.push_back(Position(0.0, 1.0, 0.0, 1));
    pos.push_back(Position(-1.0, -1.0, 1.0, 1));
    pos.push_back(Position(1.0, -1.0, 1.0, 1));

    pos.push_back(Position(0.0, 1.0, 0.0, 1));
    pos.push_back(Position(-1.0, -1.0, 1.0, 1));
    pos.push_back(Position(0.0, -1.0, -1.0, 1));

    pos.push_back(Position(0.0, 1.0, 0.0, 1));
    pos.push_back(Position(0.0, -1.0, -1.0, 1));
    pos.push_back(Position(1.0, -1.0, 1.0, 1));

    pos.push_back(Position(-1.0, -1.0, 1.0, 1));
    pos.push_back(Position(0.0, -1.0, -1.0, 1));
    pos.push_back(Position(1.0, -1.0, 1.0, 1));

    vector<Position> norm;
    norm.push_back(Position(0.0, 1.0, 0.0, 1));
    norm.push_back(Position(-1.0, -1.0, 0.0, 1));
    norm.push_back(Position(1.0, -1.0, 0.0, 1));

    norm.push_back(Position(0.0, 1.0, 0.0, 1));
    norm.push_back(Position(-1.0, -1.0, 1.0, 1));
    norm.push_back(Position(0.0, -1.0, -1.0, 1));

    norm.push_back(Position(0.0, 1.0, 0.0, 1));
    norm.push_back(Position(0.0, -1.0, -1.0, 1));
    norm.push_back(Position(1.0, -1.0, 1.0, 1));

    norm.push_back(Position(-1.0, -1.0, 1.0, 1));
    norm.push_back(Position(0.0, -1.0, -1.0, 1));
    norm.push_back(Position(1.0, -1.0, 1.0, 1));

    vector<Color> col;
    col.push_back(Color(1.0, 0.0, 0.0, 1.0));
    col.push_back(Color(1.0, 0.0, 0.0, 1.0));
    col.push_back(Color(1.0, 0.0, 0.0, 1.0));

    col.push_back(Color(0.0, 1.0, 0.0, 1.0));
    col.push_back(Color(0.0, 1.0, 0.0, 1.0));
    col.push_back(Color(0.0, 1.0, 0.0, 1.0));

    col.push_back(Color(0.0, 0.0, 1.0, 1.0));
    col.push_back(Color(0.0, 0.0, 1.0, 1.0));
    col.push_back(Color(0.0, 0.0, 1.0, 1.0));

    col.push_back(Color(1.0, 0.0, 1.0, 1.0));
    col.push_back(Color(1.0, 0.0, 1.0, 1.0));
    col.push_back(Color(1.0, 0.0, 1.0, 1.0));

    vector<TexCoordinates> tex;
    for (int i = 0; i < 4; i++)
    {
        tex.push_back(TexCoordinates(0.5, 1.0));
        tex.push_back(TexCoordinates(0.0, 0.0));
        tex.push_back(TexCoordinates(1.0, 0.0));
    }

    float *t = reinterpret_cast<float *>(pos.data());
    positions = vector<float>(t, t + (pos.size() * sizeof(Position) / sizeof(float)));
    t = reinterpret_cast<float *>(norm.data());
    normals = vector<float>(t, t + norm.size() * sizeof(Normal) / sizeof(float));
    t = reinterpret_cast<float *>(col.data());
    colors = vector<float>(t, t + col.size() * sizeof(Color) / sizeof(float));
    t = reinterpret_cast<float *>(tex.data());
    txCoords = vector<float>(t, t + tex.size() * sizeof(TexCoordinates) / sizeof(float));

    MASSERT(positions.size() / GetPositionSize() == colors.size()  / GetColorSize());
    MASSERT(positions.size() / GetPositionSize() == normals.size() / GetNormalSize());
    MASSERT(positions.size() / GetPositionSize() == txCoords.size()/ GetTxCoordSize());
}

//------------------------------------------------------------------------------
void VkGeometry::GetTorusData
(
    vector<UINT32> &indices,
    vector<float> &positions,
    vector<float> &normals,
    vector<float> &colors,
    vector<float> &txCoords
)
{
    const UINT32 rows = 119, cols = 119;
    const float torusDiameter = 10.0;
    const float torusHoleDiameter = 2.0;

    const UINT32 iEnd = rows;
    const UINT32 jEnd = cols;
    UINT32 vIdx = 0;

    for (UINT32 i = 0; i < iEnd; i++)
    {
        UINT32 stripIdx = 0;
        for (UINT32 j = 0; j <= jEnd; j++)
        {
            for (int k = 1; k >= 0; k--)
            {
                const UINT32 majorIdx = (i + k) % rows;
                const UINT32 minorIdx = j % cols;
                const bool bump = minorIdx & 1;

                const float twoPi = 2 * MATH_PI;
                const float tubeRadius = (torusDiameter - torusHoleDiameter) / 2.0f / 2.0f;
                const float torusRadius = (torusDiameter / 2.0f) - static_cast<float>(tubeRadius);
                const float tubeAngle = majorIdx * -1.0f * twoPi / rows;
                const float majorRadius = torusRadius + tubeRadius * LwDiagUtils::Cos(tubeAngle);
                const float torusAngle = minorIdx * twoPi / cols;

                float posx = static_cast<float>(majorRadius * LwDiagUtils::Cos(torusAngle));
                float posy = static_cast<float>(majorRadius * LwDiagUtils::Sin(torusAngle));
                float posz = static_cast<float>(tubeRadius * LwDiagUtils::Sin(tubeAngle));

                float normx = LwDiagUtils::Cos(tubeAngle) * LwDiagUtils::Cos(torusAngle);
                float normy = LwDiagUtils::Cos(tubeAngle) * LwDiagUtils::Sin(torusAngle);
                float normz = LwDiagUtils::Sin(tubeAngle);

                const float texRepeats = 64;
                float texs = torusAngle / twoPi * texRepeats;
                float text = tubeAngle / twoPi * texRepeats;

                if (bump)
                {
                    // "bump" out the vertex slightly to make grooves in the torus.
                    posx += 0.1F * normx;
                    posy += 0.1F * normy;
                    posz += 0.1F * normz;
                }

                positions.push_back(posx);
                positions.push_back(posy);
                positions.push_back(posz);
                positions.push_back(1);

                normals.push_back(normx);
                normals.push_back(normy);
                normals.push_back(normz);

                // colors same as normals
                colors.push_back(normx);
                colors.push_back(normy);
                colors.push_back(normz);
                colors.push_back(1.0);

                txCoords.push_back(texs);
                txCoords.push_back(text);

                if (stripIdx >= 2)
                {
                    // Store indices to generate a clockwise triangle for every
                    // new vertex (after the first 2 of a new strip).
                    if (stripIdx & 1)
                    {
                        indices.push_back(vIdx - 1);
                        indices.push_back(vIdx - 2);
                        indices.push_back(vIdx);
                    }
                    else
                    {
                        indices.push_back(vIdx - 2);
                        indices.push_back(vIdx - 1);
                        indices.push_back(vIdx);
                    }
                }
                vIdx++;
                stripIdx++;
            }
        }
    }

    MASSERT(positions.size() / GetPositionSize() == colors.size() / GetColorSize());
    MASSERT(positions.size() / GetPositionSize() == normals.size() / GetNormalSize());
    MASSERT(positions.size() / GetPositionSize() == txCoords.size() / GetTxCoordSize());
}

//------------------------------------------------------------------------------
void VkGeometry::GetTextureData
(
    vector<UINT08> &texData,
    UINT32 width,
    UINT32 height,
    UINT32 depth,
    UINT64 pitch
)
{
    MASSERT(width && height && depth);

    // Fill a random pattern
    bool black = false;
    for (UINT32 i = 0; i < height; i++)
    {
        black = !black;
        for (UINT32 j = 0; j < width; j++)
        {
            for (UINT32 k = 0; k < depth; k++)
            {
                if (!black)
                {
                    texData.push_back(255);
                    texData.push_back(255);
                }
                else
                {
                    texData.push_back(0);
                    texData.push_back(0);
                }
                texData.push_back(0);
                texData.push_back(0);
                black = !black;
            }
        }
        for (UINT64 j = 4 * width; j < pitch; j++)
        {
            texData.push_back(0);
        }
    }
}

