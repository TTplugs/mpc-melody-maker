#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
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

static intptr_t host(AEffect *fx,int32_t op,int32_t index,intptr_t value,void *ptr,float opt){(void)fx;(void)index;(void)value;(void)ptr;(void)opt;return op==1?2400:0;}
int main(int argc,char **argv){
    if(argc!=2)return 2;void *module=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!module){fprintf(stderr,"%s\n",dlerror());return 1;}
    AEffect *(*entry)(host_callback)=NULL;void *symbol=dlsym(module,"VSTPluginMain");memcpy(&entry,&symbol,sizeof symbol);
    if(!entry)return 1;AEffect *fx=entry(host);if(!fx)return 1;
    char status[128]={0},info[128]={0};int ready=0,generated=0;
    for(int i=0;i<1000;i++){fx->dispatcher(fx,7,8,0,status,0);if(strstr(status,"READY")){ready=1;break;}if(strstr(status,"FAILED"))break;usleep(10000);}
    if(ready){fx->setParameter(fx,10,1);for(int i=0;i<1000;i++){fx->dispatcher(fx,7,31,0,info,0);if(strstr(info,"NOTES")){generated=1;break;}usleep(10000);}}
    fx->dispatcher(fx,7,8,0,status,0);fx->dispatcher(fx,7,31,0,info,0);
    printf("ARM instance: parameters=%d, bank=%s, generation=%s, status=%s, info=%s\n",fx->numParams,ready?"OK":"FAILED",generated?"OK":"FAILED",status,info);
    fx->dispatcher(fx,1,0,0,NULL,0);dlclose(module);return generated?0:1;
}
