/* vl_stub.h - test controls for the Linux stub platform. */
#ifndef VL_STUB_H
#define VL_STUB_H

#include "vl_lic.h"

void stub_reset(void);                       /* the four sample licenses, nothing loaded but two */
void stub_set_size(int rows, int cols);
void stub_add_file(const char *name, const char *text);
const char *stub_log(void);                  /* every vlp_log line so far */
const char *stub_last_proc(void);            /* the last procedure vlp_dcl ran */
int  stub_count(void);                       /* licenses (history records too) */
const vl_lic *stub_find(const char *product, const char *auth);
void stub_fail_backup(int on);

#endif
