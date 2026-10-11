#!/usr/bin/env python3
"""Create a reviewed MPC settings candidate; this does not install or restart anything."""
import argparse
from pathlib import Path
import re
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('settings', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
if args.settings.resolve() == args.output.resolve():
    raise SystemExit('Write a separate candidate; never overwrite live settings.')
content = args.settings.read_bytes()
document = ET.fromstring(content)
groups = document.findall(".//VALUE[@name='pluginList-arm']/KNOWNPLUGINS")
if len(groups) != 1:
    raise SystemExit('Expected exactly one ARM plugin registry.')
if any(plugin.get('uid', '').lower() == '544d4d31' or plugin.get('name') == 'MPC Melody Maker' for plugin in groups[0]):
    raise SystemExit('MPC Melody Maker already registered; review before updating.')
entry = (b'      <PLUGIN name="MPC Melody Maker" descriptiveName="MPC Melody Maker" format="VST" '
         b'category="Synth" manufacturer="TTplugs" version="0.1" '
         b'file="/media/acvs-content/Expansions/TTplugs - VST - MPC Melody Maker/mpc_melody_maker.so" '
         b'uid="544d4d31" isInstrument="1" fileTime="0" infoUpdateTime="0" '
         b'numInputs="0" numOutputs="2" isShell="0"/>')
pattern = rb'(<VALUE\s+name="pluginList-arm"\s*>\s*<KNOWNPLUGINS>)(.*?)([ \t]*</KNOWNPLUGINS>)'
newline = b'\r\n' if b'\r\n' in content else b'\n'
candidate, count = re.subn(pattern, lambda match: match[1]+match[2]+entry+newline+match[3], content, flags=re.S)
if count != 1:
    raise SystemExit('Unsupported registry layout; candidate was not written.')
ET.fromstring(candidate)
args.output.write_bytes(candidate)
print('Separate settings candidate prepared. Live MPC must be stopped and backed up before installation.')
