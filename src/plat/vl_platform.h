/* vl_platform.h - the whole OS surface of VLMF (D3). No VMS types.
   src/plat/vms/ implements it with SMG$ (keys), a terminal channel (the screen),
   LIB$SPAWN (LICENSE runs in a subprocess, D4) and the C RTL for files;
   src/plat/stub/ fakes it for the Linux tests, with a small LICENSE of its own.

   Status values are VMS condition values: odd = success. */
#ifndef VL_PLATFORM_H
#define VL_PLATFORM_H

#include "vl_types.h"
#include "vl_screen.h"

typedef struct {
    char user[40];
    char node[16];
    char privs[80];       /* of SYSPRV CMKRNL SYSNAM, the ones the process has */
} vlp_session;

/* Take the terminal. On failure nothing has been changed. */
unsigned int vlp_start(vlp_session *s);
void vlp_stop(void);

/* Screen and keys */
void vlp_screen_size(int *rows, int *cols);
void vlp_draw(const vl_screen *s);         /* show the model (only what changed) */
void vlp_repaint(void);                    /* the next draw sends everything */
int  vlp_key(int timeout);                 /* VK_TIMEOUT when none came */
int  vlp_broadcast(char *buf, int size);   /* a trapped message, 0 when none */

void vlp_now(vl_when *w);
void vlp_message(unsigned int status, char *buf, int size);

/* Run a DCL procedure (text) in a subprocess with no terminal: input NL:, output
   captured. *out gets the output (malloc'd, caller frees; may be 0 on failure).
   Returns the procedure's exit status, or why it could not be run. */
unsigned int vlp_dcl(const char *procedure, char **out);

/* The full name of a database file: SYS$DISK:[] and type .LDB by default,
   logical names translated, no version. */
unsigned int vlp_full_spec(const char *spec, char *out, int size);

/* A whole text file, read-only (malloc'd). */
unsigned int vlp_read_file(const char *spec, char **out);

/* Append a line to the change log (VLMF$LOG, default SYS$LOGIN:VLMF.LOG). */
void vlp_log(const char *line);

#endif
