#ifndef CHAPTERS_H
#define CHAPTERS_H

#include <stdint.h>

typedef struct {
    const char *title;
    const char *brief;
} Chapter;

typedef struct {
    const char *short_name;
    const char *full_title;
    const char *authors;
    const char *imprint;
    const char *toc_note;
    uint32_t accent;
    uint32_t accent2;
    const Chapter *ch;
    int n;
    int toc_n;
} Book;

enum { NBOOKS = 3, BOOK_SPIRIT = 0, BOOK_SUBTLE = 1, BOOK_POWER = 2 };
extern const Book BOOKS[NBOOKS];

#endif
