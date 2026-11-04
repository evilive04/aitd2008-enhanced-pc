// Native NV12, preserved 4K dimensions. Constants follow decoded color metadata.
sampler2D luma : register(s0);
sampler2D chroma : register(s1);
float4 rangeScale : register(c0); // Yscale, Yoffset, Cscale, Coffset
float4 coefficients : register(c1); // Rv, Gu, Gv, Bu
float4 chromaOffset : register(c2);
float4 main(float2 uv : TEXCOORD0) : COLOR0 {
    float y=(tex2D(luma,uv).r-rangeScale.y)*rangeScale.x;
    float2 c=(tex2D(chroma,uv+chromaOffset.xy).ra-rangeScale.w)*rangeScale.z;
    return float4(saturate(float3(y+coefficients.x*c.y,
                                y+coefficients.y*c.x+coefficients.z*c.y,
                                y+coefficients.w*c.x)),1);
}
