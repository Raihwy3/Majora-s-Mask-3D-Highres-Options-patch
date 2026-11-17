"""Audit and package the working USA test2 overlay alongside supplied Restoration v100."""
from pathlib import Path
from zipfile import ZipFile,ZIP_DEFLATED
import hashlib,json,struct,sys,tarfile,types
from audit_baseline import records
from build_native import patched
from inspect_inputs import apply_bps
ROOT=Path(__file__).resolve().parent;TITLE='0004000000125500'
base=Path('/home/r/Downloads/scrn/us ver/0004000000125500.dec.code').read_bytes()
bps=Path('/home/r/Downloads/code.bps').read_bytes()
assert hashlib.sha256(bps).hexdigest()=='876983e8c1a4527040f5a71e99dcc1279ba0c43694a68c08b8aaa70cd09e8940'
restoration=apply_bps(base,bps)
with ZipFile('/home/r/Downloads/mm3d-display-menu-usa-v44-test2.zip') as z:
 files={n:z.read(n) for n in z.namelist() if n.startswith('luma/')}
patch=files[f'luma/titles/{TITLE}/code.ips'];header=files[f'luma/titles/{TITLE}/exheader.bin']
capacity=struct.unpack_from('<I',header,0x30)[0]+struct.unpack_from('<I',header,0x38)[0]-0x100000
combined=patched(restoration,patch,capacity)
for off,count in [(0x51cd90,0x150),(0x49ba10,8),(0x676c,4),(0x16a30,4)]:
 assert combined[off:off+count]==restoration[off:off+count],hex(off)
assert combined[0x51cecc:0x51ced0]==struct.pack('<I',0xe92d5fff)
assert combined[0x51cf44:0x51cf48]==bytes(4)
with ZipFile('/home/r/Downloads/mm3d-display-menu-v44.zip') as z:
 eu=json.loads(z.read('manifest.json'))
 eu_header=z.read('luma/titles/0004000000125600/exheader.bin')
 eu_patch=z.read('luma/titles/0004000000125600/code.ips')
for h in eu['hooks']:
 off=int(h['offset'],16);count=h['bytes_replaced']
 assert base[off:off+count]==restoration[off:off+count],hex(off)
for lo,hi in [(0x2e904,0x2e980),(0x1006e4,0x1006f4),(0x92008,0x92018),(0x513b8,0x513c8),(0x69e50,0x69e60),(0x4a1e2c,0x4a2018)]:
 assert base[lo:hi]==restoration[lo:hi]
assert combined[len(base):len(restoration)]==restoration[len(base):]
old=patched(restoration,eu_patch,capacity)
assert combined==old.replace(b'0004000000125600',TITLE.encode())
def svc_caps(h):return {v for (v,) in struct.iter_unpack('<I',h[0x370:0x3e0]) if v!=0xffffffff}
assert svc_caps(header)==svc_caps(eu_header)
assert header[0x208:0x370]==eu_header[0x208:0x370]
assert header[0x3e0:0x400]==eu_header[0x3e0:0x400]
assert header[0x10:0x40]==eu_header[0x10:0x40]
with tarfile.open('/home/r/mm3d-display-options/build/v44-source-before-filters.tar.gz') as t:
 source=t.extractfile('tests/test_bootstrap.py').read().decode()
module=types.ModuleType('loader_check');module.__file__=str(ROOT/'tests/test_bootstrap.py')
exec(compile(source,module.__file__,'exec'),module.__dict__)
boot=next(data for off,data in records(patch) if off==0x51cee0)
def w(off):return struct.unpack_from('<I',boot,off)[0]
def literal(off):return w(off+8+(w(off)&0xfff))
def destination(off):
 value=w(off)&0xffffff
 if value&0x800000:value-=0x1000000
 return 0x61cee0+off+8+value*4
symbols={'mm_bootstrap':0x61cee0,'mm_native_init':destination(0x70),'__module_start':literal(0x30),'__module_rx_size':literal(0x38),'mm_restoration_start':0x61cecc,'mm_original_clear_bss':0x100024,'mm_original_entry_continue':0x100004}
svcs=module.libctru_svcs()
for restoration_on in (False,True):
 for failure in (None,'pid','open','protect'):
  cpu=module.Cpu(boot,symbols,svcs,restoration_on,failure);cpu.run()
  assert cpu.ended==('exit' if failure else 'restoration' if restoration_on else 'stock')
report={'version':'USA v44 Restoration-compatible','based_on':'console-launched USA test2; installed files byte-identical','supported_executable_sha256':hashlib.sha256(base).hexdigest(),'restoration_variant':'supplied Project Restoration v100 BPS','restoration_bps_sha256':hashlib.sha256(bps).hexdigest(),'restoration_implementation_bundled':False,'existing_restoration_hooks_and_added_code_preserved':True,'packaged_loader_stock_and_restoration_paths_emulated':True,'header_services_permissions_and_memory_match_compatible_eu_build':True,'combined_console_tested':False,'files':{n:hashlib.sha256(v).hexdigest() for n,v in files.items()}}
files['manifest.json']=(json.dumps(report,indent=2)+'\n').encode()
files['README.txt']=b'''MM3D USA v44 - Project Restoration compatible

Use alongside your existing Project Restoration v100 installation for
USA title 0004000000125500, with the base executable matched by USA test2.
Merge luma/ onto the SD card and replace these three files together:
  code.ips
  exheader.bin
  romfs/layout/Joker.Main.Play.Option.gar
Keep Restoration's code.bps and its other files installed. This ZIP does
not bundle Project Restoration. Keep your game save and display settings.
Do not use the earlier update-specific test1 with this executable.

The installed files are byte-identical to the working USA test2. No further
code changes were needed. The supplied Restoration v100 BPS is checked
against the USA source, all patched instruction sites, native APIs, header
permissions/services and memory allocation. Its startup shim, entry,
gameplay-input and notebook hooks and added implementation remain intact.
The actual packaged loader is locally emulated through stock/Restoration
continuations and kernel failures. The combined installation still needs
a console test. A different Restoration build/revision is not verified.

Includes v44 display modes, 3D strength, Look speed, touch/Circle Pad/D-pad,
saved preferences, START Options and page wrap. No CPU screen filters.
'''
out=Path('/home/r/Downloads/mm3d-display-menu-usa-v44-restoration-compatible.zip')
with ZipFile(out,'w',ZIP_DEFLATED) as z:
 for n,v in files.items():z.writestr(n,v)
with ZipFile(out) as z:assert z.testzip() is None
(ROOT/'build/usa-restoration-audit.json').write_text(json.dumps(report,indent=2)+'\n')
print(out)
print('PASS: all hook/API guards, preserved Restoration additions/entry/input/notebook, equivalent header, packaged loader stock/Restoration/failure paths; installation files unchanged from test2.')
