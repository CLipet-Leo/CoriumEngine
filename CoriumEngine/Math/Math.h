#pragma once

// Conventions (cf. doc/Conventions.md) : main gauche, vecteurs ligne (v * M),
// composition S * R * T, Mat4 row-major avec la translation en m[3][0..2].
// Angles en radians dans l'API, sauf les fonctions suffixées "Deg".

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
struct Vec4 { float x, y, z, w; };
struct Quat { float x, y, z, w; };     // identité = {0,0,0,1}
struct Mat4 { float m[4][4]; };

// Layouts partagés avec les constant buffers HLSL
static_assert(sizeof(Vec2) == 8);
static_assert(sizeof(Vec3) == 12);
static_assert(sizeof(Vec4) == 16);
static_assert(sizeof(Quat) == 16);
static_assert(sizeof(Mat4) == 64);

// -------------------------------------------------------
// Vec3
// -------------------------------------------------------

inline Vec3 operator+(const Vec3& a, const Vec3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline Vec3 operator-(const Vec3& a, const Vec3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline Vec3 operator*(const Vec3& a, float s)       { return { a.x * s, a.y * s, a.z * s }; }

inline float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3 Cross(const Vec3& a, const Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline float Length(const Vec3& v) { return std::sqrt(Dot(v, v)); }

inline Vec3 Normalize(const Vec3& v) { return v * (1.0f / Length(v)); }

// -------------------------------------------------------
// Quat
// -------------------------------------------------------

// a * b = rotation a puis rotation b (même sens que les matrices, = XMQuaternionMultiply(a, b))
inline Quat operator*(const Quat& a, const Quat& b)
{
    return {
        b.w * a.x + b.x * a.w + b.y * a.z - b.z * a.y,
        b.w * a.y - b.x * a.z + b.y * a.w + b.z * a.x,
        b.w * a.z + b.x * a.y - b.y * a.x + b.z * a.w,
        b.w * a.w - b.x * a.x - b.y * a.y - b.z * a.z
    };
}

inline Quat Normalize(const Quat& q)
{
    float inv = 1.0f / std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return { q.x * inv, q.y * inv, q.z * inv, q.w * inv };
}

// axis doit être normalisé
inline Quat FromAxisAngle(const Vec3& axis, float radians)
{
    float s = std::sin(radians * 0.5f);
    return { axis.x * s, axis.y * s, axis.z * s, std::cos(radians * 0.5f) };
}

// euler = (pitch X, yaw Y, roll Z) en degrés, appliqués roll -> pitch -> yaw
// (= XMQuaternionRotationRollPitchYaw)
inline Quat FromEulerDeg(const Vec3& euler)
{
    float sp = std::sin(euler.x * kDegToRad * 0.5f), cp = std::cos(euler.x * kDegToRad * 0.5f);
    float sy = std::sin(euler.y * kDegToRad * 0.5f), cy = std::cos(euler.y * kDegToRad * 0.5f);
    float sr = std::sin(euler.z * kDegToRad * 0.5f), cr = std::cos(euler.z * kDegToRad * 0.5f);

    return {
        cr * sp * cy + sr * cp * sy,
        cr * cp * sy - sr * sp * cy,
        sr * cp * cy - cr * sp * sy,
        cr * cp * cy + sr * sp * sy
    };
}

// Inverse de FromEulerDeg (pitch dans [-90, 90])
inline Vec3 ToEulerDeg(const Quat& q)
{
    float pitch = std::asin(std::clamp(2.0f * (q.w * q.x - q.y * q.z), -1.0f, 1.0f));
    float yaw   = std::atan2(2.0f * (q.x * q.z + q.w * q.y), 1.0f - 2.0f * (q.x * q.x + q.y * q.y));
    float roll  = std::atan2(2.0f * (q.x * q.y + q.w * q.z), 1.0f - 2.0f * (q.x * q.x + q.z * q.z));
    return Vec3{ pitch, yaw, roll } * kRadToDeg;
}

// -------------------------------------------------------
// Mat4
// -------------------------------------------------------

inline Mat4 Identity()
{
    return { { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } } };
}

inline Mat4 operator*(const Mat4& a, const Mat4& b)
{
    Mat4 r = {};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k)
                r.m[i][j] += a.m[i][k] * b.m[k][j];
    return r;
}

inline Mat4 Transpose(const Mat4& a)
{
    Mat4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            r.m[i][j] = a.m[j][i];
    return r;
}

inline Mat4 Scaling(const Vec3& s)
{
    Mat4 r = Identity();
    r.m[0][0] = s.x;
    r.m[1][1] = s.y;
    r.m[2][2] = s.z;
    return r;
}

inline Mat4 Translation(const Vec3& t)
{
    Mat4 r = Identity();
    r.m[3][0] = t.x;
    r.m[3][1] = t.y;
    r.m[3][2] = t.z;
    return r;
}

// q doit être normalisé
inline Mat4 FromQuat(const Quat& q)
{
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

    return { {
        { 1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz),        2.0f * (xz - wy),        0.0f },
        { 2.0f * (xy - wz),        1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx),        0.0f },
        { 2.0f * (xz + wy),        2.0f * (yz - wx),        1.0f - 2.0f * (xx + yy), 0.0f },
        { 0.0f,                    0.0f,                    0.0f,                    1.0f }
    } };
}

inline Mat4 LookToLH(const Vec3& eye, const Vec3& dir, const Vec3& up)
{
    Vec3 z = Normalize(dir);
    Vec3 x = Normalize(Cross(up, z));
    Vec3 y = Cross(z, x);

    return { {
        { x.x,           y.x,           z.x,           0.0f },
        { x.y,           y.y,           z.y,           0.0f },
        { x.z,           y.z,           z.z,           0.0f },
        { -Dot(x, eye),  -Dot(y, eye),  -Dot(z, eye),  1.0f }
    } };
}

// Profondeur [0, 1], fovY vertical en radians
inline Mat4 PerspectiveFovLH(float fovY, float aspect, float zn, float zf)
{
    float yScale = 1.0f / std::tan(fovY * 0.5f);
    float xScale = yScale / aspect;
    float range = zf / (zf - zn);

    Mat4 r = {};
    r.m[0][0] = xScale;
    r.m[1][1] = yScale;
    r.m[2][2] = range;
    r.m[2][3] = 1.0f;
    r.m[3][2] = -zn * range;
    return r;
}

// Ligne i sans w : 0..2 = axes X/Y/Z (avec l'échelle), 3 = translation
inline Vec3 GetRow(const Mat4& m, int i) { return { m.m[i][0], m.m[i][1], m.m[i][2] }; }

// p * M avec w = 1
inline Vec3 TransformPoint(const Vec3& p, const Mat4& m)
{
    return {
        p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + m.m[3][0],
        p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + m.m[3][1],
        p.x * m.m[0][2] + p.y * m.m[1][2] + p.z * m.m[2][2] + m.m[3][2]
    };
}

// n * M avec w = 0 (ignore la translation)
inline Vec3 TransformNormal(const Vec3& n, const Mat4& m)
{
    return {
        n.x * m.m[0][0] + n.y * m.m[1][0] + n.z * m.m[2][0],
        n.x * m.m[0][1] + n.y * m.m[1][1] + n.z * m.m[2][1],
        n.x * m.m[0][2] + n.y * m.m[1][2] + n.z * m.m[2][2]
    };
}

// Compare la lib à DirectXMath (assert), appelé à l'init du renderer en Debug
void MathSelfCheck();
