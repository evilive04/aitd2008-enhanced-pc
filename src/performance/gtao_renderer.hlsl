// GTAO analytical horizon integration, independently implemented from Jimenez,
// Wu, Pesce & Jarabo, Practical Realtime Strategies for Accurate Indirect
// Occlusion (2016), equations5-8 and conservative thickness discussion§4.1.
// https://www.iryoku.com/downloads/Practical-Realtime-Strategies-for-Accurate-Indirect-Occlusion.pdf
// Three-slice quadrature is normalized by the same unoccluded arc integral,
// eliminating finite-azimuth flat-plane bias. This is finite-sample screen-space AO, not GI or a light-specific shadow.
#include "gtao_upsample.hlsli"
sampler2D Depth : register(s0);sampler2D Normals : register(s1);sampler2D Working : register(s2);
float4 Projection : register(c0); // projX,projY,depthSign,normalUnpackScale
float4 Controls : register(c1); // world radius,max working pixel radius,thickness,normalUnpackBias
float4 Dimensions : register(c2); // working width,height,1/sourcewidth,1/sourceheight
bool GtaoValidZ(float z){return z>1e-5&&z<1e7;}
float3 GtaoNormal(float2 uv,out bool valid){float3 n=tex2Dlod(Normals,float4(uv,0,0)).xyz*Projection.w+Controls.w;n.z*=Projection.z;float l=dot(n,n);valid=l>.25&&l<2.25;return valid?n*rsqrt(l):float3(0,0,-1);}
float GtaoDepth(float2 uv){return tex2Dlod(Depth,float4(uv,0,0)).r*Projection.z;}
float3 GtaoPosition(float2 uv,float z){return float3((uv.x*2-1)*z/Projection.x,(1-uv.y*2)*z/Projection.y,z);}
float4 HorizonPS(float2 uv:TEXCOORD0):COLOR0 {
 float z=GtaoDepth(uv);bool valid;float3 n=GtaoNormal(uv,valid);if(!GtaoValidZ(z)||!valid)return float4(1,0,1,1);
 float3 p=GtaoPosition(uv,z),v=normalize(-p);float3 receiver=p+n*min(Controls.x*.01,z*.001);float visibility=0,unoccluded=0;float3 basisX=normalize(cross(v,float3(0,1,0))),basisY=cross(basisX,v);if(dot(n,v)<=0)return float4(1,z,GtaoOctEncode(n));
 [loop]for(int slice=0;slice<3;++slice){float2 direction=slice==0?float2(1,0):(slice==1?float2(.5,.866025404):float2(-.5,.866025404));
  float3 tangent=basisX*direction.x+basisY*direction.y;float3 axis=normalize(cross(tangent,v));float3 projected=n-axis*dot(n,axis);float lengthN=length(projected);if(lengthN<1e-5)continue;projected/=lengthN;
  float3 ortho=tangent;float cosN=clamp(dot(projected,v),-1,1);float angleN=acos(cosN)*(dot(projected,ortho)>=0?1:-1);float2 horizon=float2(-1,-1);
  float2 projectedRadius=(tangent.xy-p.xy*(tangent.z/z))*float2(Projection.x,-Projection.y)*(.5*Controls.x/z);float pixelRadius=length(projectedRadius*Dimensions.xy);float2 radiusUV=projectedRadius*min(1,Controls.y/max(pixelRadius,1e-5));
  [loop]for(int step=1;step<=4;++step){float f=step*.25;float2 offset=radiusUV*(f*f);float pixels=length(offset*Dimensions.xy);if(pixels<1)offset*=1/max(pixels,1e-5);
   [unroll]for(int side=0;side<2;++side){float2 at=uv+offset*(side?-1:1);if(any(at<=0)||any(at>=1))continue;float sampleZ=GtaoDepth(at);if(!GtaoValidZ(sampleZ))continue;float3 delta=GtaoPosition(at,sampleZ)-receiver;float lengthSq=dot(delta,delta);if(lengthSq<1e-10||lengthSq>Controls.x*Controls.x)continue;float distance=sqrt(lengthSq);float h=clamp(dot(delta,v)/distance,-1,1);h=lerp(-1,h,saturate((Controls.x-distance)/max(Controls.x*.25,1e-6)));
    float previous=side?horizon.y:horizon.x;float next=h>previous?h:lerp(previous,h,Controls.z);if(side)horizon.y=next;else horizon.x=next;
   }
  }
  float h1=angleN+min(acos(clamp(horizon.x,-1,1))-angleN,1.570796327);float h2=angleN+max(-acos(clamp(horizon.y,-1,1))-angleN,-1.570796327);float sinN=sin(angleN);
  float arc=.25*((cosN-cos(2*h1-angleN)+2*h1*sinN)+(cosN-cos(2*h2-angleN)+2*h2*sinN));visibility+=lengthN*arc;unoccluded+=lengthN*(cosN+angleN*sinN);
 }
 return float4(saturate(visibility/max(unoccluded,1e-5)),z,GtaoOctEncode(n));
}
float4 FilterPS(float2 uv:TEXCOORD0):COLOR0 {
 float4 center=tex2Dlod(Working,float4(uv,0,0));if(!GtaoValidZ(center.g))return center;float3 normal=GtaoOctDecode(center.ba);float tolerance=max(Controls.x*.02,center.g*.002);float total=0,weight=0;
 [unroll]for(int y=-1;y<=1;++y)[unroll]for(int x=-1;x<=1;++x){float2 at=uv+float2(x,y)/Dimensions.xy;if(any(at<0)||any(at>1))continue;float4 tap=tex2Dlod(Working,float4(at,0,0));float w=(x==0?2:1)*(y==0?2:1)*GtaoEdgeWeight(tap,center.g,normal,tolerance);total+=tap.r*w;weight+=w;}
 center.r=weight>1e-5?saturate(total/weight):center.r;return center;
}
float4 UpsamplePS(float2 uv:TEXCOORD0):COLOR0 {float z=GtaoDepth(uv);bool valid;float3 normal=GtaoNormal(uv,valid);float visibility=valid?GtaoSampleVisibility(Working,uv,z,normal,Dimensions.xy,max(Controls.x*.02,z*.002)):1;return float4(visibility,visibility,visibility,1);}

