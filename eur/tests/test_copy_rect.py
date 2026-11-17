#!/usr/bin/env python3
"""Run the native rectangle helper plus built ARM hooks with integer ARM ops.
GPU rendering is not emulated. Expected rectangles come from profile dimensions,
not from the helper's original stereo-dependent height selection.
"""
from pathlib import Path
import struct
from elftools.elf.elffile import ELFFile
ROOT=Path(__file__).resolve().parents[1]
BASE=Path('/home/r/Downloads/0004000000125600.dec.code').read_bytes()

def run(elf_path,width,height,stereo,top,patched=True,bounds=(10,12,110,100)):
    with elf_path.open('rb') as f:
        elf=ELFFile(f)
        syms={s.name:s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
        sections=[(s['sh_addr'],s.data()) for s in elf.iter_sections()
                  if s['sh_flags']&2 and s['sh_type']!='SHT_NOBITS']
    mem={}
    def byte(a):
        if a in mem:return mem[a]
        if 0x100000<=a<0x100000+len(BASE):return BASE[a-0x100000]
        for start,data in sections:
            if start<=a<start+len(data):return data[a-start]
        return 0
    def read(a):return sum(byte(a+i)<<(8*i) for i in range(4))
    def write(a,v,n=4):
        for i in range(n):mem[a+i]=(v>>(8*i))&255
    if patched:
        for site,name in [(0x125bf4,'mm_hook_total_width'),(0x125c1c,'mm_hook_inverse_width')]:
            delta=(syms[name]-site-8)//4
            write(site,0xea000000|(delta&0xffffff))
    write(syms['mm_scene_width'],width);write(syms['mm_scene_height'],height)
    write(0x2000+0xc5,int(stereo),1)
    x0,y0,x1,y1=bounds
    r=[0x11000+i for i in range(16)];r[0:4]=[0x3000,x0,y0,x1]
    r[13]=0x10000000;r[14]=0x40000000;r[15]=0x125bb8
    write(r[13],y1);write(r[13]+4,int(top))
    original=r[:];zero=False
    for _ in range(160):
        pc=r[15]
        if pc==0x40000000:
            assert r[4:14]==original[4:14], 'callee-saved registers/stack damaged'
            return bytes(byte(0x3000+i) for i in range(28))
        if pc==0x1d71ac:
            r[0]=0x2000;r[15]=r[14];continue
        w=read(pc);r[15]=pc+4;c=w>>28
        assert c in (0,1,14),(hex(pc),hex(w))
        if (c==0 and not zero) or (c==1 and zero):continue
        def val(i):return pc+8 if i==15 else r[i]
        if w&0x0e000000==0x0a000000:
            d=w&0xffffff;d=d-(1<<24) if d&(1<<23) else d
            if w&(1<<24):r[14]=pc+4
            r[15]=pc+8+4*d
        elif w&0x0e000000==0x08000000:
            idx=[i for i in range(16) if w&(1<<i)];rn=w>>16&15;a=val(rn)
            up=bool(w&(1<<23));pre=bool(w&(1<<24))
            start=a+(4 if pre else 0) if up else a-4*len(idx)+(0 if pre else 4)
            for j,i in enumerate(idx):
                if w&(1<<20):r[i]=read(start+j*4)
                else:write(start+j*4,val(i))
            if w&(1<<21):r[rn]=a+(4*len(idx) if up else -4*len(idx))
        elif w&0x0fffffff==0x01d01cd5:
            v=byte(r[0]+0xc5);r[1]=v if v<128 else (v-256)&0xffffffff
        elif w&0x0c000000==0x04000000:
            assert not w&(1<<25)
            rn=w>>16&15;rd=w>>12&15;delta=(w&4095)*(1 if w&(1<<23) else -1)
            addr=val(rn)+delta if w&(1<<24) else val(rn)
            if w&(1<<20):r[rd]=byte(addr) if w&(1<<22) else read(addr)
            else:write(addr,val(rd),1 if w&(1<<22) else 4)
            if not w&(1<<24) or w&(1<<21):r[rn]=(val(rn)+delta)&0xffffffff
        elif w&0x0c000000==0:
            if w&(1<<25):
                v=w&255;rot=(w>>8&15)*2
                operand=((v>>rot)|(v<<(32-rot)))&0xffffffff if rot else v
            else:
                assert w&0x70==0,(hex(pc),hex(w))
                operand=(val(w&15)<<(w>>7&31))&0xffffffff
            op=w>>21&15;left=val(w>>16&15);rd=w>>12&15
            if op==13:v=operand
            elif op==4:v=left+operand
            elif op in (2,10):v=left-operand
            elif op==3:v=operand-left
            else:raise AssertionError((hex(pc),hex(w)))
            v&=0xffffffff
            if op==10:zero=v==0
            else:r[rd]=v
        else:raise AssertionError((hex(pc),hex(w)))
    raise AssertionError('native rectangle helper did not return')

def check():
    current=ROOT/'build/native.elf'
    cases=[(400,480,False),(400,240,True),(400,240,False),
           (800,480,False),(800,480,True)]
    for width,height,stereo in cases:
        for bounds in [(10,12,110,100),(0,0,400,240),(0,0,1,1)]:
            x0,y0,x1,y1=bounds
            actual=run(current,width,height,stereo,True,bounds=bounds)
            scale=height//240
            expected=struct.pack('<7I',1,height-y1*scale,width-x1,
                                 (y1-y0)*scale,x1-x0,height,width)
            assert actual==expected,(width,height,stereo,struct.unpack('<7I',actual))
            assert run(current,width,height,stereo,False,bounds=bounds)==run(current,width,height,stereo,False,False,bounds), 'bottom rectangle changed'
    previous=ROOT/'build/v10-native.elf'
    assert run(previous,800,480,True,True)!=struct.pack('<7I',1,280,690,176,100,480,800)
    assert run(previous,400,240,False,True)!=struct.pack('<7I',1,140,290,88,100,240,400)
    print('PASS: actual native ARM rectangle helper; selected width/height/Y scale; bottom preservation; register/stack restoration; v10 stereo/performance regressions reproduced.')
if __name__=='__main__':check()
