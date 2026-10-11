"""Native skin with photographed-style hardware materials and a shared layout system."""
import copy
import json
import math
from functools import lru_cache
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
BOLD = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
ASSETS = Path(__file__).resolve().parents[1]/'skin/assets'
WIDTH, HEIGHT, PAD_SIZE, KNOB_SIZE = 224, 72, 104, 64
NAME_X, NAME_Y, VALUE_Y = 80, 0, 30
LABEL, INK, ACCENT = '29271f', '171610', 'a56f40'


def font(size, bold=False):
    return ImageFont.truetype(BOLD if bold else FONT, size)


def gradient(size, top, bottom):
    image = Image.new('RGBA', size)
    draw = ImageDraw.Draw(image)
    for y in range(size[1]):
        t = y/max(1, size[1]-1)
        colour = tuple(round(a+(b-a)*t) for a,b in zip(top,bottom))+(255,)
        draw.line((0,y,size[0],y),fill=colour)
    return image


def card(draw, box):
    x,y,w,h = box
    draw.rounded_rectangle((x,y,x+w-1,y+h-1),radius=5,fill='#bfb9a5',outline='#918b76')
    draw.line((x+6,y+1,x+w-7,y+1),fill='#aaa38d')
    draw.line((x+6,y+h-1,x+w-7,y+h-1),fill='#d4cfba')


def chassis():
    image=gradient((1280,628),(183,178,155),(167,161,140))
    pixels=image.load()
    # Restrained uneven paint patina, strongest near edges and hand-rest areas.
    stains=((65,550,105,40,7),(1230,552,105,48,8),(880,22,145,18,3),
            (294,488,38,94,3),(965,420,24,92,3))
    for y in range(628):
        for x in range(1280):
            grain=(((x*374761393+y*668265263)^((x+y)*1274126177))&255)/255-.5
            dirt=sum(power*math.exp(-((x-cx)/rx)**2-((y-cy)/ry)**2) for cx,cy,rx,ry,power in stains)
            panel=3 if 76<=y<=584 and (8<=x<=264 or 1016<=x<=1272) else 0
            edge=2.0*math.exp(-min(x,1279-x,y,627-y)/7.0)
            base=pixels[x,y]
            pixels[x,y]=tuple(max(0,min(255,round(base[k]+grain*2.1-dirt*(.8,1,1.3)[k]-panel-edge))) for k in range(3))+(255,)
    return image


@lru_cache(maxsize=8)
def material(name, size):
    # Runtime asset sizing leaves the original generated source untouched.
    return Image.open(ASSETS/name).convert('RGBA').resize((size,size),Image.Resampling.LANCZOS)


