"""Compile original GTAO HLSL with installed SDK fxc; no downloaded dependencies."""
import argparse
import hashlib
import struct
import subprocess
from pathlib import Path
p = argparse.ArgumentParser()
p.add_argument('--fxc', required=True)
p.add_argument('--work', required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
source = root / 'src/performance/gtao_renderer.hlsl'
shared = source.with_name('gtao_upsample.hlsli')
work = Path(a.work)
work.mkdir(parents=True, exist_ok=True)
lines = ['// Generated with installed SDK fxc ps_3_0 /O3; original GTAO implementation.',
         '// HLSL SHA256 ' + hashlib.sha256(source.read_bytes()).hexdigest(),
         '// Shared reconstruction SHA256 ' + hashlib.sha256(shared.read_bytes()).hexdigest(), '#pragma once']
for entry, name in [('HorizonPS', 'gtaoHorizonShader'), ('FilterPS', 'gtaoFilterShader'), ('UpsamplePS', 'gtaoUpsampleShader')]:
    out = work / (entry + '.cso')
    asm = work / (entry + '.asm')
    subprocess.run([a.fxc, '/nologo', '/WX', '/T', 'ps_3_0', '/E', entry, '/O3', '/Fo', str(out), '/Fc', str(asm), str(source)], check=True)
    data = out.read_bytes()
    assert len(data) % 4 == 0
    words = struct.unpack('<' + 'I' * (len(data)//4), data)
    lines.append('static const DWORD ' + name + '[]={')
    for i in range(0, len(words), 8):
        lines.append(' ' + ','.join(f'0x{x:08x}u' for x in words[i:i+8]) + ',')
    lines.append('};')
(root / 'src/performance/gtao_shaders.h').write_text('\n'.join(lines) + '\n')
