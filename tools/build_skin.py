#!/usr/bin/env python3
"""Build the original MPC Melody Maker skin using native sd88me parameter bindings."""
import json
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
framework = Path(os.environ.get('MPC_VST', str(root.parent / 'mpc-vst-plugins'))).resolve()
sys.path.insert(0, str(framework / 'tools'))
import shadow_skin
from skin_art_pack import build

subprocess.run([sys.executable, str(root / 'tools/gen_parameters.py')], check=True)
renderer = root / 'build/shadow_art'
vendor = framework / 'tools/vendor/force-shadow/tools'
subprocess.run(['cc', '-O2', '-I' + str(vendor), str(framework / 'tools/shadow_art.c'), '-lm', '-o', str(renderer)], check=True)
params = json.loads((root / 'skin/params.json').read_text())['params']
output = shadow_skin.write_skin(str(root / 'build/skin'), 'TTplugs', 'MPC Melody Maker',
                                str(root / 'skin/layout.conf'), params, str(renderer))
skin = Path(output) / 'Plugin Skins'
controls = build(skin, params)
(root / 'build/preview-controls.json').write_text(json.dumps(controls, indent=2)+'\n')
print(output)
