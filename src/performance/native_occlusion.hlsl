// Own screen-space contact visibility; explicit native geometry/light inputs.
#include "gtao_upsample.hlsli"
sampler2D DepthMap:register(s0);
sampler2D NormalMap:register(s1);
sampler2D Auxiliary:register(s2); // full GTAO for trace, encoded working for upsample
float4 Shape:register(c0); // inverse source WH, projection XY
float4 Light:register(c1); // canonical +Z light position, native depth sign
float4 Contact:register(c2); // world distance, thickness, normal bias, amount
float4 Control:register(c3); // AO amount, AO present, working WH
bool Geometry(float2 uv,out float z,out float3 n){z=tex2Dlod(DepthMap,float4(uv,0,0)).r*Light.w;n=tex2Dlod(NormalMap,float4(uv,0,0)).xyz*2-1;n.z*=Light.w;float len=dot(n,n);if(!(z>0&&z<1e6&&len>.1&&len<4)){n=float3(0,0,-1);return false;}n*=rsqrt(len);return true;}
float3 Position(float2 uv,float z){return float3((uv.x*2-1)*z/Shape.z,(1-uv.y*2)*z/Shape.w,z);}
float2 Project(float3 p){return float2(.5+.5*p.x*Shape.z/p.z,.5-.5*p.y*Shape.w/p.z);}
float4 ContactPS(float2 uv:TEXCOORD0):COLOR0 {
 float z;float3 normal;if(!Geometry(uv,z,normal))return float4(1,0,.5,.5);
 float ao=Control.y>.5?lerp(1,saturate(tex2Dlod(Auxiliary,float4(uv,0,0)).r),Control.x):1;
 float3 p=Position(uv,z),toLight=Light.xyz-p;float distance=length(toLight);
 if(!(distance>Contact.z*2))return float4(ao,z,GtaoOctEncode(normal));
 float3 direction=toLight/distance;float maximum=min(Contact.x,max(0,distance-Contact.z*2));float hit=0;
 if(dot(normal,direction)>0&&maximum>Contact.z){
  float3 origin=p+normal*Contact.z;float thickness=max(Contact.y,z*.0015);
  [unroll]for(int i=0;i<8;++i){float t=maximum*(i+.5)/8;float3 ray=origin+direction*t;if(ray.z<=Contact.z)break;
   float2 at=Project(ray);float2 margin=min(at,1-at);if(any(margin<Shape.xy*1.5))break;
   float scene=tex2Dlod(DepthMap,float4(at,0,0)).r*Light.w;float delta=ray.z-scene;
   float edge=saturate(min(margin.x/Shape.x,margin.y/Shape.y)/8);
   if(scene>0&&delta>Contact.z*.5&&delta<thickness){float confidence=edge*saturate(1-t/maximum);hit=max(hit,confidence);}
  }
 }
 return float4(ao*(1-Contact.w*hit),z,GtaoOctEncode(normal));
}
float4 UpsampleContactPS(float2 uv:TEXCOORD0):COLOR0 {
 float z;float3 n;if(!Geometry(uv,z,n))return 1;
 float visibility=GtaoSampleVisibility(Auxiliary,uv,z,n,Control.zw,max(Contact.y*.5,z*.002));return float4(visibility,visibility,visibility,1);
}
