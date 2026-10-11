#!/usr/bin/env python3
"""Generate our VST parameter table and the equivalent sd88me skin parameter input."""
import json
from pathlib import Path

root = Path(__file__).resolve().parent.parent
parameters = []


def param(key, name, minimum=0, maximum=1, default=0, options=None, kind='number'):
    parameters.append(dict(key=key, name=name, min=minimum, max=maximum, default=default,
                           options=options or [], kind=kind))


param('key', 'Key', 0, 11, 9, ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'])
param('scale', 'Scale', 0, 21, 1, ['Major', 'Minor', 'Maj pent.', 'Min pent.',
    'Min blues', 'Harm. minor', 'Mel. minor', 'Dorian', 'Phrygian', 'Lydian',
    'Mixolydian', 'Locrian', 'Hirajoshi', 'Insen', 'Iwato', 'Yo',
    'Hung. minor', 'Double harm.', 'Phryg. dom.', 'Whole tone', 'Dim. W/H', 'Egypt. pent.'])
param('bars', 'Bars', 1, 8, 4, kind='integer')
param('speed', 'Speed', 0, 2, 1, ['Slow', 'Medium', 'Fast'])
param('octave', 'Octave', 2, 6, 4, kind='integer')
param('mutation', 'Intensity', 0, 1, .25)
param('recombination', 'Recombine', 0, 1, .35)
param('play', 'Power', 0, 1, 1, ['Off', 'On'])
param('status', 'Status', kind='text')
param('active_pad', 'Selected pad', 0, 15, 0, kind='integer')
param('generate', 'New melody', kind='trigger')
for index in range(16):
    param(f'pad_{index + 1}', f'Pad {index + 1}', kind='pad')
param('pad_base', 'Pad base note', 0, 112, 36, kind='integer')
param('melody_channel', 'Melody MIDI', 2, 16, 2, kind='integer')
param('chord_channel', 'Chord MIDI', 2, 16, 3, kind='integer')
param('bass_channel', 'Bass MIDI', 2, 16, 4, kind='integer')
param('info', 'Phrase', kind='text')
param('export', 'Export MIDI', kind='trigger')
param('substyle', 'Hip Hop substyle', 0, 38, 0, ['All',
    'Bare', 'Blackout', 'Block', 'Blunt', 'Cage', 'Chilled', 'Chronic', 'Cinematic',
    'Cold', 'Concrete', 'Crushed', 'Depth', 'Drip', 'Dusty', 'Eventide', 'Grim',
    'Heavy', 'Hollow', 'Hustle', 'Keyed', 'Lowkey', 'Murky', 'Pilot', 'Prowl',
    'Raw', 'Rugged', 'Scarred', 'Shadow', 'Slab', 'Slugged', 'Smoke', 'Sombre',
    'Steely', 'Stripped', 'Throne', 'Vault', 'Vertical', 'Worn'])

# ALSA sequencer addresses: 253 preserves subscriber routing; 254 disables a part.
for part in ('melody', 'chord', 'bass'):
    param(part + '_client', part.title() + ' destination', 0, 254, 253, kind='integer')
    param(part + '_port', part.title() + ' port', 0, 255, 0, kind='integer')

param('export_parts', 'Export parts', 0, 3, 0, ['All 3', 'Melody', 'Chords', 'Bass'])

for slot in range(1, 13):
    param(f'fx_{slot}_on', 'Enabled', 0, 1, 0, ['Off', 'On'])
    param(f'fx_{slot}_type', 'Effect', 0, 11, 0, ['Off','Swing','Gate','Velocity','Humanize','Repeat','Arp up','Arp down','Bass motif','Probability','Strum','Accent'])
    param(f'fx_{slot}_amount', 'Amount', 0, 1, .5)
    param(f'fx_{slot}_target', 'Apply to', 0, 3, 0, ['All 3','Melody','Chords','Bass'])
param('vary_motif', 'Vary motif', kind='trigger')

for part in ('melody','chord','bass'):
    param(part+'_enabled', part.title(), 0, 1, 1, ['Off','On'])

param('bank_slot', 'Creation', 1, 128, 1, kind='integer')
param('bank_save', 'Save creation', kind='trigger')
param('bank_load', 'Load creation', kind='trigger')
param('bank_favorite', 'Toggle favorite', kind='trigger')
param('bank_filter', 'Favorites only', 0, 1, 0, ['All','Favorites'])
param('bank_info', 'Creation info', kind='text')

param('bank_scroll', 'Scroll creations', 0, 122, 0, kind='integer')
for row in range(1, 8):
    param(f'bank_row_{row}', f'Creation row {row}', kind='trigger')

for part in ('melody','chord','bass'):
    for step in range(32):
        param(f'preview_{part}_{step}', 'Note preview', 0, 127, 0, kind='integer')

param('bank_up', 'Bank previous rows', kind='trigger')
param('bank_down', 'Bank next rows', kind='trigger')

param('variation_type', 'Variation', 0, 2, 0, ['Light / Dark', 'Rhythm', 'Masked'])
param('light_dark', 'Light - Dark', 0, 1, .5)
param('candidates', 'Candidates', 8, 32, 16, kind='integer')

param('history_back', 'Previous creation', kind='trigger')
param('history_forward', 'Next creation', kind='trigger')
param('pad_fixed', 'Fix pad', 0, 1, 0, ['Off','Fixed'])

# Visible FX controls map to one page of the existing twelve-slot chain.
param('fx_page', 'FX page', 1, 4, 1, kind='integer')
param('fx_page_previous', 'Previous FX page', kind='trigger')
param('fx_page_next', 'Next FX page', kind='trigger')
param('fx_add', 'Add effect', kind='trigger')
for row in range(1, 4):
    param(f'fx_view_{row}_on', f'FX {row}', 0, 1, 0, ['Off','On'])
    param(f'fx_view_{row}_type', 'Effect', 0, 11, 0, ['Off','Swing','Gate','Velocity','Humanize','Repeat','Arp up','Arp down','Bass motif','Probability','Strum','Accent'])
    param(f'fx_view_{row}_amount', 'Amount', 0, 1, .5)
    param(f'fx_view_{row}_target', 'Apply to', 0, 3, 0, ['All 3','Melody','Chords','Bass'])
for role in ('all','melody','chords','bass'):
    param(f'export_{role}', f'Export {role}', kind='trigger')

lines = ['/* Generated from tools/gen_parameters.py. Keep published indices stable. */',
         '#define MM_PARAM_COUNT ' + str(len(parameters)),
         'enum { MM_NUMBER, MM_INTEGER, MM_TEXT, MM_TRIGGER, MM_PAD };',
         'typedef struct { const char *key, *name; float min, max, def; int kind, nopts; const char *opts[40]; } mm_param;',
         'enum { ' + ', '.join('P_' + p['key'].upper() for p in parameters) + ' };',
         'static const mm_param mm_params[MM_PARAM_COUNT] = {']
skin = []
for p in parameters:
    options = ','.join(json.dumps(o) for o in p['options']) or '0'
    kind = 'MM_' + {'number': 'NUMBER', 'integer': 'INTEGER', 'text': 'TEXT', 'trigger': 'TRIGGER', 'pad': 'PAD'}[p['kind']]
    numbers = ','.join(f'{float(p[k])}f' for k in ('min', 'max', 'default'))
    lines.append('{' + json.dumps(p['key']) + ',' + json.dumps(p['name']) + ',' + numbers + ',' + kind + ',' + str(len(p['options'])) + ',{' + options + '}},')
    entry = {k: p[k] for k in ('key', 'name', 'min', 'max', 'default')}
    if p['options']:
        entry['options'] = p['options']
    if p['kind'] in ('trigger', 'pad'):
        entry['momentary'] = True
    if p['kind'] == 'text':
        entry['display'] = 'string'
    elif p['kind'] == 'integer':
        entry['display'] = 'int'
    skin.append(entry)
lines.append('};')
(root / 'build').mkdir(exist_ok=True)
(root / 'skin').mkdir(exist_ok=True)
(root / 'build/mm_params.h').write_text('\n'.join(lines) + '\n')
(root / 'skin/params.json').write_text(json.dumps({'params': skin}, indent=2) + '\n')
print(f'{len(parameters)} stable parameter definitions generated')
