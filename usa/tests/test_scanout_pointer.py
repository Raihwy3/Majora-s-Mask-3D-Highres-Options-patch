"""Execute native ARM scanout address calculation, not a guessed width formula.
No GSP/GPU emulation: covers the concrete v17 display-pointer mismatch only.
"""
from pathlib import Path
import struct
base=Path('/home/r/Downloads/scrn/us ver/0004000E00125500.dec.code').read_bytes()
def scanout(record_width,display_width):
    mem={0x10000+4:0x18000000,0x10000+0xc:240,0x10000+0x10:record_width,
         0x10000+0x14:0x10000,0x20000+8:display_width,0x20000+0x134:0,0x20000+0x140:0}
    r=[0]*16;r[1]=3;r[2]=0x10000;r[5]=0x20000
    for pc in range(0x148a4c,0x148a80,4):
        w=struct.unpack_from('<I',base,pc-0x100000)[0];assert w>>28==14
        if w&0x0c000000==0x04000000:
            assert w&(1<<20) and not w&(1<<25)
            r[(w>>12)&15]=mem[r[(w>>16)&15]+(w&4095)]
        elif w&0x0fc000f0==0x00000090:
            rd=(w>>16)&15;acc=r[(w>>12)&15] if w&(1<<21) else 0
            r[rd]=(r[w&15]*r[(w>>8)&15]+acc)&0xffffffff
        else:
            op=(w>>21)&15;rn=(w>>16)&15;rd=(w>>12)&15
            if w&(1<<25):
                x=w&255;rot=((w>>8)&15)*2
                v=((x>>rot)|(x<<(32-rot)))&0xffffffff if rot else x
            else:
                assert not w&0xff0
                v=r[w&15]
            if op==2:r[rd]=(r[rn]-v)&0xffffffff
            elif op==10:pass
            else:raise AssertionError((hex(pc),hex(w)))
    return r[2],r[8]
assert scanout(800,800)==(0x18000000,720)
assert scanout(400,400)==(0x18000000,720)
assert scanout(800,400)==(0x18000000+400*240*3,720)
print('PASS: native ARM scanout selects +288000 bytes for stale 800/400 record mismatch; synchronized 400/400 and 800/800 select the written buffer base with unchanged 720-byte stride.')

def right_scanout(record_width,display_width):
    r=[0]*16;r[12]=record_width;r[3]=display_width;r[10]=0
    r[14]=240;r[11]=0;r[0]=0x18100000;r[1]=3
    for pc in range(0x148ae8,0x148af8,4):
        w=struct.unpack_from('<I',base,pc-0x100000)[0];assert w>>28==14
        if w&0x0fc000f0==0x00000090:
            assert w&(1<<21)
            rd=(w>>16)&15
            r[rd]=(r[w&15]*r[(w>>8)&15]+r[(w>>12)&15])&0xffffffff
        else:
            assert (w>>21)&15==2 and not w&0xff0
            r[(w>>12)&15]=(r[(w>>16)&15]-r[w&15])&0xffffffff
    return r[3]
assert right_scanout(800,400)==0x18100000+288000
assert right_scanout(400,400)==0x18100000
print('PASS: native right-eye ARM scanout has the same stale-width offset and synchronized-width correction.')
