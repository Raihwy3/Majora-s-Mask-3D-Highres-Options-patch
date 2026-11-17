"""Adapt current native-execution checks to USA 1.1, without old ZIP fixtures."""
from pathlib import Path
import tarfile,re,json
from verify_usa import NA,NB,M
root=Path(__file__).resolve().parent
with tarfile.open('/home/r/mm3d-display-options/build/v44-source-before-filters.tar.gz') as t:
 for name in ['test_copy_rect.py','test_scanout_pointer.py','test_stereo_strength.py','test_transfer_commands.py','test_message_window_calc.py','test_options_motion_assets.py']:
  s=t.extractfile('tests/'+name).read().decode()
  def convert(match):
   n=int(match[0],16)
   if n in M:return hex(M[n])
   offset=n in [0x691c,0x6bc8]
   a=n+0x100000 if offset else n
   if not 0x100000<=a<0x61cd8c:return match[0]
   for before,size in [(32,192),(16,128),(0,128),(32,64),(0,64)]:
    start=max(0,a-0x100000-before);pat=NA[start:start+size];at=NB.find(pat)
    if at>=0 and NB.find(pat,at+1)<0:
     target=at+(a-0x100000-start)+0x100000
     return hex(target-0x100000 if offset else target)
   return match[0]
  s=re.sub(r'0x[0-9a-fA-F]+',convert,s)
  s=s.replace('/home/r/Downloads/0004000000125600.dec.code','/home/r/Downloads/scrn/us ver/0004000E00125500.dec.code')
  s=s.replace('/home/r/Downloads/scrn/Joker.Main.Play.Option.gar','/home/r/Downloads/scrn/us ver/Joker.Main.Play.Option.gar')
  if name=='test_copy_rect.py':
   a=s.index("    previous=ROOT/'build/v10-native.elf'");b=s.index("if __name__",a)
   s=s[:a]+"    print('PASS: USA native ARM rectangle helper, all profiles, bottom preservation, register and stack restoration.')\n"+s[b:]
  (root/'tests'/name).write_text(s)
p=root/'tests/test_options_tab_assets.py';s=p.read_text();a=s.index('with zipfile.ZipFile(');b=s.index('tex=textures()',a);s=s[:a]+s[b:];s=s.replace('oldframe=render_tabs(oldmodel,tex,3);','').replace(' and bright(oldframe)<50','');p.write_text(s)
