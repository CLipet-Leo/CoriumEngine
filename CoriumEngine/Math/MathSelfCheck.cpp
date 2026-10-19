#include "pch.h"
#include <DirectXMath.h>

using namespace DirectX;

// Seul endroit où DirectXMath reste : référence pour valider Math.h
namespace
{
    constexpr float kEps = 1e-4f;

    bool Near(float a, float b) { return std::fabs(a - b) <= kEps * (1.0f + std::fabs(b)); }

    bool Near(const Mat4& a, FXMMATRIX b)
    {
        XMFLOAT4X4 ref;
        XMStoreFloat4x4(&ref, b);
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                if (!Near(a.m[i][j], ref.m[i][j]))
                    return false;
        return true;
    }

    bool Near(const Mat4& a, const Mat4& b)
    {
        return Near(a, XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&b)));
    }

    bool Near(const Vec3& a, FXMVECTOR b)
    {
        XMFLOAT3 ref;
        XMStoreFloat3(&ref, b);
        return Near(a.x, ref.x) && Near(a.y, ref.y) && Near(a.z, ref.z);
    }

    XMMATRIX ToXM(const Mat4& m) { return XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&m)); }
    XMVECTOR ToXM(const Vec3& v) { return XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&v)); }
}

void MathSelfCheck()
{
    const Vec3 eulers[] = {
        { 0, 0, 0 }, { 30, 0, 0 }, { 0, 45, 0 }, { 0, 0, 60 },
        { 10, -70, 135 }, { 89, 20, -30 }, { -45, 170, 10 }
    };

    // Euler -> quat -> matrice, aller-retour Euler
    for (const Vec3& e : eulers)
    {
        Mat4 rot = FromQuat(FromEulerDeg(e));
        assert(Near(rot, XMMatrixRotationRollPitchYaw(e.x * kDegToRad, e.y * kDegToRad, e.z * kDegToRad)));
        assert(Near(FromQuat(FromEulerDeg(ToEulerDeg(FromEulerDeg(e)))), rot));
    }

    // Composition : quat et matrices dans le même sens
    Quat a = FromEulerDeg({ 10, -70, 135 });
    Quat b = Normalize(FromAxisAngle(Normalize(Vec3{ 1, 2, 3 }), 0.7f));
    assert(Near(FromQuat(a * b), FromQuat(a) * FromQuat(b)));
    assert(Near(FromQuat(b), XMMatrixRotationAxis(XMVectorSet(1, 2, 3, 0), 0.7f)));

    // S * R * T
    Vec3 s = { 2, 0.5f, 3 }, t = { -4, 5, 6 };
    Mat4 world = Scaling(s) * FromQuat(a) * Translation(t);
    XMMATRIX xmWorld = XMMatrixScaling(s.x, s.y, s.z) * ToXM(FromQuat(a)) * XMMatrixTranslation(t.x, t.y, t.z);
    assert(Near(world, xmWorld));
    assert(Near(Transpose(world), XMMatrixTranspose(xmWorld)));

    // Transformations de vecteurs
    Vec3 p = { 1, -2, 0.5f };
    assert(Near(TransformPoint(p, world), XMVector3Transform(ToXM(p), xmWorld)));
    assert(Near(TransformNormal(p, world), XMVector3TransformNormal(ToXM(p), xmWorld)));

    // Caméra
    Vec3 eye = { 3, 4, -10 }, dir = { -0.2f, -0.3f, 1 }, up = { 0, 1, 0 };
    assert(Near(LookToLH(eye, dir, up), XMMatrixLookToLH(ToXM(eye), ToXM(dir), ToXM(up))));
    assert(Near(PerspectiveFovLH(1.0f, 16.0f / 9.0f, 0.1f, 1000.0f),
                XMMatrixPerspectiveFovLH(1.0f, 16.0f / 9.0f, 0.1f, 1000.0f)));

    OutputDebugStringA("MathSelfCheck : OK\n");
}