def pad_art(number, active):
    image = material('pad-rubber-v1.png',PAD_SIZE).copy()
    if active:
        draw = ImageDraw.Draw(image)
        draw.rounded_rectangle((6,6,PAD_SIZE-7,PAD_SIZE-7),radius=5,outline='#bd9366',width=1)
        draw.line((PAD_SIZE//2-10,PAD_SIZE-6,PAD_SIZE//2+10,PAD_SIZE-6),fill='#c7955c',width=2)
    return image


def switch_art(active):
    image = Image.new('RGBA',(WIDTH,HEIGHT),(0,0,0,0))
    draw = ImageDraw.Draw(image)
    card(draw,(0,0,WIDTH,HEIGHT))
    draw.polygon(((20,27),(20,45),(33,36)),fill='#9f764f' if active else '#9d9589')
    draw.text((47,36),'PLAYBACK',font=font(15),anchor='lm',fill='#29271f')
    draw.rounded_rectangle((164,24,208,48),radius=12,fill='#cbbb9f' if active else '#d0c9bc')
    draw.text((186,36),'ON' if active else 'OFF',font=font(10),anchor='mm',fill='#3e3425')
    return image


def compact_art(label, active, width, height):
    image=Image.new('RGBA',(width,height),(0,0,0,0));draw=ImageDraw.Draw(image)
    draw.rounded_rectangle((1,1,width-2,height-2),radius=4,fill='#bcb8a8',outline='#a09883')
    draw.line((5,2,width-6,2),fill='#d5d0bd')
    if active:draw.line((5,height-4,width-6,height-4),fill='#a87344',width=2)
    draw.text((width//2,height//2-1),label,font=font(12 if width<100 else 18),anchor='mm',fill='#29271f')
    return image


def action_art(label, active):
    image = Image.new('RGBA',(WIDTH,HEIGHT),(0,0,0,0))
    draw = ImageDraw.Draw(image)
    card(draw,(0,0,WIDTH,HEIGHT))
    draw.rounded_rectangle((4,6,WIDTH-5,HEIGHT-5),radius=6,fill='#aca99f')
    draw.rounded_rectangle((5,4,WIDTH-6,HEIGHT-8),radius=6,fill='#bcb9af',outline='#a19b90')
    draw.line((12,5,WIDTH-13,5),fill='#d9d5ca')
    draw.line((12,HEIGHT-7,WIDTH-13,HEIGHT-7),fill='#929086')
    if active:
        draw.rounded_rectangle((5,4,WIDTH-6,HEIGHT-8),radius=6,outline='#b47d50',width=1)
    draw.text((WIDTH//2,35),label,font=font(18),anchor='mm',fill='#514b42')
    return image


def set_box(node,x,y,w,h):
    node['bounds']['bounds']=f'{x} {y} {w} {h}'


def label_style(node,size,colour):
    style=node['componentData']['data']['textStyle']
    style['font']['height']=float(size)
    style['colour']='ff'+colour
    style['justification']='left verticallyCentred'
    style['case']='Upper Case' if node['componentData']['data']['type']=='Name' else 'Original'


def build(skin,params):
    skin=Path(skin)
    tui=json.loads((skin/'TUI.json').read_text())
    definitions=tui['pageData']['componentDefinitions']['localComponentDefinitions']
    style_definition=copy.deepcopy(next(item for item in definitions if item['key'].startswith('shKnob')))
    style_definition['key']='mmStyle'
    definitions.append(style_definition)
    for definition in definitions:
        key=definition['key']
        children=definition['value']['componentsData']
        if key.startswith('shKnob') or key=='mmStyle':
            width=416 if key=='mmStyle' else WIDTH
            for child in children:
                kind=child['componentData']['type']
                if kind=='Focus':
                    set_box(child,NAME_X-4,VALUE_Y-2,width-NAME_X-2,40)
                    child['componentData']['data'].update(backgroundColour='00000000',outlineColour='ffd5352b',backgroundInset=0.0,outlineThickness=2.0)
                elif kind=='Knob':set_box(child,10,24,44,44)
                elif kind=='Label':
                    name=child['componentData']['data']['type']=='Name'
                    set_box(child,10 if name else NAME_X,NAME_Y if name else VALUE_Y,width-20 if name else width-NAME_X-10,28 if name else 38)
                    label_style(child,22 if name else 36,LABEL if name else INK)
        elif key=='shToggle':
            definition['value']['componentsData']=[child for child in children if child['componentData']['type']!='Label']
            for child in definition['value']['componentsData']:set_box(child,0,0,WIDTH,HEIGHT)
            for state in ('on','off'):switch_art(state=='on').save(skin/f'sh_pill_{state}.png')
        elif key.startswith('shTrig_'):
            pad=key.startswith('shTrig_pad_')
            for child in children:
                if child['componentData']['type'] not in ('Button','Focus'):continue
                set_box(child,0,0,PAD_SIZE if pad else WIDTH,PAD_SIZE if pad else HEIGHT)
                if child['componentData']['type']!='Button':continue
                data=child['componentData']['data']
                for state in ('on','off'):
                    artwork=pad_art(int(key.split('_')[2]),state=='on') if pad else action_art('SAVE CREATION' if 'bank_save' in key else 'LOAD CREATION' if 'bank_load' in key else 'FAVORITE' if 'bank_favorite' in key else 'VARY MOTIF' if 'vary_motif' in key else 'NEW MELODY' if 'generate' in key else 'EXPORT MIDI',state=='on')
                    artwork.save(skin/data[state+'Image'])
        elif key.startswith('shRow_'):
            for child in children:
                if child['componentData']['type']=='Label':label_style(child,28,INK)
                set_box(child,0,0,326,36)
                if child['componentData']['type']=='Button':
                    data=child['componentData']['data']
                    for state in ('on','off'):
                        tile=Image.new('RGBA',(326,36),(0,0,0,0));d=ImageDraw.Draw(tile)
                        d.rounded_rectangle((0,0,325,35),radius=3,fill='#c9c4b0',outline='#a87344' if state=='on' else '#827a65')
                        tile.save(skin/data[state+'Image'])
        elif key.startswith('shReadout'):
            for child in children:
                if child['componentData']['type']=='Label':label_style(child,22,LABEL)

    strip=Image.new('RGBA',(KNOB_SIZE,KNOB_SIZE*128),(0,0,0,0))
    body=material('knob-ivory-v1.png',KNOB_SIZE)
    for frame in range(128):
        tile=Image.new('RGBA',(KNOB_SIZE,KNOB_SIZE),(0,0,0,0))
        draw=ImageDraw.Draw(tile)
        angle=135+frame/127*270
        draw.arc((1,1,62,62),135,405,fill='#bbb2a2',width=1)
        tile.alpha_composite(body)
        draw=ImageDraw.Draw(tile)
        theta=math.radians(angle)
        draw.line((32+8*math.cos(theta),32+8*math.sin(theta),32+19*math.cos(theta),32+19*math.sin(theta)),fill='#655f55',width=2)
        strip.alpha_composite(tile,(0,frame*KNOB_SIZE))
    native_strip=Image.new('RGBA',(44,44*128),(0,0,0,0))
    for frame in range(128):
        tile=strip.crop((0,frame*KNOB_SIZE,KNOB_SIZE,(frame+1)*KNOB_SIZE)).resize((44,44),Image.Resampling.LANCZOS)
        native_strip.alpha_composite(tile,(0,frame*44))
    native_strip.save(skin/'sh_knob_r18.png')

    # Native read-only note tiles: 32 time bins, seven pitch bands, three parts.
    for part,colour in (('melody','#73b5c6'),('chord','#b3975b'),('bass','#b6774c')):
        strip=Image.new('RGBA',(72,72*128),(0,0,0,0))
        for mask in range(128):
            tile=Image.new('RGBA',(72,72),(0,0,0,0));d=ImageDraw.Draw(tile)
            band={'melody':0,'chord':24,'bass':48}[part]
            d.rectangle((0,band,38,band+23),fill='#272921')
            d.line((38,band,38,band+23),fill='#414237')
            for lane in range(7):
                if mask&(1<<lane):
                    y=band+1+(6-lane)*3
                    d.rectangle((0,y,36,y+1),fill=colour)
            strip.alpha_composite(tile,(0,mask*72))
        strip.save(skin/f'mm_notes_{part}.png')
        template=copy.deepcopy(style_definition);template['key']='mmNotes_'+part
        template['value']['actions']=[];template['value']['ignoreMousePresses']=True
        knob=next(c for c in template['value']['componentsData'] if c['componentData']['type']=='Knob')
        set_box(knob,0,0,39,72);knob['componentData']['data']['filmStrip']=f'mm_notes_{part}.png';knob['componentData']['data']['numFrames']=128
        template['value']['componentsData']=[knob];definitions.append(template)

    controls=[]
    def visit(node,page):
        if isinstance(node,dict):
            component=node.get('componentData',{})
            kind=component.get('type','')
            bindings=node.get('handle remapping',{}).get('map',[])
            parameter=next((item['value'] for item in bindings if item['key']=='Data'),'')
            if parameter.startswith('Parameter ') and kind.startswith(('shKnob','shTrig','shToggle','shReadout','shRow','shSlider')):
                key=params[int(parameter.split()[-1])]['key']
                x,y,w,h=map(int,node['bounds']['bounds'].split())
                if key.startswith('preview_'):
                    part=key.split('_')[1];step=int(key.split('_')[2]);component['type']='mmNotes_'+part
                    node['bounds']['acceptsHWFocus']='No';box=(24+round(step*38.5),528,round((step+1)*38.5)-round(step*38.5),72)
                elif kind.startswith('shKnob'):
                    node['bounds']['acceptsHWFocus']='Yes'
                    cx,cy=x+w//2,y+23
                    width=416 if key=='substyle' else WIDTH
                    if key=='substyle':component['type']='mmStyle'
                    box=(cx-width//2,cy-HEIGHT//2,width,HEIGHT)
                    if key=='substyle':
                        box=(424,6,416,52)
                        header=next(d for d in definitions if d['key']=='mmStyle')
                        for child in header['value']['componentsData']:
                            typ=child['componentData']['type']
                            if typ=='Knob':set_box(child,10,8,44,44)
                            elif typ=='Label':
                                name=child['componentData']['data']['type']=='Name'
                                set_box(child,80,0 if name else 20,326,20 if name else 32)
                                label_style(child,18 if name else 28,LABEL if name else INK)
                            elif typ=='Focus':set_box(child,76,20,332,32)
                elif kind.startswith('shTrig'):
                    pw,ph=(PAD_SIZE,PAD_SIZE) if key.startswith('pad_') else (WIDTH,HEIGHT)
                    box=(x+w//2-pw//2,y+h//2-ph//2,pw,ph)
                elif kind=='shToggle':box=(x+60-WIDTH//2,y+18-HEIGHT//2,WIDTH,HEIGHT)
                else:box=(x,y,w,h)
                if kind=='shToggle' or (page==0 and key in ('bank_save','bank_favorite','export','bank_up','bank_down')):
                    small=page==0 and key in ('melody_enabled','chord_enabled','bass_enabled','bank_filter','bank_save','bank_favorite','export')
                    bw,bh=(56 if key in ('bank_filter','bank_save','bank_favorite','export') else 70,32) if small else (WIDTH,HEIGHT)
                    cx=x+60 if kind=='shToggle' else x+w//2;cy=y+18 if kind=='shToggle' else y+h//2
                    if key=='play':bw,bh=112,40
                    box=(cx-bw//2,cy-bh//2,bw,bh)
                    if key=='play':box=(1144,12,112,40)
                    original=next(item for item in definitions if item['key']==kind)
                    clone=copy.deepcopy(original);clone['key']=f'mmToggle_{page}_{key}'
                    label={'play':'POWER','melody_enabled':'MELODY','chord_enabled':'CHORDS','bass_enabled':'BASS','bank_filter':'FAVS','bank_save':'SAVE','bank_favorite':'STAR','export':'MIDI','bank_up':'UP','bank_down':'DOWN'}.get(key,params[int(parameter.split()[-1])]['name'].upper())
                    if key.startswith('fx_'):label='FX '+key.split('_')[1]
                    for child in clone['value']['componentsData']:
                        set_box(child,0,0,bw,bh)
                        if child['componentData']['type']=='Button':
                            for state in ('on','off'):
                                filename=f'mm_{page}_{key}_{state}.png'
                                compact_art(label,state=='on',bw,bh).save(skin/filename)
                                child['componentData']['data'][state+'Image']=filename
                    definitions.append(clone);component['type']=clone['key']
                if kind.startswith('shSlider') and key=='bank_scroll':
                    box=(1218,188,38,196)
                    node['bounds']['acceptsHWFocus']='Yes'
                    original=next(item for item in definitions if item['key']==kind);clone=copy.deepcopy(original);clone['key']='mmBankScroll'
                    clone['value']['componentsData']=[child for child in clone['value']['componentsData'] if child['componentData']['type']!='Label']
                    for child in clone['value']['componentsData']:
                        set_box(child,0,0,38,196)
                        if child['componentData']['type']=='Focus':child['componentData']['data'].update(outlineColour='ffd5352b',outlineThickness=2.0,backgroundInset=0.0)
                        if child['componentData']['type']=='Knob':child['componentData']['data'].update(filmStrip='mm_bank_scroll.png',numFrames=128,dragOrientation='Vertical')
                    strip=Image.new('RGBA',(196,196*128))
                    for frame in range(128):
                        tile=Image.new('RGBA',(196,196),'#77725f');d=ImageDraw.Draw(tile);yy=round(frame/127*150)
                        d.rounded_rectangle((3,yy,34,yy+45),radius=4,fill='#ddd5bd',outline='#514b3d');strip.alpha_composite(tile,(0,frame*196))
                    strip.save(skin/'mm_bank_scroll.png');definitions.append(clone);component['type']=clone['key']
                if page==0:
                    if key.startswith('bank_row_'):box=(884,142+(int(key.rsplit('_',1)[1])-1)*42,326,36)
                    positions={'melody_enabled':(884,84,116,40),'chord_enabled':(1012,84,116,40),'bass_enabled':(1140,84,116,40),
                               'bank_save':(884,444,180,34),'bank_favorite':(1076,444,180,34),'bank_filter':(884,484,180,34),'export':(1076,484,180,34),
                               'bank_up':(1218,142,38,40),'bank_down':(1218,390,38,40)}
                    if key in positions:
                        box=positions[key]
                        definition=next(d for d in definitions if d['key']==component['type'])
                        for child in definition['value']['componentsData']:
                            set_box(child,0,0,box[2],box[3])
                            if child['componentData']['type']=='Button':
                                for state in ('on','off'):compact_art(label,state=='on',box[2],box[3]).save(skin/child['componentData']['data'][state+'Image'])
                set_box(node,*box)
                controls.append(dict(page=page,key=key,type=component['type'],box=box))
            for child in node.values():visit(child,page)
        elif isinstance(node,list):
            for child in node:visit(child,page)
    for page,tab in enumerate(tui['pageData']['tabs']):
        visit(next(item for item in definitions if item['key']==tab['componentName']),page)

    for page in range(len(tui['pageData']['tabs'])):
        background=chassis()
        draw=ImageDraw.Draw(background)
        draw.line((24,64,1255,64),fill='#97917d')
        draw.text((24,68 if page==0 else 172),'MUSICAL SETTINGS',font=font(15),fill='#29271f')
        draw.text((884 if page==0 else 1032,68 if page==0 else 172),'CREATIONS' if page==0 else 'OUTPUT',font=font(15),fill='#29271f')
        if page!=0:draw.text((400,84),(['HIP HOP / CREATE','HIP HOP / VARIATIONS']+[f'MIDI FX / CHAIN {i+1}' for i in range(4)]+['CREATIONS','SETTINGS'])[page],font=font(15),fill='#29271f')
        # Power control occupies the right side of the header.
        logo=Image.open(ASSETS/'logo-70s-v2.png').convert('RGBA')
        logo.thumbnail((270,90),Image.Resampling.LANCZOS)
        background.alpha_composite(logo,(24,-12))
        if page==0:
            draw.rounded_rectangle((22,526,1257,602),radius=3,fill='#252724',outline='#77735f')
            for step in range(33):draw.line((24+round(38.5*step),528,24+round(38.5*step),600),fill='#353934')
            for lane in range(8):draw.line((24,529+lane*10,1255,529+lane*10),fill='#353934')
            for i,(label,colour) in enumerate((('MELODY','#73b5c6'),('CHORDS','#b3975b'),('BASS','#b6774c'))):draw.text((850+i*132,508),label,font=font(13,True),fill=colour)
            draw.text((884,126),'BANK / TAP TO RECALL',font=font(15),fill='#29271f')
            draw.rounded_rectangle((422,82,842,502),radius=7,fill='#2b2c29',outline='#b1a99b',width=1)
            draw.line((430,82,834,82),fill='#9b9488')
            for col,text in enumerate(('SIMPLE','BALANCED','DETAILED','COMPLEX')):
                draw.text((476+104*col,77),text,font=font(15),anchor='mm',fill='#29271f')
            for row,text in enumerate(('DARK','MOODY','WARM','LIGHT')):
                label=Image.new('RGBA',(84,24),(0,0,0,0))
                ImageDraw.Draw(label).text((42,12),text,font=font(15),anchor='mm',fill='#29271f')
                label=label.rotate(90,expand=True)
                background.alpha_composite(label,(410-label.width//2,136+104*row-label.height//2))
        elif page==1:
            draw.text((400,130),'1  SELECT YOUR PAD',font=font(20,True),fill='#29271f')
            draw.text((400,244),'2  VARY THE CURRENT MELODY',font=font(20,True),fill='#29271f')
            draw.text((400,384),'Intensity: how much the variation changes.',font=font(18),fill='#29271f')
            draw.text((400,410),'Chords and bass stay the same.',font=font(18),fill='#29271f')
            draw.text((400,434),'3  OR GENERATE A NEW MELODY',font=font(20,True),fill='#29271f')
            draw.text((400,544),'Light / Dark sets the variation character.',font=font(18),fill='#29271f')
            draw.text((400,578),'Both actions replace this pad. Save first to keep it.',font=font(17,True),fill='#29271f')
        if page==len(tui['pageData']['tabs'])-1:
            draw.text((24,200),'DESTINATIONS',font=font(15),fill='#29271f')
            draw.text((24,230),'253 = SUBSCRIBERS',font=font(15),fill='#29271f')
            draw.text((24,252),'254 = OFF',font=font(15),fill='#29271f')
            draw.text((400,172),'ALSA CLIENT / PORT',font=font(15),fill='#29271f')
        if page==6:
            draw.text((400,172),'SAVE / RECALL / FAVORITES',font=font(15),fill='#29271f')
        if 2<=page<6:
            draw.text((400,172),'EFFECT / AMOUNT',font=font(15),fill='#29271f')
        for item in controls:
            if item['page']==page and item['type'].startswith(('shKnob','mmStyle')):card(draw,item['box'])
        draw.line((24,610,1255,610),fill='#96907b')
        draw.ellipse((26,614,30,618),fill='#ae8055')
        background.save(skin/f'sh_bg_{page}.png')
    (skin/'TUI.json').write_text(json.dumps(tui,indent=2)+'\n')
    return controls

