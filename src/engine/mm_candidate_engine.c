#define MMR_BYTES 131072u
#include "mm_candidate_engine.h"
#include "mm_creation_worker.h"
#include "mm_source_catalog.h"
struct mmce_engine {
 b6_tables tables;b6_profiles profiles;b6_library novelty;mmsa_library sources;mmsc_catalog catalog;
 m5_context context;m3_context vcontext;mmsa_source source,scratch;mmv_scratch vs;
 mmvs_packet packet;mmvs_arena arena;mmj_queue jobs;mmr_pool results;
 mmvs_backend variation;mmcw_backend worker;m3_pattern *bank;uint32_t patterns;
 mmv_frame pads[16],pending;unsigned char valid[16];int scales[16];
};
static int mmce_path(char p[1400],const char *dir,const char *name){int n=snprintf(p,1400,"%s/%s",dir,name);return n>0&&n<1400;}
void mmce_close(mmce_engine *e){if(!e)return;b6_tables_close(&e->tables);b6_profiles_close(&e->profiles);b6_library_close(&e->novelty);mmsa_close(&e->sources);mmsc_close(&e->catalog);free(e->bank);free(e);}
mmce_engine *mmce_open(const char *dir){
 mmce_engine *e;char path[1400],weights[1400];FILE *fp;unsigned char h[16];if(!dir||!*dir)return NULL;e=calloc(1,sizeof *e);if(!e)return NULL;
#define LOAD(name,expr) do{if(!mmce_path(path,dir,name)||!(expr))goto bad;}while(0)
 LOAD("weighted.tables",b6_tables_load(&e->tables,path));LOAD("weighted.profiles",b6_profiles_load(&e->profiles,&e->tables,path));
 if(!mmce_path(weights,dir,"selection_weights.txt"))goto bad;LOAD("theory5.mlib",!m5_load(&e->context,path,weights));
 LOAD("novelty.library",b6_library_load(&e->novelty,path));LOAD("sources.assets",mmsa_load(&e->sources,path));LOAD("sources.catalog",mmsc_load(&e->catalog,path,e->sources.count));LOAD("four_bar.mlib",!m3_load(&e->vcontext,path));
 if(!mmce_path(path,dir,"rhythm_bank.bin")||(fp=fopen(path,"rb"))==NULL)goto bad;
 if(fread(h,16,1,fp)!=1||memcmp(h,"TT4VRBK1",8)||b6_le32(h+12)!=sizeof(m3_pattern)||(e->patterns=b6_le32(h+8))>20000){fclose(fp);goto bad;}
 e->bank=calloc(e->patterns,sizeof *e->bank);if(!e->bank){fclose(fp);goto bad;}if(fread(e->bank,sizeof *e->bank,e->patterns,fp)!=e->patterns||fgetc(fp)!=EOF){fclose(fp);goto bad;}fclose(fp);
 for(uint32_t i=0;i<e->patterns;i++)if(!mmv_pattern_valid(&e->bank[i]))goto bad;
 e->variation=(mmvs_backend){&e->vcontext,e->bank,e->patterns,&e->vs,&e->packet,{{0}}};
 e->worker=(mmcw_backend){&e->tables,&e->profiles,&e->novelty,&e->sources,&e->context,&e->scratch,&e->source,&e->variation,{0},0,0};
 for(int i=0;i<16;i++)e->scales[i]=-1;mmj_init(&e->jobs,0);mmvs_init(&e->arena);mmr_init(&e->results);return e;
 bad:mmce_close(e);return NULL;
#undef LOAD
}
static int mmce_mode(int ui){switch(ui){case 0:return 4;case 1:return 0;case 5:return 3;case 7:return 1;case 8:return 2;default:return -1;}}
static int mmce_finish(mmce_engine *e,const mm_request *r,mm_phrase *out){
 mmvs_header event;const mmv_frame *frame=NULL;int status=mmvs_poll(&e->results,1,&event,&frame);if(status!=MMVS_NEW_FRAME)return status==MMVS_FAILED&&event.status==MMV_NO_COMPATIBLE_RHYTHM?7:4;
 e->pending=*frame;mmr_drop(&e->results);int shift=(r->key-e->pending.melody.key+18)%12-6;
 for(unsigned part=0;part<3;part++)for(unsigned i=0;i<e->pending.playback.parts[part].count;i++){
  mm_note *n=&e->pending.playback.parts[part].notes[i];int pitch=n->pitch+shift+(part==0?(r->octave-4)*12:0);if(pitch<0||pitch>127)return 5;pitch=mm_scale_pitch(pitch,r->key,r->scale,0,127);if(pitch<0)return 5;n->pitch=(uint8_t)pitch;
 }
 char source_name[48];snprintf(source_name,sizeof source_name,"%s",e->pending.playback.source_id);char *tag=strchr(source_name,'@');if(tag)*tag=0;
 if(strlen(source_name)>40)return 5;snprintf(e->pending.playback.source_id,48,"%.40s@%02d%02d",source_name,r->key,r->scale);
 int target_mode=mmce_mode(r->scale);if(target_mode>=0)e->pending.melody.mode=target_mode;
 e->pending.melody.key=r->key;for(int i=0;i<e->pending.melody.count;i++)e->pending.melody.notes[i].pitch=e->pending.playback.parts[0].notes[i].pitch;
 if(mmh_refresh(&e->pending.melody,&e->pending.playback.parts[1],&e->pending.playback.parts[2],&e->vs.harmony,&e->vs.refreshed)!=MMH_OK)return 5;e->pending.melody=e->vs.refreshed;
 e->pads[r->pad]=e->pending;e->valid[r->pad]=1;e->scales[r->pad]=r->scale;*out=e->pending.playback;return 0;
}
int mmce_create(mmce_engine *e,const mm_request *r,int y,int x,int candidates,mm_phrase *out){
 uint32_t source,eligible;int mode;if(!e||!r||!out||r->pad<0||r->pad>15||r->key<0||r->key>11||r->octave<2||r->octave>6)return 1;if(r->bars!=4)return 2;if(r->scale<0||r->scale>=MM_SCALE_COUNT)return 1;mode=mmce_mode(r->scale);
 mmsc_request q={(uint32_t)r->speed,(uint32_t)y,(uint32_t)x,r->substyle};if(!mmsc_select_nearest(&e->catalog,&q,mode,r->seed,&source,&eligible)&&!mmsc_select_nearest(&e->catalog,&q,-1,r->seed,&source,&eligible))return 3;
 mmcw_request request={source,(uint32_t)candidates,32,1,1};if(mmcw_submit(&e->jobs,r->pad,1,1,&request,NULL)!=MMJ_OK)return 4;
 /* The VST has already resolved the persisted pad-touch seed. Replay it once,
    while this worker owns both queue endpoints; do not hash it a second time. */
 e->jobs.slots[(e->jobs.head-1)%MMJ_CAPACITY].seed=r->seed;
 if(mmcw_step(&e->worker,&e->jobs,&e->arena,&e->results)!=MMVS_PUBLISHED)return 4;return mmce_finish(e,r,out);
}
/* Reconstruct only from a known source and its unchanged accompaniment.
   Never infer mode/groove from the current UI or substitute original melody. */
