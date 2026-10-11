"""Adapt supplied physical artwork to native MPC controls on every page."""
import copy,json
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
ASSETS=Path(__file__).resolve().parents[1]/'skin/assets/skin09'
FONT='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
def box(node,x,y,w,h):node['bounds']['bounds']=f'{x} {y} {w} {h}'
def label(node,size=20,colour='ff5c40'):
 s=node['componentData']['data']['textStyle'];s['font']['height']=float(size);s['colour']='ff'+colour;s['justification']='horizontallyCentred verticallyCentred'
def build(skin,params):
 skin=Path(skin);j=json.loads((skin/'TUI.json').read_text());defs=j['pageData']['componentDefinitions']['localComponentDefinitions'];original={d['key']:copy.deepcopy(d) for d in defs};controls=[]
 for name in ('knob_strip','key_off','key_on','key_small_off','key_small_on','pad_light'):
  Image.open(ASSETS/f'melodymaker_{name}.png').save(skin/f'mm9_{name}.png')
 # One compact, read-only piano-roll strip for each part. Native values remain 7-bit masks.
 template=next(d for d in original.values() if d['key'].startswith('shKnob'))
 for part,col,band in [('melody','#ff775b',0),('chord','#c8b786',24),('bass','#b795d7',48)]:
  strip=Image.new('RGBA',(72,72*128));d=ImageDraw.Draw(strip)
  for mask in range(128):
   yy=mask*72+band;d.rectangle((0,yy,34,yy+23),fill='#151311');d.line((34,yy,34,yy+23),fill='#34302c')
   for lane in range(7):
    if mask&(1<<lane):d.rectangle((1,yy+1+(6-lane)*3,33,yy+2+(6-lane)*3),fill=col)
  strip.save(skin/f'mm9_notes_{part}.png');t=copy.deepcopy(template);t['key']='mm9Notes_'+part;t['value']['actions']=[];t['value']['ignoreMousePresses']=True
  k=next(c for c in t['value']['componentsData'] if c['componentData']['type']=='Knob');box(k,0,0,72,72);k['componentData']['data'].update(filmStrip=f'mm9_notes_{part}.png',numFrames=127);t['value']['componentsData']=[k];defs.append(t)
 for page,tab in enumerate(j['pageData']['tabs']):
  root=next(d for d in defs if d['key']==tab['componentName']);bg=Image.open(ASSETS/'melodymaker_background.png').convert('RGBA')
  if page:
   # Keep the supplied faceplate/header, replacing the control area for other pages.
   patch=bg.crop((12,75,28,91)).resize((1256,530));bg.alpha_composite(patch,(12,75));draw=ImageDraw.Draw(bg)
   draw.rounded_rectangle((32,92,1248,606),radius=8,fill='#25211f',outline='#51413a',width=2)
   draw.text((42,78),tab['tabName'],font=ImageFont.truetype(FONT,20),fill='#ff775b',anchor='lm')
  if tab['tabName']=='EXPORT':
   d=ImageDraw.Draw(bg);f=ImageFont.truetype(FONT,22)
   d.text((70,438),'EXPORT MIDI FILES',font=f,fill='#ff775b')
   d.text((70,472),'Import the chosen .mid file from the MPC Browser into a MIDI track.',font=ImageFont.truetype(FONT,19),fill='#e0cec5')
   d.text((70,500),'ALL 3 writes a combined file plus one file per part.',font=ImageFont.truetype(FONT,19),fill='#e0cec5')
  bg.save(skin/f'sh_bg_{page}.png')
  for node in root['value']['componentsData']:
   typ=node['componentData']['type'];bindings=node.get('handle remapping',{}).get('map',[]);param=next((i['value'] for i in bindings if i['key']=='Data'),'')
   if not param.startswith('Parameter '):continue
   key=params[int(param.split()[-1])]['key'];x,y,w,h=map(int,node['bounds']['bounds'].split());clone=copy.deepcopy(original.get(typ,{}))
   if key.startswith('preview_'):
    part=key.split('_')[1];step=int(key.split('_')[-1]);node['componentData']['type']='mm9Notes_'+part;box(node,88+step*35,512,35,72);node['bounds']['acceptsHWFocus']='No';continue
   if not clone:continue
   clone['key']=f'mm9_{page}_{key}';children=clone['value']['componentsData']
   if typ.startswith('shKnob'):
    cx=x+w//2;cy=y+51;box(node,cx-60,cy-60,120,164)
    for c in children:
     k=c['componentData']['type']
     if k=='Knob':box(c,0,0,120,120);c['componentData']['data'].update(filmStrip='mm9_knob_strip.png',numFrames=63)
     elif k=='Focus':box(c,9,120,102,30);c['componentData']['data'].update(outlineColour='ffff4a2e',outlineThickness=2.,backgroundInset=0.)
     elif k=='Label':
      name=c['componentData']['data']['type']=='Name';box(c,0,100 if name else 122,120,22 if name else 30);label(c,17 if name else 22)
      if page==0 and name:c['bounds']['whenVisible']='Never'
    if page==0:
     for c in children:
      if c['componentData']['type']=='Label' and c['componentData']['data']['type']=='Value':box(c,9,122,102,26);label(c,18)
    if tab['tabName']=='SETTINGS' and key.endswith(('_client','_port','_channel')):
     box(node,cx-130,cy-60,260,164)
     for c in children:
      typc=c['componentData']['type']
      if typc=='Knob':box(c,70,0,120,120)
      elif typc=='Label':
       name=c['componentData']['data']['type']=='Name';box(c,0,100 if name else 122,260,22 if name else 30);label(c,17 if name else 20)
      elif typc=='Focus':box(c,0,120,260,30)
   elif typ.startswith(('shTrig','shToggle')):
    pad=key.startswith('pad_') and key[4:].isdigit();power=key=='play';cw,ch=(120,120) if pad else (86,40) if power else (86,56)
    cx=x+w//2;cy=y+h//2
    if typ=='shToggle':cx=x+60;cy=y+18
    box(node,cx-cw//2,cy-ch//2,cw,ch)
    if pad:
     off=Image.new('RGBA',(120,120));off.save(skin/'mm9_pad_off.png')
    for c in children:
     k=c['componentData']['type'];box(c,0,0,cw,ch)
     if k=='Focus':c['componentData']['data'].update(outlineColour='ffff4a2e',outlineThickness=1.,backgroundInset=0.)
     elif k=='Button':
      for state in ('on','off'):c['componentData']['data'][state+'Image']=('mm9_pad_light.png' if state=='on' else 'mm9_pad_off.png') if pad else f'mm9_key_{"small_" if power else ""}{state}.png'
     elif k=='Label':label(c,14,'ffab96')
    if pad:
     box(node,cx-40,cy-40,80,80)
     for c in children:
      box(c,-20,-20,120,120) if c['componentData']['type']=='Button' else box(c,0,0,80,80)
     clone['value']['componentsData']=[c for c in children if c['componentData']['type']!='Label']
    elif not key.startswith('fx_view_'):
     # Trigger components have no native Label. Bake the layout caption into
     # both switch states and remove duplicate host-name labels on toggles.
     caption=node['componentData'].get('name',params[int(param.split()[-1])]['name']).upper()
     for c in children:
      if c['componentData']['type']=='Button':
       for state in ('on','off'):
        tile=Image.open(skin/c['componentData']['data'][state+'Image']).convert('RGBA');d=ImageDraw.Draw(tile)
        d.text((cw//2,ch//2),caption,font=ImageFont.truetype(FONT,14),fill='#3a0a06' if state=='on' else '#ffab96',anchor='mm')
        filename=f'mm9_button_{page}_{key}_{state}.png';tile.save(skin/filename);c['componentData']['data'][state+'Image']=filename
     clone['value']['componentsData']=[c for c in children if c['componentData']['type']!='Label']
   elif typ.startswith('shRow'):
    for c in children:
     box(c,0,0,w,h)
     if c['componentData']['type']=='Label':label(c,19)
     elif c['componentData']['type']=='Button':
      for state in ('on','off'):
       name=f'mm9_row_{page}_{state}.png';im=Image.new('RGBA',(w,h),'#f03822' if state=='on' else '#211b18');im.save(skin/name);c['componentData']['data'][state+'Image']=name
   elif typ.startswith('shSlider'):
    # Native square frame strips avoid renderer-dependent aspect scaling.
    clone['value']['componentsData']=[c for c in children if c['componentData']['type']!='Label']
    x,y,w,side=(1120,206,26,234) if page==0 else (865,124,30,360);box(node,x,y,w,side)
    frame_size=40;strip=Image.new('RGBA',(frame_size,frame_size*128))
    for frame in range(128):
     im=Image.new('RGBA',(frame_size,frame_size));d=ImageDraw.Draw(im);d.rounded_rectangle((1,0,38,39),radius=3,fill='#211b18');yy=round(frame/127*32);d.rounded_rectangle((3,yy,36,yy+7),radius=3,fill='#f03822');strip.alpha_composite(im,(0,frame_size*frame))
    name=f'mm9_scroll_{page}.png';strip.save(skin/name)
    for c in clone['value']['componentsData']:
     box(c,0,0,w,side)
     if c['componentData']['type']=='Knob':c['componentData']['data'].update(filmStrip=name,numFrames=127)
     elif c['componentData']['type']=='Focus':c['componentData']['data'].update(outlineColour='ffff4a2e',outlineThickness=2.)
   elif typ.startswith('shReadout'):
    for c in children:
     if c['componentData']['type']=='Label':label(c,18)
   else:continue
   defs.append(clone);node['componentData']['type']=clone['key'];controls.append({'page':page,'key':key,'box':node['bounds']['bounds']})
 # Keep only reachable component definitions and images. Old renderer assets
 # otherwise remain loaded alongside the supplied pack and waste MPC memory.
 by_name={d['key']:d for d in defs};used=set();pending=[t['componentName'] for t in j['pageData']['tabs']]
 def walk(value):
  if isinstance(value,dict):
   typ=value.get('componentData',{}).get('type')
   if typ in by_name and typ not in used:pending.append(typ)
   for v in value.values():walk(v)
  elif isinstance(value,list):
   for v in value:walk(v)
 while pending:
  name=pending.pop()
  if name in used:continue
  used.add(name);walk(by_name[name])
 j['pageData']['componentDefinitions']['localComponentDefinitions']=[d for d in defs if d['key'] in used]
 images=set()
 def filenames(value):
  if isinstance(value,str) and value.lower().endswith('.png'):images.add(value)
  elif isinstance(value,dict):
   for v in value.values():filenames(v)
  elif isinstance(value,list):
   for v in value:filenames(v)
 filenames(j)
 for asset in skin.glob('*.png'):
  if asset.name not in images:asset.unlink()
 (skin/'TUI.json').write_text(json.dumps(j,indent=2)+'\n');return controls
