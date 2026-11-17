#!/usr/bin/env python3
"""Software preview of the appended native Sprite6 tab panes, not a game capture."""
from pathlib import Path
from PIL import Image,ImageDraw
import struct,tempfile
from inspect_layouts import gar,model
from inspect_texture import decode
from build_options_tabs import SOURCE,MAIN,build

def textures():
    f=gar(MAIN);a=gar(Path('/home/r/Downloads/RomFS/layout/EU_English/Joker.Main.Play.Option.gar'))
    with tempfile.TemporaryDirectory() as d:
        p=Path(d)/'atlas.ctxb';q=Path(d)/'atlas.png';p.write_bytes(a['menu_option_parts00.ctxb']);decode(p,q);tabs=Image.open(q).copy()
    b=f['num_all00.ctxb'];w,h=struct.unpack_from('<HH',b,44);offset=struct.unpack_from('<I',b,20)[0]
    numbers=Image.new('RGBA',(w,h))
    for y in range(h):
        for x in range(w):
            k=((y//8)*(w//8)+x//8)*64+sum((((x>>i)&1)<<(2*i))|(((y>>i)&1)<<(2*i+1)) for i in range(3))
            c=b[offset+k];v=(c>>4)*17;numbers.putpixel((x,y),(v,v,v,(c&15)*17))
    return {4:numbers,30:tabs}

def render_tabs(m,tex,active):
    frame=Image.new('RGBA',(320,240),(40,34,15,255));enabled=True
    sprites=[]
    for name,f in m['nodes'].items():
        if name.startswith('display_tab') and (name.endswith('_idle') or name.endswith('_selected')):
            page=int(name[len('display_tab')]);enabled=(page==active)==name.endswith('_selected')
        if not name.startswith('display_') or f[0]!=63 or (not enabled and not name.startswith('display_tabs_back')):continue
        sprites.append(f)
    for f in reversed(sprites):
        kind,b=m['shapes'][f[1]];assert kind==6
        x,y,z,w,h,u,v,uw,vh=struct.unpack_from('<9f',b);t=tex[struct.unpack_from('<H',b,38)[0]];tw,th=t.size
        left,top=round(u*tw),round(v*th);sw,sh=round(uw*tw),round(vh*th)
        flip_y=sh<0
        if flip_y:top+=sh;sh=-sh
        if sw==0: crop=t.crop((left,top,left+1,top+sh))
        elif sw<0:crop=t.crop((left+sw,top,left,top+sh)).transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        else:crop=t.crop((left,top,left+sw,top+sh))
        if flip_y:crop=crop.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
        tile=crop.resize((round(w),round(h)),Image.Resampling.NEAREST)
        alpha=b[43]
        if alpha!=255:tile.putalpha(tile.getchannel('A').point(lambda v:v*alpha//255))
        frame.alpha_composite(tile,(round(x),round(y)))
    return frame

def preview():
    p=build(Path('build/native-tabs/Joker.Main.Play.Option.gar'));m=model(gar(p)['MenuOption.mfl']);tex=textures()
    out=Image.new('RGB',(640,360),(40,34,15));draw=ImageDraw.Draw(out)
    for active in range(1,4):
        frame=render_tabs(m,tex,active)
        out.paste(frame.crop((0,196,320,240)).resize((640,88),Image.Resampling.NEAREST),(0,(active-1)*120+24))
        draw.text((4,(active-1)*120+4),f'Native Sprite6 asset preview: Page {active} selected')
    out.save('build/native-tabs/tab-preview.png')
    return out
if __name__=='__main__':preview()
