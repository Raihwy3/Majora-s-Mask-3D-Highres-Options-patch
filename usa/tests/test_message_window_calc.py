"""Check the actual native closed-window branch and private base-update target.
This is a binary control-flow contract, not glyph/GPU emulation.
"""
from pathlib import Path
import struct
base=Path('/home/r/Downloads/scrn/us ver/0004000E00125500.dec.code').read_bytes()
def word(a):return struct.unpack_from('<I',base,a-0x100000)[0]
def target(pc):
    w=word(pc);assert w&0x0e000000==0x0a000000
    d=w&0xffffff;d=d-(1<<24) if d&(1<<23) else d
    return pc+8+d*4
assert word(0x63e0f4+16)==0x606de8
assert word(0x606e00)==0xe59003dc
assert word(0x606e10)==0xe3500000
assert word(0x606e18)>>28==0 and target(0x606e18)==0x607410
assert word(0x607410)==0xe28dd008 and word(0x607418)==0xe8bd81f0
assert target(0x60740c)==0x161bdc
assert word(0x161ba4)==0xe590c010
print('PASS: real MessageWindow state-0 branch skips base layout; the active-dialog base target is 0x161bdc, dispatched by standard wrapper slot 4.')

assert word(0x606ef8)==0xe3823001
assert word(0x606efc)==0xe3833802
assert word(0x606f04)==0xe38c2802
assert word(0x606f08)==0xe5812004
print('PASS: native show setter ORs the visibility value into the active-property word; clearing it reproduces v15 tab visibility regression.')
