#ifndef LIBRARY_H
#define LIBRARY_H

#include <stdint.h>

#define UMAX 16
#define CMAX 48
#define TL 96
#define BL 640
#define MAX_LIB (3 + UMAX)
#define SMAX 12
#define UL 256
#define TK 96

typedef struct {
    char short_name[64];
    char full_title[TL];
    char authors[TL];
    char imprint[40];
    uint32_t accent, accent2;
    int n;
    char title[CMAX][TL];
    char brief[CMAX][BL];
} UserBook;

typedef struct {
    char url[UL];
    char token[TK];
    char label[64];
} Source;

extern UserBook UBOOKS[UMAX];
extern int nuser;
extern char lib_path[256];
extern Source SOURCES[SMAX];
extern int nsrc;

int lib_load(void);
int lib_save(void);
void lib_dir(char *out, int n);
void lib_ensure_tmp(void);
int lib_add_source(const char *url, const char *token);

#endif