static int mmce_recover(mmce_engine *e,int pad,const mm_phrase *current){
 char source_name[48];memcpy(source_name,current->source_id,48);source_name[47]=0;char *tag=strchr(source_name,'@');int stored_key=-1,stored_scale=-1;
 if(tag){if(strlen(tag)!=5||sscanf(tag+1,"%2d%2d",&stored_key,&stored_scale)!=2||stored_key<0||stored_key>11||stored_scale<0||stored_scale>=MM_SCALE_COUNT)return 0;*tag=0;}
 if(current->length!=15360||!current->parts[0].count||current->parts[0].count>128)return 0;
 for(uint32_t source=0;source<e->sources.count;source++){
  const unsigned char *record=e->sources.data+e->sources.offsets[source]+4;
  if(strncmp((const char *)record,source_name,48))continue;
  if(!mmsa_get(&e->sources,source,&e->scratch,&e->source))return 0;
  int shift=-1;
  for(int part=1;part<3;part++){
   const mm_part *a=part==1?&e->source.chords:&e->source.bass,*b=&current->parts[part];
   if(a->count!=b->count)return 0;
   for(unsigned i=0;i<a->count;i++){
    if(a->notes[i].start!=b->notes[i].start||a->notes[i].duration!=b->notes[i].duration||a->notes[i].velocity!=b->notes[i].velocity)return 0;
    int delta=(b->notes[i].pitch-a->notes[i].pitch+132)%12;
    if(shift<0)shift=delta;else if(!tag&&shift!=delta)return 0;
   }
  }
  if(shift<0)return 0;
  memset(&e->pending,0,sizeof e->pending);e->pending.playback=*current;
  e->pending.melody=e->source.base;e->pending.melody.count=current->parts[0].count;
  e->pending.melody.key=tag?stored_key:(e->source.base.key+shift)%12;
  int restored_mode=tag?mmce_mode(stored_scale):-1;if(restored_mode>=0)e->pending.melody.mode=restored_mode;
  for(int i=0;i<e->pending.melody.count;i++){
   const mm_note *n=&current->parts[0].notes[i];m3_note *m=&e->pending.melody.notes[i];
   memset(m,0,sizeof *m);m->start=n->start;m->duration=n->duration;m->pitch=n->pitch;m->velocity=n->velocity;
  }
  if(mmh_refresh(&e->pending.melody,&current->parts[1],&current->parts[2],&e->vs.harmony,&e->vs.refreshed)!=MMH_OK)return 0;
  e->pending.melody=e->vs.refreshed;e->pads[pad]=e->pending;e->valid[pad]=1;e->scales[pad]=stored_scale;return 1;
 }
 return 0;
}
int mmce_vary(mmce_engine *e,const mm_request *r,int operation,int dark,int candidates,const mm_phrase *current,mm_phrase *out){
 if(!e||!r||!current||!out||r->pad<0||r->pad>15||r->key<0||r->key>11||r->scale<0||r->scale>=MM_SCALE_COUNT)return 6;
 if((!e->valid[r->pad]||memcmp(current,&e->pads[r->pad].playback,sizeof *current))&&!mmce_recover(e,r->pad,current))return 6;
 if(r->mutation==0&&r->key==e->pads[r->pad].melody.key&&r->scale==e->scales[r->pad]){*out=*current;return 0;}
 mmvs_controls controls={operation,(int)lround(r->mutation*1000),dark,candidates,3,0};
 if(mmvs_submit(&e->jobs,&e->arena,r->pad,1,1,1,&e->pads[r->pad],&controls,NULL)!=MMJ_OK)return 4;e->jobs.slots[(e->jobs.head-1)%MMJ_CAPACITY].seed=r->seed;
 if(mmcw_step(&e->worker,&e->jobs,&e->arena,&e->results)!=MMVS_PUBLISHED)return 4;
 /* Finish must preserve current octave: the stored melody is already placed. */
 mm_request placed=*r;placed.octave=4;return mmce_finish(e,&placed,out);
}
int mmce_generation_diagnostic(const mmce_engine *e){return e?e->worker.generation_status*100+(int)e->worker.audit.count:-1;}
const char *mmce_error(int status){switch(status){case 0:return "CANDIDATE READY";case 2:return "UNSUPPORTED SCALE/LENGTH";case 3:return "NO COMPATIBLE SOURCE";case 4:return "GENERATION FAILED";case 5:return "REGISTER/CONTEXT ERROR";case 6:return "CREATE NEW BEFORE VARY";case 7:return "NO_COMPATIBLE_RHYTHM";default:return "INVALID REQUEST";}}
