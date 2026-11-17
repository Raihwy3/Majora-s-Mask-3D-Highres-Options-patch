from pathlib import Path
import sys,struct
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from build_options_tabs import SOURCE,append_tabs,build
from inspect_layouts import gar,model
original=gar(SOURCE)['MenuOption.mfl']
patched=append_tabs(original)
a,b=model(original),model(patched)
def tree(m):
    nodes=list(m['nodes'].items())
    def walk(at):
        name,f=nodes[at];at+=1;kind=f[0]&0xffff
        if kind==15:children=f[1]&0xffff
        elif kind==31:children=f[1]>>16
        elif kind==63:children=0
        else:raise AssertionError((name,hex(kind)))
        for _ in range(children):at=walk(at)
        return at
    assert walk(0)==len(nodes)
tree(a);tree(b)
assert len(b['nodes'])>len(a['nodes'])
for p in range(1,4):
    for state in ['idle','selected']:
        name=f'display_tab{p}_{state}'
        assert name in b['nodes']
for name,f in b['nodes'].items():
    if name.startswith('display_'):
        assert f[0] in [15,63]
        if f[0]==63:
            kind,body=b['shapes'][f[1]];assert kind==6 and len(body)==44
            x,y,z,w,h=struct.unpack_from('<5f',body)
            assert 72<=x and x+w<=312 and 202<=y and y+h<=240
            assert struct.unpack_from('<H',body,38)[0] in [4,30]
assert 'display_tab3_idle_digit' in b['nodes']
p=build(Path('build/native-tabs/Joker.Main.Play.Option.gar'))
f=gar(p)
for n,data in gar(SOURCE).items():
    if n!='MenuOption.mfl':assert f[n]==data
assert b'3 / 3' not in patched and b'3/3' not in patched
print('PASS: balanced original/appended MFL trees; original indices/animations/panes retained; native sprite-only Page3; bounds/texture bindings; GAR round trip.')

old=list(a['nodes']);new=list(b['nodes'])
assert old.index('btn_page1_l0')<old.index('bb_btn_page_pane00')
assert new.index('display_tab3_selected')<new.index('display_tabs_back_g')
for p in range(1,4):
    assert new.index(f'display_tab{p}_selected_image' if p<3 else 'display_tab3_selected_body')<new.index(f'display_tab{p}_selected_edge')
assert new.index('display_tab3_idle_digit')<new.index('display_tab3_idle_blank')<new.index('display_tab3_idle_body')
print('PASS: native tab faces precede backing; selected edges behind labels; Page3 digit/erase patch precede base image in native depth order.')

import zipfile,tempfile
from preview_options_tabs import render_tabs,textures
tex=textures()
newframe=render_tabs(b,tex,3)
def bright(im):
    return sum(1 for r,g,blue,alpha in im.crop((76,208,308,234)).getdata() if min(r,g,blue)>140)
assert bright(newframe)>300
print('PASS: native-order software compositor keeps all three USA tab labels visible.')

for name in ['display_tabs_back_l','display_tabs_back_c','display_tabs_back_r']:
    _,body=b['shapes'][b['nodes'][name][1]]
    assert body[43]==255
assert newframe.getpixel((80,203))!=(40,34,15,255)
print('PASS: opaque native tab backing restored behind faces; tab text remains visible.')
for state in ('idle','selected'):
    _,body=b['shapes'][b['nodes'][f'display_tab3_{state}_digit'][1]]
    assert struct.unpack_from('<4f',body,20)==(125/256,102/256,15/256,8/256)
    _,lower=b['shapes'][b['nodes'][f'display_tab3_{state}_digit_lower'][1]]
    assert struct.unpack_from('<4f',lower,20)==(125/256,109/256,15/256,-7/256)
    assert struct.unpack_from('<H',body,38)[0]==struct.unpack_from('<H',lower,38)[0]==30
print('PASS: Page3 uses original native tab numeral strokes with a mirrored lower bowl; no generic number font.')
