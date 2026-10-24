/* vl_lic.h - licenses as LMF shows them, and the LICENSE commands that change them.
   No I/O: the text comes from LICENSE LIST/FULL and SHOW LICENSE (run by the
   platform), and the commands go out as a DCL procedure text (D3, D4). */
#ifndef VL_LIC_H
#define VL_LIC_H

#include <stddef.h>

#define VL_F    64           /* one PAK field */
#define VL_LST  1024         /* an include, exclude or reserve list */
#define VL_CMD  2048         /* one LICENSE command, before it is split into lines */

/* One record of LICENSE LIST/FULL. Every field is the text LMF showed, trimmed;
   "(none)" is kept as LMF wrote it except where noted. */
typedef struct {
    char product[VL_F], producer[VL_F], issuer[VL_F], auth[VL_F];
    char units[16], mod_units[16];      /* mod_units: "Modified Units", "" if none */
    char version[16], release[24];
    char term[24], mod_term[24];        /* PAK and "Modified Termination Date" */
    char avail[32], activ[32];
    char options[160], hwid[VL_F], token[VL_F];
    char weight[40];                    /* "50 (Modified Weight)", "" if not shown */
    char revision[8], status[16], command[16], comment[80];
    char mod_user[16], mod_on[32];
    char include[VL_LST], exclude[VL_LST], reserve[VL_LST];
    char other[512];                    /* lines this version doesn't know, '\n'-separated */
    int loaded;                         /* SHOW LICENSE lists product and producer */
} vl_lic;

typedef struct {
    vl_lic *l;
    int n, cap;
    char file[256];                     /* "License Database File:" */
    char created[32], created_by[32], lmf[16];
} vl_db;

void vl_db_init(vl_db *db);
void vl_db_free(vl_db *db);
/* Parse LICENSE LIST/FULL output into db (appending). Returns the number of
   records read, -1 when out of memory. */
int  vl_parse_list(const char *text, vl_db *db);
/* Mark the records SHOW LICENSE lists as loaded. Returns how many products it listed. */
int  vl_parse_show(const char *text, vl_db *db);
/* The first %FACILITY-L-IDENT line of LICENSE or DCL output, or "". */
void vl_first_message(const char *text, char *buf, size_t size);

int  vl_lic_extinct(const vl_lic *l);   /* a history record */
int  vl_lic_active(const vl_lic *l);    /* status Active */
/* The termination date in force (modified, else the PAK's) as days since 1970,
   or -1 when there is none. */
long vl_lic_term_days(const vl_lic *l);
const char *vl_lic_term(const vl_lic *l);   /* its text, "" when none */

/* ---------- what is typed in: a PAK, and the changes LICENSE MODIFY can make ---------- */

typedef struct {
    char product[VL_F], issuer[VL_F], auth[VL_F], producer[VL_F];
    char units[16], version[16], release[24], term[24];
    char avail[32], activ[32], options[160], token[VL_F], hwid[VL_F], checksum[40];
} vl_pak;

enum { VL_K_NAME, VL_K_INT, VL_K_VERSION, VL_K_DATE, VL_K_UNITCODE, VL_K_OPTIONS,
       VL_K_TEXT, VL_K_CHECKSUM, VL_K_NODES, VL_K_COMMENT, VL_K_WEIGHT };

typedef struct {
    const char *label;        /* as LIST/FULL names it */
    const char *qual;         /* REGISTER qualifier, 0 for the product parameter */
    size_t off, size;         /* where it is in vl_pak */
    int kind, required;
    const char *hint;
} vl_field;

extern const vl_field vl_pak_fields[];
extern const int vl_pak_nfields;

/* What the Edit form changes. Lists are comma-separated names. */
typedef struct {
    char units[16], term[24], comment[80], weight[16];
    char include[VL_LST], exclude[VL_LST], reserve[VL_LST];
} vl_mods;

extern const vl_field vl_mod_fields[];
extern const int vl_mod_nfields;

void vl_mods_from(const vl_lic *l, vl_mods *m);

/* Check one value of the given kind; empty passes unless required. 1 = good,
   else 0 with the reason in err. */
int vl_check(int kind, int required, const char *value, char *err, size_t errsz);

/* LICENSE commands, without the word LICENSE and without /DATABASE. Each returns
   the number of commands written to cmds (0: nothing to do), or -1 with err. */
int vl_build_register(const vl_pak *p, char cmds[][VL_CMD], int max, char *err, size_t errsz);
int vl_build_modify(const vl_lic *l, const vl_mods *m, char cmds[][VL_CMD], int max,
                    char *err, size_t errsz);
/* verb: DELETE, ENABLE, DISABLE or LOAD, for exactly this license. */
int vl_build_simple(const char *verb, const vl_lic *l, char cmds[][VL_CMD], int max,
                    char *err, size_t errsz);

/* A database file specification VLMF will write into a procedure, unquoted (DCL
   would keep quotes as part of a file name): no quotes, apostrophes, blanks,
   slashes, exclamation marks or control characters. */
int vl_check_db(const char *spec);

/* The DCL procedures the platform runs in a subprocess (D4).
   List: LIST/FULL (with /HISTORY), then a marker line, then SHOW LICENSE.
   Change: an optional backup COPY of the database first, then each command,
   stopping at the first failure; the exit status is the failing one. Lines are
   split with "-" continuations so that none is longer than 200 characters.
   Return 0 when out does not hold it. */
#define VL_SHOW_MARK "==== VLMF SHOW LICENSE"
int vl_proc_list(const char *db, int history, char *out, size_t size);
int vl_proc_change(const char *db, int backup, char cmds[][VL_CMD], int n,
                   char *out, size_t size);

/* ---------- reading commands back: PAK files, and the Linux stub ---------- */

#define VL_MAXQ 32
typedef struct {
    char verb[16];            /* "REGISTER" */
    char param[VL_F * 2];     /* product name(s) */
    int nq;
    struct { char name[24]; char value[VL_LST + 64]; int neg; } q[VL_MAXQ];
} vl_cmd;

/* Parse "LICENSE REGISTER BASIC/ISSUER=VSI ..." (one joined command, "$" optional,
   the LICENSE verb needed, abbreviated to 4 letters or more). Unquoted text is upper-cased, quotes are removed and
   "" undoubled; a parenthesised value is kept with its parentheses. Qualifier
   names are given in full (abbreviations of 4 or more letters accepted).
   Returns 1, or 0 if it is not a LICENSE command it understands. */
int vl_cmd_parse(const char *line, vl_cmd *c);
const char *vl_cmd_value(const vl_cmd *c, const char *name);   /* 0 if absent */

/* Join a DCL procedure's continuation lines; calls fn for each command line
   (the "$" removed, comments dropped). */
typedef void (*vl_line_fn)(void *ctx, const char *line);
void vl_dcl_lines(const char *text, vl_line_fn fn, void *ctx);

/* PAKs in a text: LICENSE REGISTER commands (a LICENSE ISSUE/PROCEDURE file or
   a VMSLICENSE-style procedure) or PAK listings ("Issuer:  VSI" lines, as VSI
   sends them and LICENSE ISSUE prints them). Returns how many were found (up to max). */
int vl_pak_import(const char *text, vl_pak *out, int max);

#endif
