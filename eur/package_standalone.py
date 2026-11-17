"""Package the v44 stock-game install without reading Project Restoration inputs."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib,json
from build_native import patched
root=Path(__file__).resolve().parent
source=Path('/home/r/Downloads/0004000000125600.dec.code').read_bytes()
assert hashlib.sha256(source).hexdigest()=='050a58834365187ca67211eef0a29816715e9ab9f7289e8372b06fb906620580'
with ZipFile('/home/r/Downloads/mm3d-display-menu-v44.zip') as old:
    files={n:old.read(n) for n in old.namelist() if n.startswith('luma/') or n=='tab-preview.png'}
    metadata=json.loads(old.read('manifest.json'))
patch=next(v for k,v in files.items() if k.endswith('/code.ips'))
result=patched(source,patch,metadata['mapped_code_bytes'])
files['README.txt']=b'''MM3D EU display options v44 - standalone stock-game package

Project Restoration is not required or bundled. The executable patch is
identical to v44; this is a standalone installation package, not a new engine.
Includes touch support, D-pad/Circle Pad row navigation through Page 3, and the single-pixel numeral join.
Tap rows to select, arrows to change, and the Display value to apply.
Tap Page 1/2/3 tabs to switch pages. Held touches do not repeat. Page3 also supports the native B/Cancel touch target
and uses the original Options heading orange (#FFBE00). Rows now align with
the native page positions and spacing during transitions.

Use the supported stock EU game/update (title 0004000000125600).
Merge luma/ onto the SD card, replacing the three included files.

If Project Restoration was installed previously, disable/remove its code.bps
and any other files installed by Restoration before testing this standalone
setup. Copying this ZIP alone does not uninstall an existing mod.
Keep your game save and display-settings-a.bin / display-settings-b.bin.

Our code still uses Nintendo's native game functions, UI assets and animations.
The supplied Options GAR contains modified original game layout data.

Stock executable patch application verified locally. Console testing of this
standalone installation is still required. Older ZIPs remain available.
'''
files['manifest.json']=(json.dumps({'package':'v44 standalone stock-game install','title_id':'0004000000125600','project_restoration_required':False,'project_restoration_bundled':False,'code_identical_to_v44':True,'stock_patch_application_verified':True,'stock_patched_sha256':hashlib.sha256(result).hexdigest(),'files':{n:hashlib.sha256(v).hexdigest() for n,v in files.items()}},indent=2)+'\n').encode()
out=Path('/home/r/Downloads/mm3d-display-menu-v44-standalone.zip')
with ZipFile(out,'w',ZIP_DEFLATED) as z:
    for n,v in files.items():z.writestr(n,v)
with ZipFile(out) as z:
    assert z.testzip() is None
    assert not any(n.endswith('code.bps') for n in z.namelist())
print(out)
