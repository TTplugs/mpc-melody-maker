/* Resolve the ARM plugin in a separate process without instantiating it or opening MIDI. */
#include <dlfcn.h>
#include <stdio.h>
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    void *module=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!module){fprintf(stderr,"%s\n",dlerror());return 1;}
    int ok=dlsym(module,"VSTPluginMain")!=NULL;
    printf("Plugin load: %s\n",ok?"OK":"VST entry point missing");
    dlclose(module);return ok?0:1;
}
