"""Relocate the isolated v44 sources to the supplied USA 1.1 image."""
from pathlib import Path
import json,re,struct
code=json.loads(Path('build/usa-address-candidates.json').read_text())
mapping={int(k,16):int(v['target'],16) for k,v in code.items() if 'target' in v}
literals=json.loads(Path('build/usa-literal-candidates.json').read_text())
for k,v in literals.items():
 if len(v['values'])==1:mapping[int(k,16)]=int(next(iter(v['values'])),16)
mapping.update({0x6f7348:0x6f8350,0x656b00:0x657b00,0x656b08:0x657b08,0x61cee0:0x61d400})
Path('build/usa-port-map.json').write_text(json.dumps({hex(k):hex(v) for k,v in sorted(mapping.items())},indent=2)+'\n')
for folder in ['native','tests']:
 for p in Path(folder).glob('*'):
  if p.suffix not in ['.c','.h','.S','.ld','.py']:continue
  s=p.read_text()
  def replace(m):
   v=int(m[0],16);return hex(mapping[v]) if v in mapping else m[0]
  s=re.sub(r'0x[0-9a-fA-F]+',replace,s)
  s=s.replace('0004000000125600','0004000000125500')
  p.write_text(s)
p=Path('native/bootstrap.S');s=p.read_text();a=s.index('    /* Both supported baselines:');b=s.index('    bl mm_original_clear_bss',a);s=s[:a]+s[b:];p.write_text(s)
p=Path('native/module.ld');s=p.read_text().replace('mm_restoration_start = 0x61cecc;\n','');p.write_text(s)
p=Path('build_options_tabs.py');s=p.read_text().replace('/home/r/Downloads/scrn/Joker.Main.Play.Option.gar','/home/r/Downloads/scrn/us ver/Joker.Main.Play.Option.gar').replace('/home/r/Downloads/RomFS/layout/Joker.Main.gar','/home/r/Downloads/scrn/us ver/Joker.Main.gar');p.write_text(s)
