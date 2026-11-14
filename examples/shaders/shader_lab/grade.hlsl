// Parameters 0.xyz: channel gains; 0.w: vignette strength.
float4 modPost(float2 uv, float4 color) {
    float2 offset=(uv-0.5)*2;
    float vignette=saturate(1-dot(offset,offset)*modParameters[0].w);
    return float4(color.rgb*modParameters[0].rgb*vignette,color.a);
}
