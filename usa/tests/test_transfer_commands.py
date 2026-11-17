"""Execute the native integer ARM transfer builder; no GPU emulation.
Checks packed texture-copy shape separately from active final resolve shape.
"""
from pathlib import Path
import struct
from elftools.elf.elffile import ELFFile
BASE=Path('/home/r/Downloads/scrn/us ver/0004000E00125500.dec.code').read_bytes()

def command(entry,registers,stack):
    mem={}
    def read(a,n=4):
        return sum((mem[a+i] if a+i in mem else BASE[a+i-0x100000] if 0x100000<=a+i<0x100000+len(BASE) else 0)<<(8*i) for i in range(n))
    def write(a,v,n=4):
        for i in range(n):mem[a+i]=(v>>(8*i))&255
    r=[0]*16;r[:4]=registers
    r[13]=0x10000000;r[14]=0x40000000;r[15]=entry
    for i,v in enumerate(stack):write(r[13]+4*i,v)
    write(0x20000+0x18,0x30000);write(0x20000+0x20,0)
    write(0x7b3700+0x9c,0x20000)
    z=n=c=v=False
    for _ in range(300):
        pc=r[15]
        if pc==0x40000000:
            assert read(0x20000+0x20)==1
            return bytes(read(0x30000+i,1) for i in range(28))
        if pc in (0x12f8f8,0x12f908):r[15]=r[14];continue
        w=read(pc);r[15]=pc+4;cond=w>>28
        ok={0:z,1:not z,2:c,3:not c,10:n==v,11:n!=v,14:True}
        assert cond in ok,(hex(pc),hex(w))
        if not ok[cond]:continue
        def reg(i):return pc+8 if i==15 else r[i]
        def shift(x,t,s):
            if t==0:return (x<<s)&0xffffffff
            if t==1:return x>>(s or 32)
            if t==2:return ((x if x<0x80000000 else x-0x100000000)>>(s or 32))&0xffffffff
            raise AssertionError((hex(pc),t,s))
        if w&0x0e000000==0x0a000000:
            d=w&0xffffff;d=d-(1<<24) if d&(1<<23) else d
            if w&(1<<24):r[14]=pc+4
            r[15]=pc+8+4*d
        elif w&0x0e000000==0x08000000:
            ids=[i for i in range(16) if w&(1<<i)];rn=w>>16&15;a=reg(rn)
            up=bool(w&(1<<23));pre=bool(w&(1<<24))
            start=a+(4 if pre else 0) if up else a-4*len(ids)+(0 if pre else 4)
            for j,i in enumerate(ids):
                if w&(1<<20):r[i]=read(start+4*j)
                else:write(start+4*j,reg(i))
            if w&(1<<21):r[rn]=(a+(4*len(ids) if up else -4*len(ids)))&0xffffffff
        elif w&0x0fc000f0==0x00000090:
            acc=reg(w>>12&15) if w&(1<<21) else 0
            r[w>>16&15]=(reg(w&15)*reg(w>>8&15)+acc)&0xffffffff
        elif w&0x0e000090==0x00000090:
            assert not w&(1<<20) and w&(1<<22) and w&0x60==0x20,(hex(pc),hex(w))
            delta=((w>>4)&0xf0)|(w&15);write(reg(w>>16&15)+delta,reg(w>>12&15),2)
        elif w&0x0c000000==0x04000000:
            assert not w&(1<<25)
            rn=w>>16&15;rd=w>>12&15;d=(w&4095)*(1 if w&(1<<23) else -1)
            a=reg(rn)+d if w&(1<<24) else reg(rn);size=1 if w&(1<<22) else 4
            if w&(1<<20):r[rd]=read(a,size)
            else:write(a,reg(rd),size)
            if not w&(1<<24) or w&(1<<21):r[rn]=(reg(rn)+d)&0xffffffff
        elif w&0x0fff0ff0==0x06ff0070:r[w>>12&15]=reg(w&15)&0xffff
        elif w&0x0c000000==0:
            if w&(1<<25):
                x=w&255;rot=(w>>8&15)*2;b=((x>>rot)|(x<<(32-rot)))&0xffffffff if rot else x
            else:
                assert not w&0x10,(hex(pc),hex(w))
                b=shift(reg(w&15),(w>>5)&3,(w>>7)&31)
            op=w>>21&15;a=reg(w>>16&15);rd=w>>12&15
            if op==0:ans=a&b
            elif op in (2,10):ans=a-b
            elif op==3:ans=b-a
            elif op==4:ans=a+b
            elif op==8:ans=a&b
            elif op==12:ans=a|b
            elif op==13:ans=b
            elif op==14:ans=a&~b
            else:raise AssertionError((hex(pc),hex(w)))
            ans&=0xffffffff
            if op in (8,10) or w&(1<<20):
                z=ans==0;n=bool(ans&0x80000000)
                if op in (2,10):c=a>=b;v=bool((a^b)&(a^ans)&0x80000000)
            if op not in (8,10):r[rd]=ans
        else:raise AssertionError((hex(pc),hex(w)))
    raise AssertionError('native transfer did not return')

def transfer(scale,height,width,iw=512,ih=800):
    return command(0x148bd8,[0x20000,0x14000000,iw,ih],
                   [0x8058,0x18000000,height,width,0x8051,scale,0,8])

