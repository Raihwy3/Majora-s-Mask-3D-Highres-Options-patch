#!/usr/bin/env python3
"""Build the stock USA 1.1 port of EU v44, with exact input/hook guards."""
from pathlib import Path
import struct,json,hashlib,subprocess,zipfile,shutil,sys
from elftools.elf.elffile import ELFFile
from audit_baseline import records
from build_native import branch,make_ips,patched
from build_options_tabs import build as build_tabs
ROOT=Path(__file__).resolve().parent
INPUT=Path('/home/r/Downloads/scrn/us ver')
TITLE='0004000000125500'
EXPECTED='a1d1724df4857a68df200d2543ab6c96df172738fe511f58168b77c58fe36e05'
def sha(b):return hashlib.sha256(b).hexdigest()
def word(b,o):return struct.unpack_from('<I',b,o)[0]
M={int(k,16):int(v,16) for k,v in json.loads((ROOT/'build/usa-port-map.json').read_text()).items()}
def addr(n):return M[n]
def main():
 source=(INPUT/'0004000E00125500.dec.code').read_bytes()
 original=(INPUT/'0004000000125500.dec.code').read_bytes()
 assert sha(source)==EXPECTED
 assert sha(original)=='050a58834365187ca67211eef0a29816715e9ab9f7289e8372b06fb906620580'
 header=bytearray((INPUT/'up1.1extheader.bin').read_bytes())
 assert sha(header)=='af4d4cf00676038e7e394f614431306f62cf761683386283fe8ae8ee0e13a9a7'
 assert struct.unpack_from('<Q',header,0x200)[0]==int(TITLE,16)
 subprocess.run(['make','native','check','check-native','check-menu','check-render','check-text','check-rect','check-style','check-tabs','check-scanout','check-resolve','check-depth','check-settings','check-motion'],check=True,cwd=ROOT)
 subprocess.run([sys.executable,'verify_usa.py'],check=True,cwd=ROOT)
 with (ROOT/'build/native.elf').open('rb') as f:
  elf=ELFFile(f);syms={s.name:s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
  assert not any(s['sh_type'] in ('SHT_REL','SHT_RELA') and s['sh_size'] for s in elf.iter_sections())
 start=syms['__module_start'];rx_end=syms['__module_rx_end'];end=syms['__module_file_end'];bss_end=syms['__module_bss_end']
 text,pages=struct.unpack_from('<II',header,0x10);ro,ro_pages=struct.unpack_from('<II',header,0x20);data,data_pages=struct.unpack_from('<II',header,0x30)
 assert (text,pages,ro,ro_pages,data)==(0x100000,0x51e,0x61e000,0x2e,0x64c000)
 old_end=data+4096*data_pages
 assert start>=old_end+((word(header,0x3c)+4095)&~4095)
 module=(ROOT/'build/module.bin').read_bytes();module=module.ljust(end-start,b'\0')
 assert len(module)==end-start and start<rx_end<=end<=bss_end
 boot=(ROOT/'build/bootstrap.bin').read_bytes();boot_address=syms['mm_bootstrap']
 assert boot_address==0x61d400 and len(boot)<=0x120
 assert 0x100000+word(header,0x18)<=boot_address<boot_address+len(boot)<=ro
 assert source[boot_address-text:boot_address-text+len(boot)]==bytes(len(boot))
 struct.pack_into('<I',header,0x18,pages*4096)
 constant_sites=[0x5bc8,0x8d94,0x8fc8,0x15ea4,0x15eb8,0x4ed5b8]
 spans=[];guards=[]
 for off,b in records((ROOT/'baseline/code.ips').read_bytes()):
  if off in constant_sites:
   target=addr(off+text)-text
   assert source[target:target+len(b)]==original[off:off+len(b)]
   spans.append((target,b));guards.append({'kind':'constant','source_offset':hex(off),'target_offset':hex(target),'original':source[target:target+len(b)].hex()})
 for off in [0x527952,0x5a39aa]:
  pattern=original[off-16:off+18];hit=source.find(pattern)
  assert hit>=0 and source.find(pattern,hit+1)<0
  target=hit+16;assert source[target:target+2]==b'\0\1'
  spans.append((target,b'\0\2'));guards.append({'kind':'descriptor','source_offset':hex(off),'target_offset':hex(target)})
 def hook(old,target,expected,link=False,size=None):
  at=addr(old);off=at-text
  if isinstance(expected,int): expected=struct.pack('<I',expected)
  assert source[off:off+len(expected)]==expected,(hex(at),target,source[off:off+len(expected)].hex(),expected.hex())
  n=size or len(expected)
  value=branch(at,syms[target],link)+struct.pack('<I',0xe320f000)*((n-4)//4)
  spans.append((off,value));guards.append({'kind':'hook','target_offset':hex(off),'handler':target,'original':expected.hex(),'bytes_replaced':n})
 for old,target,callee in [(0x105bd0,'mm_hires_create',0x108fac),(0x106c58,'mm_hires_bottom',None),(0x108d74,'mm_hires_setup',None)]:
  at=addr(old);oldword=word(original,old-text)
  expected=source[at-text:at-text+4]
  assert oldword>>24==0xeb and word(source,at-text)>>24==0xeb
  hook(old,target,expected,True)
 for old,target,opcode in [
  (0x10691c,'mm_hook_stereo_depth',0xed9f0aa9),
  (0x110ad8,'mm_hook_rect_width',0xe3a03e19),
  (0x1258fc,'mm_hook_copy_a',0xe3a03e19),(0x125ac8,'mm_hook_copy_b',0xe3a03e19),
  (0x125bf4,'mm_hook_total_width',0xe3a0ce19),(0x125c1c,'mm_hook_inverse_width',0xe2692e19),
  (0x5a31bc,'mm_options_draw_hook',0xe92d4030),(0x5a3318,'mm_options_close_hook',0xe92d4070),
  (0x5a34d0,'mm_options_finish_hook',0xe92d41f0),(0x59b6a0,'mm_main_calc_hook',0xe92d4ff0),
  (0x59b27c,'mm_main_draw_hook',0xe92d4070)]:hook(old,target,opcode)
 hook(0x1250bc,'mm_hook_scene_size',struct.pack('<3I',0xe3a00e19,0x13a030f0,0x03a03e1e))
 hook(0x61be64,'mm_render_stereo',struct.pack('<2I',0xe59f0028,0xe5d01084))
 for old,target,callee in [
  (0x116748,'mm_cstick_speed',0x12e904),(0x11675c,'mm_cstick_speed',0x12e904),
  (0x5a3e40,'mm_options_input_hook',0x5a2ac8),(0x5a3d9c,'mm_options_touch_hook',0x5a2580),
  (0x5a41f0,'mm_help_calc_hook',0x196c38),(0x115aa0,'mm_render_transfer',0x1254d8),
  (0x115b10,'mm_render_transfer',0x1254d8),(0x148b28,'mm_render_present',0x15bc60),
  (0x125594,'mm_resolve_packet',0x136890)]:hook(old,target,branch(addr(old),addr(callee),True),True)
 for name,old in [('mm_game_options_draw',0x5a31bc),('mm_game_options_close',0x5a3318),('mm_game_options_finish',0x5a34d0),('mm_game_main_calc',0x59b6a0),('mm_game_main_draw',0x59b27c)]:
  at=syms[name]-start;target=addr(old)
  assert module[at:at+4]==source[target-text:target-text+4]
  assert module[at+4:at+8]==branch(syms[name]+4,target+4)
 spans.extend([(0,branch(text,boot_address)),(boot_address-text,boot),(old_end-text,bytes(start-old_end)),(start-text,module)])
 patch=make_ips(spans)
 struct.pack_into('<II',header,0x34,(end-data)//4096,end-data)
 struct.pack_into('<I',header,0x3c,bss_end-end)
 header[0x248]|=0x80;struct.pack_into('<I',header,0x3f0,word(header,0x3f0)|(1<<9))
 for svc in [3,0x23,0x32,0x33,0x35,0x70]:
  found=False
  for off in range(0x370,0x3e0,4):
   v=word(header,off)
   if v>>27==30 and (v>>24)&7==svc//24:
    struct.pack_into('<I',header,off,v|(1<<(svc%24)));found=True;break
  if not found:
   off=next(off for off in range(0x370,0x3e0,4) if word(header,off)==0xffffffff)
   struct.pack_into('<I',header,off,0xf0000000|((svc//24)<<24)|(1<<(svc%24)))
 capacity=(end-text);result=patched(source,patch,capacity)
 assert result[4:0x100]==source[4:0x100]
 assert result[0x51e000:0x54c000]!=bytes(0x2e000)
 assert b'mm_filter_present' not in module
 out=ROOT/'build/usa-1.1-v44';title=out/'luma/titles'/TITLE;title.mkdir(parents=True,exist_ok=True)
 (title/'code.ips').write_bytes(patch);(title/'exheader.bin').write_bytes(header)
 layout=build_tabs(title/'romfs/layout/Joker.Main.Play.Option.gar')
 installed=[title/'code.ips',title/'exheader.bin',layout]
 report={'version':'USA 1.1 port of working v44','title_id':TITLE,'source_sha256':sha(source),'base_source_sha256':sha(original),'update_header_sha256':sha((INPUT/'up1.1extheader.bin').read_bytes()),'module_start':hex(start),'bootstrap':hex(boot_address),'module_bytes':len(module),'module_bss_bytes':bss_end-end,'hooks_and_constants':guards,'native_port_audit_passed':True,'host_checks_passed':True,'no_v45_cpu_filters':True,'project_restoration_bundled':False,'project_restoration_compatibility_verified':False,'hardware_tested':False,'files':{str(p.relative_to(out)):sha(p.read_bytes()) for p in installed}}
 (out/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
 (out/'README.txt').write_text('''MM3D USA update 1.1 - v44 display-options port (experimental)

Requires USA title 0004000000125500 and the supplied 1.1 update executable.
Merge the luma/ folder onto the SD card. Replace all three included files:
code.ips, exheader.bin and romfs/layout/Joker.Main.Play.Option.gar.
Enable Luma game patching. This build targets the stock updated executable.
Project Restoration is not bundled; compatibility with its update-specific
patch has not been verified. Do not assume the old EU patch fits this update.

Features from the working v44 baseline: display modes, 3D strength, Look
speed, saved preferences, touch, Circle Pad/D-pad, page wrapping and START
Options during gameplay. Includes the native B button and corrected heading
orange. No CPU screen filters, no depth of field, no 60 FPS patch.

Settings are separate from the game save, under this USA title folder as
display-settings-a.bin / display-settings-b.bin. No setting file is bundled.
USA-specific code, data/global addresses, native vtables, Options object,
settings path and bootstrap have been remapped. Rendering backing sizes
and the v44 gameplay behavior are retained.

Local guards/tests are not a console test. Back up the current three mod
files before replacing them. To remove this test, remove those three files
or restore your previous copies. Keep your game save.
''')
 dest=Path('/home/r/Downloads/mm3d-display-menu-usa-1.1-v44-test1.zip')
 with zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED) as z:
  for p in sorted(out.rglob('*')):
   if p.is_file():z.write(p,p.relative_to(out))
 with zipfile.ZipFile(dest) as z:assert z.testzip() is None
 print(dest)
if __name__=='__main__':main()
