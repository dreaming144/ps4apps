#include "covers.h"
#include "library.h"
#include "chapters.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_FAILURE_STRINGS
#include <stb/stb_image.h>

#define ASSETS "/app0/assets/"

static CoverImg builtin[NBOOKS];
static CoverImg user[UMAX];

static void cov_free(CoverImg *im) {
    if (!im) return;
    free(im->px);
    im->px = 0;
    im->w = im->h = 0;
}

static int load_rgba(CoverImg *im, unsigned char *p, int w, int h) {
    if (!p || w < 1 || h < 1) return 0;
    uint32_t *px = (uint32_t *)malloc((size_t)w * (size_t)h * 4);
    if (!px) return 0;
    for (int i = 0; i < w * h; i++) {
        unsigned char *q = p + i * 4;
        px[i] = ((uint32_t)q[0] << 16) | ((uint32_t)q[1] << 8) | q[2];
    }
    cov_free(im);
    im->px = px;
    im->w = w;
    im->h = h;
    return 1;
}

static int load_path(CoverImg *im, const char *path) {
    int w = 0, h = 0, n = 0;
    unsigned char *p = stbi_load(path, &w, &h, &n, 4);
    if (!p) return 0;
    int ok = load_rgba(im, p, w, h);
    stbi_image_free(p);
    return ok;
}

static int load_mem(CoverImg *im, const unsigned char *data, int len) {
    int w = 0, h = 0, n = 0;
    unsigned char *p = stbi_load_from_memory(data, len, &w, &h, &n, 4);
    if (!p) return 0;
    int ok = load_rgba(im, p, w, h);
    stbi_image_free(p);
    return ok;
}

static const char *ext_from(const unsigned char *p, int n, const char *hint) {
    if (n >= 3 && p[0] == 0xFF && p[1] == 0xD8 && p[2] == 0xFF) return "jpg";
    if (n >= 4 && p[0] == 0x89 && p[1] == 'P' && p[2] == 'N' && p[3] == 'G') return "png";
    if (hint && strstr(hint, "png")) return "png";
    return "jpg";
}

static void cover_paths(int u, char *png, char *jpg, int n) {
    char dir[192];
    lib_dir(dir, sizeof dir);
    snprintf(png, n, "%s/covers/u%d.png", dir, u);
    snprintf(jpg, n, "%s/covers/u%d.jpg", dir, u);
}

static void ensure_cover_dir(void) {
    char dir[192], cov[208];
    lib_dir(dir, sizeof dir);
    mkdir(dir, 0777);
    snprintf(cov, sizeof cov, "%s/covers", dir);
    mkdir(cov, 0777);
}

int cov_init(void) {
    load_path(&builtin[0], ASSETS "cover-spirit.png");
    load_path(&builtin[1], ASSETS "cover-subtle.png");
    load_path(&builtin[2], ASSETS "cover-power.png");
    return builtin[0].px && builtin[1].px && builtin[2].px;
}

const CoverImg *cov_for_book(int book_i) {
    if (book_i >= 0 && book_i < NBOOKS) return builtin[book_i].px ? &builtin[book_i] : 0;
    int u = book_i - NBOOKS;
    if (u >= 0 && u < nuser && user[u].px) return &user[u];
    return 0;
}

void cov_load_users(void) {
    for (int i = 0; i < UMAX; i++) cov_free(&user[i]);
    for (int i = 0; i < nuser; i++) {
        char png[256], jpg[256];
        cover_paths(i, png, jpg, 256);
        if (!load_path(&user[i], png)) load_path(&user[i], jpg);
    }
}

static int write_user_file(int u, const unsigned char *p, int n) {
    if (u < 0 || u >= UMAX || !p || n < 8) return 0;
    ensure_cover_dir();
    char png[256], jpg[256];
    cover_paths(u, png, jpg, 256);
    remove(png);
    remove(jpg);
    const char *ext = ext_from(p, n, 0);
    char dest[256];
    snprintf(dest, sizeof dest, "%s", ext[0] == 'p' ? png : jpg);
    FILE *f = fopen(dest, "wb");
    if (!f) return 0;
    int ok = fwrite(p, 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    return ok;
}

int cov_store_user_mem(int u, const unsigned char *p, int n) {
    if (!write_user_file(u, p, n)) return 0;
    return load_mem(&user[u], p, n);
}

int cov_store_user_path(int u, const char *src) {
    if (!src || !src[0]) return 0;
    FILE *f = fopen(src, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long s = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (s < 8 || s > 4 * 1024 * 1024) { fclose(f); return 0; }
    unsigned char *b = malloc((size_t)s);
    if (!b) { fclose(f); return 0; }
    if (fread(b, 1, (size_t)s, f) != (size_t)s) { fclose(f); free(b); return 0; }
    fclose(f);
    int ok = cov_store_user_mem(u, b, (int)s);
    free(b);
    return ok;
}

void cov_delete_user(int u) {
    if (u < 0 || u >= nuser) return;
    cov_free(&user[u]);
    for (int k = u; k < nuser - 1; k++) user[k] = user[k + 1];
    memset(&user[nuser - 1], 0, sizeof user[0]);
    char dir[192];
    lib_dir(dir, sizeof dir);
    for (int k = u; k < nuser - 1; k++) {
        char a[256], b[256], c[256], d[256];
        snprintf(a, sizeof a, "%s/covers/u%d.png", dir, k);
        snprintf(b, sizeof b, "%s/covers/u%d.jpg", dir, k);
        snprintf(c, sizeof c, "%s/covers/u%d.png", dir, k + 1);
        snprintf(d, sizeof d, "%s/covers/u%d.jpg", dir, k + 1);
        remove(a);
        remove(b);
        rename(c, a);
        rename(d, b);
    }
    {
        char a[256], b[256];
        snprintf(a, sizeof a, "%s/covers/u%d.png", dir, nuser - 1);
        snprintf(b, sizeof b, "%s/covers/u%d.jpg", dir, nuser - 1);
        remove(a);
        remove(b);
    }
}
