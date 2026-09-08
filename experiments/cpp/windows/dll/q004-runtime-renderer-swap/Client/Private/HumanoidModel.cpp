#include "HumanoidModel.h"
#include <cmath>
namespace Lab
{
void HumanoidModel::append(vector<SceneVertex>& out, const XMMATRIX& vp, float time) const
{
    auto part = [&](XMFLOAT3 scale, const XMMATRIX& world, XMFLOAT4 color) {
        const int faces[] = {0, 2, 3, 0, 3, 1, 4, 5, 7, 4, 7, 6, 0, 4, 6, 0, 6, 2,
                             1, 3, 7, 1, 7, 5, 2, 6, 7, 2, 7, 3, 0, 1, 5, 0, 5, 4};
        for (int face = 0; face < 6; ++face)
            for (int corner = 0; corner < 6; ++corner)
            {
                const int i = faces[face * 6 + corner];
                auto p = XMVectorSet((i & 1) ? scale.x : -scale.x, (i & 2) ? scale.y : -scale.y,
                                     (i & 4) ? scale.z : -scale.z, 1);
                SceneVertex v;
                XMStoreFloat4(&v.position, XMVector4Transform(p, world * vp));
                const float shade = 0.55f + face * 0.08f;
                v.color = {color.x * shade, color.y * shade, color.z * shade, 1};
                out.push_back(v);
            }
    };
    // DLL 전환 장면의 idle: 동일한 시뮬레이션 시간으로 호흡과 관절 흔들림을 재현한다.
    const float breath = sin(time * 2.0f) * 0.025f;
    const auto hips = XMMatrixTranslation(0, 1.3f, 0);
    const auto chest =
        XMMatrixRotationZ(sin(time) * 0.012f) * XMMatrixTranslation(0, 0.65f + breath, 0) * hips;
    part({0.48f, 0.26f, 0.3f}, hips, {0.3f, 0.4f, 0.6f, 1});
    part({0.6f, 0.52f, 0.32f}, chest, {0.2f, 0.8f, 0.9f, 1});
    part({0.16f, 0.14f, 0.16f}, XMMatrixTranslation(0, 0.65f, 0) * chest, {0.6f, 0.65f, 0.7f, 1});
    part({0.36f, 0.38f, 0.34f},
         XMMatrixRotationY(sin(time * 0.7f) * 0.04f) * XMMatrixTranslation(0, 1.1f, 0) * chest,
         {0.95f, 0.8f, 0.35f, 1});
    // 좌우 팔과 다리를 부모 관절 기준으로 구성해 모델 단독 재사용이 가능하다.
    for (float side : {-1.0f, 1.0f})
    {
        const auto shoulder =
            XMMatrixRotationZ(side * (0.12f + breath)) * XMMatrixTranslation(side * 0.78f, 0.35f, 0) * chest;
        part({0.19f, 0.35f, 0.23f}, XMMatrixTranslation(0, -0.32f, 0) * shoulder, {0.35f, 0.6f, 0.8f, 1});
        const auto elbow = XMMatrixRotationX(-0.1f + breath) * XMMatrixTranslation(0, -0.65f, 0) * shoulder;
        part({0.16f, 0.3f, 0.2f}, XMMatrixTranslation(0, -0.28f, 0) * elbow, {0.3f, 0.65f, 0.75f, 1});
        part({0.17f, 0.16f, 0.18f}, XMMatrixTranslation(0, -0.69f, 0) * elbow, {0.95f, 0.8f, 0.35f, 1});
        part({0.21f, 0.35f, 0.25f}, XMMatrixTranslation(side * 0.26f, -0.55f, 0) * hips,
             {0.3f, 0.45f, 0.65f, 1});
        part({0.18f, 0.3f, 0.2f}, XMMatrixTranslation(side * 0.26f, -1.16f, 0) * hips, {0.3f, 0.5f, 0.7f, 1});
        part({0.22f, 0.12f, 0.34f}, XMMatrixTranslation(side * 0.26f, -1.38f, -0.12f) * hips,
             {0.15f, 0.22f, 0.3f, 1});
    }
}
} // namespace Lab
