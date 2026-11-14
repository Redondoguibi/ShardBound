cbuffer Frame : register(b0) {
    float4x4 viewProjection;
    float4x4 lightViewProjection;
    float4 sunDirection;
    float4 sunColor;
    float4 ambientColor;
    float4 fogColor;
    float4 eyeFogStart;
    float4 effects;
    float4 pointPositions[32];
    float4 pointColors[32];
};
Texture2D baseTexture : register(t0);
Texture2D<float> shadowMap : register(t1);
SamplerState baseSampler : register(s0);
SamplerComparisonState shadowSampler : register(s1);
struct VertexInput {
    float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD0;
    float4 w0 : WORLD0; float4 w1 : WORLD1; float4 w2 : WORLD2; float4 w3 : WORLD3;
    float4 n0 : INORMAL0; float4 n1 : INORMAL1; float4 n2 : INORMAL2; float4 n3 : INORMAL3;
    float4 color : COLOR0; float4 vertexColor : COLOR1; float4 material : PARAMS0;
};
struct VertexOutput {
    float4 position : SV_POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD0;
    float3 world : TEXCOORD1; float4 shadow : TEXCOORD2;
    float4 color : COLOR0; nointerpolation float4 material : TEXCOORD3;
};
// Shader ABI v1. x=time in seconds; yz=internal resolution; w=reserved.
#if defined(SHARDBOUND_SURFACE) || defined(SHARDBOUND_POST)
cbuffer ModParameters : register(b1) { float4 modParameters[4]; float4 modFrame; };
Texture2D modTexture : register(t2);
SamplerState modSampler : register(s2);
#endif
#ifdef SHARDBOUND_SURFACE
#include "mod.hlsl"
#endif
VertexOutput vertexMain(VertexInput v) {
    VertexOutput o;
    float4 world=mul(float4(v.position,1),float4x4(v.w0,v.w1,v.w2,v.w3));
    o.position=mul(world,viewProjection); o.world=world.xyz;
    o.normal=mul(float4(v.normal,0),float4x4(v.n0,v.n1,v.n2,v.n3)).xyz;
    o.shadow=mul(world,lightViewProjection); o.uv=v.uv; o.color=v.color*v.vertexColor; o.material=v.material; return o;
}
void shadowPixel(VertexOutput v) {
    float4 base=baseTexture.Sample(baseSampler,v.uv)*v.color;
#ifdef SHARDBOUND_SURFACE
    float3 normal=normalize(v.normal),emission=0;
    if(v.material.z==2) base.rgb=v.color.rgb;
    modSurface(v,base,normal,emission);
#endif
    if(v.material.y==1) clip(base.a-v.material.x);
}
float shadowVisibility(VertexOutput v,float3 normal) {
    if(effects.y==0) return 1;
    float3 p=v.shadow.xyz/v.shadow.w; float2 uv=p.xy*float2(.5,-.5)+.5;
    if(any(uv<0) || any(uv>1) || p.z<0 || p.z>1) return 1;
    float bias=max(.00015,.0006*(1-dot(normal,sunDirection.xyz)));
    float visibility=0;
    [unroll] for(int y=-1;y<=1;y++) [unroll] for(int x=-1;x<=1;x++)
        visibility+=shadowMap.SampleCmpLevelZero(shadowSampler,uv+float2(x,y)/2048,p.z-bias);
    return visibility/9;
}
float3 toSrgb(float3 value) { return lerp(12.92*value,1.055*pow(max(value,0),1.0/2.4)-.055,step(.0031308,value)); }
float4 pixelMain(VertexOutput v,bool frontFace : SV_IsFrontFace) : SV_TARGET {
    float4 base=baseTexture.Sample(baseSampler,v.uv)*v.color;
    float3 normal=normalize(v.normal); if(v.material.w!=0 && !frontFace) normal=-normal;
    float3 emission=0;
#ifdef SHARDBOUND_SURFACE
    if(v.material.z==2) base.rgb=v.color.rgb;
    modSurface(v,base,normal,emission);
    normal=normalize(normal);
#endif
    if(v.material.y==1) clip(base.a-v.material.x);
    if(v.material.z==2) {
#ifdef SHARDBOUND_SURFACE
        if(v.material.x>=0) clip(base.a-v.material.x);
        return float4(base.rgb+emission,base.a);
#else
        float a=baseTexture.Sample(baseSampler,v.uv).a; if(v.material.x>=0) clip(a-v.material.x); return float4(v.color.rgb,v.color.a);
#endif
    }
    if(v.material.z!=0) return float4(base.rgb+emission,base.a);
    float3 points=0;
    for(int i=0;i<(int)effects.w;i++) {
        float3 delta=pointPositions[i].xyz-v.world; float d=length(delta); float cutoff=pointPositions[i].w;
        float attenuation=cutoff>0 ? pow(saturate(1-pow(d/cutoff,4)),2):1;
        points+=pointColors[i].rgb*pointColors[i].w*attenuation/max(d*d,.01)*saturate(dot(normal,delta/max(d,.0001)));
    }
    if(effects.z==0) return float4(base.rgb*(.3+.7*saturate(dot(normal,normalize(float3(-.4,1,-.6))))+points)+emission,v.material.y==2 ? base.a : 1);
    float sun=saturate(dot(normal,sunDirection.xyz))*shadowVisibility(v,normal);
    float hemisphere=.55+.45*saturate(normal.y*.5+.5);
    float3 light=ambientColor.rgb*hemisphere+sunColor.rgb*sun+points;
    float3 rgb=toSrgb(saturate(base.rgb*light+emission));
    float distanceToEye=length(v.world-eyeFogStart.xyz);
    float fog=smoothstep(eyeFogStart.w,effects.x,distanceToEye);
    return float4(lerp(rgb,fogColor.rgb,fog),v.material.y==2 ? base.a : 1);
}
struct ScreenOutput { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
ScreenOutput screenVertex(uint id : SV_VertexID) {
    ScreenOutput o; o.uv=float2((id<<1)&2,id&2); o.position=float4(o.uv*float2(2,-2)+float2(-1,1),0,1); return o;
}
float4 screenPixel(ScreenOutput v) : SV_TARGET { return baseTexture.Sample(baseSampler,v.uv); }

#ifdef SHARDBOUND_POST
#include "mod.hlsl"
float4 modPostMain(ScreenOutput v) : SV_TARGET { return modPost(v.uv,baseTexture.Sample(baseSampler,v.uv)); }
#endif
