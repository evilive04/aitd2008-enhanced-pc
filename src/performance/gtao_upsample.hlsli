// Original implementation. Shared by visibility output and explicit ambient-only
// native composition. Working texture carries visibility,Z,oct-normal, not color.
float2 GtaoSignNotZero(float2 v){return float2(v.x>=0?1:-1,v.y>=0?1:-1);}
float2 GtaoOctEncode(float3 n){n/=max(1e-8,abs(n.x)+abs(n.y)+abs(n.z));float2 p=n.xy;if(n.z<0)p=(1-abs(p.yx))*GtaoSignNotZero(p);return p*.5+.5;}
float3 GtaoOctDecode(float2 p){p=p*2-1;float3 n=float3(p,1-abs(p.x)-abs(p.y));if(n.z<0)n.xy=(1-abs(n.yx))*GtaoSignNotZero(n.xy);return normalize(n);}
float GtaoEdgeWeight(float4 tap,float depth,float3 normal,float tolerance){float delta=abs(tap.g-depth);if(tap.g<=0||delta>tolerance*4)return 0;float alignment=saturate(dot(GtaoOctDecode(tap.ba),normal));alignment*=alignment;alignment*=alignment;alignment*=alignment;alignment*=alignment;return exp2(-delta/max(tolerance,1e-6))*alignment;}
float GtaoSampleVisibility(sampler2D working,float2 uv,float depth,float3 normal,float2 size,float tolerance){
 if(!(depth>0&&depth<1e7))return 1;float2 pixel=uv*size-.5,base=floor(pixel),phase=frac(pixel);float total=0,weight=0;
 [unroll]for(int y=0;y<2;++y)[unroll]for(int x=0;x<2;++x){float2 at=(base+float2(x,y)+.5)/size;if(any(at<0)||any(at>1))continue;float4 tap=tex2Dlod(working,float4(at,0,0));float spatial=(x?phase.x:1-phase.x)*(y?phase.y:1-phase.y);float w=spatial*GtaoEdgeWeight(tap,depth,normal,tolerance);total+=tap.r*w;weight+=w;}
 return weight>1e-5?saturate(total/weight):1;
}
