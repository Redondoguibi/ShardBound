cbuffer Frame : register(b0) {
    float4x4 viewProjection;
    float4x4 lightViewProjection;
    float4 sunDirection;
    float4 sunColor;
    float4 ambientColor;
    float4 fogColor;
    float4 eyeFogStart;
    float4 effects;
};
Texture2D baseTexture : register(t0);
Texture2D<float> shadowMap : register(t1);
SamplerState baseSampler : register(s0);
SamplerComparisonState shadowSampler : register(s1);
struct VertexInput {
    float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD0;
    float4 w0 : WORLD0; float4 w1 : WORLD1; float4 w2 : WORLD2; float4 w3 : WORLD3;
    float4 n0 : INORMAL0; float4 n1 : INORMAL1; float4 n2 : INORMAL2; float4 n3 : INORMAL3;
    float4 color : COLOR0; float4 material : PARAMS0;
};
struct VertexOutput {
    float4 position : SV_POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD0;
    float3 world : TEXCOORD1; float4 shadow : TEXCOORD2;
    float4 color : COLOR0; nointerpolation float4 material : TEXCOORD3;
};
VertexOutput vertexMain(VertexInput v) {
    VertexOutput o;
    float4 world=mul(float4(v.position,1),float4x4(v.w0,v.w1,v.w2,v.w3));
    o.position=mul(world,viewProjection); o.world=world.xyz;
    o.normal=mul(float4(v.normal,0),float4x4(v.n0,v.n1,v.n2,v.n3)).xyz;
    o.shadow=mul(world,lightViewProjection); o.uv=v.uv; o.color=v.color; o.material=v.material; return o;
}
void shadowPixel(VertexOutput v) {
    if(v.material.y!=0) clip(baseTexture.Sample(baseSampler,v.uv).a*v.color.a-v.material.x);
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
    if(v.material.y!=0) clip(base.a-v.material.x);
    float3 normal=normalize(v.normal); if(v.material.w!=0 && !frontFace) normal=-normal;
    if(v.material.z!=0) return float4(base.rgb,1);
    if(effects.z==0) return float4(base.rgb*(.3+.7*saturate(dot(normal,normalize(float3(-.4,1,-.6))))),1);
    float sun=saturate(dot(normal,sunDirection.xyz))*shadowVisibility(v,normal);
    float hemisphere=.55+.45*saturate(normal.y*.5+.5);
    float3 light=ambientColor.rgb*hemisphere+sunColor.rgb*sun;
    float3 rgb=toSrgb(saturate(base.rgb*light));
    float distanceToEye=length(v.world-eyeFogStart.xyz);
    float fog=smoothstep(eyeFogStart.w,effects.x,distanceToEye);
    return float4(lerp(rgb,fogColor.rgb,fog),1);
}
struct ScreenOutput { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
ScreenOutput screenVertex(uint id : SV_VertexID) {
    ScreenOutput o; o.uv=float2((id<<1)&2,id&2); o.position=float4(o.uv*float2(2,-2)+float2(-1,1),0,1); return o;
}
float4 screenPixel(ScreenOutput v) : SV_TARGET { return baseTexture.Sample(baseSampler,v.uv); }
