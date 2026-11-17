"""Find address correspondences using normalized ARM instruction windows."""
from pathlib import Path
import struct,json,re
BASE=Path('/home/r/Downloads/scrn/us ver/0004000000125500.dec.code').read_bytes()
UPDATE=Path('/home/r/Downloads/scrn/us ver/0004000E00125500.dec.code').read_bytes()
def normalized(b,n):
 out=bytearray(n)
 for at in range(0,n,4):
  w=struct.unpack_from('<I',b,at)[0]
  if w&0x0e000000==0x0a000000:w&=0xff000000
  elif w&0x0f7f0000==0x051f0000:w&=~0xfff
  elif w&0x0f3f0000==0x0d1f0000:w&=~0xff
  elif w&0x0e0f0000==0x020f0000:w&=~0xfff
  elif 0x100000<=w<0x900000:w=0x12345678
  struct.pack_into('<I',out,at,w)
 return bytes(out)
A=normalized(BASE,0x51d000);B=normalized(UPDATE,0x51e000)
def candidates(address,before=32,length=128):
 at=address-0x100000;start=max(0,at-before);pat=A[start:start+length]
 result=[];p=0
 while True:
  p=B.find(pat,p)
  if p<0:break
  if p%4==0:result.append(p+(at-start)+0x100000)
  p+=1
 return result
addresses=set()
for p in Path('native').glob('*'):
 if p.suffix not in ['.S','.c','.ld']:continue
 for s in re.findall(r'0x[0-9a-fA-F]+',p.read_text()):
  n=int(s,16)
  if 0x100000<=n<0x61cd8c:addresses.add(n)
from audit_baseline import records
for off,data in records(Path('baseline/code.ips').read_bytes()):
 if off<0x51cd8c:addresses.add(off+0x100000)
addresses.update([0x100000,0x10691c,0x110ad8,0x1250bc,0x1258fc,0x125ac8,0x125bf4,0x125c1c,0x5a31bc,0x5a3318,0x5a34d0,0x59b6a0,0x59b27c,0x61be64,0x116748,0x11675c,0x5a3e40,0x5a3d9c,0x5a41f0,0x115aa0,0x115b10,0x148b28,0x125594])
report={}
for n in sorted(addresses):
 found=[]
 for before,length in [(32,192),(16,128),(0,128),(32,64),(0,64),(16,48),(0,32)]:
  hits=candidates(n,before,length)
  if len(hits)==1:found=hits;break
 if found:report[hex(n)]={'target':hex(found[0]),'delta':hex(found[0]-n),'window':[before,length]}
 else:report[hex(n)]={'unresolved':True,'candidates':candidates(n,0,32)}
Path('build/usa-address-candidates.json').write_text(json.dumps(report,indent=2)+'\n')
for k,v in report.items():print(k,v)
globals_=[0x6a3a48,0x7b2700,0x7cdc9c,0x6f7348,0x656b08,0x63a11c,0x63bc50,0x6a3edc,0x61cf44,0x61cecc,0x7b12e8]
from collections import Counter
literal_report={}
for value in globals_:
 raw=struct.pack('<I',value);at=0;found=[];unmatched=0
 while True:
  at=BASE.find(raw,at,0x51cd8c)
  if at<0:break
  hits=candidates(at+0x100000,32,128)
  if len(hits)==1:
   mapped=struct.unpack_from('<I',UPDATE,hits[0]-0x100000)[0]
   found.append((at+0x100000,hits[0],mapped))
  else:unmatched+=1
  at+=4
 counts=Counter(v for _,_,v in found)
 literal_report[hex(value)]={'values':{hex(k):v for k,v in counts.items()},'unmatched':unmatched,'references':[[hex(a),hex(b),hex(v)] for a,b,v in found]}
 print('GLOBAL',hex(value),literal_report[hex(value)]['values'],'unmatched',unmatched)
Path('build/usa-literal-candidates.json').write_text(json.dumps(literal_report,indent=2)+'\n')
