#ifndef MM_CRYPTO_H
#define MM_CRYPTO_H
#include <stdio.h>
#include <stddef.h>
FILE *mm_library_open(const char *,unsigned char **,size_t *,char *,unsigned);
void mm_library_close(FILE *,unsigned char *,size_t);
#endif
