cbuffer Object : register(b0) {
    float4x4 mvp;
    float4x4 normalMatrix;
    float4 color;
    // cutoff, mask enabled, unlit, double sided
    float4 material;
};
Texture2D baseTexture : register(t0);
SamplerState baseSampler : register(s0);
struct VertexInput { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD0; };
struct VertexOutput { float4 position : SV_POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD0; };
VertexOutput vertexMain(VertexInput v) {
    VertexOutput o;
    o.position = mul(float4(v.position,1),mvp);
    o.normal = mul(float4(v.normal,0),normalMatrix).xyz;
    o.uv = v.uv;
    return o;
}
float4 pixelMain(VertexOutput v, bool frontFace : SV_IsFrontFace) : SV_TARGET {
    float4 base = baseTexture.Sample(baseSampler,v.uv) * color;
    if (material.y != 0) clip(base.a - material.x);
    float3 normal = normalize(v.normal);
    if (material.w != 0 && !frontFace) normal = -normal;
    float light = material.z != 0 ? 1 : 0.3 + 0.7 * saturate(dot(normal,normalize(float3(-0.4,1,-0.6))));
    return float4(base.rgb * light,1);
}
struct ScreenOutput { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
ScreenOutput screenVertex(uint id : SV_VertexID) {
    ScreenOutput o;
    o.uv = float2((id << 1) & 2,id & 2);
    o.position = float4(o.uv * float2(2,-2) + float2(-1,1),0,1);
    return o;
}
float4 screenPixel(ScreenOutput v) : SV_TARGET { return baseTexture.Sample(baseSampler,v.uv); }
