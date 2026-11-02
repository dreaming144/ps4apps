/* Chapter Briefs 1.2.1 — ebook-style summary reader. OpenOrbis + SDL2.
   Built-in TOCs plus user-added summary books. Archive fetch. Original notes only. */
#include <SDL2/SDL.h>
#include <orbis/SystemService.h>
#include <orbis/UserService.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "chapters.h"
#include "library.h"
#include "covers.h"
#include "import.h"
#include "net.h"

#define W 1920
#define H 1080
#define ASSETS "/app0/assets/"
#define APPVER "1.2.1"

#define C_BG     0x0C0D11
#define C_BG2    0x14161C
#define C_GLASS  0x16181F
#define C_PANEL  0x1C1F28
#define C_LINE   0x2C313C
#define C_PAPER  0xF4EFE4
#define C_INK    0x1A1814
#define C_INK2   0x5A5348
#define C_TEXT   0xF0EEE8
#define C_DIM    0x8E938C
#define C_MUTE   0x5C616A
#define C_GOLD   0xE8B86D

static uint32_t *fb, *bgb, *T;
static SDL_Window *win;
static SDL_Surface *wsurf, *fbs;

static inline float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static inline uint32_t mix(uint32_t a, uint32_t b, float t) {
    t = clampf(t, 0, 1);
    int r = (int)(((a >> 16) & 255) * (1 - t) + ((b >> 16) & 255) * t);
    int g = (int)(((a >> 8) & 255) * (1 - t) + ((b >> 8) & 255) * t);
    int bl = (int)((a & 255) * (1 - t) + (b & 255) * t);
    return (r << 16) | (g << 8) | bl;
}
static inline void px(int x, int y, uint32_t c, float a) {
    if ((unsigned)x >= W || (unsigned)y >= H || a <= 0.004f) return;
    uint32_t *p = &T[y * W + x];
    if (a >= 0.996f) { *p = c; return; }
    uint32_t d = *p;
    int ia = (int)(a * 256);
    int r = ((d >> 16) & 255) + (((((c >> 16) & 255) - (int)((d >> 16) & 255)) * ia) >> 8);
    int g = ((d >> 8) & 255) + (((((c >> 8) & 255) - (int)((d >> 8) & 255)) * ia) >> 8);
    int b = (d & 255) + ((((c & 255) - (int)(d & 255)) * ia) >> 8);
    *p = (r << 16) | (g << 8) | b;
}
static void rect(int x, int y, int w, int h, uint32_t c, float a) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > W) w = W - x;
    if (y + h > H) h = H - y;
    if (w <= 0 || h <= 0) return;
    for (int j = y; j < y + h; j++) {
        if (a >= 0.996f) { uint32_t *p = &T[j * W + x]; for (int i = 0; i < w; i++) p[i] = c; }
        else for (int i = 0; i < w; i++) px(x + i, j, c, a);
    }
}
static float sd_rr(float X, float Y, float cx, float cy, float hw, float hh, float r) {
    float qx = fabsf(X - cx) - (hw - r), qy = fabsf(Y - cy) - (hh - r);
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    float in = fmaxf(qx, qy); if (in > 0) in = 0;
    return sqrtf(ox * ox + oy * oy) + in - r;
}
static void rrect(float x, float y, float w, float h, float r, uint32_t c, float a, float t) {
    float cx = x + w / 2, cy = y + h / 2, hw = w / 2, hh = h / 2;
    for (int j = (int)y - 1; j <= (int)(y + h) + 1; j++) {
        if (j < 0 || j >= H) continue;
        for (int i = (int)x - 1; i <= (int)(x + w) + 1; i++) {
            float d = sd_rr(i + 0.5f, j + 0.5f, cx, cy, hw, hh, r);
            float cv = t > 0 ? clampf(0.5f - (fabsf(d + t * 0.5f) - t * 0.5f), 0, 1) : clampf(0.5f - d, 0, 1);
            if (cv > 0) px(i, j, c, cv * a);
        }
    }
}
static void circle(float cx, float cy, float R, uint32_t c, float a, float t) {
    for (int j = (int)(cy - R - 2); j <= (int)(cy + R + 2); j++)
        for (int i = (int)(cx - R - 2); i <= (int)(cx + R + 2); i++) {
            float dx = i + 0.5f - cx, dy = j + 0.5f - cy;
            float d = sqrtf(dx * dx + dy * dy) - R;
            float cv = t > 0 ? clampf(0.5f - (fabsf(d + t * 0.5f) - t * 0.5f), 0, 1) : clampf(0.5f - d, 0, 1);
            if (cv > 0) px(i, j, c, cv * a);
        }
}
static void line(float x0, float y0, float x1, float y1, float t, uint32_t c, float a) {
    float mnx = fminf(x0, x1) - t - 1, mxx = fmaxf(x0, x1) + t + 1;
    float mny = fminf(y0, y1) - t - 1, mxy = fmaxf(y0, y1) + t + 1;
    float vx = x1 - x0, vy = y1 - y0, ll = vx * vx + vy * vy; if (ll < 1e-4f) ll = 1e-4f;
    for (int j = (int)mny; j <= (int)mxy; j++) for (int i = (int)mnx; i <= (int)mxx; i++) {
        float px_ = i + 0.5f - x0, py_ = j + 0.5f - y0;
        float h = clampf((px_ * vx + py_ * vy) / ll, 0, 1);
        float dx = px_ - vx * h, dy = py_ - vy * h;
        float cv = clampf(0.5f - (sqrtf(dx * dx + dy * dy) - t * 0.5f), 0, 1);
        if (cv > 0) px(i, j, c, cv * a);
    }
}

