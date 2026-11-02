#ifndef NET_H
#define NET_H

int net_init(void);
/* GET url to dest path. token may be empty. err is a short public message — never a secret. */
int net_get(const char *url, const char *token, const char *dest, char *err, int errn);

#endif
