#!/usr/bin/env python3
"""Generate anonymous processed variations in memory and encrypt; activation key stays separate."""
import argparse, ctypes as C, ctypes.util, os, struct
from pathlib import Path
class Note(C.Structure):
    _fields_=[('start',C.c_uint32),('duration',C.c_uint32),('pitch',C.c_uint8),('velocity',C.c_uint8)]
class Part(C.Structure):
    _fields_=[('count',C.c_uint32),('notes',Note*2048)]
class Phrase(C.Structure):
    _fields_=[('source_id',C.c_char*48),('length',C.c_uint32),('parts',Part*3)]
class Request(C.Structure):
    _fields_=[('seed',C.c_uint32)]+[(n,C.c_int) for n in ('key','scale','bars','speed','pad','octave')]+[('mutation',C.c_float),('recombination',C.c_float),('substyle',C.c_char_p)]
def encrypt(data,key):
    lib=C.CDLL(ctypes.util.find_library('sodium'))
    lib.sodium_init.restype=C.c_int
    if lib.sodium_init()<0:raise RuntimeError('libsodium unavailable')
    lib.crypto_secretbox_easy.argtypes=[C.c_void_p,C.c_void_p,C.c_ulonglong,C.c_void_p,C.c_void_p]
    nonce=os.urandom(24);out=C.create_string_buffer(len(data)+16)
    if lib.crypto_secretbox_easy(out,data,len(data),nonce,key):raise RuntimeError('Encryption failed')
    return b'TTMME001'+nonce+out.raw

def main():
    a=argparse.ArgumentParser(description=__doc__);a.add_argument('source',type=Path);a.add_argument('output',type=Path);a.add_argument('--key-file',required=True,type=Path);a.add_argument('--count',type=int,default=8060);a.add_argument('--preserve-families',action='store_true',help='Encrypt a private family bank without destroying metadata; never distribute it publicly');args=a.parse_args()
    if args.preserve_families:
        data=args.source.read_bytes()
        if data[:8]!=b'TTMMB002':raise SystemExit('Family-preserving mode requires TTMMB002')
        key=args.key_file.read_bytes()
        if len(key)!=32:raise SystemExit('Invalid existing key')
        args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(encrypt(data,key));os.chmod(args.output,0o600)
        print('Encrypted private family bank; contains private source phrases and must not be published.')
        return
    if args.source.open('rb').read(8)==b'TTMMB002':raise SystemExit('Use --preserve-families for a private family bank')
    if not 1<=args.count<=100000:raise SystemExit('Invalid variation count')
    core=C.CDLL(str(Path(__file__).resolve().parents[1]/'build/mpc_melody_maker-host.so'))
    core.mm_bank_load.argtypes=[C.c_char_p,C.c_char_p,C.c_uint];core.mm_bank_load.restype=C.c_void_p
    core.mm_bank_free.argtypes=[C.c_void_p];core.mm_generate.argtypes=[C.c_void_p,C.POINTER(Request),C.POINTER(Phrase)]
    error=C.create_string_buffer(160);bank=core.mm_bank_load(os.fsencode(args.source),error,160)
    if not bank:raise SystemExit(error.value.decode())
    # Read only public category metadata; original names and note bytes are never copied into the result.
    styles=[]
    with args.source.open('rb') as f:
        magic,ppq,count=struct.unpack('<8sII',f.read(16))
        if magic!=b'TTMMB001':raise SystemExit('Source must be the private plaintext development bank')
        for i in range(count):
            f.read(48);style=f.read(32).split(b'\0')[0];props=f.read(8);length,*counts=struct.unpack('<IIII',f.read(16))
            styles.append(style);f.seek(sum(counts)*12,1)
    key=args.key_file.read_bytes() if args.key_file.exists() else os.urandom(32)
    if len(key)!=32:raise SystemExit('Activation key must contain 32 bytes')
    blob=bytearray(struct.pack('<8sII',b'TTMMB001',960,args.count));phrase=Phrase()
    try:
        for i in range(args.count):
            pad=i%16;speed=(i//16)%3;style=styles[i%len(styles)]
            request=Request(i+100001,9,1,4,speed,pad,4,.55,.5,style or None)
            if core.mm_generate(bank,C.byref(request),C.byref(phrase)):raise RuntimeError('Variation generation failed')
            blob.extend(('Variation_%05d'%i).encode().ljust(48,b'\0'));blob.extend(style.ljust(32,b'\0'))
            blob.extend(bytes((speed,pad%4+1,4-pad//4,1,9,1,0,0)))
            blob.extend(struct.pack('<IIII',phrase.length,*(p.count for p in phrase.parts)))
            for part in phrase.parts:
                for n in part.notes[:part.count]:blob.extend(struct.pack('<IIBBH',n.start,n.duration,n.pitch,n.velocity,0))
    finally:core.mm_bank_free(bank)
    args.output.parent.mkdir(parents=True,exist_ok=True);args.key_file.parent.mkdir(parents=True,exist_ok=True)
    if not args.key_file.exists():
        fd=os.open(args.key_file,os.O_CREAT|os.O_EXCL|os.O_WRONLY,0o600)
        with os.fdopen(fd,'wb') as f:f.write(key)
    payload=encrypt(bytes(blob),key);args.output.write_bytes(payload)
    print(f'Encrypted processed variations: {args.count}; {len(payload)} bytes. Activation key kept separate.')
if __name__=='__main__':main()
