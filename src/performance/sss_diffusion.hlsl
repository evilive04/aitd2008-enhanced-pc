// Original bounded diffusion implementation. Separable skin diffusion concept:
// Jimenez et al., Separable Subsurface Scattering (2015). No copied kernels/code.
sampler2D source0:register(s0);
sampler2D source1:register(s1);
sampler2D source2:register(s2);
sampler2D source3:register(s3);
sampler2D source4:register(s4);
sampler2D source5:register(s5); // point-filtered duplicate of diffuse source
sampler2D source6:register(s6); // identically resampled, unblurred reference
float4 stepAxis:register(c0); // working texel size, direction
float4 radiusDepth:register(c1); // world radius * projection /2, depth gates
float4 controls:register(c2); // max working-pixel radius, overall amount
float4 dimensions:register(c3); // full texel size, working dimensions
float4 sourceDimensions:register(c4); // source texel size, source dimensions
float4 depthOptions:register(c5); // signed-Z multiplier, mask-depth validation enabled
float4 Sample0(sampler2D source,float2 uv){return tex2Dlod(source,float4(uv,0,0));}
bool ValidDepth(float d){return d>0.000001 && d<100000000.0;}
float LinearDepth(sampler2D source,float2 uv){return Sample0(source,uv).r*depthOptions.x;}
bool SurfaceVisible(float d,float3 material){
    if(!ValidDepth(d)||material.x<=0.0||material.y<=0.0)return false;
    if(depthOptions.y>0.5){
        float faceDepth=material.z*depthOptions.x;
        return ValidDepth(faceDepth)&&abs(faceDepth-d)<=max(radiusDepth.z,d*radiusDepth.w);
    }
    return true;
}
bool Accept(float2 uv,float d,float3 material,float otherDepth,float3 otherMaterial){
    return all(uv>=0.0)&&all(uv<=1.0)&&ValidDepth(otherDepth)&&
        SurfaceVisible(otherDepth,otherMaterial)&&abs(otherMaterial.y-material.y)<0.001&&
        abs(d-otherDepth)<=max(radiusDepth.z,d*radiusDepth.w);
}
float4 SmoothSource(float2 uv,float d,float3 material){
    // Hardware bilinear avoids tap snapping. Its complete footprint must stay
    // on this skin surface; otherwise use the already gated nearest texel.
    float2 cell=floor(uv*sourceDimensions.zw-0.5);
    bool safe=true;
    [loop]for(int y=0;y<2;++y){[loop]for(int x=0;x<2;++x){
        float2 at=(cell+float2(x,y)+0.5)*sourceDimensions.xy;
        safe=safe&&Accept(at,d,material,LinearDepth(source1,at),Sample0(source2,at).rgb);
    }}
    return safe?Sample0(source0,uv):Sample0(source5,uv);
}
void FilterPS(float2 uv:TEXCOORD0,out float4 filtered:COLOR0,out float4 reference:COLOR1) {
    float4 center=Sample0(source5,uv);
    filtered=reference=center;
    float d=LinearDepth(source1,uv);float3 material=Sample0(source2,uv).rgb;
    if(!SurfaceVisible(d,material))return;
    center=SmoothSource(uv,d,material);
    reference=center;
    float2 radius=min(radiusDepth.xy/d,stepAxis.xy*controls.x)*stepAxis.zw;
    float3 sum=center.rgb,weight=1.0;
    [loop]for(int i=1;i<=3;++i){
        float t=i/3.0;float3 w=exp(-float3(2.0,5.0,10.0)*t*t);
        [unroll]for(int side=-1;side<=1;side+=2){
            float2 at=uv+radius*(t*side);
            float z=LinearDepth(source1,at);float3 m=Sample0(source2,at).rgb;
            if(Accept(at,d,material,z,m)){sum+=w*SmoothSource(at,d,material).rgb;weight+=w;}
        }
    }
    filtered=float4(sum/weight,center.a);
}
float4 ComposePS(float2 uv:TEXCOORD0):COLOR0 {
    float4 scene=Sample0(source0,uv);float d=LinearDepth(source3,uv);float3 material=Sample0(source4,uv).rgb;
    if(!SurfaceVisible(d,material))return scene;
    float2 p=uv*dimensions.zw-0.5,cell=floor(p),f=frac(p);
    float3 sum=0;float weights=0;
    [unroll]for(int y=0;y<2;++y){[unroll]for(int x=0;x<2;++x){
        float2 at=(cell+float2(x,y)+0.5)*stepAxis.xy;
        float z=LinearDepth(source3,at);float3 m=Sample0(source4,at).rgb;
        if(Accept(at,d,material,z,m)){
            float w=(x?f.x:1-f.x)*(y?f.y:1-f.y);
            // Subtract on the identical working-grid sample before upsampling.
            // This prevents downsampling alone from softening full-res diffuse
            // when the physical scattering radius approaches zero.
            sum+=w*(Sample0(source2,at).rgb-Sample0(source6,at).rgb);weights+=w;
        }
    }}
    if(weights<=0.000001)return scene;
    return float4(max(0.0,scene.rgb+controls.y*saturate(material.x)*sum/weights),scene.a);
}
// Equal-area golden-angle disk quadrature of an original bounded RGB Gaussian
// profile. Full-resolution diffuse and scene share the exact center sample;
// zero radius therefore tends continuously to an exactly zero scattering delta.
float4 DirectPS(float2 uv:TEXCOORD0):COLOR0 {
    float4 scene=Sample0(source3,uv);
    float d=LinearDepth(source1,uv);float3 material=Sample0(source2,uv).rgb;
    [branch]if(!SurfaceVisible(d,material))return scene;
    float4 center=Sample0(source5,uv);
    float2 radius=min(radiusDepth.xy/d,stepAxis.xy*controls.x);
    float3 sum=center.rgb,weight=1.0;
    [loop]for(int i=0;i<8;++i){
        float r2=(i+0.5)/8.0;
        float angle=i*2.39996323;
        float2 offset=float2(cos(angle),sin(angle))*sqrt(r2);
        [unroll]for(int side=-1;side<=1;side+=2){
        float2 at=uv+radius*offset*side;
        float z=LinearDepth(source1,at);float3 m=Sample0(source2,at).rgb;
        [branch]if(Accept(at,d,material,z,m)){
            float3 w=exp(-float3(2.0,5.0,10.0)*r2);
            sum+=w*SmoothSource(at,d,material).rgb;weight+=w;
        }
        }
    }
    return float4(max(0.0,scene.rgb+controls.y*saturate(material.x)*(sum/weight-center.rgb)),scene.a);
}
