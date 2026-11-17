#!/usr/bin/env python3
"""Inspect actual supplied native layouts; no game assets are redistributed."""
from pathlib import Path
import struct
from hashlib import sha256

def gar(path):
    data=path.read_bytes()
    assert data[:4]==b'GAR\x02' and struct.unpack_from('<I',data,4)[0]==len(data)
    count=struct.unpack_from('<H',data,10)[0]
    table,pointers=struct.unpack_from('<II',data,16)
    files={}
    for i in range(count):
        size,short,name=struct.unpack_from('<III',data,table+i*12)
        offset=struct.unpack_from('<I',data,pointers+i*4)[0]
        assert offset+size<=len(data)
        files[data[name:data.index(0,name)].decode()]=data[offset:offset+size]
    return files

def model(data):
    assert data[:8]==b'MFL '+struct.pack('<I',4)
    cls,count,parameters,panes,players,animations=struct.unpack_from('<6H',data,8)
    pane_start,anim_start,names_start=struct.unpack_from('<3I',data,20)
    assert pane_start==32+count*68
    assert pane_start<=anim_start<=names_start<len(data)
    at=names_start
    def string():
        nonlocal at
        end=data.index(0,at);value=data[at:end].decode();at=end+1;return value
    name=string()
    param_names=[string() for _ in range(parameters)]
    pane_names=[string() for _ in range(panes)]
    widget_names=[string() for _ in range(count)]
    nodes={name:struct.unpack_from('<17I',data,32+i*68) for i,name in enumerate(widget_names)}
    shapes=[];at=pane_start
    while at<anim_start:
        kind,size=struct.unpack_from('<HH',data,at)
        assert at+size+4<=anim_start
        shapes.append((kind,data[at+4:at+4+size]));at+=size+4
    assert len(shapes)==panes
    return dict(name=name,cls=cls,parameters=param_names,nodes=nodes,shapes=shapes)

def check():
    main=gar(Path('/home/r/Downloads/RomFS/layout/Joker.Main.gar'))
    edge=model(main['EdgeText.mfl']);window=model(main['MessageWindow.mfl'])
    assert edge['parameters']==['text'] and edge['nodes']['text'][0]==0x2f
    assert window['parameters']==[]
    assert {'root','all','front','plate','text','mainText','e_text'}<=window['nodes'].keys()
    assert window['nodes']['e_text'][0]==0x1f
    for locale in ['e_text_us_en','e_text_eu_en']:
        fields=window['nodes'][locale]
        assert fields[0]==0x3f
        kind,body=window['shapes'][fields[1]]
        assert kind==3 and struct.unpack_from('<5f',body)==(-130.0,-39.0,-0.0,260.0,78.0)
    plate=window['nodes']['e_plate_eu_en']
    kind,body=window['shapes'][plate[1]]
    assert kind==6 and struct.unpack_from('<5f',body)==(-152.0,-42.0,-0.0,304.0,84.0)
    base=Path('/home/r/Downloads/0004000000125600.dec.code').read_bytes()
    assert struct.unpack_from('<I',base,0xff6c)[0]==0xe28f1f93
    assert base[0x101c0:].startswith(b'Joker.Main.MessageWindow\0')
    assert struct.unpack_from('<I',base,0xff84)[0]==0xe3a02803
    assert struct.unpack_from('<I',base,0xff8c)[0]==0xeb0165a6
    return {'message_window_mfl_sha256':sha256(main['MessageWindow.mfl']).hexdigest(),
            'message_window_external_parameters':0,'edge_text_requires_external_text_parameter':True,
            'english_font_rectangle':[-130,-39,260,78],
            'native_equivalent_creation_call':'0x10ff8c'}
if __name__=='__main__':
    import json
    print(json.dumps(check(),indent=2))
