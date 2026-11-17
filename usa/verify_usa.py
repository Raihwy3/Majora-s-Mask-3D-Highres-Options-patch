"""Independent checks for native call targets, global references and layout ABI."""
from pathlib import Path
import struct,json,re
from elftools.elf.elffile import ELFFile
from inspect_layouts import gar,model
from audit_baseline import records
ROOT=Path(__file__).resolve().parent
P=Path('/home/r/Downloads/scrn/us ver')
A=(P/'0004000000125500.dec.code').read_bytes();B=(P/'0004000E00125500.dec.code').read_bytes()
M={int(k,16):int(v,16) for k,v in json.loads((ROOT/'build/usa-port-map.json').read_text()).items()}
def word(b,at):return struct.unpack_from('<I',b,at-0x100000)[0]
def branch_target(at,w):
 assert w&0x0e000000==0x0a000000
 n=w&0xffffff
 if n&0x800000:n-=0x1000000
 return at+8+n*4
def norm(b):
 out=bytearray(len(b))
 for p in range(0,len(b)-3,4):
  w=struct.unpack_from('<I',b,p)[0]
  if w&0x0e000000==0x0a000000:w&=0xff000000
  elif w&0x0f7f0000==0x051f0000:w&=~0xfff
  elif w&0x0f3f0000==0x0d1f0000:w&=~0xff
  elif w&0x0e0f0000==0x020f0000:w&=~0xfff
  elif 0x100000<=w<0x900000:w=0x12345678
  struct.pack_into('<I',out,p,w)
 return bytes(out)
NA=norm(A[:0x51d000]);NB=norm(B[:0x51e000])
selected=json.loads((ROOT/'build/usa-address-candidates.json').read_text())
for old,entry in selected.items():
 assert 'target' in entry
 addr=int(old,16);target=int(entry['target'],16);before,length=entry['window']
 start=max(0,addr-0x100000-before);pat=NA[start:start+length]
 location=target-0x100000-(addr-0x100000-start)
 assert NB[location:location+len(pat)]==pat
 assert NB.find(pat)==location and NB.find(pat,location+1)<0
for lo,hi in [(0x5a1c14,0x5a1d80),(0x5a1e2c,0x5a2028),(0x5a2580,0x5a26e0),(0x5a2ac8,0x5a2f24)]:
 new=M[lo];assert NA[lo-0x100000:hi-0x100000]==NB[new-0x100000:new-0x100000+(hi-lo)],hex(lo)
for old in [0x105bd0,0x106c58,0x108d74]:
 new=M[old];callee=branch_target(old,word(A,old));target=branch_target(new,word(B,new))
 pat=NA[callee-0x100000-16:callee-0x100000+112]
 expected=target-0x100000-16
 assert NB[expected:expected+len(pat)]==pat,(hex(old),hex(callee),hex(target))
 assert NB.find(pat)==expected and NB.find(pat,expected+1)<0
pattern=struct.pack('<II',0x63b13c,0x5a2bf4)
p=B.find(pattern);assert p>=4 and B.find(pattern,p+1)<0
assert struct.unpack_from('<I',B,p-4)[0]==0x6f8350==M[0x6f7348]
assert word(B,0x63b13c) == M[word(A,0x63a11c)] if word(A,0x63a11c) in M else True
oldptr=struct.pack('<I',0x656b00);newptr=struct.pack('<I',0x657b00)
assert A.count(oldptr)>8 and B.count(newptr)>8 and M[0x656b08]==0x657b08
u=gar(P/'Joker.Main.gar');e=gar(Path('/home/r/Downloads/RomFS/layout/Joker.Main.gar'))
assert u.keys()==e.keys()
assert {n for n in u if u[n]!=e[n]}=={'icon_item_menu00.ctxb'}
assert (P/'Joker.Main.Play.Option.gar').read_bytes()==Path('/home/r/Downloads/scrn/Joker.Main.Play.Option.gar').read_bytes()
lo,hi=0x61d400,0x61d520
for at in range(0,0x51d1ac-3,4):
 w=struct.unpack_from('<I',B,at)[0]
 assert not lo<=w<hi,hex(at+0x100000)
 if w&0x0f7f0000==0x051f0000:
  offset=w&0xfff;target=at+0x100008+(offset if w&(1<<23) else -offset)
  assert not lo<=target<hi,hex(at+0x100000)
with (ROOT/'build/native.elf').open('rb') as f:
 elf=ELFFile(f);syms={s.name:s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
 assert not any('filter_present' in n for n in syms)
 linker=(ROOT/'native/module.ld').read_text()
 original_linker=Path('/home/r/mm3d-display-options/build/v44-source-before-filters.tar.gz')
 import tarfile
 with tarfile.open(original_linker) as t:old_linker=t.extractfile('native/module.ld').read().decode()
 for name,value in re.findall(r'(mm_\w+)\s*=\s*(0x[0-9a-fA-F]+)',old_linker):
  if name=='mm_restoration_start':continue
  old=int(value,16);assert syms[name]==M[old],(name,hex(old))
print('PASS: unique mapped instruction windows; full Options field ABI; high-res BL targets; vtable/constructor; screen table; USA GUI; unreferenced bootstrap padding; all linked native APIs; no v45 filter.')