typedef struct { unsigned char *bmp; int w, h, xo, yo; float adv; } Glyph;
typedef struct { Glyph g[96]; } Font;
enum { F_CAP, F_LBL, F_BODY, F_TITLE, F_MID, F_HERO, NF };
static Font fonts[NF];
static unsigned char *readfile(const char *p, long *n) {
    FILE *f = fopen(p, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc((size_t)s + 1); if (!b) { fclose(f); return 0; }
    if (fread(b, 1, (size_t)s, f) != (size_t)s) {}
    b[s] = 0; fclose(f); if (n) *n = s; return b;
}
static void font_bake(Font *F, stbtt_fontinfo *fi, float pxh) {
    float sc = stbtt_ScaleForPixelHeight(fi, pxh * 1.28f);
    for (int c = 32; c < 127; c++) {
        Glyph *g = &F->g[c - 32]; int adv, lsb;
        stbtt_GetCodepointHMetrics(fi, c, &adv, &lsb); g->adv = adv * sc;
        g->bmp = stbtt_GetCodepointBitmap(fi, 0, sc, c, &g->w, &g->h, &g->xo, &g->yo);
    }
}
static void fonts_init(void) {
    static stbtt_fontinfo m, s;
    unsigned char *mb = readfile(ASSETS "Inter-Medium.ttf", 0);
    unsigned char *sb = readfile(ASSETS "Inter-SemiBold.ttf", 0);
    if (!mb || !sb) return;
    stbtt_InitFont(&m, mb, 0); stbtt_InitFont(&s, sb, 0);
    font_bake(&fonts[F_CAP], &s, 13);
    font_bake(&fonts[F_LBL], &m, 16);
    font_bake(&fonts[F_BODY], &m, 21);
    font_bake(&fonts[F_TITLE], &s, 28);
    font_bake(&fonts[F_MID], &s, 40);
    font_bake(&fonts[F_HERO], &s, 58);
}
static float text_w(int f, const char *s) {
    float w = 0; Font *F = &fonts[f];
    for (; *s; s++) { int c = (unsigned char)*s; if (c < 32 || c > 126) c = '?'; w += F->g[c - 32].adv; }
    return w;
}
static float text(int f, float x, float y, const char *s, uint32_t col, float a) {
    Font *F = &fonts[f];
    for (; *s; s++) {
        int c = (unsigned char)*s; if (c < 32 || c > 126) c = '?';
        Glyph *g = &F->g[c - 32];
        if (g->bmp) {
            int gx = (int)(x + 0.5f) + g->xo, gy = (int)y + g->yo;
            for (int j = 0; j < g->h; j++) for (int i = 0; i < g->w; i++) {
                unsigned char v = g->bmp[j * g->w + i];
                if (v) px(gx + i, gy + j, col, a * v / 255.0f);
            }
        }
        x += g->adv;
    }
    return x;
}
static void text_c(int f, float xc, float y, const char *s, uint32_t c, float a) {
    text(f, xc - text_w(f, s) / 2, y, s, c, a);
}
static void text_r(int f, float xr, float y, const char *s, uint32_t c, float a) {
    text(f, xr - text_w(f, s), y, s, c, a);
}
static void text_fit(int f, float x, float y, float maxw, const char *s, uint32_t c, float a) {
    char b[220]; snprintf(b, sizeof b, "%s", s);
    if (text_w(f, b) <= maxw) { text(f, x, y, b, c, a); return; }
    int n = (int)strlen(b);
    while (n > 1) {
        b[--n] = 0;
        char t[228]; snprintf(t, sizeof t, "%s...", b);
        if (text_w(f, t) <= maxw) { text(f, x, y, t, c, a); return; }
    }
}

#define WRAP_N 18
#define WRAP_L 160
static int wrap_text(int f, const char *s, float maxw, char lines[][WRAP_L], int maxn) {
    int n = 0;
    char word[WRAP_L], line[WRAP_L];
    line[0] = 0;
    const char *p = s;
    while (*p && n < maxn) {
        while (*p == ' ') p++;
        if (!*p) break;
        int wi = 0;
        while (*p && *p != ' ' && wi < WRAP_L - 1) word[wi++] = *p++;
        word[wi] = 0;
        char trial[WRAP_L];
        if (line[0]) snprintf(trial, sizeof trial, "%s %s", line, word);
        else snprintf(trial, sizeof trial, "%s", word);
        if (text_w(f, trial) <= maxw) snprintf(line, sizeof line, "%s", trial);
        else {
            if (line[0] && n < maxn) snprintf(lines[n++], WRAP_L, "%s", line);
            snprintf(line, sizeof line, "%s", word);
        }
    }
    if (line[0] && n < maxn) snprintf(lines[n++], WRAP_L, "%s", line);
    return n;
}

enum { G_X, G_O, G_SQ, G_TRI };
static void pad_icon(int k, float cx, float cy, float r, uint32_t c) {
    if (k == G_O) circle(cx, cy, r * 0.72f, c, 1, 2);
    else if (k == G_SQ) rrect(cx - r * 0.68f, cy - r * 0.68f, r * 1.36f, r * 1.36f, 1.5f, c, 1, 2);
    else if (k == G_X) {
        float d = r * 0.62f;
        line(cx - d, cy - d, cx + d, cy + d, 2, c, 1);
        line(cx - d, cy + d, cx + d, cy - d, 2, c, 1);
    } else {
        float d = r * 0.8f;
        line(cx, cy - d, cx + d * 0.95f, cy + d * 0.65f, 2, c, 1);
        line(cx + d * 0.95f, cy + d * 0.65f, cx - d * 0.95f, cy + d * 0.65f, 2, c, 1);
        line(cx - d * 0.95f, cy + d * 0.65f, cx, cy - d, 2, c, 1);
    }
}
static float key_chip(float x, float y, const char *k, const char *label) {
    float w = text_w(F_CAP, k) + 16;
    rrect(x, y - 15, w, 22, 4, C_LINE, 1, 0);
    text(F_CAP, x + 8, y, k, C_TEXT, 1);
    return text(F_LBL, x + w + 8, y, label, C_DIM, 1) + 24;
}
static float icon_chip(float x, float y, int g, const char *label) {
    rrect(x, y - 15, 22, 22, 11, C_LINE, 1, 0);
    pad_icon(g, x + 11, y - 4, 6.5f, C_TEXT);
    return text(F_LBL, x + 30, y, label, C_DIM, 1) + 24;
}

static int nlib(void) { return NBOOKS + nuser; }
static int is_user(int i) { return i >= NBOOKS; }
static int bn(int i) { return is_user(i) ? UBOOKS[i - NBOOKS].n : BOOKS[i].n; }
static uint32_t bacc(int i) { return is_user(i) ? UBOOKS[i - NBOOKS].accent : BOOKS[i].accent; }
static uint32_t bacc2(int i) { return is_user(i) ? UBOOKS[i - NBOOKS].accent2 : BOOKS[i].accent2; }
static const char *bshort(int i) { return is_user(i) ? UBOOKS[i - NBOOKS].short_name : BOOKS[i].short_name; }
static const char *bfull(int i) { return is_user(i) ? UBOOKS[i - NBOOKS].full_title : BOOKS[i].full_title; }
static const char *bauth(int i) { return is_user(i) ? UBOOKS[i - NBOOKS].authors : BOOKS[i].authors; }
static const char *bimp(int i) { return is_user(i) ? UBOOKS[i - NBOOKS].imprint : BOOKS[i].imprint; }
static const char *ctitle(int i, int c) { return is_user(i) ? UBOOKS[i - NBOOKS].title[c] : BOOKS[i].ch[c].title; }
static const char *cbrief(int i, int c) { return is_user(i) ? UBOOKS[i - NBOOKS].brief[c] : BOOKS[i].ch[c].brief; }

enum { SCR_SHELF, SCR_TOC, SCR_READ, SCR_ADD, SCR_ABOUT, SCR_FETCH, SCR_CATALOG, SCR_CONFLICT };
static int screen = SCR_SHELF;
static int book_i, ch_i, toc_top, last_ch[MAX_LIB];
static float page_x, page_tgt;
static int from_scr, confirm_del;
static char toast[128];
static uint32_t toast_until;

static char fetch_url[UL], fetch_tok[TK];
static int fetch_step, src_sel;
static const char *KBURL[5] = { "1234567890", "QWERTYUIOP", "ASDFGHJKL'", "ZXCVBNM-._", ":/?=&+#%@" };
static CatItem catalog[CATMAX];
static int ncat, cat_i, syncing;
static UserBook pending;
static char pending_cover[256];
static int pending_match;

static UserBook draft;
static char d_cht[TL], d_chb[BL];
static int add_step, kb_r, kb_c, kb_shift;
static const uint32_t UCOL[] = { 0x7EC8C5, 0xC8A050, 0x8FA86A, 0xB080C0, 0xE87A40 };
static const uint32_t UCOL2[] = { 0x1A2830, 0x3A2018, 0x1E2A1A, 0x2A1A30, 0x2A323C };

static void set_toast(const char *s) {
    snprintf(toast, sizeof toast, "%s", s);
    toast_until = SDL_GetTicks() + 2600;
}
static void build_bg(void) {
    for (int y = 0; y < H; y++) {
        float t = y / (float)(H - 1);
        uint32_t c = mix(C_BG, C_BG2, t);
        uint32_t *p = &bgb[y * W];
        for (int x = 0; x < W; x++) {
            float v = 0.97f + 0.03f * sinf(x * 0.01f + y * 0.007f);
            p[x] = mix(c, 0x000000, (1 - v) * 0.4f);
        }
    }
}

static const char *KB[4] = { "1234567890", "QWERTYUIOP", "ASDFGHJKL'", "ZXCVBNM-.*" };

static char *cur_field(void) {
    if (add_step == 0) return draft.full_title;
    if (add_step == 1) return draft.authors;
    if (add_step == 2) return d_cht;
    return d_chb;
}
static int cur_lim(void) { return add_step == 3 ? BL : TL; }

static void kb_insert(char ch) {
    char *s = cur_field();
    int n = (int)strlen(s), lim = cur_lim() - 1;
    if (n >= lim) return;
    s[n] = ch; s[n + 1] = 0;
}
static void kb_bksp(void) {
    char *s = cur_field();
    int n = (int)strlen(s);
    if (n) s[n - 1] = 0;
}

static void start_add(void) {
    if (nuser >= UMAX) { set_toast("Library full (16 added books)"); return; }
    memset(&draft, 0, sizeof draft);
    d_cht[0] = d_chb[0] = 0;
    add_step = 0; kb_r = 1; kb_c = 0; kb_shift = 1;
    draft.accent = UCOL[nuser % 5];
    draft.accent2 = UCOL2[nuser % 5];
    snprintf(draft.imprint, sizeof draft.imprint, "Added notes");
    screen = SCR_ADD;
}

static void commit_chapter(void) {
    if (!d_cht[0]) return;
    if (draft.n >= CMAX) { set_toast("Chapter limit 48"); return; }
    snprintf(draft.title[draft.n], TL, "%s", d_cht);
    snprintf(draft.brief[draft.n], BL, "%s", d_chb[0] ? d_chb : "(no summary yet)");
    draft.n++;
    d_cht[0] = d_chb[0] = 0;
    add_step = 2;
}

static void save_draft(void) {
    if (d_cht[0]) commit_chapter();
    if (!draft.full_title[0] || draft.n < 1) { set_toast("Need a title and one chapter summary"); return; }
    snprintf(draft.short_name, sizeof draft.short_name, "%s", draft.full_title);
    draft.short_name[28] = 0;
    UBOOKS[nuser] = draft;
    nuser++;
    if (lib_save()) set_toast("Saved to console. Stays after quit.");
    else set_toast("Saved in session. Disk write failed.");
    book_i = NBOOKS + nuser - 1;
    ch_i = 0;
    screen = SCR_TOC;
}

static void delete_user(int i) {
    int u = i - NBOOKS;
    if (u < 0 || u >= nuser) return;
    cov_delete_user(u);
    for (int k = u; k < nuser - 1; k++) UBOOKS[k] = UBOOKS[k + 1];
    memset(&UBOOKS[nuser - 1], 0, sizeof UBOOKS[0]);
    nuser--;
    lib_save();
    if (book_i >= nlib()) book_i = nlib() - 1;
    if (book_i < 0) book_i = 0;
    set_toast("Removed added book");
}

static void start_fetch(void) {
    fetch_step = 0; kb_r = 1; kb_c = 0; kb_shift = 0; src_sel = 0;
    if (!fetch_url[0] && nsrc) snprintf(fetch_url, sizeof fetch_url, "%s", SOURCES[0].url);
    screen = SCR_FETCH;
}

static const char *fetch_token(void) {
    if (fetch_tok[0]) return fetch_tok;
    for (int i = 0; i < nsrc; i++)
        if (!strcmp(SOURCES[i].url, fetch_url) && SOURCES[i].token[0]) return SOURCES[i].token;
    return fetch_tok;
}

static void try_sidecar_url(const char *file_url);

static void apply_pending(int replace) {
    if (replace && pending_match >= NBOOKS) {
        int u = pending_match - NBOOKS;
        UBOOKS[u] = pending;
        if (pending_cover[0]) cov_store_user_path(u, pending_cover);
        lib_save();
        book_i = pending_match;
        set_toast("Replaced existing book");
    } else {
        if (nuser >= UMAX) { set_toast("Library full (16 added books)"); return; }
        int u = nuser;
        UBOOKS[u] = pending;
        nuser++;
        if (pending_cover[0]) cov_store_user_path(u, pending_cover);
        lib_save();
        book_i = NBOOKS + u;
        set_toast(replace ? "Added alongside built-in" : "Imported to shelf");
    }
    memset(&pending, 0, sizeof pending);
    pending_cover[0] = 0;
    pending_match = -1;
}

static int import_path(const char *path, const char *file_url) {
    char err[96];
    memset(&pending, 0, sizeof pending);
    pending_cover[0] = 0;
    pending_match = -1;
    if (!imp_file(path, &pending, pending_cover, sizeof pending_cover, err, sizeof err)) {
        set_toast(err);
        return 0;
    }
    if (!pending_cover[0] && file_url) try_sidecar_url(file_url);
    pending_match = imp_title_match(pending.full_title);
    if (pending_match >= 0) {
        screen = SCR_CONFLICT;
        return 2;
    }
    apply_pending(0);
    return 1;
}

static void try_sidecar_url(const char *file_url) {
    if (pending_cover[0]) return;
    const char *slash = strrchr(file_url, '/');
    if (!slash) return;
    char dir[UL], dest[256], err[64];
    snprintf(dir, sizeof dir, "%s", file_url);
    char *cut = strrchr(dir, '/');
    if (cut) *cut = 0;
    const char *names[] = { "cover.jpg", "cover.png", "cover.jpeg", 0 };
    char tmp[256];
    lib_ensure_tmp();
    lib_dir(tmp, sizeof tmp);
    snprintf(dest, sizeof dest, "%s/tmp/sidecar.jpg", tmp);
    for (int i = 0; names[i]; i++) {
        char u[UL];
        snprintf(u, sizeof u, "%s/%s", dir, names[i]);
        if (net_get(u, fetch_token(), dest, err, sizeof err)) {
            snprintf(pending_cover, sizeof pending_cover, "%s", dest);
            return;
        }
    }
}

static int fetch_one(const char *url) {
    char dest[256], dir[192], err[96];
    lib_ensure_tmp();
    lib_dir(dir, sizeof dir);
    snprintf(dest, sizeof dest, "%s/tmp/fetch.bin", dir);
    if (!net_get(url, fetch_token(), dest, err, sizeof err)) {
        set_toast(err);
        return 0;
    }
    lib_add_source(url, fetch_tok);
    lib_save();
    return import_path(dest, url);
}

static void continue_sync(void) {
    while (syncing && cat_i < ncat) {
        int r = fetch_one(catalog[cat_i].url);
        cat_i++;
        if (r == 2) return;
        if (!r) { /* keep going after a miss */ }
    }
    syncing = 0;
    screen = SCR_SHELF;
    if (ncat) set_toast("Sync finished");
}

static void start_browse(void) {
    if (!fetch_url[0]) { set_toast("Type a URL first"); return; }
    char dest[256], dir[192], err[96];
    lib_ensure_tmp();
    lib_dir(dir, sizeof dir);
    snprintf(dest, sizeof dest, "%s/tmp/catalog.bin", dir);
    if (!net_get(fetch_url, fetch_token(), dest, err, sizeof err)) {
        set_toast(err);
        return;
    }
    lib_add_source(fetch_url, fetch_tok);
    lib_save();
    ncat = imp_catalog(dest, fetch_url, catalog, CATMAX);
    if (!ncat) {
        if (import_path(dest, fetch_url) == 2) return;
        screen = SCR_SHELF;
        return;
    }
    cat_i = 0;
    screen = SCR_CATALOG;
}

static void start_sync(void) {
    if (!fetch_url[0]) { set_toast("Type a URL first"); return; }
    char dest[256], dir[192], err[96];
    lib_ensure_tmp();
    lib_dir(dir, sizeof dir);
    snprintf(dest, sizeof dest, "%s/tmp/catalog.bin", dir);
    if (!net_get(fetch_url, fetch_token(), dest, err, sizeof err)) {
        set_toast(err);
        return;
    }
    lib_add_source(fetch_url, fetch_tok);
    lib_save();
    ncat = imp_catalog(dest, fetch_url, catalog, CATMAX);
    if (!ncat) {
        syncing = 0;
        if (import_path(dest, fetch_url) != 2) screen = SCR_SHELF;
        return;
    }
    cat_i = 0;
    syncing = 1;
    continue_sync();
}

static void draw_footer(void) {
    rect(0, 1028, W, 52, 0x0A0B0E, 0.92f);
    float x = 40, y = 1060;
    if (screen == SCR_SHELF) {
        x = key_chip(x, y, "L R", "shelf");
        x = icon_chip(x, y, G_X, "open");
        x = icon_chip(x, y, G_TRI, "add book");
        x = icon_chip(x, y, G_SQ, "remove added");
        x = key_chip(x, y, "SHARE", "archive");
        key_chip(x, y, "OPT", "about");
    } else if (screen == SCR_TOC) {
        x = key_chip(x, y, "U D", "chapter");
        x = icon_chip(x, y, G_X, "read summary");
        icon_chip(x, y, G_O, "shelf");
    } else if (screen == SCR_READ) {
        x = key_chip(x, y, "L1  LEFT", "prev page");
        x = key_chip(x, y, "R1  RIGHT", "next page");
        icon_chip(x, y, G_O, "contents");
    } else if (screen == SCR_ADD) {
        x = key_chip(x, y, "PAD", "keys");
        x = icon_chip(x, y, G_X, "type");
        x = icon_chip(x, y, G_SQ, "space");
        x = icon_chip(x, y, G_TRI, "bksp");
        x = key_chip(x, y, "R1", "next field");
        x = key_chip(x, y, "OPT", "save book");
        icon_chip(x, y, G_O, "cancel");
    } else if (screen == SCR_FETCH) {
        x = key_chip(x, y, "PAD", "keys");
        x = icon_chip(x, y, G_X, "type");
        x = key_chip(x, y, "OPT", "browse");
        x = key_chip(x, y, "SHARE", "full sync");
        icon_chip(x, y, G_O, "shelf");
    } else if (screen == SCR_CATALOG) {
        x = key_chip(x, y, "U D", "item");
        x = icon_chip(x, y, G_X, "fetch");
        x = key_chip(x, y, "SHARE", "sync rest");
        icon_chip(x, y, G_O, "back");
    } else if (screen == SCR_CONFLICT) {
        x = icon_chip(x, y, G_X, "replace");
        x = icon_chip(x, y, G_SQ, "skip");
        icon_chip(x, y, G_O, "cancel");
    } else icon_chip(x, y, G_O, "back");
}

static void blit_cover(const CoverImg *im, float x, float y, float w, float h) {
    if (!im || !im->px || im->w < 1 || im->h < 1) return;
    float cx = x + w / 2, cy = y + h / 2, hw = w / 2, hh = h / 2, r = 14;
    int x0 = (int)x, y0 = (int)y, dw = (int)w, dh = (int)h;
    for (int j = 0; j < dh; j++) {
        int sy = j * im->h / dh;
        int py = y0 + j;
        if ((unsigned)py >= H) continue;
        for (int i = 0; i < dw; i++) {
            int sx = i * im->w / dw;
            int px_ = x0 + i;
            if ((unsigned)px_ >= W) continue;
            if (sd_rr(px_ + 0.5f, py + 0.5f, cx, cy, hw, hh, r) > 0.45f) continue;
            px(px_, py, im->px[sy * im->w + sx], 1);
        }
    }
}

static void draw_cover(float x, float y, float w, float h, int i, int on) {
    float lift = on ? -16 : 0;
    const CoverImg *im = cov_for_book(i);
    rrect(x + 10, y + 16 + lift, w, h, 14, 0x000000, 0.35f, 0);
    if (im) {
        blit_cover(im, x, y + lift, w, h);
        rrect(x, y + lift, 18, h, 4, 0x000000, 0.28f, 0);
    } else {
        rrect(x, y + lift, w, h, 14, bacc2(i), 1, 0);
        rrect(x + 14, y + 12 + lift, 14, h - 24, 3, mix(bacc2(i), 0x000000, 0.28f), 1, 0);
        rrect(x + 40, y + 36 + lift, w - 62, 5, 2, bacc(i), 0.9f, 0);
        rrect(x + 28, y + h - 132 + lift, w - 46, 100, 8, C_PAPER, 0.96f, 0);
        char nbuf[16]; snprintf(nbuf, sizeof nbuf, "%d", bn(i));
        text(F_MID, x + 40, y + h - 96 + lift, nbuf, C_INK, 1);
        text(F_CAP, x + 40, y + h - 60 + lift, "SUMMARIES", C_INK2, 1);
        text_fit(F_TITLE, x + 40, y + 118 + lift, w - 64, bshort(i), C_TEXT, 1);
        text_fit(F_LBL, x + 40, y + 156 + lift, w - 64, bauth(i), C_DIM, 1);
        text_fit(F_CAP, x + 40, y + 186 + lift, w - 64, bimp(i), C_MUTE, 1);
    }
    if (on) rrect(x, y + lift, w, h, 14, bacc(i), 0.95f, 2.2f);
}

static void draw_add_cover(float x, float y, float w, float h, int on) {
    float lift = on ? -16 : 0;
    rrect(x + 10, y + 16 + lift, w, h, 14, 0x000000, 0.28f, 0);
    rrect(x, y + lift, w, h, 14, C_GLASS, 0.95f, 0);
    rrect(x, y + lift, w, h, 14, C_GOLD, on ? 0.9f : 0.4f, 2);
    text_c(F_HERO, x + w / 2, y + h / 2 - 10 + lift, "+", C_GOLD, 1);
    text_c(F_TITLE, x + w / 2, y + h / 2 + 50 + lift, "Add book", C_TEXT, 1);
    text_c(F_CAP, x + w / 2, y + h / 2 + 84 + lift, "ORIGINAL SUMMARIES", C_MUTE, 1);
}

static void draw_fetch_cover(float x, float y, float w, float h, int on) {
    float lift = on ? -16 : 0;
    rrect(x + 10, y + 16 + lift, w, h, 14, 0x000000, 0.28f, 0);
    rrect(x, y + lift, w, h, 14, C_GLASS, 0.95f, 0);
    rrect(x, y + lift, w, h, 14, 0x7EC8C5, on ? 0.9f : 0.4f, 2);
    text_c(F_HERO, x + w / 2, y + h / 2 - 10 + lift, "A", 0x7EC8C5, 1);
    text_c(F_TITLE, x + w / 2, y + h / 2 + 50 + lift, "Archive", C_TEXT, 1);
    text_c(F_CAP, x + w / 2, y + h / 2 + 84 + lift, "URL  ·  ZIP MD EPUB PDF", C_MUTE, 1);
}

static void draw_shelf(void) {
    text(F_HERO, 80, 108, "Chapter Briefs", C_TEXT, 1);
    text(F_LBL, 80, 150, "Ebook shelf   ·   original chapter summaries only   ·   " APPVER, C_DIM, 1);
    int slots = nlib() + 2;
    float cw = 300, ch = 460, gap = 36;
    float origin = 90;
    if (slots * (cw + gap) > 1740) origin = 90 - book_i * (cw + gap) + 420;
    rect(70, 668, 1780, 28, 0x1A1612, 1);
    rect(70, 668, 1780, 5, 0x2A241C, 1);
    for (int i = 0; i < slots; i++) {
        float x = origin + i * (cw + gap);
        if (x > 1960 || x + cw < -20) continue;
        int on = (i == book_i);
        if (i < nlib()) draw_cover(x, 220, cw, ch, i, on);
        else if (i == nlib()) draw_add_cover(x, 220, cw, ch, on);
        else draw_fetch_cover(x, 220, cw, ch, on);
    }
    rrect(70, 760, 1780, 230, 16, C_GLASS, 0.82f, 0);
    if (book_i < nlib()) {
        text_fit(F_TITLE, 110, 816, 1680, bfull(book_i), C_TEXT, 1);
        text(F_BODY, 110, 860, bauth(book_i), C_GOLD, 1);
        char meta[128];
        snprintf(meta, sizeof meta, "%s   ·   %d chapter summaries   ·   %s",
                 bimp(book_i), bn(book_i), is_user(book_i) ? "added by you" : "built-in");
        text(F_LBL, 110, 900, meta, C_DIM, 1);
        text(F_LBL, 110, 940, confirm_del ? "X confirms remove   O cancels" : "X opens contents   Triangle adds   SHARE fetches an archive", C_MUTE, 1);
    } else if (book_i == nlib()) {
        text(F_TITLE, 110, 816, "Add a summary book", C_TEXT, 1);
        text(F_BODY, 110, 860, "Title, author, then ordered chapter titles and original briefs.", C_GOLD, 1);
        text(F_LBL, 110, 900, "Do not paste book text. Notes only. Saved on this PS4.", C_DIM, 1);
    } else {
        text(F_TITLE, 110, 816, "Fetch from an archive", C_TEXT, 1);
        text(F_BODY, 110, 860, "Your URL. Zip, markdown, epub, or pdf. Optional token. None baked in.", C_GOLD, 1);
        text(F_LBL, 110, 900, "Browse a catalog or full-sync. Title match always asks replace or skip.", C_DIM, 1);
    }
}

static void draw_toc(void) {
    rrect(80, 70, 1760, 70, 12, C_GLASS, 0.88f, 0);
    text_fit(F_TITLE, 108, 116, 1100, bfull(book_i), C_TEXT, 1);
    char meta[96]; snprintf(meta, sizeof meta, "%s   ·   %d summaries", bauth(book_i), bn(book_i));
    text_r(F_LBL, 1808, 116, meta, bacc(book_i), 1);
    const int row_h = 52, vis = 15, list_y = 168;
    if (ch_i < toc_top) toc_top = ch_i;
    if (ch_i >= toc_top + vis) toc_top = ch_i - vis + 1;
    if (toc_top < 0) toc_top = 0;
    rrect(80, list_y, 1760, vis * row_h + 16, 14, C_GLASS, 0.78f, 0);
    for (int i = 0; i < vis; i++) {
        int idx = toc_top + i;
        if (idx >= bn(book_i)) break;
        int on = idx == ch_i;
        float y = list_y + 8 + i * row_h;
        if (on) rrect(96, y, 1728, row_h - 6, 8, C_PANEL, 1, 0);
        if (on) rrect(96, y, 6, row_h - 6, 3, bacc(book_i), 1, 0);
        char num[8]; snprintf(num, sizeof num, "%02d", idx + 1);
        text(F_LBL, 128, y + 32, num, on ? bacc(book_i) : C_MUTE, 1);
        text_fit(F_BODY, 188, y + 32, 1540, ctitle(book_i, idx), on ? C_TEXT : C_DIM, 1);
    }
}

static void draw_read(void) {
    float ox = page_x;
    rrect(140 + ox, 64, 1640, 940, 18, C_PAPER, 1, 0);
    rrect(140 + ox, 64, 14, 940, 4, bacc(book_i), 1, 0);
    text_fit(F_CAP, 190 + ox, 112, 900, bfull(book_i), C_INK2, 1);
    text_r(F_CAP, 1740 + ox, 112, "SUMMARY  ·  " APPVER, C_INK2, 1);
    char num[16]; snprintf(num, sizeof num, "%02d", ch_i + 1);
    text(F_MID, 190 + ox, 186, num, bacc2(book_i), 1);
    char lines[WRAP_N][WRAP_L];
    int nt = wrap_text(F_TITLE, ctitle(book_i, ch_i), 1480, lines, 3);
    for (int i = 0; i < nt; i++) text(F_TITLE, 190 + ox, 250 + i * 40, lines[i], C_INK, 1);
    rrect(190 + ox, 250 + nt * 40 + 8, 160, 2, 1, bacc(book_i), 1, 0);
    int nb = wrap_text(F_BODY, cbrief(book_i, ch_i), 1480, lines, WRAP_N);
    float by = 250 + nt * 40 + 40;
    for (int i = 0; i < nb; i++) text(F_BODY, 190 + ox, by + i * 34, lines[i], C_INK, 1);
    char pos[48]; snprintf(pos, sizeof pos, "%d  /  %d", ch_i + 1, bn(book_i));
    text_c(F_LBL, 960 + ox, 960, pos, C_INK2, 1);
}

static void draw_kb(void) {
    float x0 = 360, y0 = 560, kw = 108, kh = 56, gap = 8;
    for (int r = 0; r < 4; r++) {
        int n = (int)strlen(KB[r]);
        for (int c = 0; c < n; c++) {
            int on = (kb_r == r && kb_c == c);
            float x = x0 + c * (kw + gap), y = y0 + r * (kh + gap);
            rrect(x, y, kw, kh, 8, on ? C_PANEL : C_BG, 1, 0);
            if (on) rrect(x, y, kw, kh, 8, C_GOLD, 0.8f, 1.6f);
            char ch[2] = { KB[r][c], 0 };
            if (!kb_shift && ch[0] >= 'A' && ch[0] <= 'Z') ch[0] = (char)(ch[0] + 32);
            text_c(F_TITLE, x + kw / 2, y + 38, ch, C_TEXT, 1);
        }
    }
    const char *act[] = { "space", "bksp", "shift", "next", "save" };
    for (int i = 0; i < 5; i++) {
        int on = (kb_r == 4 && kb_c == i);
        float x = x0 + i * 220, y = y0 + 4 * (kh + gap);
        rrect(x, y, 208, kh, 8, on ? C_PANEL : C_BG, 1, 0);
        if (on) rrect(x, y, 208, kh, 8, C_GOLD, 0.8f, 1.6f);
        text_c(F_LBL, x + 104, y + 36, act[i], C_TEXT, 1);
    }
}

static void draw_add(void) {
    rrect(80, 50, 1760, 980, 16, C_GLASS, 0.9f, 0);
    text(F_TITLE, 120, 100, "Add a summary book", C_TEXT, 1);
    text(F_LBL, 120, 136, "Original briefs only. Do not paste or reconstruct book text.  " APPVER, C_GOLD, 1);
    const char *labs[] = { "Title", "Author", "Chapter title", "Chapter summary" };
    char *vals[] = { draft.full_title, draft.authors, d_cht, d_chb };
    for (int i = 0; i < 4; i++) {
        float y = 170 + i * 78;
        int on = add_step == i;
        rrect(120, y, 1680, 66, 10, on ? C_PANEL : C_BG, 0.9f, 0);
        if (on) rrect(120, y, 6, 66, 3, C_GOLD, 1, 0);
        text(F_CAP, 148, y + 22, labs[i], C_MUTE, 1);
        text_fit(F_BODY, 148, y + 50, 1600, vals[i][0] ? vals[i] : "_", on ? C_TEXT : C_DIM, 1);
    }
    char st[64]; snprintf(st, sizeof st, "Chapters ready: %d / %d", draft.n, CMAX);
    text(F_LBL, 120, 500, st, C_DIM, 1);
    draw_kb();
}

static void draw_about(void) {
    rrect(160, 80, 1600, 900, 18, C_GLASS, 0.9f, 0);
    text(F_TITLE, 200, 148, "About Chapter Briefs", C_TEXT, 1);
    text(F_BODY, 200, 200, "TITLE_ID BOOK00001    VERSION " APPVER "    summary reader, not the books", C_GOLD, 1);
    text(F_LBL, 200, 246, "Public titles plus original paraphrases. No quoted passages. Added books stay on this PS4.", C_DIM, 1);
    const char *rows[3][2] = {
        { "Spirit Medicine", "36 of 36 printed TOC summaries" },
        { "The Subtle Art of Not Giving a F*ck", "9 of 9 numbered-chapter summaries" },
        { "The 48 Laws of Power", "49 of 49  (Preface + Laws 1-48)" }
    };
    for (int i = 0; i < 3; i++) {
        float y = 300 + i * 100;
        rrect(200, y, 1520, 86, 10, C_PANEL, 0.9f, 0);
        rrect(200, y, 8, 86, 4, BOOKS[i].accent, 1, 0);
        text(F_TITLE, 236, y + 38, rows[i][0], C_TEXT, 1);
        text(F_LBL, 236, y + 70, rows[i][1], BOOKS[i].accent, 1);
    }
    text(F_LBL, 200, 640, "Triangle: type a book. SHARE or the Archive tile: fetch a URL (zip / md / epub / pdf).", C_DIM, 1);
    text(F_LBL, 200, 676, "Token field is optional and never hardcoded. Title match always asks replace or skip.", C_DIM, 1);
    text(F_LBL, 200, 712, lib_path[0] ? lib_path : "No save file yet  ·  first add writes /data/BOOK00001/library.bin", C_MUTE, 1);
    text(F_LBL, 200, 748, "Built-in jackets are original recognizable covers, not scans. Imports use zip/epub/pdf art or cover.jpg.", C_MUTE, 1);
    text(F_LBL, 200, 800, "Inter typeface — SIL Open Font License.", C_MUTE, 1);
}

static void draw_toast(void) {
    if (SDL_GetTicks() >= toast_until || !toast[0]) return;
    float tw = text_w(F_BODY, toast) + 48;
    rrect(960 - tw / 2, 980, tw, 40, 10, C_PANEL, 0.95f, 0);
    rrect(960 - tw / 2, 980, tw, 40, 10, C_GOLD, 0.55f, 1);
    text_c(F_BODY, 960, 1006, toast, C_TEXT, 1);
}

static void draw_kb_rows(const char **rows, int nrows) {
    float x0 = 300, y0 = 560, kw = 100, kh = 50, gap = 8;
    for (int r = 0; r < nrows; r++) {
        int n = (int)strlen(rows[r]);
        for (int c = 0; c < n; c++) {
            int on = (kb_r == r && kb_c == c);
            float x = x0 + c * (kw + gap), y = y0 + r * (kh + gap);
            rrect(x, y, kw, kh, 8, on ? C_PANEL : C_BG, 1, 0);
            if (on) rrect(x, y, kw, kh, 8, C_GOLD, 0.8f, 1.6f);
            char ch[2] = { rows[r][c], 0 };
            if (!kb_shift && ch[0] >= 'A' && ch[0] <= 'Z') ch[0] = (char)(ch[0] + 32);
            text_c(F_TITLE, x + kw / 2, y + 34, ch, C_TEXT, 1);
        }
    }
}

static void draw_fetch(void) {
    rrect(80, 50, 1760, 980, 16, C_GLASS, 0.9f, 0);
    text(F_TITLE, 120, 100, "Archive fetch", C_TEXT, 1);
    text(F_LBL, 120, 136, "Your URL. HTTP LAN or HTTPS. Optional token — not stored in toasts.  " APPVER, C_GOLD, 1);
    const char *labs[] = { "URL", "Token (optional)" };
    const char *vals[] = { fetch_url[0] ? fetch_url : "_", fetch_tok[0] ? "(token set)" : "_" };
    for (int i = 0; i < 2; i++) {
        float y = 170 + i * 78;
        int on = fetch_step == i;
        rrect(120, y, 1680, 66, 10, on ? C_PANEL : C_BG, 0.9f, 0);
        if (on) rrect(120, y, 6, 66, 3, C_GOLD, 1, 0);
        text(F_CAP, 148, y + 22, labs[i], C_MUTE, 1);
        text_fit(F_BODY, 148, y + 50, 1600, vals[i], on ? C_TEXT : C_DIM, 1);
    }
    text(F_LBL, 120, 340, "Saved sources  ·  L1 R1 pick  ·  X loads URL", C_DIM, 1);
    if (!nsrc) text(F_LBL, 120, 376, "None yet. OPTIONS browses. SHARE full-syncs.", C_MUTE, 1);
    for (int i = 0; i < nsrc && i < 5; i++) {
        int on = fetch_step == 2 && src_sel == i;
        float y = 370 + i * 34;
        text_fit(F_LBL, 120, y, 1600, SOURCES[i].url, on ? C_GOLD : C_DIM, 1);
    }
    draw_kb_rows(KBURL, 5);
    {
        const char *act[] = { "space", "bksp", "shift", "next", "browse" };
        float x0 = 300, y = 560 + 5 * 58, kw = 208;
        for (int i = 0; i < 5; i++) {
            int on = (kb_r == 5 && kb_c == i);
            float x = x0 + i * 220;
            rrect(x, y, kw, 50, 8, on ? C_PANEL : C_BG, 1, 0);
            if (on) rrect(x, y, kw, 50, 8, C_GOLD, 0.8f, 1.6f);
            text_c(F_LBL, x + 104, y + 34, act[i], C_TEXT, 1);
        }
    }
}

static void draw_catalog(void) {
    rrect(80, 70, 1760, 70, 12, C_GLASS, 0.88f, 0);
    text(F_TITLE, 108, 116, "Archive catalog", C_TEXT, 1);
    char meta[64]; snprintf(meta, sizeof meta, "%d items", ncat);
    text_r(F_LBL, 1808, 116, meta, C_GOLD, 1);
    const int row_h = 52, vis = 14, list_y = 168;
    if (cat_i < 0) cat_i = 0;
    if (cat_i >= ncat) cat_i = ncat ? ncat - 1 : 0;
    int top = cat_i - vis / 2;
    if (top < 0) top = 0;
    rrect(80, list_y, 1760, vis * row_h + 16, 14, C_GLASS, 0.78f, 0);
    for (int i = 0; i < vis; i++) {
        int idx = top + i;
        if (idx >= ncat) break;
        int on = idx == cat_i;
        float y = list_y + 8 + i * row_h;
        if (on) rrect(96, y, 1728, row_h - 6, 8, C_PANEL, 1, 0);
        text_fit(F_BODY, 128, y + 32, 900, catalog[idx].title, on ? C_TEXT : C_DIM, 1);
        text_fit(F_LBL, 1060, y + 32, 740, catalog[idx].url, on ? C_GOLD : C_MUTE, 1);
    }
}

static void draw_conflict(void) {
    rrect(260, 220, 1400, 560, 18, C_GLASS, 0.94f, 0);
    text_c(F_TITLE, 960, 300, "This title is already on the shelf", C_TEXT, 1);
    text_c(F_BODY, 960, 360, pending.full_title, C_GOLD, 1);
    text_c(F_LBL, 960, 410, pending_match >= NBOOKS ? "An added book uses this title." : "A built-in book uses this title.", C_DIM, 1);
    text_c(F_LBL, 960, 450, "Replace the existing copy, or skip this import. Always asked.", C_MUTE, 1);
    rrect(520, 520, 360, 80, 12, C_PANEL, 1, 0);
    rrect(1040, 520, 360, 80, 12, C_PANEL, 1, 0);
    text_c(F_TITLE, 700, 570, "X  Replace", C_TEXT, 1);
    text_c(F_TITLE, 1220, 570, "Square  Skip", C_TEXT, 1);
    text_c(F_LBL, 960, 700, "O cancels the rest of a sync", C_MUTE, 1);
}

static void draw(void) {
    memcpy(fb, bgb, (size_t)W * H * 4);
    T = fb;
    if (screen == SCR_SHELF) draw_shelf();
    else if (screen == SCR_TOC) draw_toc();
    else if (screen == SCR_READ) draw_read();
    else if (screen == SCR_ADD) draw_add();
    else if (screen == SCR_FETCH) draw_fetch();
    else if (screen == SCR_CATALOG) draw_catalog();
    else if (screen == SCR_CONFLICT) draw_conflict();
    else draw_about();
    draw_footer();
    draw_toast();
}
static void present(void) { SDL_BlitSurface(fbs, NULL, wsurf, NULL); SDL_UpdateWindowSurface(win); }

enum { B_X = 0, B_O, B_SQ, B_TRI, B_L1, B_R1, B_SHARE, B_OPT, B_L3, B_R3, B_UP = 13, B_DN, B_LT, B_RT };

static float axis(SDL_Joystick *j, int a) {
    if (!j) return 0;
    float v = SDL_JoystickGetAxis(j, a) / 32767.0f;
    float dz = 0.28f;
    if (fabsf(v) < dz) return 0;
    return (v > 0 ? v - dz : v + dz) / (1 - dz);
}
static void go(int s) {
    from_scr = screen;
    screen = s;
    confirm_del = 0;
    if (s == SCR_READ) page_x = 70, page_tgt = 0;
}
static void clamp_ch(void) {
    int n = bn(book_i);
    if (n < 1) { ch_i = 0; return; }
    if (ch_i < 0) ch_i = 0;
    if (ch_i >= n) ch_i = n - 1;
    last_ch[book_i] = ch_i;
}
static void kb_move(int dx, int dy) {
    kb_r += dy; kb_c += dx;
    if (kb_r < 0) kb_r = 4;
    if (kb_r > 4) kb_r = 0;
    int maxc = kb_r < 4 ? (int)strlen(KB[kb_r]) - 1 : 4;
    if (kb_c < 0) kb_c = maxc;
    if (kb_c > maxc) kb_c = 0;
}
static void kb_fire(void) {
    if (kb_r < 4) {
        char ch = KB[kb_r][kb_c];
        if (!kb_shift && ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
        kb_insert(ch);
        return;
    }
    if (kb_c == 0) kb_insert(' ');
    else if (kb_c == 1) kb_bksp();
    else if (kb_c == 2) kb_shift = !kb_shift;
    else if (kb_c == 3) {
        if (add_step == 3) commit_chapter();
        else add_step = (add_step + 1) % 4;
    } else save_draft();
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    sceSystemServiceHideSplashScreen();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) { for (;;) {} }
    win = SDL_CreateWindow("Chapter Briefs", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, W, H, 0);
    if (!win) for (;;) {}
    wsurf = SDL_GetWindowSurface(win);
    fb = malloc((size_t)W * H * 4); bgb = malloc((size_t)W * H * 4); T = fb;
    if (!fb || !bgb) for (;;) {}
    fbs = SDL_CreateRGBSurfaceWithFormatFrom(fb, W, H, 32, W * 4, SDL_PIXELFORMAT_RGB888);
    fonts_init();
    build_bg();
    lib_load();
    cov_init();
    cov_load_users();
    net_init();
    SDL_Joystick *pad = SDL_NumJoysticks() > 0 ? SDL_JoystickOpen(0) : 0;
    if (nuser) { char m[64]; snprintf(m, sizeof m, "Loaded %d added book%s", nuser, nuser == 1 ? "" : "s"); set_toast(m); }

    Uint32 last = SDL_GetTicks();
    int held[17] = {0};
    Uint32 rep = 0;
    float stick_acc = 0;
    for (;;) {
        Uint32 now = SDL_GetTicks();
        float dt = (now - last) / 1000.0f; if (dt > 0.1f) dt = 0.1f; last = now;
        page_x += (page_tgt - page_x) * clampf(dt * 14, 0, 1);
        int pr[17] = {0};
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) goto out;
            if (ev.type == SDL_JOYBUTTONDOWN && ev.jbutton.button < 17) {
                pr[ev.jbutton.button] = 1; held[ev.jbutton.button] = 1; rep = now + 320;
            }
            if (ev.type == SDL_JOYBUTTONUP && ev.jbutton.button < 17) held[ev.jbutton.button] = 0;
        }
        int dx = 0, dy = 0;
        if (pr[B_LT]) dx = -1; if (pr[B_RT]) dx = 1;
        if (pr[B_UP]) dy = -1; if (pr[B_DN]) dy = 1;
        if (!dx && !dy && now > rep && (held[B_LT] || held[B_RT] || held[B_UP] || held[B_DN])) {
            rep = now + 90;
            dx = held[B_LT] ? -1 : held[B_RT] ? 1 : 0;
            dy = held[B_UP] ? -1 : held[B_DN] ? 1 : 0;
        }
        float lx = axis(pad, 0), ly = axis(pad, 1);
        stick_acc += (fabsf(lx) > fabsf(ly) ? lx : ly) * dt * 9;
        if (stick_acc > 1) { stick_acc -= 1; if (fabsf(lx) > fabsf(ly)) dx = 1; else dy = 1; }
        if (stick_acc < -1) { stick_acc += 1; if (fabsf(lx) > fabsf(ly)) dx = -1; else dy = -1; }

        if (pr[B_OPT] && screen != SCR_ADD && screen != SCR_FETCH && screen != SCR_CATALOG && screen != SCR_CONFLICT) {
            if (screen == SCR_ABOUT) go(from_scr == SCR_ABOUT ? SCR_SHELF : from_scr);
            else go(SCR_ABOUT);
        }

        if (screen == SCR_SHELF) {
            int slots = nlib() + 2;
            if (pr[B_L1]) book_i -= 3;
            if (pr[B_R1]) book_i += 3;
            if (dx) book_i += dx;
            if (book_i < 0) book_i = 0;
            if (book_i >= slots) book_i = slots - 1;
            if (pr[B_TRI]) start_add();
            if (pr[B_SHARE]) start_fetch();
            if (pr[B_O] && confirm_del) confirm_del = 0;
            else if (pr[B_O]) go(SCR_ABOUT);
            if (pr[B_SQ] && book_i >= NBOOKS && book_i < nlib()) {
                if (confirm_del) { delete_user(book_i); confirm_del = 0; }
                else { confirm_del = 1; set_toast("X to remove this added book"); }
            }
            if (pr[B_X]) {
                if (confirm_del && book_i >= NBOOKS && book_i < nlib()) { delete_user(book_i); confirm_del = 0; }
                else if (book_i == nlib()) start_add();
                else if (book_i > nlib()) start_fetch();
                else { ch_i = last_ch[book_i]; clamp_ch(); go(SCR_TOC); }
            }
        } else if (screen == SCR_TOC) {
            if (pr[B_L1]) ch_i -= 8;
            if (pr[B_R1]) ch_i += 8;
            if (dy) ch_i += dy;
            clamp_ch();
            if (pr[B_X] && bn(book_i) > 0) go(SCR_READ);
            if (pr[B_O]) go(SCR_SHELF);
        } else if (screen == SCR_READ) {
            int step = 0;
            if (pr[B_L1] || dx < 0) step = -1;
            if (pr[B_R1] || dx > 0) step = 1;
            if (step) { ch_i += step; clamp_ch(); page_x = step > 0 ? 90 : -90; page_tgt = 0; }
            if (pr[B_O]) go(SCR_TOC);
        } else if (screen == SCR_ADD) {
            if (dx || dy) kb_move(dx, dy);
            if (pr[B_X]) kb_fire();
            if (pr[B_SQ]) kb_insert(' ');
            if (pr[B_TRI]) kb_bksp();
            if (pr[B_L1]) kb_shift = !kb_shift;
            if (pr[B_R1]) {
                if (add_step == 3) commit_chapter();
                else add_step = (add_step + 1) % 4;
            }
            if (pr[B_OPT]) save_draft();
            if (pr[B_O]) { screen = SCR_SHELF; set_toast("Add cancelled"); }
        } else if (screen == SCR_FETCH) {
            int nrows = 5, maxc;
            if (fetch_step == 2) {
                if (dx) { src_sel += dx; if (src_sel < 0) src_sel = nsrc ? nsrc - 1 : 0; if (src_sel >= nsrc) src_sel = 0; }
                if (pr[B_X] && nsrc) {
                    snprintf(fetch_url, sizeof fetch_url, "%s", SOURCES[src_sel].url);
                    fetch_tok[0] = 0;
                    if (SOURCES[src_sel].token[0]) snprintf(fetch_tok, sizeof fetch_tok, "%s", SOURCES[src_sel].token);
                    fetch_step = 0;
                    set_toast("Loaded source URL");
                }
            } else if (dx || dy) {
                kb_r += dy; kb_c += dx;
                if (kb_r < 0) kb_r = nrows;
                if (kb_r > nrows) kb_r = 0;
                maxc = kb_r < nrows ? (int)strlen(KBURL[kb_r]) - 1 : 4;
                if (kb_c < 0) kb_c = maxc;
                if (kb_c > maxc) kb_c = 0;
            }
            if (pr[B_R1] || (pr[B_X] && kb_r == 5 && kb_c == 3)) fetch_step = (fetch_step + 1) % 3;
            if (pr[B_X] && fetch_step < 2 && kb_r < 5) {
                char ch = KBURL[kb_r][kb_c];
                if (!kb_shift && ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
                char *s = fetch_step == 0 ? fetch_url : fetch_tok;
                int lim = fetch_step == 0 ? UL - 1 : TK - 1;
                int n = (int)strlen(s);
                if (n < lim) { s[n] = ch; s[n + 1] = 0; }
            }
            if (pr[B_X] && kb_r == 5) {
                char *s = fetch_step == 0 ? fetch_url : fetch_tok;
                if (kb_c == 0) { int n = (int)strlen(s); if (n < (fetch_step ? TK : UL) - 1) { s[n] = ' '; s[n + 1] = 0; } }
                else if (kb_c == 1) { int n = (int)strlen(s); if (n) s[n - 1] = 0; }
                else if (kb_c == 2) kb_shift = !kb_shift;
                else if (kb_c == 4) start_browse();
            }
            if (pr[B_SQ]) {
                char *s = fetch_step == 0 ? fetch_url : fetch_tok;
                int n = (int)strlen(s), lim = fetch_step == 0 ? UL - 1 : TK - 1;
                if (n < lim) { s[n] = ' '; s[n + 1] = 0; }
            }
            if (pr[B_TRI]) {
                char *s = fetch_step == 0 ? fetch_url : fetch_tok;
                int n = (int)strlen(s); if (n) s[n - 1] = 0;
            }
            if (pr[B_L1]) kb_shift = !kb_shift;
            if (pr[B_OPT]) start_browse();
            if (pr[B_SHARE]) start_sync();
            if (pr[B_O]) { screen = SCR_SHELF; set_toast("Fetch cancelled"); }
        } else if (screen == SCR_CATALOG) {
            if (dy) cat_i += dy;
            if (pr[B_L1]) cat_i -= 8;
            if (pr[B_R1]) cat_i += 8;
            if (cat_i < 0) cat_i = 0;
            if (cat_i >= ncat) cat_i = ncat ? ncat - 1 : 0;
            if (pr[B_X] && ncat) fetch_one(catalog[cat_i].url);
            if (pr[B_SHARE] && ncat) { syncing = 1; continue_sync(); }
            if (pr[B_O]) screen = SCR_FETCH;
        } else if (screen == SCR_CONFLICT) {
            if (pr[B_X]) { apply_pending(1); if (syncing) continue_sync(); else screen = SCR_SHELF; }
            if (pr[B_SQ]) { memset(&pending, 0, sizeof pending); pending_cover[0] = 0; set_toast("Skipped"); if (syncing) continue_sync(); else screen = SCR_FETCH; }
            if (pr[B_O]) { syncing = 0; screen = SCR_SHELF; set_toast("Import cancelled"); }
        } else {
            if (pr[B_O] || pr[B_X]) go(from_scr == SCR_ABOUT ? SCR_SHELF : from_scr);
        }

        draw();
        present();
        Uint32 el = SDL_GetTicks() - now;
        if (el < 16) SDL_Delay(16 - (int)el);
    }
out:
    SDL_Quit();
    return 0;
}
