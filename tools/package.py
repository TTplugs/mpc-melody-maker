#!/usr/bin/env python3
"""Package the integrated engine for personal use; never publish the archive."""
import argparse
from pathlib import Path
import shutil
import zipfile

FILES = ('weighted.tables','weighted.profiles','theory5.mlib','selection_weights.txt',
         'novelty.library','sources.assets','sources.catalog','four_bar.mlib','rhythm_bank.bin')
root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--engine-dir', required=True, type=Path,
                    help='Directory containing all nine privately prepared integrated-engine assets')
args = parser.parse_args()
assets = args.engine_dir.resolve(strict=True)
if not assets.is_dir():
    parser.error('--engine-dir must be a directory')
for name in FILES:
    path = assets / name
    if not path.is_file() or not path.stat().st_size:
        parser.error('Missing or empty integrated-engine asset: '+name)
    if path.is_symlink():
        parser.error('Runtime assets must be regular files: '+name)
skins = list((root / 'build/skin').glob('*/Plugin Skins'))
if len(skins) != 1:
    parser.error('Build exactly one native skin before packaging')
source = skins[0].parent
binary = root / 'build/mpc_melody_maker.so'
if not binary.is_file():
    parser.error('Build the ARM VST before packaging')
# A fresh staging folder prevents obsolete images or a legacy bank being reused.
parent = root / 'dist/private'
parent.mkdir(parents=True, exist_ok=True)
import tempfile
with tempfile.TemporaryDirectory(prefix='package-', dir=parent) as temporary:
    destination = Path(temporary) / source.name
    shutil.copytree(source, destination)
    shutil.copy2(binary, destination / binary.name)
    engine = destination / 'private/engine'
    engine.mkdir(parents=True)
    for name in FILES:
        shutil.copy2(assets / name, engine / name)
    (destination / 'plugin-meta.xml').write_text(
        '<PLUGIN name="MPC Melody Maker" descriptiveName="MPC Melody Maker" format="VST" '
        'category="Synth" manufacturer="TTplugs" version="0.2" '
        f'file="%payload-path%/{source.name}/{binary.name}" uid="544d4d31" '
        'isInstrument="1" fileTime="0" infoUpdateTime="0" numInputs="0" numOutputs="2" isShell="0"/>\n',
        encoding='utf-8')
    (destination / 'PERSONAL-ONLY.txt').write_text(
        'Personal installation containing PRIVATE musical assets. Do not publish.\n'
        'This integrated bundle is not protected by the legacy bank encryption.\n'
        'MIDI exports are written to private/exports/ unless overridden.\n',encoding='utf-8')
    archive = parent / 'mpc-melody-maker-PERSONAL.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as output:
        for path in sorted(destination.rglob('*')):
            if path.is_file():
                output.write(path, path.relative_to(destination.parent))
print(f'PRIVATE installation archive: {archive}')
