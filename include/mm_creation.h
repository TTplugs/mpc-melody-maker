#ifndef MM_CREATION_H
#define MM_CREATION_H
#include "mm_core.h"
#define MM_CREATION_SETTINGS 64
typedef struct {mm_phrase phrase;float settings[MM_CREATION_SETTINGS];int favorite;} mm_creation;
int mm_creation_write(const char *path,const mm_creation *creation);
int mm_creation_read(const char *path,mm_creation *creation);
#endif
