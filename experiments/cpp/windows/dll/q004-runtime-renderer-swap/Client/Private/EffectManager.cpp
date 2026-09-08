#include "EffectManager.h"
#include "HumanoidModel.h"
#include <algorithm>
#include <cmath>
namespace Lab
{
bool EffectManager::load(const filesystem::path& path, string& message)
{
    return EffectPresetLoader::load(path, m_presets, message);
}
void EffectManager::reset(const BenchmarkOptions& o)
{
    m_pool.reset(static_cast<unsigned>(clamp(o.capacity, 1, 16384)));
    m_random.seed(static_cast<unsigned>(o.seed));
    m_time = 0;
    m_next_preset = 0;
    m_vertices.clear();
    m_vertices.reserve(16384 * 6 + 512);
}
void EffectManager::update(float dt, const BenchmarkOptions& o)
{
    m_time += dt;
    m_pool.update(dt, m_presets);
    const auto target = static_cast<unsigned>(clamp(o.instances, 1, 16384));
    unsigned active = m_pool.active_count();
    unsigned missing = active < target ? target - active : 0;
    if (o.test == TestMode::PoolChurn)
        missing = max(missing, target / 8);
    for (unsigned i = 0; i < missing; ++i)
    {
        // SubEffect의 InstanceCount 비율로 생성 순서를 배분한다.
        unsigned total = 0;
        for (const auto& p : m_presets)
            total += p.count;
        unsigned selection = m_next_preset++ % total, index = 0;
        while (selection >= m_presets[index].count)
        {
            selection -= m_presets[index].count;
            ++index;
        }
        if (!m_pool.emit(m_presets[index], index, m_random))
            break;
    }
}
SceneFrame EffectManager::scene(CameraMode mode, float speed)
{
    m_vertices.clear();
    // 벤치마크 카메라: 프레임 기준 시간으로 경로를 재현한다.
    const float angle = mode == CameraMode::Fixed ? 0.35f : m_time * 0.35f * speed;
    XMFLOAT3 eye{sin(angle) * 11, 6, cos(angle) * -11};
    if (mode == CameraMode::Path)
    {
        eye.x = sin(angle) * 9;
        eye.y = 4 + sin(angle * 0.7f) * 2;
        eye.z = -10 + cos(angle) * 2;
    }
    const auto view =
        XMMatrixLookAtLH(XMLoadFloat3(&eye), XMVectorSet(0, 1.5f, 0, 1), XMVectorSet(0, 1, 0, 0));
    const auto matrix = view * XMMatrixPerspectiveFovLH(XM_PIDIV4, 16.0f / 9.0f, 0.1f, 100);
    auto vertex = [&](XMFLOAT3 p, XMFLOAT4 c) {
        SceneVertex v;
        XMStoreFloat4(&v.position, XMVector4Transform(XMVectorSet(p.x, p.y, p.z, 1), matrix));
        v.color = c;
        m_vertices.push_back(v);
    };
    auto box = [&](XMFLOAT3 center, XMFLOAT3 size, XMFLOAT4 color) {
        XMFLOAT3 v[8];
        for (int i = 0; i < 8; ++i)
            v[i] = {center.x + ((i & 1) ? size.x : -size.x), center.y + ((i & 2) ? size.y : -size.y),
                    center.z + ((i & 4) ? size.z : -size.z)};
        const int indices[] = {0, 2, 3, 0, 3, 1, 4, 5, 7, 4, 7, 6, 0, 4, 6, 0, 6, 2,
                               1, 3, 7, 1, 7, 5, 2, 6, 7, 2, 7, 3, 0, 1, 5, 0, 5, 4};
        for (int f = 0; f < 6; ++f)
        {
            const float shade = 0.55f + 0.08f * f;
            for (int j = 0; j < 6; ++j)
                vertex(v[indices[f * 6 + j]], {color.x * shade, color.y * shade, color.z * shade, 1});
        }
    };
    // Scene View의 테스트 모델: 바닥과 로봇을 같은 삼각형 파이프라인으로 그린다.
    box({0, -0.18f, 0}, {6, 0.1f, 6}, {0.2f, 0.3f, 0.35f, 1});
    HumanoidModel{}.append(m_vertices, matrix, m_time);
    const auto opaque = static_cast<unsigned>(m_vertices.size());
    const auto inverse = XMMatrixInverse(nullptr, view);
    const auto right = inverse.r[0], up = inverse.r[1];
    vector<const EffectParticle*> particles;
    for (const auto& p : m_pool.particles())
        if (p.active && p.age >= 0)
            particles.push_back(&p);
    // 일반 알파 입자: 카메라에서 먼 입자부터 그리고 깊이 쓰기는 끈다.
    sort(particles.begin(), particles.end(), [&](auto a, auto b) {
        return XMVectorGetZ(XMVector3TransformCoord(XMLoadFloat3(&a->position), view)) >
               XMVectorGetZ(XMVector3TransformCoord(XMLoadFloat3(&b->position), view));
    });
    for (const auto* p : particles)
    {
        XMFLOAT3 corners[4];
        const auto center = XMLoadFloat3(&p->position);
        for (int i = 0; i < 4; ++i)
            XMStoreFloat3(&corners[i], center + right * ((i & 1) ? p->size.x : -p->size.x) +
                                           up * ((i & 2) ? p->size.y : -p->size.y));
        for (int i : {0, 1, 2, 2, 1, 3})
            vertex(corners[i], p->color);
    }
    return {m_vertices.data(), static_cast<unsigned>(m_vertices.size()), opaque};
}
} // namespace Lab
