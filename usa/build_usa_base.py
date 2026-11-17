"""USA revision confirmed by dump62: same executable as the tested EU v44 input."""
from pathlib import Path
from zipfile import ZipFile,ZIP_DEFLATED
import struct,hashlib,json,sys
from audit_baseline import records
from build_native import make_ips,patched
sys.path.insert(0,'/home/r/mm3d-display-options')
from inspect_crash import inspect
ROOT=Path(__file__).resolve().parent
INPUT=Path('/home/r/Downloads/scrn/us ver')
base=(INPUT/'0004000000125500.dec.code').read_bytes()
assert hashlib.sha256(base).hexdigest()=='050a58834365187ca67211eef0a29816715e9ab9f7289e8372b06fb906620580'
TITLE='0004000000125500';EU='0004000000125600'
with ZipFile('/home/r/Downloads/mm3d-display-menu-v44.zip') as z:
 old_patch=z.read(f'luma/titles/{EU}/code.ips')
 old_header=z.read(f'luma/titles/{EU}/exheader.bin')
 layout=z.read(f'luma/titles/{EU}/romfs/layout/Joker.Main.Play.Option.gar')
spans=[];changed=0
for off,payload in records(old_patch):
 changed+=payload.count(EU.encode());spans.append((off,payload.replace(EU.encode(),TITLE.encode())))
assert changed==2
patch=make_ips(spans)
header=bytearray((INPUT/'extheader.bin').read_bytes())
assert struct.unpack_from('<Q',header,0x200)[0]==int(TITLE,16)
header[0x10:0x40]=old_header[0x10:0x40]
header[0x248]|=0x80
struct.pack_into('<I',header,0x3f0,struct.unpack_from('<I',header,0x3f0)[0]|(1<<9))
for svc in [3,0x23,0x32,0x33,0x35,0x70]:
 for off in range(0x370,0x3e0,4):
  value=struct.unpack_from('<I',header,off)[0]
  if value>>27==30 and (value>>24)&7==svc//24:
   struct.pack_into('<I',header,off,value|(1<<(svc%24)));break
 else:
  off=next(o for o in range(0x370,0x3e0,4) if struct.unpack_from('<I',header,o)[0]==0xffffffff)
  struct.pack_into('<I',header,off,0xf0000000|((svc//24)<<24)|(1<<(svc%24)))
capacity=struct.unpack_from('<I',header,0x30)[0]+struct.unpack_from('<I',header,0x38)[0]-0x100000
result=patched(base,patch,capacity);old=patched(base,old_patch,capacity)
assert result==old.replace(EU.encode(),TITLE.encode())
dump=Path('/home/r/Downloads/scrn/crash_dump_00000062.dmp')
b=dump.read_bytes();_,_,_,_,_,_,regsize,codesize,_,_=struct.unpack_from('<10I',b)
pc=struct.unpack_from('<I',b,40+15*4)[0]
captured=b[40+regsize:40+regsize+codesize];start=pc-(codesize-4)-0x100000
wrong=patched(base,(ROOT/'build/usa-1.1-v44/luma/titles'/TITLE/'code.ips').read_bytes(),0x900000)
assert wrong[start:start+codesize]==captured
assert struct.unpack_from('<I',base,0x25c68)[0]==0xe2804010
assert struct.unpack_from('<I',wrong,0x25c68)[0]>>24==0xea
assert result[0x25c58:0x25c90]==base[0x25c58:0x25c90]
assert (INPUT/'Joker.Main.Play.Option.gar').read_bytes()==Path('/home/r/Downloads/scrn/Joker.Main.Play.Option.gar').read_bytes()
from inspect_inputs import apply_bps
bps=Path('/home/r/Downloads/code.bps')
restoration_checked=False
if bps.exists():
 restored=apply_bps(base,bps.read_bytes())
 r=patched(restored,patch,capacity);prior=patched(restored,old_patch,capacity)
 assert r==prior.replace(EU.encode(),TITLE.encode())
 restoration_checked=True
out=ROOT/'build/usa-base-v44-test2';title=out/'luma/titles'/TITLE;title.mkdir(parents=True,exist_ok=True)
files={f'luma/titles/{TITLE}/code.ips':patch,f'luma/titles/{TITLE}/exheader.bin':bytes(header),f'luma/titles/{TITLE}/romfs/layout/Joker.Main.Play.Option.gar':layout}
report={'revision':'USA base executable, matched by crash62','baseline':'working EU v44, byte-identical game executable','title_id':TITLE,'source_sha256':hashlib.sha256(base).hexdigest(),'wrong_revision_crash_reproduced_from_dump':True,'base_constructor_restored':True,'only_native_payload_changes':'two USA settings paths','supplied_restoration_v100_bps_checked':restoration_checked,'hardware_tested':False,'files':{n:hashlib.sha256(v).hexdigest() for n,v in files.items()}}
files['manifest.json']=(json.dumps(report,indent=2)+'\n').encode()
files['README.txt']=b'''MM3D USA v44 - executable revision confirmed by crash62 (test2)

This replaces the incorrect update-revision test1 for the executable shown
in your crash dump. It targets the supplied USA BASE code, SHA256
050a58834365187ca67211eef0a29816715e9ab9f7289e8372b06fb906620580.
The supplied update code has different addresses; do not mix these builds.

Replace ALL THREE files together by merging luma/ onto the SD card:
  luma/titles/0004000000125500/code.ips
  luma/titles/0004000000125500/exheader.bin
  luma/titles/0004000000125500/romfs/layout/Joker.Main.Play.Option.gar
Enable Luma game patching. Keep your game save and display-settings files.

Same features as working EU v44, no CPU filters. The native executable is
byte-identical to v44's original input. Its patch payload changes only the
two saved-setting paths to the USA title folder. The header preserves USA
identity/services with v44's expanded memory allocation and required SVCs.
The supplied Restoration v100 BPS is compatibility-checked, not bundled.

The first USA test crashed because an update-only rendering hook landed on
the base constructor's ADD r4,r0,#16 at125c68. Its captured code bytes match
base+test1 exactly. This build leaves that constructor unchanged.
Local byte/patch and header checks pass; actual console boot remains untested.
To revert, restore the previous three files or remove this mod's three files.
'''
for n,v in files.items():
 p=out/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(v)
dest=Path('/home/r/Downloads/mm3d-display-menu-usa-v44-test2.zip')
with ZipFile(dest,'w',ZIP_DEFLATED) as z:
 for n,v in files.items():z.writestr(n,v)
with ZipFile(dest) as z:assert z.testzip() is None
print(dest)
print(json.dumps(report,indent=2))
