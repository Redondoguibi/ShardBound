// Parameters 0.xyz: tint; 0.w: emission strength; 1.x: pulse speed.
void modSurface(VertexOutput v, inout float4 base, inout float3 normal, inout float3 emission) {
    float pulse=0.5+0.5*sin(modFrame.x*modParameters[1].x+v.world.y*2);
    base.rgb*=modParameters[0].rgb;
    emission=modParameters[0].rgb*modParameters[0].w*pulse;
}
