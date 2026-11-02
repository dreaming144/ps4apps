#include "import.h"
#include "chapters.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stb/stb_image.h>

static void seterr(char *err, int n, const char *s) {
    if (err && n) snprintf(err, n, "%s", s);
}

static void trim(char *s) {
    char *a = s;
    while (*a && isspace((unsigned char)*a)) a++;
    if (a != s) memmove(s, a, strlen(a) + 1);
    int n = (int)strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = 0;
}

static void norm(char *d, int n, const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (!strncasecmp(s, "the ", 4)) s += 4;
    int i = 0, sp = 0;
    for (; *s && i < n - 1; s++) {
        unsigned char c = (unsigned char)*s;
        if (isspace(c)) {
            if (!sp && i) { d[i++] = ' '; sp = 1; }
            continue;
        }
        sp = 0;
        d[i++] = (char)tolower(c);
    }
    while (i > 0 && d[i - 1] == ' ') i--;
    d[i] = 0;
}

int imp_title_match(const char *title) {
    char a[TL], b[TL];
    norm(a, sizeof a, title ? title : "");
    if (!a[0]) return -1;
    for (int i = 0; i < NBOOKS; i++) {
        norm(b, sizeof b, BOOKS[i].full_title);
        if (!strcmp(a, b)) return i;
        norm(b, sizeof b, BOOKS[i].short_name);
        if (!strcmp(a, b)) return i;
    }
    for (int i = 0; i < nuser; i++) {
        norm(b, sizeof b, UBOOKS[i].full_title);
        if (!strcmp(a, b)) return NBOOKS + i;
        norm(b, sizeof b, UBOOKS[i].short_name);
        if (!strcmp(a, b)) return NBOOKS + i;
    }
    return -1;
}

static unsigned char *slurp(const char *path, int *n) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long s = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (s < 0 || s > 8 * 1024 * 1024) { fclose(f); return 0; }
    unsigned char *b = malloc((size_t)s + 1);
    if (!b) { fclose(f); return 0; }
    if (fread(b, 1, (size_t)s, f) != (size_t)s) { fclose(f); free(b); return 0; }
    fclose(f);
    b[s] = 0;
    if (n) *n = (int)s;
    return b;
}

static int looks_text(const unsigned char *p, int n) {
    int m = n < 400 ? n : 400;
    if (m < 4) return 0;
    for (int i = 0; i < m; i++) {
        if (p[i] == 0) return 0;
        if (p[i] < 9) return 0;
    }
    return 1;
}

