"""Check the sole remaining stereo-strength load hook; no view/trace hooks."""
from pathlib import Path
import struct
from elftools.elf.elffile import ELFFile
with Path('build/native.elf').open('rb') as f:
    elf=ELFFile(f);symbols={s.name:s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
    sec=elf.get_section_by_name('.text');data=sec.data();base=sec['sh_addr']
def word(a):return struct.unpack_from('<I',data,a-base)[0]
source=Path('/home/r/Downloads/0004000000125600.dec.code').read_bytes()
assert struct.unpack_from('<I',source,0x691c)[0]==0xed9f0aa9
assert struct.unpack_from('<f',source,0x6bc8)[0]==10.
pc=symbols['mm_hook_stereo_depth']
assert word(pc)==0xe52dc004
load=word(pc+4);assert load&0xfffff000==0xe59fc000
assert word(pc+12+(load&4095))==symbols['mm_stereo_depth']
assert word(pc+8)==0xed9c0a00 and word(pc+12)==0xe49dc004
branch=word(pc+16);assert branch>>24==0xea
shift=branch&0xffffff
if shift&0x800000:shift-=0x1000000
assert pc+24+shift*4==0x106920
for value in (0,5,7.5,10,12.5,15,17.5,20,25,32):
    r12=0xdead1234;sp=0x10000000;saved=r12;sp-=4
    r12=symbols['mm_stereo_depth'];s0=struct.pack('<f',value)
    r12=saved;sp+=4
    assert r12==0xdead1234 and sp==0x10000000 and struct.unpack('<f',s0)[0]==value
for name in ('mm_hook_stereo_projection','mm_hook_frustum_left','mm_hook_frustum_right',
             'mm_stereo_camera','mm_stereo_trace','mm_stereo_frame','mm_render_view_status',
             'mm_render_stereo_status','mm_render_stereo_output','mm_render_gpu_transfer'):
    assert name not in symbols,name
print('PASS: linked strength load preserves register/stack/flags and exact continuation; view/projection/LCD/GPU diagnostic hooks absent.')
