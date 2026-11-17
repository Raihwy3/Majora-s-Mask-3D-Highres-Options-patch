from pathlib import Path
from PIL import Image
import struct
mods=[(2,8),(5,17),(9,29),(13,42),(18,60),(24,80),(33,106),(47,183)]
def decode(path,out):
 b=Path(path).read_bytes();w,h=struct.unpack_from('<HH',b,44);p=struct.unpack_from('<I',b,20)[0]
 im=Image.new('RGBA',(w,h))
 for ty in range(0,h,8):
  for tx in range(0,w,8):
   for q in range(4):
    alpha,rgb=struct.unpack_from('<QQ',b,p);p+=16
    diff=(rgb>>33)&1;flip=(rgb>>32)&1
    tables=[(rgb>>37)&7,(rgb>>34)&7]
    colors=[[],[]]
    for shift in [59,51,43]:
     if diff:
      a=(rgb>>shift)&31;d=(rgb>>(shift-3))&7;d=d-8 if d&4 else d
      for j,v in enumerate([a,a+d]): colors[j].append((v<<3)|(v>>2))
     else:
      for j,sh in enumerate([shift+1,shift-3]):
       v=(rgb>>sh)&15;colors[j].append(v*17)
    for y in range(4):
     for x in range(4):
      i=x*4+y;s=(y>=2) if flip else (x>=2)
      idx=(rgb>>i)&1;sign=(rgb>>(i+16))&1
      d=mods[tables[s]][idx]*(-1 if sign else 1)
      c=tuple(max(0,min(255,a+d)) for a in colors[s])+(((alpha>>(4*i))&15)*17,)
      im.putpixel((tx+(q&1)*4+x,ty+(q>>1)*4+y),c)
 im.save(out)
if __name__=='__main__':
 import sys
 decode(*sys.argv[1:])