static const char *basename_of(const char *p) {
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

static void title_from_name(char *out, int n, const char *name) {
    snprintf(out, n, "%s", basename_of(name));
    char *dot = strrchr(out, '.');
    if (dot) *dot = 0;
    for (char *c = out; *c; c++) if (*c == '_' || *c == '-') *c = ' ';
    trim(out);
}

static int is_cover_name(const char *name) {
    const char *b = basename_of(name);
    char low[128];
    snprintf(low, sizeof low, "%s", b);
    for (char *c = low; *c; c++) *c = (char)tolower((unsigned char)*c);
    if (!strcmp(low, "cover.jpg") || !strcmp(low, "cover.jpeg") || !strcmp(low, "cover.png")) return 2;
    if (!strcmp(low, "jacket.jpg") || !strcmp(low, "jacket.png")) return 2;
    if (strstr(low, "cover.") && (strstr(low, ".jpg") || strstr(low, ".png") || strstr(low, ".jpeg"))) return 1;
    return 0;
}

static int is_img_name(const char *name) {
    const char *b = basename_of(name);
    const char *d = strrchr(b, '.');
    if (!d) return 0;
    return !strcasecmp(d, ".jpg") || !strcasecmp(d, ".jpeg") || !strcasecmp(d, ".png");
}

static int is_md_name(const char *name) {
    const char *d = strrchr(name, '.');
    return d && (!strcasecmp(d, ".md") || !strcasecmp(d, ".txt"));
}

static int is_html_name(const char *name) {
    const char *d = strrchr(name, '.');
    return d && (!strcasecmp(d, ".html") || !strcasecmp(d, ".htm") || !strcasecmp(d, ".xhtml"));
}

int imp_is_book_file(const unsigned char *p, int n, const char *name) {
    if (n >= 4 && p[0] == 'P' && p[1] == 'K' && p[2] == 3 && p[3] == 4) return 1;
    if (n >= 5 && !memcmp(p, "%PDF-", 5)) return 1;
    if (name) {
        const char *d = strrchr(name, '.');
        if (d && (!strcasecmp(d, ".md") || !strcasecmp(d, ".epub") || !strcasecmp(d, ".zip") ||
                  !strcasecmp(d, ".pdf") || !strcasecmp(d, ".txt")))
            return 1;
    }
    if (looks_text(p, n) && (strstr((const char *)p, "\n## ") || p[0] == '#')) return 1;
    return 0;
}

static void html_to_text(char *s) {
    char *d = s;
    int in = 0;
    for (char *p = s; *p; p++) {
        if (*p == '<') {
            if (!strncasecmp(p, "<br", 3) || !strncasecmp(p, "<p", 2) || !strncasecmp(p, "<h", 2) ||
                !strncasecmp(p, "</p", 3) || !strncasecmp(p, "</h", 3) || !strncasecmp(p, "<div", 4)) {
                *d++ = '\n';
            }
            in = 1;
            continue;
        }
        if (*p == '>') { in = 0; continue; }
        if (!in) {
            if (*p == '&') {
                if (!strncmp(p, "&amp;", 5)) { *d++ = '&'; p += 4; }
                else if (!strncmp(p, "&lt;", 4)) { *d++ = '<'; p += 3; }
                else if (!strncmp(p, "&gt;", 4)) { *d++ = '>'; p += 3; }
                else if (!strncmp(p, "&nbsp;", 6)) { *d++ = ' '; p += 5; }
                else if (!strncmp(p, "&#39;", 5) || !strncmp(p, "&apos;", 6)) { *d++ = '\''; while (*p && *p != ';') p++; }
                else { *d++ = ' '; while (*p && *p != ';') p++; if (!*p) break; }
            } else *d++ = *p;
        }
    }
    *d = 0;
}

static void add_ch(UserBook *b, const char *title, const char *brief) {
    if (!b || b->n >= CMAX) return;
    char t[TL], br[BL];
    snprintf(t, sizeof t, "%s", title && title[0] ? title : "Untitled");
    snprintf(br, sizeof br, "%s", brief && brief[0] ? brief : "(no summary yet)");
    trim(t);
    trim(br);
    if (!t[0]) return;
    snprintf(b->title[b->n], TL, "%s", t);
    snprintf(b->brief[b->n], BL, "%s", br[0] ? br : "(no summary yet)");
    b->n++;
}

static void parse_md(UserBook *b, char *text) {
    if ((unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) text += 3;
    char *p = text;
    char pending_t[TL];
    char pending_b[BL];
    pending_t[0] = pending_b[0] = 0;
    int got_title = 0;
    while (*p) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        char line[512];
        snprintf(line, sizeof line, "%s", p);
        if (line[strlen(line) ? strlen(line) - 1 : 0] == '\r') line[strlen(line) - 1] = 0;
        trim(line);
        if (!got_title && line[0] == '#' && line[1] != '#') {
            const char *t = line + 1;
            while (*t == '#' || *t == ' ') t++;
            snprintf(b->full_title, TL, "%s", t);
            trim(b->full_title);
            got_title = 1;
        } else if (!b->authors[0] && (!strncasecmp(line, "by ", 3) || !strncasecmp(line, "author:", 7))) {
            const char *a = strchr(line, ' ');
            if (a) {
                while (*a == ' ' || *a == ':') a++;
                snprintf(b->authors, TL, "%s", a);
                trim(b->authors);
            }
        } else if (line[0] == '#' && line[1] == '#') {
            if (pending_t[0]) add_ch(b, pending_t, pending_b);
            const char *t = line + 2;
            while (*t == '#' || *t == ' ') t++;
            snprintf(pending_t, sizeof pending_t, "%s", t);
            pending_b[0] = 0;
        } else if (line[0]) {
            if (!got_title) {
                snprintf(b->full_title, TL, "%s", line);
                got_title = 1;
            } else if (pending_t[0]) {
                size_t used = strlen(pending_b);
                if (used && used < BL - 2) { pending_b[used++] = ' '; pending_b[used] = 0; }
                snprintf(pending_b + strlen(pending_b), BL - strlen(pending_b), "%s", line);
            }
        }
        if (!nl) break;
        p = nl + 1;
    }
    if (pending_t[0]) add_ch(b, pending_t, pending_b);
    if (!b->n && b->full_title[0]) add_ch(b, "Notes", "Imported notes. Add original chapter summaries — do not paste book text.");
}

static int parse_md_mem(UserBook *b, unsigned char *p, int n) {
    char *t = malloc((size_t)n + 1);
    if (!t) return 0;
    memcpy(t, p, (size_t)n);
    t[n] = 0;
    parse_md(b, t);
    free(t);
    return b->n > 0 || b->full_title[0];
}

static int parse_html_mem(UserBook *b, unsigned char *p, int n) {
    char *t = malloc((size_t)n + 1);
    if (!t) return 0;
    memcpy(t, p, (size_t)n);
    t[n] = 0;
    html_to_text(t);
    parse_md(b, t);
    free(t);
    return b->n > 0 || b->full_title[0];
}

static uint16_t ru16(const unsigned char *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t ru32(const unsigned char *p) { return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

typedef struct {
    UserBook *b;
    unsigned char *cover;
    int cover_n;
    int cover_rank;
    int best_img_n;
    unsigned char *best_img;
} ZipAcc;

static void zip_use(const char *name, const unsigned char *data, int len, ZipAcc *z) {
    if (!data || len < 1) return;
    int cr = is_cover_name(name);
    if (cr && (!z->cover || cr > z->cover_rank || (cr == z->cover_rank && len > z->cover_n))) {
        unsigned char *c = malloc((size_t)len);
        if (c) {
            memcpy(c, data, (size_t)len);
            free(z->cover);
            z->cover = c;
            z->cover_n = len;
            z->cover_rank = cr;
        }
    } else if (is_img_name(name) && len > z->best_img_n && len < 2 * 1024 * 1024) {
        unsigned char *c = malloc((size_t)len);
        if (c) {
            memcpy(c, data, (size_t)len);
            free(z->best_img);
            z->best_img = c;
            z->best_img_n = len;
        }
    }
    if (is_md_name(name)) parse_md_mem(z->b, (unsigned char *)data, len);
    else if (is_html_name(name) && !z->b->n) parse_html_mem(z->b, (unsigned char *)data, len);
}

static int zip_walk(const unsigned char *z, int n, ZipAcc *acc) {
    int i = 0, files = 0;
    while (i + 30 <= n) {
        if (z[i] != 'P' || z[i + 1] != 'K') break;
        if (z[i + 2] == 1 && z[i + 3] == 2) break;
        if (z[i + 2] != 3 || z[i + 3] != 4) break;
        uint16_t method = ru16(z + i + 8);
        uint32_t csz = ru32(z + i + 18);
        uint32_t usz = ru32(z + i + 22);
        uint16_t nlen = ru16(z + i + 26);
        uint16_t xlen = ru16(z + i + 28);
        if ((uint64_t)i + 30 + nlen + xlen + csz > (uint64_t)n) break;
        char name[256];
        int nl = nlen < 255 ? nlen : 255;
        memcpy(name, z + i + 30, (size_t)nl);
        name[nl] = 0;
        const unsigned char *payload = z + i + 30 + nlen + xlen;
        if (method == 0) {
            zip_use(name, payload, (int)(usz && usz <= csz ? usz : csz), acc);
            files++;
        } else if (method == 8 && csz > 0 && usz < 4 * 1024 * 1024) {
            int olen = 0;
            char *dec = stbi_zlib_decode_noheader_malloc((const char *)payload, (int)csz, &olen);
            if (dec) {
                zip_use(name, (unsigned char *)dec, olen, acc);
                free(dec);
                files++;
            }
        }
        i += 30 + nlen + xlen + (int)csz;
    }
    return files;
}

static int parse_zip(UserBook *b, unsigned char *p, int n, char *cover_src, int cover_n) {
    ZipAcc z;
    memset(&z, 0, sizeof z);
    z.b = b;
    int files = zip_walk(p, n, &z);
    if (!b->full_title[0]) snprintf(b->full_title, TL, "Imported archive");
    if (!b->n) add_ch(b, "Notes", "Archive imported. Add original chapter summaries — do not paste book text.");
    if (!z.cover && z.best_img) {
        z.cover = z.best_img;
        z.cover_n = z.best_img_n;
        z.best_img = 0;
    }
    if (z.cover && cover_src && cover_n) {
        char dir[192], tmp[256];
        lib_dir(dir, sizeof dir);
        snprintf(tmp, sizeof tmp, "%s/tmp/cover.bin", dir);
        lib_ensure_tmp();
        FILE *f = fopen(tmp, "wb");
        if (f) {
            fwrite(z.cover, 1, (size_t)z.cover_n, f);
            fclose(f);
            snprintf(cover_src, cover_n, "%s", tmp);
        }
    }
    free(z.cover);
    free(z.best_img);
    return files > 0;
}

static int parse_pdf(UserBook *b, const char *path) {
    title_from_name(b->full_title, TL, path);
    if (!b->full_title[0]) snprintf(b->full_title, TL, "Imported PDF");
    add_ch(b, "Notes", "PDF listing imported. Text was not extracted. Add your own original briefs — do not paste book text.");
    return 1;
}

void imp_sibling_cover(const char *bookpath, char *out, int n) {
    if (!bookpath || !out || n < 8) return;
    out[0] = 0;
    char dir[192];
    snprintf(dir, sizeof dir, "%s", bookpath);
    char *slash = strrchr(dir, '/');
    char base[128];
    snprintf(base, sizeof base, "%s", slash ? slash + 1 : bookpath);
    if (slash) *slash = 0;
    else snprintf(dir, sizeof dir, ".");
    char *dot = strrchr(base, '.');
    if (dot) *dot = 0;
    const char *try[] = { 0, 0, 0, 0, 0 };
    char a[256], b[256], c[256], d[256], e[256];
    snprintf(a, sizeof a, "%s/cover.jpg", dir);
    snprintf(b, sizeof b, "%s/cover.png", dir);
    snprintf(c, sizeof c, "%s/cover.jpeg", dir);
    snprintf(d, sizeof d, "%s/%s.jpg", dir, base);
    snprintf(e, sizeof e, "%s/%s.png", dir, base);
    try[0] = a; try[1] = b; try[2] = c; try[3] = d; try[4] = e;
    for (int i = 0; i < 5; i++) {
        FILE *f = fopen(try[i], "rb");
        if (!f) continue;
        fclose(f);
        snprintf(out, n, "%s", try[i]);
        return;
    }
}

int imp_file(const char *path, UserBook *out, char *cover_src, int cover_n, char *err, int errn) {
    if (cover_src && cover_n) cover_src[0] = 0;
    memset(out, 0, sizeof *out);
    snprintf(out->imprint, sizeof out->imprint, "Imported notes");
    out->accent = 0x7EC8C5;
    out->accent2 = 0x1A2830;
    int n = 0;
    unsigned char *p = slurp(path, &n);
    if (!p) { seterr(err, errn, "Cannot read download"); return 0; }
    int ok = 0;
    if (n >= 4 && p[0] == 'P' && p[1] == 'K') {
        ok = parse_zip(out, p, n, cover_src, cover_n);
        if (!ok) seterr(err, errn, "Zip/epub had no usable notes");
    } else if (n >= 5 && !memcmp(p, "%PDF-", 5)) {
        ok = parse_pdf(out, path);
    } else if (looks_text(p, n)) {
        if (strstr((char *)p, "<html") || strstr((char *)p, "<HTML") || strstr((char *)p, "<body"))
            ok = parse_html_mem(out, p, n);
        else
            ok = parse_md_mem(out, p, n);
        if (!ok) seterr(err, errn, "No title or chapter headings found");
    } else {
        seterr(err, errn, "Not zip, md, epub, or pdf");
    }
    free(p);
    if (!ok) return 0;
    if (!out->full_title[0]) title_from_name(out->full_title, TL, path);
    snprintf(out->short_name, sizeof out->short_name, "%s", out->full_title);
    out->short_name[28] = 0;
    if (!out->authors[0]) snprintf(out->authors, TL, "Imported");
    if (!out->n) add_ch(out, "Notes", "Imported. Add original chapter summaries — do not paste book text.");
    if (cover_src && cover_n && !cover_src[0]) imp_sibling_cover(path, cover_src, cover_n);
    seterr(err, errn, "OK");
    return 1;
}

static int already(CatItem *dst, int n, const char *url) {
    for (int i = 0; i < n; i++) if (!strcmp(dst[i].url, url)) return 1;
    return 0;
}

static void add_cat(CatItem *dst, int maxn, int *n, const char *title, const char *url) {
    if (*n >= maxn || !url || !url[0]) return;
    if (already(dst, *n, url)) return;
    if (strncmp(url, "http://", 7) && strncmp(url, "https://", 8)) return;
    snprintf(dst[*n].url, sizeof dst[*n].url, "%s", url);
    if (title && title[0]) snprintf(dst[*n].title, TL, "%s", title);
    else title_from_name(dst[*n].title, TL, url);
    trim(dst[*n].title);
    (*n)++;
}

static void url_join(char *out, int n, const char *base, const char *rel) {
    if (!rel || !rel[0]) { out[0] = 0; return; }
    if (!strncmp(rel, "http://", 7) || !strncmp(rel, "https://", 8)) {
        snprintf(out, n, "%s", rel);
        return;
    }
    const char *scheme = strstr(base, "://");
    const char *host = scheme ? scheme + 3 : base;
    if (rel[0] == '/') {
        const char *slash = strchr(host, '/');
        int prefix = slash ? (int)(slash - base) : (int)strlen(base);
        snprintf(out, n, "%.*s%s", prefix, base, rel);
        return;
    }
    const char *slash = strrchr(host, '/');
    if (!slash) snprintf(out, n, "%s/%s", base, rel);
    else snprintf(out, n, "%.*s%s", (int)(slash - base + 1), base, rel);
}

static int looks_book_href(const char *u) {
    const char *d = strrchr(u, '.');
    if (!d) return 0;
    return !strncasecmp(d, ".md", 3) || !strncasecmp(d, ".zip", 4) || !strncasecmp(d, ".epub", 5) ||
           !strncasecmp(d, ".pdf", 4) || !strncasecmp(d, ".txt", 4);
}

int imp_catalog(const char *path, const char *base_url, CatItem *dst, int maxn) {
    int n = 0, sz = 0;
    unsigned char *p = slurp(path, &sz);
    if (!p) return 0;
    if (imp_is_book_file(p, sz, path) && !(looks_text(p, sz) && strstr((char *)p, "href="))) {
        free(p);
        return 0;
    }
    if (!looks_text(p, sz)) { free(p); return 0; }
    char *s = (char *)p;
    /* href="..." */
    for (char *h = s; (h = strstr(h, "href=")); h += 5) {
        char q = h[5];
        if (q != '"' && q != '\'') continue;
        char *a = h + 6;
        char *e = strchr(a, q);
        if (!e) break;
        char rel[256], abs[256];
        int ln = (int)(e - a);
        if (ln < 1 || ln > 240) continue;
        memcpy(rel, a, (size_t)ln);
        rel[ln] = 0;
        if (!looks_book_href(rel) && strncmp(rel, "http", 4)) continue;
        url_join(abs, sizeof abs, base_url, rel);
        char title[TL];
        title_from_name(title, TL, rel);
        add_cat(dst, maxn, &n, title, abs);
    }
    /* title | url   or bare url lines */
    char *line = s;
    while (*line && n < maxn) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        char buf[400];
        snprintf(buf, sizeof buf, "%s", line);
        trim(buf);
        if (buf[0] && buf[0] != '<' && buf[0] != '#') {
            char *bar = strchr(buf, '|');
            if (bar) {
                *bar = 0;
                trim(buf);
                trim(bar + 1);
                char abs[256];
                url_join(abs, sizeof abs, base_url, bar + 1);
                add_cat(dst, maxn, &n, buf, abs);
            } else if (!strncmp(buf, "http://", 7) || !strncmp(buf, "https://", 8)) {
                add_cat(dst, maxn, &n, 0, buf);
            }
        }
        if (!nl) break;
        line = nl + 1;
    }
    free(p);
    return n;
}
