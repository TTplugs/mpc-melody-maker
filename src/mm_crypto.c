#define _GNU_SOURCE
#include "mm_crypto.h"
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
static void wipe(void *data,size_t n){volatile unsigned char *p=data;while(n--)*p++=0;}
void mm_library_close(FILE *f,unsigned char *m,size_t n){if(f)fclose(f);if(m){wipe(m,n);free(m);}}
FILE *mm_library_open(const char *path,unsigned char **memory,size_t *length,char *error,unsigned cap){
    *memory=NULL;*length=0;FILE *f=fopen(path,"rb");
    if(!f){snprintf(error,cap,"Cannot open musical bank");return NULL;}
    unsigned char magic[8];if(fread(magic,1,8,f)!=8){fclose(f);snprintf(error,cap,"Incomplete bank");return NULL;}
    if(memcmp(magic,"TTMME001",8)){rewind(f);return f;}
    unsigned char key[32]={0},nonce[24],*cipher=NULL,*plain=NULL;size_t size=0;
    void *sodium=NULL;FILE *kf=NULL;const char *problem="Invalid encrypted bank";
    if(fseek(f,0,SEEK_END))goto fail;
    long total=ftell(f);if(total<64||total>128*1024*1024)goto fail;
    size=(size_t)total-32;rewind(f);
    if(fread(magic,1,8,f)!=8||fread(nonce,1,24,f)!=24)goto fail;
    cipher=malloc(size);plain=malloc(size-16);if(!cipher||!plain)goto fail;
    if(fread(cipher,1,size,f)!=size)goto fail;
    const char *kp=getenv("TTMM_BANK_KEY_FILE");char fallback[1200];
    if(!kp||!*kp){const char *home=getenv("HOME");if(!home){struct passwd *entry=getpwuid(geteuid());if(entry)home=entry->pw_dir;}if(!home){problem="Bank key path missing";goto fail;}
        snprintf(fallback,sizeof fallback,"%s/.config/mpc-melody-maker/bank.key",home);kp=fallback;
        if(access(kp,R_OK))kp="/data/Settings/MPC/TTplugs/MPCMelodyMaker/bank.key";}
    kf=fopen(kp,"rb");if(!kf){problem="Bank activation key missing";goto fail;}
    if(fread(key,1,32,kf)!=32||fgetc(kf)!=EOF){problem="Invalid bank activation key";goto fail;}
    fclose(kf);kf=NULL;
    const char *libs[]={"libsodium.so.26","libsodium.so.23","libsodium.so"};
    for(unsigned i=0;i<3&&!sodium;i++)sodium=dlopen(libs[i],RTLD_NOW|RTLD_LOCAL);
    if(!sodium){problem="libsodium unavailable";goto fail;}
    int (*init)(void)=NULL;
    int (*decrypt)(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *,const unsigned char *)=NULL;
    void *symbol=dlsym(sodium,"sodium_init");memcpy(&init,&symbol,sizeof symbol);
    symbol=dlsym(sodium,"crypto_secretbox_open_easy");memcpy(&decrypt,&symbol,sizeof symbol);
    if(!init||!decrypt||init()<0)goto fail;
    if(decrypt(plain,cipher,size,nonce,key)){problem="Bank key incorrect or bank tampered";goto fail;}
    if(memcmp(plain,"TTMMB001",8)&&memcmp(plain,"TTMMB002",8))goto fail;
    FILE *stream=fmemopen(plain,size-16,"rb");if(!stream)goto fail;
    wipe(key,sizeof key);free(cipher);dlclose(sodium);fclose(f);*memory=plain;*length=size-16;return stream;
fail:
    wipe(key,sizeof key);if(kf)fclose(kf);if(sodium)dlclose(sodium);free(cipher);
    if(plain){wipe(plain,size-16);free(plain);}fclose(f);snprintf(error,cap,"%s",problem);return NULL;
}
