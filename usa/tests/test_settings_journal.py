from pathlib import Path
import subprocess,tempfile,struct,zlib
root=Path(__file__).resolve().parents[1]
exe=root/'build/test_settings_io'
with tempfile.TemporaryDirectory(prefix='mm-settings-') as d:
 def run(phase):subprocess.run([str(exe),d,phase],check=True)
 run('write');run('load');run('delayed')
 p=Path(d)/'display-settings-b.bin';b=bytearray(p.read_bytes());b[9]^=1;p.write_bytes(b)
 run('fallback')
 for slot,sequence,mode,strength,sens in [('a',255,0,3,2),('b',0,4,6,5)]:
  b=bytearray(b'MMGD\x02\x00\x10\x00'+bytes([mode,strength,sens,sequence]))
  b+=struct.pack('<I',zlib.crc32(b));(Path(d)/f'display-settings-{slot}.bin').write_bytes(b)
 run('wrap');run('load')
print('PASS: actual restart round-trip, torn write, corrupt record, generation wrap and GPU readiness')
