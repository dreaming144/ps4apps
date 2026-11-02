#include "library.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

UserBook UBOOKS[UMAX];
int nuser;
char lib_path[256];
Source SOURCES[SMAX];
int nsrc;

static const char *PATHS[] = {
    "/data/BOOK00001/library.bin",
    "/user/data/BOOK00001/library.bin",
    "/data/chapterbriefs/library.bin",
    0
};
static const char *DIRS[] = {
    "/data",
    "/data/BOOK00001",
    "/data/BOOK00001/tmp",
    "/data/BOOK00001/covers",
    "/user/data",
    "/user/data/BOOK00001",
    "/user/data/BOOK00001/tmp",
    "/user/data/BOOK00001/covers",
    "/data/chapterbriefs",
    "/data/chapterbriefs/tmp",
    "/data/chapterbriefs/covers",
    0
};

#define MAGIC03 0x33424343u /* CB03 */
#define MAGIC04 0x34424343u /* CB04 */

static void ensure_dirs(void) {
    for (int i = 0; DIRS[i]; i++) mkdir(DIRS[i], 0777);
}

void lib_dir(char *out, int n) {
    if (!out || n < 8) return;
    if (!lib_path[0]) {
        snprintf(out, n, "/data/BOOK00001");
        return;
    }
    snprintf(out, n, "%s", lib_path);
    char *s = strrchr(out, '/');
    if (s) *s = 0;
}

void lib_ensure_tmp(void) {
    ensure_dirs();
    char dir[192], tmp[208];
    lib_dir(dir, sizeof dir);
    mkdir(dir, 0777);
    snprintf(tmp, sizeof tmp, "%s/tmp", dir);
    mkdir(tmp, 0777);
}

static int write_book(FILE *f, const UserBook *b) {
    if (fwrite(b->short_name, 1, 64, f) != 64) return 0;
    if (fwrite(b->full_title, 1, TL, f) != TL) return 0;
    if (fwrite(b->authors, 1, TL, f) != TL) return 0;
    if (fwrite(b->imprint, 1, 40, f) != 40) return 0;
    if (fwrite(&b->accent, 4, 1, f) != 1) return 0;
    if (fwrite(&b->accent2, 4, 1, f) != 1) return 0;
    if (fwrite(&b->n, 4, 1, f) != 1) return 0;
    for (int i = 0; i < CMAX; i++) {
        if (fwrite(b->title[i], 1, TL, f) != TL) return 0;
        if (fwrite(b->brief[i], 1, BL, f) != BL) return 0;
    }
    return 1;
}
static int read_book(FILE *f, UserBook *b) {
    memset(b, 0, sizeof *b);
    if (fread(b->short_name, 1, 64, f) != 64) return 0;
    if (fread(b->full_title, 1, TL, f) != TL) return 0;
    if (fread(b->authors, 1, TL, f) != TL) return 0;
    if (fread(b->imprint, 1, 40, f) != 40) return 0;
    if (fread(&b->accent, 4, 1, f) != 1) return 0;
    if (fread(&b->accent2, 4, 1, f) != 1) return 0;
    if (fread(&b->n, 4, 1, f) != 1) return 0;
    if (b->n < 0 || b->n > CMAX) return 0;
    for (int i = 0; i < CMAX; i++) {
        if (fread(b->title[i], 1, TL, f) != TL) return 0;
        if (fread(b->brief[i], 1, BL, f) != BL) return 0;
        b->title[i][TL - 1] = 0;
        b->brief[i][BL - 1] = 0;
    }
    b->short_name[63] = 0;
    b->full_title[TL - 1] = 0;
    b->authors[TL - 1] = 0;
    b->imprint[39] = 0;
    return 1;
}

static int write_src(FILE *f, const Source *s) {
    return fwrite(s->url, 1, UL, f) == UL &&
           fwrite(s->token, 1, TK, f) == TK &&
           fwrite(s->label, 1, 64, f) == 64;
}
static int read_src(FILE *f, Source *s) {
    memset(s, 0, sizeof *s);
    if (fread(s->url, 1, UL, f) != UL) return 0;
    if (fread(s->token, 1, TK, f) != TK) return 0;
    if (fread(s->label, 1, 64, f) != 64) return 0;
    s->url[UL - 1] = 0;
    s->token[TK - 1] = 0;
    s->label[63] = 0;
    return 1;
}

int lib_load(void) {
    nuser = 0;
    nsrc = 0;
    memset(UBOOKS, 0, sizeof UBOOKS);
    memset(SOURCES, 0, sizeof SOURCES);
    lib_path[0] = 0;
    for (int i = 0; PATHS[i]; i++) {
        FILE *f = fopen(PATHS[i], "rb");
        if (!f) continue;
        uint32_t mag = 0, n = 0;
        if (fread(&mag, 4, 1, f) != 1 || (mag != MAGIC03 && mag != MAGIC04)) { fclose(f); continue; }
        if (fread(&n, 4, 1, f) != 1 || n > UMAX) { fclose(f); continue; }
        int ok = 1;
        for (uint32_t k = 0; k < n; k++) if (!read_book(f, &UBOOKS[k])) { ok = 0; break; }
        if (ok && mag == MAGIC04) {
            uint32_t ns = 0;
            if (fread(&ns, 4, 1, f) == 1 && ns <= SMAX) {
                for (uint32_t k = 0; k < ns; k++) {
                    if (!read_src(f, &SOURCES[k])) { ok = 0; break; }
                }
                if (ok) nsrc = (int)ns;
            }
        }
        fclose(f);
        if (!ok) {
            nuser = 0;
            nsrc = 0;
            memset(UBOOKS, 0, sizeof UBOOKS);
            memset(SOURCES, 0, sizeof SOURCES);
            continue;
        }
        nuser = (int)n;
        snprintf(lib_path, sizeof lib_path, "%s", PATHS[i]);
        return 1;
    }
    return 0;
}

int lib_save(void) {
    ensure_dirs();
    for (int i = 0; PATHS[i]; i++) {
        FILE *f = fopen(PATHS[i], "wb");
        if (!f) continue;
        uint32_t mag = MAGIC04, n = (uint32_t)nuser, ns = (uint32_t)nsrc;
        int ok = fwrite(&mag, 4, 1, f) == 1 && fwrite(&n, 4, 1, f) == 1;
        for (int k = 0; ok && k < nuser; k++) ok = write_book(f, &UBOOKS[k]);
        if (ok) ok = fwrite(&ns, 4, 1, f) == 1;
        for (int k = 0; ok && k < nsrc; k++) ok = write_src(f, &SOURCES[k]);
        fclose(f);
        if (!ok) continue;
        snprintf(lib_path, sizeof lib_path, "%s", PATHS[i]);
        return 1;
    }
    return 0;
}

int lib_add_source(const char *url, const char *token) {
    if (!url || !url[0]) return 0;
    for (int i = 0; i < nsrc; i++) {
        if (!strcmp(SOURCES[i].url, url)) {
            if (token) snprintf(SOURCES[i].token, TK, "%s", token);
            return 1;
        }
    }
    if (nsrc >= SMAX) return 0;
    memset(&SOURCES[nsrc], 0, sizeof SOURCES[0]);
    snprintf(SOURCES[nsrc].url, UL, "%s", url);
    if (token) snprintf(SOURCES[nsrc].token, TK, "%s", token);
    const char *host = strstr(url, "://");
    host = host ? host + 3 : url;
    snprintf(SOURCES[nsrc].label, 64, "%s", host);
    SOURCES[nsrc].label[40] = 0;
    nsrc++;
    return 1;
}
