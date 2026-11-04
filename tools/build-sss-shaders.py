"""Compile our original HLSL with an installed SDK fxc; no runtime compiler dependency."""
import argparse,hashlib,struct,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--fxc',required=True);p.add_argument('--work',required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];src=root/'src/performance/sss_diffusion.hlsl';work=Path(a.work);work.mkdir(parents=True,exist_ok=True)
lines=['// Generated from sss_diffusion.hlsl using installed SDK fxc ps_3_0 /O3.', '// Source SHA256 '+hashlib.sha256(src.read_bytes()).hexdigest(),'#pragma once']
for entry,name in [('FilterPS','sssFilterShader'),('ComposePS','sssComposeShader'),('DirectPS','sssDirectShader')]:
    out=work/(entry+'.cso');subprocess.run([a.fxc,'/nologo','/WX','/T','ps_3_0','/E',entry,'/O3','/Fo',str(out),str(src)],check=True)
    b=out.read_bytes();assert len(b)%4==0;values=struct.unpack('<'+'I'*(len(b)//4),b);lines.append('static const DWORD '+name+'[]={')
    for i in range(0,len(values),8):lines.append('    '+','.join(f'0x{x:08x}u' for x in values[i:i+8])+',')
    lines.append('};')
(root/'src/performance/sss_diffusion_shaders.h').write_text('\n'.join(lines)+'\n')
