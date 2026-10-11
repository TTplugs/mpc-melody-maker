#!/usr/bin/env python3
"""Reject known private assets and keys from the exact staged Git blobs."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
entries = subprocess.check_output(['git','ls-files','--stage','-z'],cwd=root).split(b'\0')
records=[]
bad=[]
private_suffixes={'.mid','.midi','.bank','.ttbank','.activation','.creation','.model',
                 '.npy','.npz','.pem','.key','.mlib','.tables','.profiles','.library',
                 '.assets','.catalog','.bin','.chunk','.zip','.tar','.gz','.so','.o','.a'}
private_names={'selection_weights.txt','id_rsa','id_ed25519','.env'}
magic=(b'MThd',b'TTMMB001',b'TTMME001',b'TTMMC001',b'TTMMST0',b'TTMM6',
       b'TTMMCT01',b'TT4VRBK1')
key_headers=(b'-----BEGIN OPENSSH PRIVATE KEY-----',b'-----BEGIN RSA PRIVATE KEY-----',
             b'-----BEGIN PRIVATE KEY-----')
for entry in filter(None,entries):
    metadata,raw_path=entry.split(b'\t',1)
    mode,sha,stage=metadata.split()
    relative=raw_path.decode('utf-8',errors='replace');path=Path(relative)
    if stage!=b'0' or mode not in (b'100644',b'100755'):
        bad.append(relative+' (non-regular or unmerged entry)');continue
    if ((any(part.lower() in ('private','data','dist','build') for part in path.parts)
         and path.name!='.gitkeep') or path.suffix.lower() in private_suffixes
        or path.name.lower() in private_names or path.name.lower().endswith('.activation')):
        bad.append(relative);continue
    records.append((sha,relative))
# One batch reads index objects, not potentially different working-tree files.
result=subprocess.run(['git','cat-file','--batch'],input=b''.join(sha+b'\n' for sha,_ in records),
                      stdout=subprocess.PIPE,check=True,cwd=root)
stream=result.stdout;offset=0
for sha,relative in records:
    end=stream.index(b'\n',offset);header=stream[offset:end].split();offset=end+1
    if len(header)!=3 or header[0]!=sha or header[1]!=b'blob':
        raise SystemExit('Cannot inspect staged object: '+relative)
    size=int(header[2]);content=stream[offset:offset+size];offset+=size+1
    if content.startswith(magic) or any(content.startswith(h) or b'\n'+h in content for h in key_headers):
        bad.append(relative)
if bad:
    print('Publication blocked: private or unsupported staged files:',*bad,sep='\n',file=sys.stderr)
    sys.exit(1)
print(f'Inspected {len(records)} staged source blobs: no known private MIDI, model assets, keys or archives.')
