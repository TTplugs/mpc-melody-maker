#define _GNU_SOURCE
#include "mm_history.h"
#include "mm_core.h"
#include "mm_fx.h"
#include "mm_creation.h"
#include "mm_params.h"
#include "ttplugs_styles_db.h"
#ifdef MM_CANDIDATE_ENGINE
#ifndef MM_FIFO_INPUT
#error Candidate engine requires FIFO input
#endif
#include "mm_candidate_engine.h"
#endif
#ifdef MM_FIFO_INPUT
#include "mm_input_queue.h"
#include "mm_jobs.h"
#endif
#include <dlfcn.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sound/asequencer.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#define PUBLIC __attribute__((visibility("default")))
typedef struct AEffect AEffect;
typedef intptr_t (*host_callback)(AEffect *,int32_t,int32_t,intptr_t,void *,float);
struct AEffect {
    int32_t magic;
    intptr_t (*dispatcher)(AEffect *,int32_t,int32_t,intptr_t,void *,float);
    void (*process)(AEffect *,float **,float **,int32_t);
    void (*setParameter)(AEffect *,int32_t,float);
    float (*getParameter)(AEffect *,int32_t);
    int32_t numPrograms,numParams,numInputs,numOutputs,flags;
    intptr_t resvd1,resvd2;
    int32_t initialDelay,realQualities,offQualities;
    float ioRatio; void *object,*user;
    int32_t uniqueID,version;
    void (*processReplacing)(AEffect *,float **,float **,int32_t);
    void (*processDoubleReplacing)(AEffect *,double **,double **,int32_t);
    char future[56];
};
typedef struct { int32_t type,byteSize,deltaFrames,flags,noteLength,noteOffset; uint8_t midiData[4]; char tail[4]; } vst_midi;
typedef struct { int32_t count; intptr_t reserved; void *events[]; } vst_events;
typedef struct {
    double samplePos,sampleRate,nanoSeconds,ppqPos,tempo,barStartPos,cycleStartPos,cycleEndPos;
    int32_t numerator,denominator,smpteOffset,smpteFrameRate,samplesToNextClock,flags;
} vst_time;
typedef struct { double when; uint8_t part,pitch,velocity,on; } scheduled;
typedef struct {
    AEffect fx; host_callback host;
    _Atomic float values[MM_PARAM_COUNT];
    atomic_int run,busy,command,selected,reader,port_ready,state_epoch;
    atomic_int export_command,fx_command,vary_command,bank_commands;
    _Atomic float tempo;
#ifdef MM_FIFO_INPUT
    mmi_queue input_queue;atomic_uint input_rejected;
    uint32_t pad_seeds[16];atomic_uint pad_touches[16];
#endif
    atomic_uint published[16];atomic_uint fixed[16];mm_history history[16];
    mm_phrase phrases[16][3],dry[16];uint8_t dry_valid[16];
    pthread_t worker; pthread_mutex_t save_lock,text_lock;
#ifdef MM_CANDIDATE_ENGINE
    mmce_engine *candidate;mm_phrase candidate_current,candidate_output;atomic_int candidate_status;atomic_int candidate_diagnostic;
#endif
    mm_bank *bank; char library[1024],status[64],info[64],bank_info[64],bank_rows[7][24];
    atomic_int row_slots[7];
    double rate,last_end; int was_playing,last_pad; unsigned last_token;
    uint8_t active[3][128]; int channels[3],dest_clients[3],dest_ports[3],enabled[3]; scheduled events[512];
    void *asound,*seq; int port;
    int (*seq_open)(void **,const char *,int,int);
    int (*seq_name)(void *,const char *);
    int (*seq_port)(void *,const char *,unsigned,unsigned);
    int (*seq_send)(void *,struct snd_seq_event *);
    int (*seq_close)(void *);
    char *chunk;char midi_names[3][2][24];time_t midi_names_time;
    atomic_uint note_events,qa_blocks,ui_dirty;
    atomic_int qa_time_flags,qa_send_result;
    _Atomic float qa_ppq;
} plugin;
static int module_anchor;
static int integer(plugin *p,int index) {
#ifdef MM_CANDIDATE_ENGINE
    if(index==P_BARS)return 4;
#endif
    if(index==P_PAD_FIXED)return atomic_load(&p->fixed[atomic_load(&p->selected)])?1:0;
    const mm_param *def=&mm_params[index];
    return (int)lroundf(def->min+atomic_load(&p->values[index])*(def->max-def->min));
}
/* A page exposes three real inserts; one empty page becomes available as
   the preceding page fills. Audio and saved creations keep the full chain. */
