/* vl_util.h - pure helpers: strings, dates, wildcards. */
#ifndef VL_UTIL_H
#define VL_UTIL_H

#include <stddef.h>
#include "vl_types.h"

/* Copy at most size-1 characters and always terminate. Returns strlen(src). */
size_t vl_strlcpy(char *dst, const char *src, size_t size);
size_t vl_strlcat(char *dst, const char *src, size_t size);
int vl_strcasecmp(const char *a, const char *b);
int vl_strncasecmp(const char *a, const char *b, size_t n);
/* Blanks off both ends, in place. Returns s. */
char *vl_trim(char *s);
/* Upper case, in place. Returns s. */
char *vl_upper(char *s);

/* "24-OCT-2026 15:02" */
char *vl_fmt_when(const vl_when *w, char *buf, size_t size);
/* "1-JAN-2027" or " 1-JAN-2027" (any case) -> 1; *days = days since 1970-01-01.
   Checks the day against the month. */
int vl_parse_date(const char *s, long *days);
long vl_days(int y, int m, int d);

/* VMS-style wildcard match, case-blind: * any run, % one character. */
int vl_match(const char *pattern, const char *s);

#endif