for scale,h,w in [(0x500,240,400),(0x501,240,400),(0x501,240,800),(0x502,240,400)]:
    cmd=transfer(scale,h,w)
    src,dst=struct.unpack_from('<II',cmd,4)
    ih,iw,oh,ow=struct.unpack_from('<4H',cmd,12)
    flags=struct.unpack_from('<I',cmd,20)[0]
    assert src==0x14000000 and dst==0x18000000
    assert (ih,iw)==(512,800)
    assert (oh,ow)==(h*(1 if scale==0x500 else 2),w*(2 if scale==0x502 else 1))
    assert oh<=ih and ow<=iw
    assert (flags>>7)&3==scale-0x500
with Path('build/native.elf').open('rb') as f:
    elf=ELFFile(f);syms={s.name:s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
    sec=elf.get_section_by_name('.text');data=sec.data();start=sec['sh_addr']
    for name,target in [('mm_hook_copy_a',0x125aa0),('mm_hook_copy_b',0x125c6c)]:
        pc=syms[name];mov,branch=struct.unpack_from('<2I',data,pc-start)
        assert mov==0xe3a03e32
        d=branch&0xffffff;d=d-(1<<24) if d&(1<<23) else d
        assert branch>>24==0xea and pc+12+4*d==target
print('PASS: actual native ARM transfer commands retain source backing 512x800, resolve to valid active rectangles for all profiles; both linked packed-copy hooks retain width800.')

for scale in range(0x500,0x503):
    x=1 if scale==0x500 else 2;y=2 if scale==0x502 else 1
    w,h=240*x,400*y;line=w*8*4;gap=(512-w)*8*4
    copy=command(0x1369dc,[0x1f16ffc0,line,gap,0x14800000],[line,0,w*h*4])
    assert copy[0]==4
    assert struct.unpack_from('<3I',copy,4)==(0x1f16ffc0,0x14800000,w*h*4)
    assert struct.unpack_from('<4H',copy,16)==(gap//16,line//16,0,line//16)
    cmd=transfer(scale,240,400,w,h)
    assert struct.unpack_from('<4H',cmd,12)==(w,h,w,h)
    iw,ih,ow,oh=struct.unpack_from('<4H',cmd,12)
    assert ow==iw and oh==ih
print('PASS: native ARM queued repack line/gap units, byte counts and compact input/resolve dimensions for NoScale/ScaleX/ScaleXY.')

source=bytes(((i//256)*17+i)%251 for i in range(512*800*4))
def tiled(width,x,y):
    morton=sum(((x>>i)&1)<<(2*i) | ((y>>i)&1)<<(2*i+1) for i in range(3))
    return (((y//8)*(width//8)+x//8)*64+morton)*4
for w,h in [(240,400),(480,400),(480,800)]:
    line=w*8*4;gap=(512-w)*8*4
    cmd=command(0x1369dc,[0x1f16ffc0,line,gap,0x14800000],[line,0,w*h*4])
    gap_units,line_units=struct.unpack_from('<2H',cmd,16)
    line_bytes,gap_bytes=line_units*16,gap_units*16
    packed=b''.join(source[start:start+line_bytes] for start in range(0,h*512*4,line_bytes+gap_bytes))
    assert len(packed)==w*h*4<=1536000
    for x in [0,7,8,w//2,w-1]:
        for y in [0,7,8,h//2,h-1]:
            a,b=tiled(w,x,y),tiled(512,x,y)
            assert packed[a:a+4]==source[b:b+4]
print('PASS: native packet line/gap values preserve tiled pixel coordinates and remain within owned staging capacity.')

for w,h in [(240,400),(480,400),(480,800)]:
    rendered=bytearray(512*800*4)
    for raster_y in range(h):
        for x in range(w):
            at=tiled(512,x,799-raster_y)
            rendered[at:at+4]=bytes((1+x%254,1+raster_y%254,123,255))
    offset=(800-h)*512*4
    line=w*8*4
    packed=b''.join(rendered[offset+y*512*4:offset+y*512*4+line]
                    for y in range(0,h,8))
    assert len(packed)==w*h*4
    for raster_y in range(h):
        for x in (0,7,8,w//2,w-1):
            at=tiled(w,x,h-1-raster_y)
            assert packed[at:at+4]==bytes((1+x%254,1+raster_y%254,123,255))
    if h==400:
        old=b''.join(rendered[y*512*4:y*512*4+line] for y in range(0,h,8))
        assert not any(old), 'old source origin must reproduce empty native-resolution output'
        assert offset==819200
    else:
        assert offset==0, 'working supersampling path must retain its source origin'
print('PASS: inverted physical framebuffer Y reproduces black old native crop; corrected origin retains viewport pixels; SS origin unchanged.')

tail=0x1f16ffc0+400*512*4
cmd=command(0x148bd8,[0x20000,tail,512,400],
            [0x8058,0x14109120,240,400,0x8051,0x501,0,8])
assert struct.unpack_from('<II',cmd,4)==(tail,0x14109120)
assert struct.unpack_from('<4H',cmd,12)==(512,400,480,400)
assert (struct.unpack_from('<I',cmd,20)[0]>>7)&3==1
assert tiled(512,479,399)+4<=512*400*4
assert 400*512*4+512*400*4==512*800*4
print('PASS: native ARM400x480 SS3D offset resolve retains pitch512, halves only GPU X, and stays within original allocation.')
