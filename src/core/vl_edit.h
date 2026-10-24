/* vl_edit.h - the one-line editor used by every prompt (DCL-style keys). */
#ifndef VL_EDIT_H
#define VL_EDIT_H

#include "vl_types.h"

typedef struct {
    char buf[VL_SPEC_MAX];
    int len, pos;
    int overstrike;              /* Ctrl/A toggles, as in DCL */
} vl_edit;

enum { VL_EDIT_MORE, VL_EDIT_DONE, VL_EDIT_CANCEL };

void vl_edit_init(vl_edit *e, const char *initial);
/* Feed one key. Return toggles done; F10 and Ctrl/Z cancel.
   Left/right, Ctrl/H (start), Ctrl/E (end), Delete (rub out), Ctrl/U (erase to start),
   Ctrl/A (insert/overstrike). */
int vl_edit_key(vl_edit *e, int key);

#endif
