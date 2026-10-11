"""Private family-preserving bank builder. Never publish its output or report."""
import argparse,collections,csv,json,struct,re
from pathlib import Path
from import_library import read_midi,estimate_key,fixed_text,parse_small_int,PPQ

def pack(source,output):
    rows=list(csv.DictReader((source/'index_global.csv').open(encoding='utf-8-sig')))
    # These indexes are versions of two datasets, not five independent corpora.
    extras=list(csv.DictReader((source/'index_extra_clasificadas.csv').open(encoding='utf-8-sig'))) if (source/'index_extra_clasificadas.csv').exists() else []
    recovered=[]
    groups=collections.defaultdict(list)
    for row in rows:groups[(row['speed_folder'],row['origen'],row['chunk'],row['base_in_chunk'])].append(row)
    clips=[];issues=[];families=0;operations=collections.Counter();rhythm_matches=collections.Counter()
    for identity,members in groups.items():
        base=next((r for r in members if r['kind']=='base'),None)
        if base is None:
            reference=members[0]
            expected='Hip-hop '+reference['clip_number']
            candidates=[r for r in extras if r['origen']==reference['origen'] and r['nombre_original'].strip()==expected]
            if len(candidates)==1:
                extra=candidates[0];base=dict(reference);base.update(id=extra['id'],kind='base',copy='0',swap='',melody=extra['melody'],chord=extra['chord'],bass=extra['bass'],parametros_fiables='no')
                members=[base]+members;recovered.append({'family':list(identity),'base':extra['id'],'basis':'exact original name, origin and clip number; parameters unverified'})
            else:issues.append({'family':list(identity),'reason':'missing or ambiguous base'});continue
        try:
            base_notes,base_length=read_midi(source/base['melody'])
            if not base_notes:raise ValueError('empty base melody')
            key,scale=estimate_key(base_notes)
        except (OSError,ValueError) as exc:issues.append({'family':list(identity),'reason':str(exc)});continue
        families+=1
        for row in members:
            try:
                parts=[];length=0
                for part in ('melody','chord','bass'):
                    relative=row.get(part,'')
                    if not relative:parts.append([]);continue
                    path=(source/relative).resolve(strict=True)
                    if not path.is_relative_to(source.resolve()):raise ValueError('path outside corpus')
                    notes,end=read_midi(path);parts.append(notes);length=max(length,end)
                if not parts[0]:raise ValueError('empty member melody')
                variant=0 if row['kind']=='base' else {'light':1,'dark':2,'rhythm':3}.get(row['swap'],-1)
                if variant<0:raise ValueError('unknown variation operation')
                reliable=row['parametros_fiables'].lower() in ('si','sí','yes','true','1')
                props=bytes((('slow','medium','fast').index(row['speed_folder']),parse_small_int(row,'pad_simple_complex',4),parse_small_int(row,'pad_light_dark',3),int(reliable),key,scale,int(row.get('groove_mode')=='swing'),0))
                metadata=struct.pack('<IBBBB',families,variant,int(row.get('movement')=='dynamic'),{'strict':0,'balanced':1,'relaxed':2}.get(row.get('chord_fit'),1),int(reliable))
                blob=fixed_text(row['id'],48)+fixed_text(base['style'],32)+props+metadata+struct.pack('<IIII',length,*(len(p) for p in parts))
                for part in parts:
                    for start,duration,pitch,velocity in part:blob+=struct.pack('<IIBBH',start,duration,pitch,velocity,0)
                clips.append(blob);operations[variant]+=1
                if variant in (1,2):
                    rhythm_matches['matching' if [(n[0],n[1]) for n in parts[0]]==[(n[0],n[1]) for n in base_notes] else 'different']+=1
            except (OSError,ValueError,KeyError,struct.error) as exc:issues.append({'id':row['id'],'reason':str(exc)})
    output.parent.mkdir(parents=True,exist_ok=True)
    temporary=output.with_suffix(output.suffix+'.new');temporary.write_bytes(struct.pack('<8sII',b'TTMMB002',PPQ,len(clips))+b''.join(clips));temporary.replace(output)
    report={'format':'TTMMB002','families':families,'clips':len(clips),'operations':dict(operations),'light_dark_rhythm_audit':dict(rhythm_matches),'issues':issues,'recovered_bases':recovered,'extra_metadata':dict(collections.Counter(r.get('parametros','') for r in extras)),'scope':'index_global families; exact identified missing bases recovered from classified extras; other extras excluded from family recombination'}
    output.with_suffix('.report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='issues'},indent=2));print('Excluded records:',len(issues))
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('source',type=Path);parser.add_argument('output',type=Path);args=parser.parse_args();pack(args.source,args.output)
