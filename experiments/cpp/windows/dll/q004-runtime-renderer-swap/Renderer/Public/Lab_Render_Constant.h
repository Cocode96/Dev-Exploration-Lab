#pragma once
namespace Lab
{
inline constexpr unsigned scene_width = 960, scene_height = 540, vertex_capacity = 100000;
inline constexpr char scene_shader[] = R"(
struct V { float4 p:POSITION; float4 c:COLOR; };
struct P { float4 p:SV_Position; float4 c:COLOR; };
P VS(V v) { P o; o.p=v.p; o.c=v.c; return o; }
float4 PS(P p):SV_Target { return p.c; }
)";
} // namespace Lab
