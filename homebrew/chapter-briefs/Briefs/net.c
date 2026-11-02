#include "net.h"
#include <stdbool.h>
#include <orbis/Http.h>
#include <orbis/Net.h>
#include <orbis/Ssl.h>
#include <orbis/Sysmodule.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define UA "ChapterBriefs/1.2.1 (PLAYSTATION 4)"
#define NET_POOL (64 * 1024)
#define TO_RESOLVE 8000000u
#define TO_CONNECT 8000000u
#define TO_SEND    12000000u

static int net_pool, http_ctx, ssl_ctx, ready;

static int skip_ssl(int id, unsigned int err, void * const cert[], int n, void *arg) {
    (void)id; (void)err; (void)cert; (void)n; (void)arg;
    return 1;
}

static void seterr(char *err, int n, const char *s) {
    if (err && n) snprintf(err, n, "%s", s);
}

int net_init(void) {
    if (ready) return 1;
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET) < 0) return 0;
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP) < 0) return 0;
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL) < 0) return 0;
    sceNetInit();
    net_pool = sceNetPoolCreate("briefs", NET_POOL, 0);
    if (net_pool < 0) return 0;
    ssl_ctx = sceSslInit(SSL_POOLSIZE);
    if (ssl_ctx < 0) return 0;
    http_ctx = sceHttpInit(net_pool, ssl_ctx, LIBHTTP_POOLSIZE);
    if (http_ctx < 0) return 0;
    ready = 1;
    return 1;
}

int net_get(const char *url, const char *token, const char *dest, char *err, int errn) {
    int tpl = 0, conn = 0, req = 0, ok = 0;
    if (!url || !url[0]) { seterr(err, errn, "No URL"); return 0; }
    if (!net_init()) { seterr(err, errn, "Network modules failed to load"); return 0; }
    if (strncmp(url, "http://", 7) && strncmp(url, "https://", 8)) {
        seterr(err, errn, "URL must start with http:// or https://");
        return 0;
    }
    tpl = sceHttpCreateTemplate(http_ctx, UA, ORBIS_HTTP_VERSION_1_1, 1);
    if (tpl < 0) { seterr(err, errn, "HTTP template failed"); return 0; }
    sceHttpSetResolveTimeOut(tpl, TO_RESOLVE);
    sceHttpSetConnectTimeOut(tpl, TO_CONNECT);
    sceHttpSetSendTimeOut(tpl, TO_SEND);
    sceHttpsSetSslCallback(tpl, skip_ssl, NULL);
    conn = sceHttpCreateConnectionWithURL(tpl, url, 0);
    if (conn < 0) { seterr(err, errn, "Cannot reach host (DNS, LAN, or timeout)"); goto done; }
    req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url, 0);
    if (req < 0) { seterr(err, errn, "Request create failed"); goto done; }
    if (token && token[0]) {
        char hdr[160];
        if (!strncmp(token, "Bearer ", 7) || !strncmp(token, "Token ", 6) || strchr(token, ' '))
            snprintf(hdr, sizeof hdr, "%s", token);
        else
            snprintf(hdr, sizeof hdr, "Bearer %s", token);
        sceHttpAddRequestHeader(req, "Authorization", hdr, 0);
    }
    {
        int send = sceHttpSendRequest(req, NULL, 0);
        if (send < 0) {
            if ((unsigned)send == 0x804101A3u || (unsigned)send == 0x80410100u)
                seterr(err, errn, "Timeout — check Wi-Fi, DNS blocker, or LAN address");
            else
                seterr(err, errn, "Send failed (offline, DNS blocked, or HTTPS error)");
            goto done;
        }
    }
    {
        int32_t code = 0;
        if (sceHttpGetStatusCode(req, &code) < 0) { seterr(err, errn, "No HTTP status (timeout or drop)"); goto done; }
        if (code == 401 || code == 403) { seterr(err, errn, "Auth failed — check token field"); goto done; }
        if (code == 404) { seterr(err, errn, "HTTP 404 not found"); goto done; }
        if (code == 408) { seterr(err, errn, "HTTP 408 timeout"); goto done; }
        if (code != 200) { char b[48]; snprintf(b, sizeof b, "HTTP %d", (int)code); seterr(err, errn, b); goto done; }
    }
    {
        FILE *f = fopen(dest, "wb");
        if (!f) { seterr(err, errn, "Cannot write download"); goto done; }
        uint8_t buf[32 * 1024];
        uint64_t total = 0;
        for (;;) {
            int n = sceHttpReadData(req, buf, sizeof buf);
            if (n < 0) { fclose(f); seterr(err, errn, "Read timed out or dropped"); goto done; }
            if (n == 0) break;
            if ((int)fwrite(buf, 1, (size_t)n, f) != n) { fclose(f); seterr(err, errn, "Disk write failed"); goto done; }
            total += (uint64_t)n;
            if (total > 8ull * 1024 * 1024) { fclose(f); seterr(err, errn, "File too large (8 MB cap)"); goto done; }
        }
        fclose(f);
        if (total == 0) { seterr(err, errn, "Empty response"); goto done; }
    }
    ok = 1;
    seterr(err, errn, "OK");
done:
    if (req > 0) sceHttpDeleteRequest(req);
    if (conn > 0) sceHttpDeleteConnection(conn);
    if (tpl > 0) sceHttpDeleteTemplate(tpl);
    return ok;
}