static int fx_pages(plugin *p){int last=-1;for(int i=0;i<MM_FX_SLOTS;i++)if(atomic_load(&p->values[P_FX_1_ON+i*4])>.5f)last=i;int pages=last<0?1:last/3+2;return pages>4?4:pages;}
static int fx_page(plugin *p){int page=integer(p,P_FX_PAGE);int max=fx_pages(p);return page>max?max:page;}
static int fx_parameter(plugin *p,int index){return index>=P_FX_VIEW_1_ON&&index<=P_FX_VIEW_3_TARGET?P_FX_1_ON+(fx_page(p)-1)*12+index-P_FX_VIEW_1_ON:index;}
static void text(plugin *p,const char *status,const char *info) {
    pthread_mutex_lock(&p->text_lock);
    snprintf(p->status,sizeof p->status,"%s",status);
    if(info)snprintf(p->info,sizeof p->info,"%.63s",info);
    pthread_mutex_unlock(&p->text_lock);atomic_store(&p->ui_dirty,1);
}
static const int input_parameters[12]={P_KEY,P_SCALE,P_BARS,P_SPEED,P_OCTAVE,P_MUTATION,P_RECOMBINATION,P_SUBSTYLE,P_VARIATION_TYPE,P_LIGHT_DARK,P_CANDIDATES,P_STYLE};
static void remember_creation(plugin *p,int pad){float controls[12];for(int i=0;i<12;i++)controls[i]=atomic_load(&p->values[input_parameters[i]]);mm_history_record(&p->history[pad],&p->dry[pad],controls);}
#ifdef MM_FIFO_INPUT
static void enqueue_input(plugin *p,int pad,int action){
    mmi_event event={.pad=(uint32_t)pad,.action=(uint32_t)action,.epoch=(uint32_t)atomic_load(&p->state_epoch)};
    for(int i=0;i<12;i++)event.parameters[i]=atomic_load(&p->values[input_parameters[i]]);
#ifdef MM_CANDIDATE_ENGINE
    event.parameters[2]=3.f/7.f;
#endif
    if(mmi_push(&p->input_queue,&event)!=MMI_OK){atomic_fetch_add(&p->input_rejected,1);return;}
    atomic_store(&p->selected,pad);atomic_store(&p->values[P_ACTIVE_PAD],pad/15.0f);
}
#endif
static void request(plugin *p,int pad) {
#ifdef MM_FIFO_INPUT
    enqueue_input(p,pad,MMJ_CREATE);
#else
    atomic_store(&p->selected,pad);
    atomic_store(&p->values[P_ACTIVE_PAD],pad/15.0f);
    atomic_fetch_add(&p->command,1);
#endif
}
static void open_port(plugin *p) {
#ifdef MM_HOST_TEST
    (void)p; return;
#else
    p->asound=dlopen("libasound.so.2",RTLD_NOW|RTLD_LOCAL); if(!p->asound)return;
#define SYMBOL(field,name) do {void *s=dlsym(p->asound,name);memcpy(&p->field,&s,sizeof s);if(!s)return;}while(0)
    SYMBOL(seq_open,"snd_seq_open");SYMBOL(seq_name,"snd_seq_set_client_name");
    SYMBOL(seq_port,"snd_seq_create_simple_port");SYMBOL(seq_send,"snd_seq_event_output_direct");SYMBOL(seq_close,"snd_seq_close");
#undef SYMBOL
    if(p->seq_open(&p->seq,"default",1,1)<0)return;
    p->seq_name(p->seq,"MPC Melody Maker");
    p->port=p->seq_port(p->seq,"Parts",SNDRV_SEQ_PORT_CAP_READ|SNDRV_SEQ_PORT_CAP_SUBS_READ,
                       SNDRV_SEQ_PORT_TYPE_MIDI_GENERIC|SNDRV_SEQ_PORT_TYPE_APPLICATION);
    if(p->port>=0)atomic_store(&p->port_ready,1);
#endif
}
static void emit(plugin *p,int part,int pitch,int velocity,int on) {
    atomic_fetch_add(&p->note_events,1);
    if(!atomic_load(&p->port_ready))return;
    struct snd_seq_event e;memset(&e,0,sizeof e);
    e.type=on?SNDRV_SEQ_EVENT_NOTEON:SNDRV_SEQ_EVENT_NOTEOFF;e.queue=SNDRV_SEQ_QUEUE_DIRECT;
    if(p->dest_clients[part]==254)return;
    e.source.port=(uint8_t)p->port;e.dest.client=p->dest_clients[part]==253?SNDRV_SEQ_ADDRESS_SUBSCRIBERS:(uint8_t)p->dest_clients[part];
    e.dest.port=(uint8_t)p->dest_ports[part];
    e.data.note.channel=(uint8_t)p->channels[part];e.data.note.note=(uint8_t)pitch;e.data.note.velocity=(uint8_t)velocity;
    atomic_store(&p->qa_send_result,p->seq_send(p->seq,&e));
}
static void panic(plugin *p) {
    for(int part=0;part<3;part++)for(int note=0;note<128;note++) {
        if(p->active[part][note])emit(p,part,note,0,0);
        p->active[part][note]=0;
    }
}
static void effects(plugin *p,const mm_phrase *dry,mm_phrase *wet){
    mm_fx chain[MM_FX_SLOTS];
    for(int slot=0;slot<MM_FX_SLOTS;slot++){
        int index=P_FX_1_ON+slot*4;
        chain[slot]=(mm_fx){integer(p,index),integer(p,index+1),integer(p,index+3),atomic_load(&p->values[index+2])};
    }
    mm_fx_apply(dry,wet,chain);
}
static void refresh_effects(plugin *p){
    pthread_mutex_lock(&p->save_lock);
    for(int pad=0;pad<16;pad++)if(p->dry_valid[pad]){
        unsigned token=atomic_load(&p->published[pad]);int slot=-1;
        for(int i=0;i<3;i++)if((token==UINT32_MAX||(unsigned)i!=token%3)&&atomic_load(&p->reader)!=pad*3+i){slot=i;break;}
        if(slot<0)continue;
        effects(p,&p->dry[pad],&p->phrases[pad][slot]);
        atomic_store(&p->published[pad],token==UINT32_MAX?(unsigned)slot:((token/3)+1)*3+(unsigned)slot);
    }
    pthread_mutex_unlock(&p->save_lock);
}
static const int creation_parameters[8]={P_KEY,P_SCALE,P_BARS,P_SPEED,P_OCTAVE,P_MUTATION,P_RECOMBINATION,P_SUBSTYLE};
static void creation_job(plugin *p,int flags){
    if(flags&48){
        pthread_mutex_lock(&p->save_lock);int pad=atomic_load(&p->selected);
        const mm_history_entry *entry=mm_history_move(&p->history[pad],flags&16?-1:1);
        if(entry){atomic_fetch_add(&p->state_epoch,1);p->dry[pad]=entry->phrase;p->dry_valid[pad]=1;for(int i=0;i<12;i++)atomic_store(&p->values[input_parameters[i]],entry->controls[i]);}
        pthread_mutex_unlock(&p->save_lock);
        if(entry){refresh_effects(p);text(p,"HISTORY LOADED",NULL);}else text(p,"NO MORE HISTORY",NULL);
        flags&=~48;if(!flags)return;
    }

    char directory[1200],path[1400];const char *override=getenv("TTMM_CREATIONS_DIR");
    if(override&&*override)snprintf(directory,sizeof directory,"%s",override);
    else {snprintf(directory,sizeof directory,"%s",p->library);char *slash=strrchr(directory,'/');if(!slash)return;snprintf(slash,sizeof directory-(size_t)(slash-directory),"/creations");}
    if(mkdir(directory,0700)<0&&access(directory,W_OK)){text(p,"CREATION FOLDER ERROR",NULL);return;}
    mm_creation *c=calloc(1,sizeof *c);if(!c){text(p,"CREATION MEMORY ERROR",NULL);return;}
    int mapping[128],count=0,filtered=integer(p,P_BANK_FILTER);
    if(filtered){for(int i=0;i<128;i++){snprintf(path,sizeof path,"%s/creation-%03d.creation",directory,i+1);if(!mm_creation_read(path,c)&&c->favorite)mapping[count++]=i;}}
    else {count=128;for(int i=0;i<128;i++)mapping[i]=i;}
    int position=integer(p,P_BANK_SLOT)-1,slot=position<count?mapping[position]:-1;
    int read=-1;if(slot>=0){snprintf(path,sizeof path,"%s/creation-%03d.creation",directory,slot+1);read=mm_creation_read(path,c);}
    if(flags&1){
        if(!read||slot<0){
            slot=-1;
            for(int empty=0;empty<128;empty++){snprintf(path,sizeof path,"%s/creation-%03d.creation",directory,empty+1);if(access(path,F_OK)){slot=empty;break;}}
            if(slot<0){free(c);text(p,"CREATION BANK FULL",NULL);return;}
            atomic_store(&p->values[P_BANK_FILTER],0);atomic_store(&p->values[P_BANK_SLOT],slot/127.0f);
            read=-1;
        }
        pthread_mutex_lock(&p->save_lock);int pad=atomic_load(&p->selected);
        if(p->dry_valid[pad]){
            int favorite=!read&&c->favorite;memset(c,0,sizeof *c);c->favorite=favorite;c->phrase=p->dry[pad];
            for(int i=0;i<8;i++)c->settings[i]=i==1?(float)integer(p,P_SCALE):atomic_load(&p->values[creation_parameters[i]]);
            for(int i=0;i<MM_FX_SLOTS*4;i++)c->settings[8+i]=atomic_load(&p->values[P_FX_1_ON+i]);
            c->settings[56]=atomic_load(&p->values[P_VARIATION_TYPE]);
            c->settings[57]=atomic_load(&p->values[P_LIGHT_DARK]);
            c->settings[58]=atomic_load(&p->values[P_CANDIDATES]);
            c->settings[63]=1; /* Extension marker: old creation records contain zero. */
            read=mm_creation_write(path,c);text(p,read?"SAVE CREATION FAILED":"CREATION SAVED",NULL);
        }else text(p,"GENERATE A PHRASE FIRST",NULL);
        pthread_mutex_unlock(&p->save_lock);
    }
    if((flags&4)&&!read){c->favorite=!c->favorite;read=mm_creation_write(path,c);text(p,read?"FAVORITE SAVE FAILED":"FAVORITE UPDATED",NULL);}
    if((flags&2)&&!read){
        pthread_mutex_lock(&p->save_lock);int pad=atomic_load(&p->selected);atomic_fetch_add(&p->state_epoch,1);
        for(int i=0;i<8;i++)atomic_store(&p->values[creation_parameters[i]],i==1?c->settings[i]/(MM_SCALE_COUNT-1):c->settings[i]);
        for(int i=0;i<MM_FX_SLOTS*4;i++)atomic_store(&p->values[P_FX_1_ON+i],c->settings[8+i]);
        if(c->settings[63]==1){
            atomic_store(&p->values[P_VARIATION_TYPE],c->settings[56]);
            atomic_store(&p->values[P_LIGHT_DARK],c->settings[57]);
            atomic_store(&p->values[P_CANDIDATES],c->settings[58]);
        }else {
            atomic_store(&p->values[P_VARIATION_TYPE],0);
            atomic_store(&p->values[P_LIGHT_DARK],.5f);
            atomic_store(&p->values[P_CANDIDATES],(16.0f-8)/24);
        }
        p->dry[pad]=c->phrase;p->dry_valid[pad]=1;remember_creation(p,pad);
        pthread_mutex_unlock(&p->save_lock);refresh_effects(p);
        char loaded[64],info[64];snprintf(loaded,sizeof loaded,"LOADED %03d / PAD %d",slot+1,pad+1);
        snprintf(info,sizeof info,"PAD %d / %u NOTES",pad+1,p->dry[pad].parts[0].count);text(p,loaded,info);
    }else if((flags&2)&&read)text(p,"CREATION SLOT EMPTY",NULL);
    pthread_mutex_lock(&p->text_lock);
    snprintf(p->bank_info,sizeof p->bank_info,slot<0?"NO FAVORITE":read?"EMPTY SLOT %03d":"CREATION %03d%s",slot+1,!read&&c->favorite?" *":"");
    pthread_mutex_unlock(&p->text_lock);
    int offset=integer(p,P_BANK_SCROLL);
    for(int row=0;row<7;row++){
        int index=offset+row,actual=index<count?mapping[index]:-1;atomic_store(&p->row_slots[row],index<count?index:-1);
        int result=-1;if(actual>=0){snprintf(path,sizeof path,"%s/creation-%03d.creation",directory,actual+1);result=mm_creation_read(path,c);}
        pthread_mutex_lock(&p->text_lock);
        if(actual<0)snprintf(p->bank_rows[row],24,"--");else snprintf(p->bank_rows[row],24,result?"%03d EMPTY":"%03d CREATION%s",actual+1,!result&&c->favorite?" *":"");
        pthread_mutex_unlock(&p->text_lock);
    }
    free(c);if(flags&5)atomic_fetch_or(&p->bank_commands,8);p->host(&p->fx,42,0,0,NULL,0);
}
static void export_phrase(plugin *p) {
    char directory[1200],path[1250];
    const char *override=getenv("TTMM_EXPORT_DIR");
    if(override&&*override)snprintf(directory,sizeof directory,"%s",override);
    else {
        snprintf(directory,sizeof directory,"%s",p->library);
        char *slash=strrchr(directory,'/');
        if(!slash){text(p,"EXPORT DIRECTORY MISSING",NULL);return;}
        snprintf(slash,sizeof directory-(size_t)(slash-directory),"/exports");
    }
    if(mkdir(directory,0700)<0&&access(directory,W_OK)!=0){text(p,"EXPORT DIRECTORY NOT WRITABLE",NULL);return;}
    int pad=atomic_load(&p->selected);
    pthread_mutex_lock(&p->save_lock);
    unsigned token=atomic_load(&p->published[pad]);
    if(token==UINT32_MAX){pthread_mutex_unlock(&p->save_lock);text(p,"GENERATE A PHRASE FIRST",NULL);return;}
    snprintf(path,sizeof path,"%s/pad-%02d.mid",directory,pad+1);
    int channels[3]={integer(p,P_MELODY_CHANNEL),integer(p,P_CHORD_CHANNEL),integer(p,P_BASS_CHANNEL)};
    int selection=integer(p,P_EXPORT_PARTS);
    mm_phrase *copy=malloc(sizeof *copy);
    if(!copy){pthread_mutex_unlock(&p->save_lock);text(p,"EXPORT MEMORY ERROR",NULL);return;}
    *copy=p->phrases[pad][token%3];
    if(selection)for(int part=0;part<3;part++)if(part!=selection-1)copy->parts[part].count=0;
    const char *names[4]={"all","melody","chords","bass"};
    snprintf(path,sizeof path,"%s/pad-%02d-%s.mid",directory,pad+1,names[selection]);
    double bpm=atomic_load(&p->tempo);
    int rc=mm_write_midi_channels(copy,path,bpm,channels);
    if(!selection&&!rc)for(int part=0;part<3;part++) {
        *copy=p->phrases[pad][token%3];
        for(int other=0;other<3;other++)if(other!=part)copy->parts[other].count=0;
        snprintf(path,sizeof path,"%s/pad-%02d-%s.mid",directory,pad+1,names[part+1]);
        if(mm_write_midi_channels(copy,path,bpm,channels))rc=-1;
    }
    free(copy);
    pthread_mutex_unlock(&p->save_lock);
    char info[64];snprintf(info,sizeof info,"PAD %d / %s",pad+1,names[selection]);
    text(p,rc?"MIDI EXPORT FAILED":"MIDI EXPORTED",info);
}
/* Human-readable ALSA destinations, refreshed outside the audio thread. */
static void refresh_midi_names(plugin *p){
 time_t now=time(NULL);if(now==p->midi_names_time)return;p->midi_names_time=now;
 const int clients[3]={P_MELODY_CLIENT,P_CHORD_CLIENT,P_BASS_CLIENT},ports[3]={P_MELODY_PORT,P_CHORD_PORT,P_BASS_PORT};
 int wanted[3],port[3];char names[3][2][24]={0};
 for(int i=0;i<3;i++){wanted[i]=integer(p,clients[i]);port[i]=integer(p,ports[i]);snprintf(names[i][0],24,"%d: UNAVAILABLE",wanted[i]);snprintf(names[i][1],24,"%d: UNAVAILABLE",port[i]);if(wanted[i]==253){snprintf(names[i][0],24,"SUBSCRIBERS");snprintf(names[i][1],24,"AUTO");}else if(wanted[i]==254){snprintf(names[i][0],24,"DISABLED");snprintf(names[i][1],24,"OFF");}}
 FILE *f=fopen("/proc/asound/seq/clients","r");
 if(f){char line[512],name[256];int client=-1,id;while(fgets(line,sizeof line,f)){
  if(sscanf(line,"Client %d : \"%255[^\"]",&id,name)==2){client=id;for(int i=0;i<3;i++)if(wanted[i]==id)snprintf(names[i][0],24,"%d: %.17s",id,name);}
  else if(sscanf(line," Port %d : \"%255[^\"]",&id,name)==2)for(int i=0;i<3;i++)if(wanted[i]==client&&port[i]==id)snprintf(names[i][1],24,"%d: %.17s",id,name);
 }fclose(f);}
 pthread_mutex_lock(&p->text_lock);if(memcmp(p->midi_names,names,sizeof names)){memcpy(p->midi_names,names,sizeof names);atomic_store(&p->ui_dirty,1);}pthread_mutex_unlock(&p->text_lock);
}
static void *worker(void *pointer) {
    plugin *p=pointer;struct sched_param background_priority={0};pthread_setschedparam(pthread_self(),SCHED_OTHER,&background_priority);char error[128];open_port(p);
#ifndef MM_CANDIDATE_ENGINE
    p->bank=mm_bank_load(p->library,error,sizeof error);
#endif
#ifdef MM_CANDIDATE_ENGINE
    char engine_directory[1200];const char *engine_override=getenv("TTMM_CANDIDATE_DIR");
    if(engine_override&&*engine_override)snprintf(engine_directory,sizeof engine_directory,"%s",engine_override);
    else {snprintf(engine_directory,sizeof engine_directory,"%s",p->library);char *slash=strrchr(engine_directory,'/');if(slash)snprintf(slash,sizeof engine_directory-(size_t)(slash-engine_directory),"/engine");else snprintf(engine_directory,sizeof engine_directory,"private/engine");}
    p->candidate=mmce_open(engine_directory);
    text(p,p->candidate?"CANDIDATE READY":"CANDIDATE LOAD FAILED",NULL);
#else
    text(p,p->bank?"HIP HOP READY":"BANK LOAD FAILED",p->bank?NULL:error);
#endif
    p->host(&p->fx,42,0,0,NULL,0);
    int handled=0,exported=0,fx_handled=0,vary_handled=0;uint32_t seed=(uint32_t)time(NULL)^(uint32_t)(uintptr_t)p;
    creation_job(p,0);
    time_t qa_last=0;int preview_pad=-1;unsigned preview_token=0;
    while(atomic_load(&p->run)) {
        refresh_midi_names(p);
        if(atomic_exchange(&p->ui_dirty,0))p->host(&p->fx,42,0,0,NULL,0);
        int view_pad=atomic_load(&p->selected);unsigned view_token=atomic_load(&p->published[view_pad]);
        if(view_pad!=preview_pad||view_token!=preview_token){
            preview_pad=view_pad;preview_token=view_token;unsigned masks[3][32]={{0}};
            pthread_mutex_lock(&p->save_lock);
            if(view_token!=UINT32_MAX){const mm_phrase *view=&p->phrases[view_pad][view_token%3];int low=127,high=0;
                for(int part=0;part<3;part++)for(unsigned n=0;n<view->parts[part].count;n++){int pitch=view->parts[part].notes[n].pitch;if(pitch<low)low=pitch;if(pitch>high)high=pitch;}
                for(int part=0;part<3;part++)for(unsigned n=0;n<view->parts[part].count;n++){const mm_note *note=&view->parts[part].notes[n];int lane=(note->pitch-low)*6/(high>low?high-low:1);unsigned first=(uint64_t)note->start*32/view->length,last=((uint64_t)(note->start+note->duration)*32+view->length-1)/view->length;if(last>32)last=32;for(unsigned step=first;step<last;step++)masks[part][step]|=1u<<lane;}
                char info[64];snprintf(info,sizeof info,"PAD %d / %u NOTES",view_pad+1,view->parts[0].count);pthread_mutex_lock(&p->text_lock);snprintf(p->info,sizeof p->info,"%s",info);pthread_mutex_unlock(&p->text_lock);
            }else{pthread_mutex_lock(&p->text_lock);snprintf(p->info,sizeof p->info,"PAD %d / EMPTY",view_pad+1);pthread_mutex_unlock(&p->text_lock);}
            pthread_mutex_unlock(&p->save_lock);
            for(int part=0;part<3;part++)for(int step=0;step<32;step++)atomic_store(&p->values[P_PREVIEW_MELODY_0+part*32+step],(masks[part][step]==127 ? 1.0f : masks[part][step] ? (masks[part][step]+0.25f)/127.0f : 0.0f));
            p->host(&p->fx,42,0,0,NULL,0);
        }
        time_t qa_now=time(NULL);
        if(qa_now!=qa_last){qa_last=qa_now;
            if(!access("/tmp/ttmm-debug.enable",F_OK)){
                FILE *qa=fopen("/tmp/ttmm-playback.log","a");
                if(qa){int pad=atomic_load(&p->selected);fprintf(qa,"time=%ld blocks=%u flags=%x ppq=%.3f play=%d melody=%d pad=%d phrase=%u events=%u send=%d client=%d port=%d channel=%d\n",(long)qa_now,atomic_load(&p->qa_blocks),atomic_load(&p->qa_time_flags),(double)atomic_load(&p->qa_ppq),integer(p,P_PLAY),integer(p,P_MELODY_ENABLED),pad+1,atomic_load(&p->published[pad]),atomic_load(&p->note_events),atomic_load(&p->qa_send_result),integer(p,P_MELODY_CLIENT),integer(p,P_MELODY_PORT),integer(p,P_MELODY_CHANNEL));fclose(qa);}
            }
        }
        int bank_commands=atomic_exchange(&p->bank_commands,0);if(bank_commands)creation_job(p,bank_commands);
        int fx_command=atomic_load(&p->fx_command);
        if(fx_command!=fx_handled){fx_handled=fx_command;refresh_effects(p);}
        int export_command=atomic_load(&p->export_command);
        if(export_command!=exported){exported=export_command;export_phrase(p);p->host(&p->fx,0,P_EXPORT,0,NULL,0);p->host(&p->fx,42,0,0,NULL,0);}
#ifdef MM_FIFO_INPUT
        mmi_event input_event;
        if(mmi_take(&p->input_queue,&input_event)!=MMI_OK){usleep(1000);continue;}
        int varying=input_event.action==MMJ_VARIATION,pad=(int)input_event.pad,epoch=(int)input_event.epoch;
        pthread_mutex_lock(&p->save_lock);
        if(epoch!=atomic_load(&p->state_epoch)){pthread_mutex_unlock(&p->save_lock);continue;}
        if(p->pad_touches[pad]==UINT32_MAX){pthread_mutex_unlock(&p->save_lock);text(p,"PAD SEED EXHAUSTED",NULL);continue;}
        if(atomic_load(&p->fixed[pad])){pthread_mutex_unlock(&p->save_lock);text(p,"PAD FIXED",NULL);continue;}
        uint32_t job_seed=mmj_seed(p->pad_seeds[pad],++p->pad_touches[pad]);
        pthread_mutex_unlock(&p->save_lock);
#else
        int command=atomic_load(&p->command),vary=atomic_load(&p->vary_command);
        int varying=vary!=vary_handled;vary_handled=vary;
        if(command==handled&&!varying){usleep(10000);continue;}
        handled=command;int pad=atomic_load(&p->selected),epoch=atomic_load(&p->state_epoch);
#endif
#ifdef MM_CANDIDATE_ENGINE
        if(!p->candidate){text(p,"CANDIDATE LOAD FAILED",NULL);continue;}
#else
        if(!p->bank&&!varying){text(p,"PRIVATE LIBRARY MISSING",NULL);p->host(&p->fx,42,0,0,NULL,0);continue;}
#endif
        atomic_store(&p->busy,1);
        char gen_msg[48]; snprintf(gen_msg, sizeof gen_msg, "GENERATING %s", ttmm_genre_name(integer(p,P_STYLE)));
        text(p,gen_msg,NULL);p->host(&p->fx,42,0,0,NULL,0);
        pthread_mutex_lock(&p->save_lock);
        if(epoch!=atomic_load(&p->state_epoch)){pthread_mutex_unlock(&p->save_lock);atomic_store(&p->busy,0);continue;}
        unsigned token=atomic_load(&p->published[pad]);int slot=-1;
        for(int i=0;i<3;i++)if((token==UINT32_MAX||(unsigned)i!=token%3)&&atomic_load(&p->reader)!=pad*3+i){slot=i;break;}
        mm_request r={.seed=++seed,.key=integer(p,P_KEY),.scale=integer(p,P_SCALE),.bars=integer(p,P_BARS),
                      .speed=integer(p,P_SPEED),.pad=pad,.octave=integer(p,P_OCTAVE),
                      .mutation=atomic_load(&p->values[P_MUTATION]),.recombination=atomic_load(&p->values[P_RECOMBINATION]),
                      .substyle=integer(p,P_SUBSTYLE)?mm_params[P_SUBSTYLE].opts[integer(p,P_SUBSTYLE)]:NULL,
                      .genre=integer(p,P_STYLE)};
#ifdef MM_FIFO_INPUT
        int captured[12];
        for(int i=0;i<12;i++){const mm_param *def=&mm_params[input_parameters[i]];captured[i]=(int)lroundf(def->min+input_event.parameters[i]*(def->max-def->min));}
        r.seed=job_seed;r.key=captured[0];r.scale=captured[1];r.bars=captured[2];r.speed=captured[3];r.octave=captured[4];
        r.mutation=input_event.parameters[5];r.recombination=input_event.parameters[6];
        r.substyle=captured[7]?mm_params[P_SUBSTYLE].opts[captured[7]]:NULL;
        r.genre=captured[11];
#endif
        int rc=-1;
#ifdef MM_CANDIDATE_ENGINE
        int current_valid=p->dry_valid[pad];if(current_valid)p->candidate_current=p->dry[pad];
        pthread_mutex_unlock(&p->save_lock);
        static const int source_complexity[4]={0,1,3,4};
        if(varying)rc=current_valid?mmce_vary(p->candidate,&r,captured[8],(int)lroundf(input_event.parameters[9]*1000),captured[10],&p->candidate_current,&p->candidate_output):6;
        else rc=mmce_create(p->candidate,&r,pad/4>=2?1:0,source_complexity[pad%4],captured[10],&p->candidate_output);
        atomic_store(&p->candidate_status,rc);
        atomic_store(&p->candidate_diagnostic,mmce_generation_diagnostic(p->candidate));
        pthread_mutex_lock(&p->save_lock);
        token=atomic_load(&p->published[pad]);slot=-1;
        for(int i=0;i<3;i++)if((token==UINT32_MAX||(unsigned)i!=token%3)&&atomic_load(&p->reader)!=pad*3+i){slot=i;break;}
        if(!rc&&slot>=0)p->phrases[pad][slot]=p->candidate_output;else if(!rc)rc=4;
#else
        if(slot>=0){
            if(varying&&p->dry_valid[pad]){p->phrases[pad][slot]=p->dry[pad];rc=mm_vary_family(p->bank,&p->phrases[pad][slot],&r);}
            else if(!varying)rc=mm_generate(p->bank,&r,&p->phrases[pad][slot]);
        }
#endif
#ifdef MM_FIFO_INPUT
        if(!rc&&epoch==atomic_load(&p->state_epoch)&&!atomic_load(&p->fixed[pad])) {
#else
        if(!rc&&command==atomic_load(&p->command)&&!atomic_load(&p->fixed[pad])) {
#endif
            p->dry[pad]=p->phrases[pad][slot];p->dry_valid[pad]=1;remember_creation(p,pad);
#ifdef MM_FIFO_INPUT
            if(p->history[pad].entries)memcpy(p->history[pad].entries[p->history[pad].cursor].controls,input_event.parameters,sizeof input_event.parameters);
#endif
            effects(p,&p->dry[pad],&p->phrases[pad][slot]);
            unsigned next=token==UINT32_MAX?(unsigned)slot:((token/3)+1)*3+(unsigned)slot;
            atomic_store(&p->published[pad],next);
            char info[64];snprintf(info,sizeof info,"PAD %d / %u NOTES",pad+1,p->phrases[pad][slot].parts[0].count);
            text(p,atomic_load(&p->port_ready)?"HIP HOP READY":"READY / MIDI PORT OFF",info);
        }else if(rc){
#ifdef MM_CANDIDATE_ENGINE
            text(p,mmce_error(rc),NULL);
#else
            text(p,varying?"NO FAMILY / CREATE NEW":"GENERATION FAILED",NULL);
#endif
        }
        pthread_mutex_unlock(&p->save_lock);atomic_store(&p->busy,0);
        for(int i=0;i<16;i++)p->host(&p->fx,0,P_PAD_1+i,0,NULL,0.0f);
        p->host(&p->fx,0,P_GENERATE,0,NULL,0.0f);
        p->host(&p->fx,42,0,0,NULL,0);
    }
#ifdef MM_CANDIDATE_ENGINE
    mmce_close(p->candidate);p->candidate=NULL;
#endif
    return NULL;
}
static int event_order(const void *x,const void *y) {
    const scheduled *a=x,*b=y;
    if(a->when!=b->when)return a->when<b->when?-1:1;
    return (int)a->on-b->on;
}
static void process(AEffect *fx,float **input,float **output,int32_t frames) {
    (void)input;plugin *p=fx->object;
    if(frames<=0)return;
    for(int ch=0;ch<2;ch++)if(output&&output[ch])memset(output[ch],0,(size_t)frames*sizeof(float));
    vst_time *t=(vst_time *)p->host(fx,7,0,(1<<9)|(1<<10),NULL,0);
    atomic_fetch_add(&p->qa_blocks,1);atomic_store(&p->qa_time_flags,t?t->flags:0);atomic_store(&p->qa_ppq,t?(float)t->ppqPos:0);
    if(t&&(t->flags&(1<<10))&&isfinite(t->tempo)&&t->tempo>=20&&t->tempo<=400)atomic_store(&p->tempo,(float)t->tempo);
    int playing=t&&(t->flags&(1<<1))&&(t->flags&(1<<9))&&(t->flags&(1<<10))&&
                isfinite(t->tempo)&&t->tempo>=20&&t->tempo<=400&&isfinite(t->ppqPos)&&integer(p,P_PLAY);
    int pad=atomic_load(&p->selected);unsigned token=atomic_load(&p->published[pad]);
    if(!playing||token==UINT32_MAX){panic(p);p->was_playing=0;return;}
    int index=pad*3+(int)(token%3);atomic_store(&p->reader,index);
    if(token!=atomic_load(&p->published[pad])){atomic_store(&p->reader,-1);return;}
    const mm_phrase *phrase=&p->phrases[pad][token%3];
    int changed=0;int channels[3]={integer(p,P_MELODY_CHANNEL)-1,integer(p,P_CHORD_CHANNEL)-1,integer(p,P_BASS_CHANNEL)-1};
    int enabled[3]={integer(p,P_MELODY_ENABLED),integer(p,P_CHORD_ENABLED),integer(p,P_BASS_ENABLED)};
    int clients[3]={integer(p,P_MELODY_CLIENT),integer(p,P_CHORD_CLIENT),integer(p,P_BASS_CLIENT)};
    int ports[3]={integer(p,P_MELODY_PORT),integer(p,P_CHORD_PORT),integer(p,P_BASS_PORT)};
    for(int part=0;part<3;part++)if(channels[part]!=p->channels[part]||clients[part]!=p->dest_clients[part]||ports[part]!=p->dest_ports[part]||enabled[part]!=p->enabled[part])changed=1;
    if(!p->was_playing||pad!=p->last_pad||token!=p->last_token||changed||fabs(t->ppqPos-p->last_end)>.02)panic(p);
    memcpy(p->channels,channels,sizeof channels);
    memcpy(p->enabled,enabled,sizeof enabled);memcpy(p->dest_clients,clients,sizeof clients);memcpy(p->dest_ports,ports,sizeof ports);p->last_pad=pad;p->last_token=token;p->was_playing=1;
    double span=frames/p->rate*t->tempo/60.0,loop=phrase->length/(double)MM_PPQ;
    if(!isfinite(span)||span>=loop){panic(p);p->was_playing=0;atomic_store(&p->reader,-1);return;}
    double start=fmod(t->ppqPos,loop);if(start<0)start+=loop;
    unsigned count=0;int overflow=0;
    for(int part=0;part<3;part++)for(unsigned i=0;i<phrase->parts[part].count;i++) {
        if(!enabled[part])continue;
        const mm_note *n=&phrase->parts[part].notes[i];
        for(int on=0;on<2;on++) {
            double when=fmod((n->start+(on?0:n->duration))/(double)MM_PPQ,loop);
            double offset=when-start;if(offset<0)offset+=loop;
            if(offset>=0&&offset<span) {
                if(count>=512){overflow=1;continue;}
                p->events[count++]=(scheduled){offset,(uint8_t)part,n->pitch,n->velocity,(uint8_t)on};
            }
        }
    }
    if(overflow)panic(p);
    else {
        qsort(p->events,count,sizeof(scheduled),event_order);
        for(unsigned i=0;i<count;i++) {
            const scheduled *e=&p->events[i];uint8_t *active=&p->active[e->part][e->pitch];
            if(e->on){
                if(e->part!=1)for(int old=0;old<128;old++)if(p->active[e->part][old]){
                    emit(p,e->part,old,0,0);p->active[e->part][old]=0;
                }
                emit(p,e->part,e->pitch,e->velocity,1);if(*active<255)(*active)++;
            }
            else if(*active){emit(p,e->part,e->pitch,0,0);(*active)--;}
        }
    }
    p->last_end=t->ppqPos+span;atomic_store(&p->reader,-1);
}
static void set_parameter(AEffect *fx,int32_t index,float value) {
    plugin *p=fx->object;if(index<0||index>=MM_PARAM_COUNT||!isfinite(value))return;
    atomic_store(&p->ui_dirty,1);
#ifdef MM_CANDIDATE_ENGINE
    if(index==P_BARS)value=3.f/7.f;
#endif
    if(index==P_FX_PAGE_PREVIOUS||index==P_FX_PAGE_NEXT){if(value>.5f){int page=fx_page(p)+(index==P_FX_PAGE_NEXT?1:-1);if(page<1)page=1;if(page>fx_pages(p))page=fx_pages(p);atomic_store(&p->values[P_FX_PAGE],(page-1)/3.f);}return;}
    if(index==P_FX_ADD){if(value>.5f){int last=-1;for(int i=0;i<MM_FX_SLOTS;i++)if(atomic_load(&p->values[P_FX_1_ON+i*4])>.5f)last=i;if(last+1<MM_FX_SLOTS){int slot=last+1;atomic_store(&p->values[P_FX_1_ON+slot*4],1);atomic_store(&p->values[P_FX_PAGE],(slot/3)/3.f);atomic_fetch_add(&p->fx_command,1);}else text(p,"FX CHAIN FULL / 12",NULL);}return;}
    if(index>=P_EXPORT_ALL&&index<=P_EXPORT_BASS){if(value>.5f){atomic_store(&p->values[P_EXPORT_PARTS],(index-P_EXPORT_ALL)/3.f);atomic_fetch_add(&p->export_command,1);}return;}
    if(index==P_FX_PAGE){int page=(int)lroundf(value*3)+1;if(page>fx_pages(p))page=fx_pages(p);atomic_store(&p->values[index],(page-1)/3.f);return;}
    index=fx_parameter(p,index);
    if(value<0)value=0;
    if(value>1)value=1;
    int kind=mm_params[index].kind;
    if(kind==MM_TEXT)return;
    if(index==P_PAD_FIXED){atomic_store(&p->fixed[atomic_load(&p->selected)],value>.5f);return;}
    if(kind==MM_TRIGGER||kind==MM_PAD) {
        if(index==P_HISTORY_BACK||index==P_HISTORY_FORWARD){if(value>.5f)atomic_fetch_or(&p->bank_commands,index==P_HISTORY_BACK?16:32);return;}

        if(index==P_BANK_UP||index==P_BANK_DOWN){
            if(value>.5f){int offset=integer(p,P_BANK_SCROLL)+(index==P_BANK_DOWN?7:-7);if(offset<0)offset=0;if(offset>122)offset=122;
                atomic_store(&p->values[P_BANK_SCROLL],offset/122.0f);atomic_fetch_or(&p->bank_commands,8);}
            return;
        }
        if(index>=P_BANK_ROW_1&&index<=P_BANK_ROW_7){
            if(value>.5f){int selected=atomic_load(&p->row_slots[index-P_BANK_ROW_1]);if(selected>=0){atomic_store(&p->values[P_BANK_SLOT],selected/127.0f);atomic_fetch_or(&p->bank_commands,2);}}return;
        }
        if(index==P_BANK_SAVE||index==P_BANK_LOAD||index==P_BANK_FAVORITE){if(value>.5f)atomic_fetch_or(&p->bank_commands,index==P_BANK_SAVE?1:index==P_BANK_LOAD?2:4);return;}
        if(index==P_VARY_MOTIF){if(value>.5f){
#ifdef MM_FIFO_INPUT
            enqueue_input(p,atomic_load(&p->selected),MMJ_VARIATION);
#else
            atomic_fetch_add(&p->vary_command,1);
#endif
        }return;}
        if(index==P_EXPORT){if(value>.5f)atomic_fetch_add(&p->export_command,1);return;}
        /* Momentary trigger: reset the host value after generation so every tap sends a fresh rising edge. Selection is stored separately. */
        if(kind==MM_PAD){if(value>.5f)request(p,index-P_PAD_1);}
        else if(value>.5f)request(p,atomic_load(&p->selected));
        return;
    }
    atomic_store(&p->values[index],value);
    if(index==P_BANK_SLOT||index==P_BANK_FILTER||index==P_BANK_SCROLL)atomic_fetch_or(&p->bank_commands,8);
    if(index>=P_FX_1_ON&&index<P_VARY_MOTIF)atomic_fetch_add(&p->fx_command,1);
    if(index==P_ACTIVE_PAD)atomic_store(&p->selected,integer(p,index));
}
static float get_parameter(AEffect *fx,int32_t index) {
    plugin *p=fx->object;if(index<0||index>=MM_PARAM_COUNT)return 0;
#ifdef MM_CANDIDATE_ENGINE
    if(index==P_BARS)return 3.f/7.f;
#endif
    if(index==P_FX_PAGE)return (fx_page(p)-1)/3.f;
    index=fx_parameter(p,index);
    if(index==P_PAD_FIXED)return atomic_load(&p->fixed[atomic_load(&p->selected)])?1.f:0.f;
    int kind=mm_params[index].kind;
    if(index>=P_PREVIEW_MELODY_0&&index<P_PREVIEW_MELODY_0+96)return atomic_load(&p->values[index]);
    if(index>=P_BANK_ROW_1&&index<=P_BANK_ROW_7)return atomic_load(&p->row_slots[index-P_BANK_ROW_1])==integer(p,P_BANK_SLOT)-1?1:0;
    if(kind==MM_PAD)return 0;
    if(kind==MM_TRIGGER||kind==MM_TEXT)return 0;
    return atomic_load(&p->values[index]);
}
/* Portable project chunks: parameters and all 16 pad phrases, independent of the private bank. */
static void encode32(unsigned char **cursor,uint32_t n) {
    for(int i=0;i<4;i++)*(*cursor)++=(uint8_t)(n>>(i*8));
}
static uint32_t decode32(const unsigned char **cursor) {
    uint32_t n=0;for(int i=0;i<4;i++)n|=(uint32_t)*(*cursor)++<<(i*8);return n;
}
static intptr_t save(plugin *p,void **out) {
    size_t capacity=32+192+MM_PARAM_COUNT*4+16*(60+MM_PARTS*4+MM_PARTS*MM_MAX_NOTES*12);
    unsigned char *data=malloc(capacity);if(!data)return 0;
    pthread_mutex_lock(&p->save_lock);unsigned char *cursor=data;
    #ifdef MM_FIFO_INPUT
    memcpy(cursor,"TTMMST04",8);
#else
    memcpy(cursor,"TTMMST02",8);
#endif
    cursor+=8;encode32(&cursor,MM_PARAM_COUNT);
    for(int i=0;i<MM_PARAM_COUNT;i++) {float value=(i>=P_PREVIEW_MELODY_0&&i<P_PREVIEW_MELODY_0+96)?0:i==P_SCALE?(float)integer(p,P_SCALE):get_parameter(&p->fx,i);uint32_t bits;memcpy(&bits,&value,4);encode32(&cursor,bits);}
    for(int pad=0;pad<16;pad++) {
        unsigned token=atomic_load(&p->published[pad]);encode32(&cursor,token==UINT32_MAX?0:1);
        if(token==UINT32_MAX)continue;
        const mm_phrase *phrase=p->dry_valid[pad]?&p->dry[pad]:&p->phrases[pad][token%3];memcpy(cursor,phrase->source_id,48);cursor+=48;encode32(&cursor,phrase->length);
        for(int part=0;part<3;part++) {
            encode32(&cursor,phrase->parts[part].count);
            for(unsigned i=0;i<phrase->parts[part].count;i++) {
                const mm_note *n=&phrase->parts[part].notes[i];encode32(&cursor,n->start);encode32(&cursor,n->duration);
                encode32(&cursor,(uint32_t)n->pitch|((uint32_t)n->velocity<<8));
            }
        }
    }
#ifdef MM_FIFO_INPUT
    for(int pad=0;pad<16;pad++){encode32(&cursor,p->pad_seeds[pad]);encode32(&cursor,atomic_load(&p->pad_touches[pad]));}
    for(int pad=0;pad<16;pad++)encode32(&cursor,atomic_load(&p->fixed[pad]));
#endif
    pthread_mutex_unlock(&p->save_lock);free(p->chunk);p->chunk=(char *)data;*out=data;return cursor-data;
}
static int restore(plugin *p,const void *data,intptr_t size) {
    if(size<12||size>2000000||(memcmp(data,"TTMMST01",8)&&memcmp(data,"TTMMST02",8)&&memcmp(data,"TTMMST03",8)&&memcmp(data,"TTMMST04",8)))return 0;
    int legacy_scales=!memcmp(data,"TTMMST01",8);
    int pinned=!memcmp(data,"TTMMST04",8);int seeded=!memcmp(data,"TTMMST03",8)||pinned;uint32_t saved_fixed[16]={0};
    uint32_t saved_seeds[16],saved_touches[16];
    const unsigned char *cursor=(const unsigned char *)data+8,*end=(const unsigned char *)data+size;
    unsigned saved_count=decode32(&cursor);
    if(saved_count!=32&&saved_count!=34&&saved_count!=40&&saved_count!=41&&saved_count!=90&&saved_count!=93&&saved_count!=107&&saved_count!=203&&saved_count!=205&&saved_count!=208&&saved_count!=211&&saved_count!=231&&saved_count!=MM_PARAM_COUNT)return 0;
    float values[MM_PARAM_COUNT];
    for(int i=0;i<MM_PARAM_COUNT;i++)values[i]=(mm_params[i].def-mm_params[i].min)/(mm_params[i].max-mm_params[i].min);
    mm_phrase *phrases=calloc(16,sizeof(mm_phrase));uint8_t valid[16]={0};if(!phrases)return 0;
#define NEED(n) do {if((size_t)(end-cursor)<(n))goto invalid;}while(0)
    for(unsigned i=0;i<saved_count;i++) {NEED(4);uint32_t bits=decode32(&cursor);memcpy(&values[i],&bits,4);float maximum=(i==P_SCALE&&!legacy_scales)?MM_SCALE_COUNT-1:1;
        if(!isfinite(values[i])||values[i]<0||values[i]>maximum)goto invalid;
        if(i==P_SCALE&&!legacy_scales&&values[i]!=roundf(values[i]))goto invalid;}
    /* Old chunks encoded natural minor as 1.0. Preserve its meaning in the expanded enum. */
    values[P_SCALE]=(legacy_scales?(float)lroundf(values[P_SCALE]):values[P_SCALE])/(MM_SCALE_COUNT-1);
    for(int pad=0;pad<16;pad++) {
        NEED(4);uint32_t present=decode32(&cursor);if(present>1)goto invalid;if(!present)continue;
        valid[pad]=1;mm_phrase *phrase=&phrases[pad];NEED(52);memcpy(phrase->source_id,cursor,48);phrase->source_id[47]=0;cursor+=48;
        phrase->length=decode32(&cursor);if(!phrase->length||phrase->length>MM_PPQ*32)goto invalid;
        for(int part=0;part<3;part++) {
            NEED(4);uint32_t count=decode32(&cursor);if(count>MM_MAX_NOTES)goto invalid;phrase->parts[part].count=count;
            for(unsigned i=0;i<count;i++) {
                NEED(12);mm_note *n=&phrase->parts[part].notes[i];n->start=decode32(&cursor);n->duration=decode32(&cursor);uint32_t pv=decode32(&cursor);
                if(pv>0x7f7f||!(pv>>8))goto invalid;
                n->pitch=pv&255;n->velocity=pv>>8;
                if(n->pitch>127||n->velocity>127||!n->duration||n->start>=phrase->length||n->duration>phrase->length-n->start)goto invalid;
            }
        }
        if(!phrase->parts[0].count)goto invalid;
    }
    if(seeded){for(int pad=0;pad<16;pad++){NEED(8);saved_seeds[pad]=decode32(&cursor);saved_touches[pad]=decode32(&cursor);}}
    if(pinned){for(int pad=0;pad<16;pad++){NEED(4);saved_fixed[pad]=decode32(&cursor);if(saved_fixed[pad]>1)goto invalid;}}
    if(cursor!=end)goto invalid;
    pthread_mutex_lock(&p->save_lock);atomic_fetch_add(&p->state_epoch,1);
#ifdef MM_FIFO_INPUT
    for(int pad=0;pad<16;pad++){p->pad_seeds[pad]=seeded?saved_seeds[pad]:mmj_seed(0x544d4d31u,pad);atomic_store(&p->pad_touches[pad],seeded?saved_touches[pad]:0);}
#endif
    for(int i=0;i<MM_PARAM_COUNT;i++)if(!(i>=P_PREVIEW_MELODY_0&&i<P_PREVIEW_MELODY_0+96)&&mm_params[i].kind!=MM_PAD&&mm_params[i].kind!=MM_TRIGGER&&mm_params[i].kind!=MM_TEXT)atomic_store(&p->values[i],values[i]);
    atomic_store(&p->selected,integer(p,P_ACTIVE_PAD));
    for(int pad=0;pad<16;pad++) {
        atomic_store(&p->fixed[pad],saved_fixed[pad]);mm_history_clear(&p->history[pad]);p->dry_valid[pad]=valid[pad];
        if(!valid[pad]){atomic_store(&p->published[pad],UINT32_MAX);continue;}
        unsigned token=atomic_load(&p->published[pad]);int slot=0;
        for(int s=0;s<3;s++)if((token==UINT32_MAX||(unsigned)s!=token%3)&&atomic_load(&p->reader)!=pad*3+s){slot=s;break;}
        p->dry[pad]=phrases[pad];remember_creation(p,pad);effects(p,&p->dry[pad],&p->phrases[pad][slot]);atomic_store(&p->published[pad],token==UINT32_MAX?(unsigned)slot:((token/3)+1)*3+(unsigned)slot);
    }
    pthread_mutex_unlock(&p->save_lock);free(phrases);text(p,"PROJECT RESTORED",NULL);return 1;
invalid:
    free(phrases);return 0;
#undef NEED
}
static intptr_t dispatch(AEffect *fx,int32_t op,int32_t index,intptr_t value,void *ptr,float opt) {
    plugin *p=fx->object;
    switch(op) {
    case 0:return 0;
    case 1:atomic_store(&p->run,0);pthread_join(p->worker,NULL);panic(p);
        if(p->seq&&p->seq_close)p->seq_close(p->seq);
        if(p->asound)dlclose(p->asound);
        mm_bank_free(p->bank);pthread_mutex_destroy(&p->save_lock);pthread_mutex_destroy(&p->text_lock);free(p->chunk);for(int pad=0;pad<16;pad++)mm_history_close(&p->history[pad]);free(p);return 0;
    case 6:if(ptr)((char *)ptr)[0]=0;return 0;
    case 7:
        if(!ptr||index<0||index>=MM_PARAM_COUNT)return 0;
        if(index==P_FX_PAGE){snprintf(ptr,24,"%d / %d",fx_page(p),fx_pages(p));return 0;}
        index=fx_parameter(p,index);
        if(index>=P_BANK_ROW_1&&index<=P_BANK_ROW_7){pthread_mutex_lock(&p->text_lock);snprintf(ptr,24,"%s",p->bank_rows[index-P_BANK_ROW_1]);pthread_mutex_unlock(&p->text_lock);}
        else if(index==P_MELODY_CLIENT||index==P_MELODY_PORT||index==P_CHORD_CLIENT||index==P_CHORD_PORT||index==P_BASS_CLIENT||index==P_BASS_PORT){int part=index==P_MELODY_CLIENT||index==P_MELODY_PORT?0:index==P_CHORD_CLIENT||index==P_CHORD_PORT?1:2;int port=index==P_MELODY_PORT||index==P_CHORD_PORT||index==P_BASS_PORT;pthread_mutex_lock(&p->text_lock);snprintf(ptr,24,"%s",p->midi_names[part][port]);pthread_mutex_unlock(&p->text_lock);}
        else if(mm_params[index].kind==MM_TEXT) {pthread_mutex_lock(&p->text_lock);snprintf(ptr,24,"%.23s",index==P_STATUS?p->status:index==P_BANK_INFO?p->bank_info:p->info);pthread_mutex_unlock(&p->text_lock);}
        else if(mm_params[index].nopts)snprintf(ptr,24,"%s",mm_params[index].opts[integer(p,index)]);
        else if(mm_params[index].kind==MM_INTEGER)snprintf(ptr,24,"%d",integer(p,index)+(index==P_ACTIVE_PAD));
        else snprintf(ptr,24,"%d%%",(int)lroundf(get_parameter(fx,index)*100));
        return 0;
    case 8:if(ptr&&index>=0&&index<MM_PARAM_COUNT){if(index>=P_FX_VIEW_1_ON&&index<=P_FX_VIEW_3_TARGET&&(index-P_FX_VIEW_1_ON)%4==0)snprintf(ptr,24,"FX %d",(fx_page(p)-1)*3+(index-P_FX_VIEW_1_ON)/4+1);else snprintf(ptr,24,"%s",mm_params[index].name);}return 0;
    case 10:if(isfinite(opt)&&opt>=8000&&opt<=192000)p->rate=opt;return 0;
    case 12:if(!value)panic(p);return 0;
    case 23:return ptr?save(p,ptr):0;
    case 24:return ptr&&value>0?restore(p,ptr,value):0;
    case 25: {
        if(!ptr)return 0;
        vst_events *events=ptr;if(events->count<0||events->count>1024)return 0;
        for(int i=0;i<events->count;i++) {
            vst_midi *e=events->events[i];if(!e||e->type!=1||e->byteSize<(int)sizeof(vst_midi))continue;
            /* Input channel 1 is reserved for controls. Generated parts default to 2/3/4. */
            if(e->midiData[0]==0x90&&e->midiData[2]) {
                int pad=(int)e->midiData[1]-integer(p,P_PAD_BASE);if(pad>=0&&pad<16)request(p,pad);
            }
        }
        return 1;
    }
    case 26:return index>=0&&index<MM_PARAM_COUNT&&!(index>=P_PREVIEW_MELODY_0&&index<P_PREVIEW_MELODY_0+96)&&mm_params[index].kind!=MM_TEXT;
    case 35:return 2;
    case 45:case 48:if(ptr)snprintf(ptr,32,"MPC Melody Maker");return 1;
    case 47:if(ptr)snprintf(ptr,16,"TTplugs");return 1;
    case 49:return 100;
    case 51:return ptr&&(!strcmp(ptr,"receiveVstEvents")||!strcmp(ptr,"receiveVstMidiEvent")||!strcmp(ptr,"receiveVstTimeInfo"))?1:-1;
    case 58:return 2400;
    }
    return 0;
}
PUBLIC AEffect *VSTPluginMain(host_callback host) {
    if(!host)return NULL;
    plugin *p=calloc(1,sizeof *p);if(!p)return NULL;
#ifdef MM_FIFO_INPUT
    mmi_init(&p->input_queue);atomic_init(&p->input_rejected,0);
    for(unsigned pad=0;pad<16;pad++){p->pad_seeds[pad]=mmj_seed(0x544d4d31u,pad);atomic_init(&p->pad_touches[pad],0);}
#endif
#ifdef MM_CANDIDATE_ENGINE
    atomic_init(&p->candidate_status,-1);atomic_init(&p->candidate_diagnostic,-1);
#endif
    p->host=host;p->rate=44100;p->last_pad=-1;p->channels[0]=1;p->channels[1]=2;p->channels[2]=3;
    for(int part=0;part<3;part++)p->dest_clients[part]=253;
    pthread_mutex_init(&p->save_lock,NULL);pthread_mutex_init(&p->text_lock,NULL);
    for(int i=0;i<MM_PARAM_COUNT;i++)atomic_init(&p->values[i],(mm_params[i].def-mm_params[i].min)/(mm_params[i].max-mm_params[i].min));
    for(int pad=0;pad<16;pad++){atomic_init(&p->published[pad],UINT32_MAX);atomic_init(&p->fixed[pad],0);}
    atomic_init(&p->ui_dirty,0);atomic_init(&p->reader,-1);atomic_init(&p->run,1);
    atomic_init(&p->busy,0);atomic_init(&p->command,0);atomic_init(&p->selected,0);
    atomic_init(&p->port_ready,0);atomic_init(&p->state_epoch,0);atomic_init(&p->note_events,0);atomic_init(&p->qa_blocks,0);atomic_init(&p->qa_time_flags,0);atomic_init(&p->qa_send_result,0);atomic_init(&p->qa_ppq,0);
    atomic_init(&p->bank_commands,0);for(int row=0;row<7;row++)atomic_init(&p->row_slots[row],row);
    atomic_init(&p->fx_command,0);atomic_init(&p->vary_command,0);
    atomic_init(&p->export_command,0);atomic_init(&p->tempo,90.0f);
    snprintf(p->status,sizeof p->status,"LOADING PRIVATE LIBRARY");
    const char *library=getenv("TTMM_LIBRARY");
    if(library&&*library)snprintf(p->library,sizeof p->library,"%s",library);
    else {
        Dl_info info;char path[1024]=".";
        if(dladdr(&module_anchor,&info)&&info.dli_fname){snprintf(path,sizeof path,"%s",info.dli_fname);char *slash=strrchr(path,'/');if(slash)*slash=0;}
        snprintf(p->library,sizeof p->library,"%.990s/private/hiphop.ttbank",path);
        if(access(p->library,R_OK))snprintf(p->library,sizeof p->library,"%.990s/private/hiphop.bank",path);
    }
    p->fx.magic=0x56737450;p->fx.dispatcher=dispatch;p->fx.process=process;p->fx.processReplacing=process;
    p->fx.setParameter=set_parameter;p->fx.getParameter=get_parameter;p->fx.numParams=MM_PARAM_COUNT;
    p->fx.numOutputs=2;p->fx.flags=(1<<4)|(1<<5)|(1<<8);p->fx.object=p;p->fx.uniqueID=0x544d4d31;p->fx.version=100;
    if(pthread_create(&p->worker,NULL,worker,p)){pthread_mutex_destroy(&p->save_lock);pthread_mutex_destroy(&p->text_lock);free(p);return NULL;}
    return &p->fx;
}
#if defined(MM_HOST_TEST) && defined(MM_FIFO_INPUT)
PUBLIC unsigned mm_test_input_rejected(AEffect *fx){return atomic_load(&((plugin *)fx->object)->input_rejected);}
PUBLIC unsigned mm_test_pad_touches(AEffect *fx,int pad){return pad>=0&&pad<16?atomic_load(&((plugin *)fx->object)->pad_touches[pad]):0;}
#endif
#if defined(MM_HOST_TEST) && defined(MM_CANDIDATE_ENGINE)
PUBLIC int mm_test_candidate_diagnostic(AEffect *fx){return atomic_load(&((plugin *)fx->object)->candidate_diagnostic);}
PUBLIC int mm_test_candidate_status(AEffect *fx){return atomic_load(&((plugin *)fx->object)->candidate_status);}
#endif
#ifdef MM_HOST_TEST
PUBLIC unsigned mm_test_events(AEffect *fx){return atomic_load(&((plugin *)fx->object)->note_events);}
PUBLIC int mm_test_busy(AEffect *fx){return atomic_load(&((plugin *)fx->object)->busy);}
#endif
