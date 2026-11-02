#ifndef IMPORT_H
#define IMPORT_H

#include "library.h"

#define CATMAX 32

typedef struct {
    char title[TL];
    char url[256];
} CatItem;

int imp_is_book_file(const unsigned char *p, int n, const char *name);
int imp_file(const char *path, UserBook *out, char *cover_src, int cover_n, char *err, int errn);
int imp_catalog(const char *path, const char *base_url, CatItem *dst, int maxn);
void imp_sibling_cover(const char *bookpath, char *out, int n);
int imp_title_match(const char *title);

#endif
