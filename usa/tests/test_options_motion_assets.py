from pathlib import Path
import struct,sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from inspect_layouts import gar
b=gar(Path('/home/r/Downloads/scrn/us ver/Joker.Main.Play.Option.gar'))['MenuOption.mfl']
_,count,params,panes,players,total=struct.unpack_from('<6H',b,8)
a,start,names=struct.unpack_from('<III',b,20)
strings=b[names:].split(b'\0');nodes=[x.decode() for x in strings[1+params+panes:1+params+panes+count]]
animation_names=[x.decode() for x in strings[1+params+panes+count+players:]][:total]
assert animation_names[3:5]==['from_page1to2_ani','from_page2to1_ani']
parsed=[];p=start
for _ in range(total):
 n,fps,duration=struct.unpack_from('<HHI',b,p);p+=8;tracks={}
 for _ in range(n):
  node,prop,num,last=struct.unpack_from('<HHII',b,p);p+=12;keys=[]
  for _ in range(num):
   frame,flags,value=struct.unpack_from('<IIf',b,p);p+=12
   keys.append((frame,flags,value))
  tracks[nodes[node],prop]=keys
 parsed.append((fps,duration,tracks))
assert p==names
for index,duration in [(3,6),(4,7)]:
 fps,frames,t=parsed[index];assert fps==30 and frames==duration
 for row in ['b_plate_subjective_cam_l','b_plate_motion_l','b_plate_swim_l']:
  assert (row,18) in t and all(0<=v<=1 for _,_,v in t[row,18])
 assert t['page2_g',1][0][2]==(240 if index==3 else 0)
 assert t['page2_g',1][-1][2]==(0 if index==3 else 240)
 assert t['page1_g',1][0][2]==(0 if index==3 else -240)
 assert t['page1_g',1][-1][2]==(-240 if index==3 else 0)
print('PASS: supplied native page tracks really use vertical translation and staggered row alpha at30Hz over6/7 frames')

base=Path('/home/r/Downloads/scrn/us ver/0004000E00125500.dec.code').read_bytes()
def word(address):return struct.unpack_from('<I',base,address-0x100000)[0]
assert word(0x198990)==0xe281208c
assert word(0x1bbc94)==0xed840a01
assert word(0x1bc184)==0xed840a11
assert word(0x1cad3c)==0xe284103c
assert word(0x1cad40)==0xe284008c
assert word(0x18010c)==0xe590000c
assert word(0x180118)>>28==0
assert word(0x191fd8)==0xe580100c
print('PASS: actual native opcodes distinguish base/evaluated pose; Y+90/alpha+d0 and detached/restored native player contracts')
