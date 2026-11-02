#ifndef COVERS_H
#define COVERS_H

#include <stdint.h>

typedef struct {
    uint32_t *px;
    int w, h;
} CoverImg;

int cov_init(void);
const CoverImg *cov_for_book(int book_i);
void cov_load_users(void);
int cov_store_user_mem(int u, const unsigned char *p, int n);
int cov_store_user_path(int u, const char *src);
void cov_delete_user(int u);

#endif
