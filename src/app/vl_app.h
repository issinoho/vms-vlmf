/* vl_app.h - VLMF's state and behaviour. Portable: it reaches the OS only through
   vl_platform.h, so it runs unchanged on VMS and against the Linux stub. */
#ifndef VL_APP_H
#define VL_APP_H

#include "vl_types.h"
#include "vl_lic.h"
#include "vl_edit.h"
#include "vl_screen.h"
#include "vl_platform.h"

enum { M_LIST, M_INFO, M_HELP, M_FORM, M_CONFIRM };
enum { F_ADD, F_EDIT };
enum { P_NONE, P_FILTER, P_FILE, P_ASK };
enum { S_PRODUCT, S_PRODUCER, S_TERM, S_STATUS };
enum { OP_REGISTER = 1, OP_MODIFY, OP_DELETE, OP_ENABLE, OP_DISABLE, OP_LOAD };

#define VL_MAXCMDS 8
#define VL_QUEUE 32

typedef struct {
    vlp_session sess;
    vl_screen scr;
    int rows, cols;
    int mode, back_mode;
    int gold, quit;

    char db_spec[256];        /* as given (default LMF$LICENSE) */
    vl_db db;
    unsigned int load_status;
    int history;              /* show history (extinct) records too */
    int *vis; int nvis;       /* records shown, in order: indexes into db.l */
    int cur, top;
    char filter[64];
    int sort;
    long today;               /* days since 1970 */

    char msg[256]; int msg_err;

    /* Info, Help and Confirm text */
    char **text; int ntext, ttop;
    char text_title[128];

    /* the Add and Edit forms */
    int form_kind, fcur;
    vl_pak pak;
    vl_mods mods;
    vl_lic form_lic;          /* the license being edited */
    vl_edit fedit;            /* the field being typed in */
    char form_err[200];
    vl_pak queue[VL_QUEUE];   /* PAKs read from a file, still to be offered */
    int nqueue, qpos;

    /* the prompt line */
    int prompt;
    char prompt_label[128];
    vl_edit pedit;

    /* the operation waiting for Y */
    int op;
    vl_lic op_lic;
    char cmds[VL_MAXCMDS][VL_CMD];
    int ncmds;
    int backed_up;
    char backup[256];
} vl_app;

int  vl_app_init(vl_app *a, const char *db, int rows, int cols);
void vl_app_free(vl_app *a);
void vl_app_draw(vl_app *a);
void vl_app_key(vl_app *a, int key);
void vl_app_run(vl_app *a);

/* shared by vl_app.c and vl_draw.c */
vl_lic *vl_cur(vl_app *a);
const char *vl_status_text(vl_app *a, const vl_lic *l);
void vl_set_msg(vl_app *a, int err, const char *fmt, ...);
const vl_field *vl_form_fields(vl_app *a, int *n);
char *vl_form_value(vl_app *a, int i);

#endif
