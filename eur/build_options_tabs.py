#!/usr/bin/env python3
"""Extend the supplied MenuOption with three native sprite tabs.

No texture is generated or edited. The third label reuses the original
Page 2 lettering and curved numeral strokes, mirrored into a matching 3.
Original widget/pane/animation indices are preserved by appending entries.
"""
from pathlib import Path
import struct
from inspect_layouts import gar, model

SOURCE=Path('/home/r/Downloads/scrn/Joker.Main.Play.Option.gar')
MAIN=Path('/home/r/Downloads/RomFS/layout/Joker.Main.gar')

def append_tabs(data):
    m=model(data)
    cls,count,params,panes,players,anims=struct.unpack_from('<6H',data,8)
    pane_at,anim_at,names_at=struct.unpack_from('<3I',data,20)
    nodes=bytearray(data[32:pane_at]); shape_bytes=bytearray(data[pane_at:anim_at])
    extra_nodes=[];extra_shapes=[];node_names=[];pane_names=[]
    root=list(m['nodes']['root']);all_=list(m['nodes']['all'])
    assert root[:2]==[15,1] and all_[:2]==[15,6]
    all_[1]+=1;struct.pack_into('<17I',nodes,68,*all_)
    template=m['nodes']['all']
    def node(name,flags,arg):
        f=list(template);f[:2]=flags,arg
        extra_nodes.append(struct.pack('<17I',*f));node_names.append(name)
    def group(name,children):node(name,15,children)
    def sprite(name,rect,uv,texture=30,alpha=255):
        i=panes+len(extra_shapes)
        body=struct.pack('<9fHH4B',*rect[:2],-0.0,*rect[2:],*uv,0,texture,255,255,255,alpha)
        assert len(body)==44
        extra_shapes.append(struct.pack('<HH',6,len(body))+body);pane_names.append(name+'_pane')
        node(name,63,i)
    group('display_tabs_g',4)
    for page,x in enumerate((74,154,234),1):
        group(f'display_tab{page}_g',2)
        group(f'display_tab{page}_idle',1 if page<3 else 4)
        def label(suffix,y):
            if page<3:
                sprite(f'display_tab{page}_{suffix}_image',(x,y,76,32),((page-1)*76/256,96/256,76/256,32/256))
            else:
                sprite(f'display_tab3_{suffix}_digit',(x+49,y+6,15,8),(125/256,102/256,15/256,8/256))
                sprite(f'display_tab3_{suffix}_digit_lower',(x+49,y+14,15,7),(125/256,109/256,15/256,-7/256))
                sprite(f'display_tab3_{suffix}_blank',(x+49,y+6,15,18),(84/256,102/256,8/256,18/256))
                sprite(f'display_tab3_{suffix}_body',(x,y,76,32),(76/256,96/256,76/256,32/256))
        label('idle',208)
        group(f'display_tab{page}_selected',2 if page<3 else 5)
        label('selected',204)
        sprite(f'display_tab{page}_selected_edge',(x,204,76,36),(0,132/256,76/256,32/256))
    group('display_tabs_back_g',3)
    sprite('display_tabs_back_l',(72,202,8,38),(152/256,84/256,8/256,38/256),alpha=255)
    sprite('display_tabs_back_c',(80,202,224,38),(160/256,84/256,0,38/256),alpha=255)
    sprite('display_tabs_back_r',(304,202,8,38),(160/256,84/256,-8/256,38/256),alpha=255)
    at=names_at
    def strings(n):
        nonlocal at
        out=[]
        for _ in range(n):
            end=data.index(0,at);out.append(data[at:end+1]);at=end+1
        return b''.join(out)
    prefix=strings(1+params);old_panes=strings(panes);old_nodes=strings(count);tail=data[at:]
    enc=lambda names:b''.join(n.encode()+b'\0' for n in names)
    new_nodes=bytes(nodes)+b''.join(extra_nodes)
    new_shapes=bytes(shape_bytes)+b''.join(extra_shapes)
    animation=data[anim_at:names_at]
    header=bytearray(data[:32])
    struct.pack_into('<6H3I',header,8,cls,count+len(extra_nodes),params,panes+len(extra_shapes),players,anims,
                     32+len(new_nodes),32+len(new_nodes)+len(new_shapes),32+len(new_nodes)+len(new_shapes)+len(animation))
    result=bytes(header)+new_nodes+new_shapes+animation+prefix+old_panes+enc(pane_names)+old_nodes+enc(node_names)+tail
    validate(data,result)
    return result

def validate(original,patched):
    a,b=model(original),model(patched)
    for n,f in a['nodes'].items():
        if n=='all':assert b['nodes'][n][1]==f[1]+1
        else:assert b['nodes'][n]==f
    assert b['shapes'][:len(a['shapes'])]==a['shapes']
    aa,an=struct.unpack_from('<II',original,24);ba,bn=struct.unpack_from('<II',patched,24)
    assert original[aa:an]==patched[ba:bn]
    assert b['parameters']==a['parameters']==[]
    for p in range(1,4):
        assert f'display_tab{p}_idle' in b['nodes'] and f'display_tab{p}_selected' in b['nodes']
    assert b'\x00\x00\x04\x00num_all00.ctxb\0' in gar(MAIN)['Main.mfpk']
    assert b'\x00\x00\x1e\x80menu_option_parts00.ctxb\0' in gar(SOURCE)['Option.mfpk']

def replace_gar(source,files):
    count=struct.unpack_from('<H',source,10)[0];table,pointers=struct.unpack_from('<II',source,16)
    offsets=[struct.unpack_from('<I',source,pointers+4*i)[0] for i in range(count)]
    result=bytearray(source[:min(offsets)])
    for i in range(count):
        name_at=struct.unpack_from('<I',source,table+12*i+8)[0]
        name=source[name_at:source.index(0,name_at)].decode()
        result.extend(bytes((-len(result))%4));offset=len(result);payload=files[name]
        result.extend(payload);struct.pack_into('<I',result,table+12*i,len(payload));struct.pack_into('<I',result,pointers+4*i,offset)
    struct.pack_into('<I',result,4,len(result))
    return bytes(result)

def build(destination):
    files=gar(SOURCE);files['MenuOption.mfl']=append_tabs(files['MenuOption.mfl'])
    destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_bytes(replace_gar(SOURCE.read_bytes(),files))
    assert gar(destination)==files
    return destination
if __name__=='__main__':
    print(build(Path('build/native-tabs/Joker.Main.Play.Option.gar')))
