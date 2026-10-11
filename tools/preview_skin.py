#!/usr/bin/env python3
"""Render design previews with default values; these are not device screenshots."""
import json
from pathlib import Path
from PIL import Image, ImageDraw
from skin_art import font, pad_art, switch_art, action_art, compact_art, KNOB_SIZE, NAME_X, NAME_Y, VALUE_Y, LABEL, INK

root = Path(__file__).resolve().parents[1]
skin = next((root/'build/skin').glob('*/Plugin Skins'))
params = {p['key']:p for p in json.loads((root/'skin/params.json').read_text())['params']}
controls = json.loads((root/'build/preview-controls.json').read_text())
strip = Image.open(skin/'sh_knob_r18.png').convert('RGBA')
frame_size=strip.width
for page in sorted({item['page'] for item in controls}):
    image = Image.open(skin/f'sh_bg_{page}.png').convert('RGBA')
    draw = ImageDraw.Draw(image)
    for item in controls:
        if item['page'] != page:
            continue
        x, y, w, h = item['box']
        kind, key = item['type'], item['key']
        p = params[key]
        value = p['default']
        if kind.startswith('shKnob') or kind == 'mmStyle':
            style = kind == 'mmStyle'
            normalized = (value-p['min'])/(p['max']-p['min'])
            frame = round(normalized*127)
            image.alpha_composite(strip.crop((0, frame*frame_size, frame_size, (frame+1)*frame_size)).resize((44,44),Image.Resampling.LANCZOS), (x+10, y+24))
            draw.text((x+10, y+NAME_Y), p['name'].upper(), font=font(18), fill='#'+LABEL)
            if p.get('options'):
                text = p['options'][round(value)]
            elif key in ('mutation', 'recombination'):
                text = f'{round(value*100)}%'
            else:
                text = str(round(value)+(key == 'active_pad'))
            draw.text((x+NAME_X, y+VALUE_Y), text, font=font(28), fill='#'+INK)
        elif key.startswith('pad_'):
            number = int(key.split('_')[1])
            image.alpha_composite(pad_art(number, number == 1), (x, y))
        elif kind.startswith('mmToggle'):
            label={'melody_enabled':'MELODY','chord_enabled':'CHORDS','bass_enabled':'BASS','bank_filter':'FAVS','bank_save':'SAVE','bank_favorite':'STAR','export':'MIDI'}.get(key,p['name'].upper())
            if key.startswith('fx_'):label='FX '+key.split('_')[1]
            image.alpha_composite(compact_art(label,value>.5,w,h),(x,y))
        elif kind=='mmBankScroll':
            draw.rectangle((x,y,x+w-1,y+h-1),fill='#a9a38e');draw.rounded_rectangle((x+1,y+1,x+w-2,y+40),radius=3,fill='#d0cab6',outline='#8a826d')
        elif kind.startswith('shRow'):
            draw.rounded_rectangle((x,y,x+w-1,y+h-1),radius=3,fill='#bfbba9',outline='#a9a38d');draw.text((x+12,y+h//2),key[-1]+' EMPTY',font=font(12),anchor='lm',fill='#'+INK)
        elif kind == 'shToggle':
            image.alpha_composite(switch_art(value > .5), (x, y))
        elif kind.startswith('shTrig'):
            image.alpha_composite(action_art('SAVE CREATION' if key == 'bank_save' else 'LOAD CREATION' if key == 'bank_load' else 'FAVORITE' if key == 'bank_favorite' else 'VARY MOTIF' if key == 'vary_motif' else 'NEW MELODY' if key == 'generate' else 'EXPORT MIDI', False), (x, y))
        elif key == 'status':
            draw.text((x+8, y+h//2), 'HIP HOP READY', font=font(11), anchor='lm', fill='#'+LABEL)
    image.convert('RGB').save(root/f'build/preview_{page}.png')
    print(f'Design preview, default values: build/preview_{page}.png')

