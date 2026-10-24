/* vl_plat_stub.c - vl_platform.h for the Linux tests: a fixed clock, no terminal,
   and a small LICENSE that runs VLMF's procedures against licenses in memory,
   answering as LMF V2.0 did on the nodes (NOTES N1-N4). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vl_platform.h"
#include "vl_util.h"
#include "vl_stub.h"

#define MAXL 64
static vl_lic lic[MAXL];
static int nlic;
static int rows_ = 24, cols_ = 80;
static char logbuf[16384], lastproc[32768];
static struct { char name[128]; char *text; } files[8];
static int nfiles, fail_backup;

static void put_lic(const char *product, const char *auth, const char *units, const char *term,
                    const char *options, const char *status, int loaded)
{
    vl_lic *l = &lic[nlic++];
    memset(l, 0, sizeof *l);
    vl_strlcpy(l->product, product, sizeof l->product);
    vl_strlcpy(l->producer, "VSI", sizeof l->producer);
    vl_strlcpy(l->issuer, "VSI", sizeof l->issuer);
    vl_strlcpy(l->auth, auth, sizeof l->auth);
    vl_strlcpy(l->units, units, sizeof l->units);
    vl_strlcpy(l->version, "0.0", sizeof l->version);
    vl_strlcpy(l->release, "(none)", sizeof l->release);
    vl_strlcpy(l->term, term, sizeof l->term);
    vl_strlcpy(l->options, options, sizeof l->options);
    vl_strlcpy(l->revision, "1", sizeof l->revision);
    vl_strlcpy(l->status, status, sizeof l->status);
    vl_strlcpy(l->command, "REGISTER", sizeof l->command);
    vl_strlcpy(l->mod_user, "SYSTEM", sizeof l->mod_user);
    vl_strlcpy(l->mod_on, "20-APR-2023 13:05:09.16", sizeof l->mod_on);
    l->loaded = loaded;
}

void stub_reset(void)
{
    int i;
    nlic = 0;
    put_lic("BASIC", "TEST-AUTH-0001", "0", "30-APR-2027", "X86_64", "Active", 1);
    put_lic("C", "TEST-AUTH-0002", "0", "(none)", "X86_64", "Active", 1);
    put_lic("FORTRAN", "TEST-AUTH-0003", "100", "1-NOV-2026", "X86_64, MOD_UNITS", "Active", 0);
    put_lic("VMS-WEBUI", "TEST-AUTH-0004", "0", "1-JAN-2026", "X86_64", "Disabled", 0);
    vl_strlcpy(lic[0].comment, "Site licence", sizeof lic[0].comment);
    logbuf[0] = lastproc[0] = '\0';
    for (i = 0; i < nfiles; i++) free(files[i].text);
    nfiles = 0;
    fail_backup = 0;
}

void stub_set_size(int rows, int cols) { rows_ = rows; cols_ = cols; }

void stub_add_file(const char *name, const char *text)
{
    if (nfiles >= 8) return;
    vl_strlcpy(files[nfiles].name, name, sizeof files[0].name);
    files[nfiles].text = (char *) malloc(strlen(text) + 1);
    strcpy(files[nfiles].text, text);
    nfiles++;
}

const char *stub_log(void) { return logbuf; }
const char *stub_last_proc(void) { return lastproc; }
int stub_count(void) { return nlic; }
void stub_fail_backup(int on) { fail_backup = on; }

const vl_lic *stub_find(const char *product, const char *auth)
{
    int i;
    for (i = 0; i < nlic; i++)
        if (!strcmp(lic[i].product, product) && !strcmp(lic[i].auth, auth) && strcmp(lic[i].status, "Extinct"))
            return &lic[i];
    return 0;
}

/* ---------- terminal ---------- */

unsigned int vlp_start(vlp_session *s)
{
    memset(s, 0, sizeof *s);
    strcpy(s->user, "IAIN"); strcpy(s->node, "X86VMS"); strcpy(s->privs, "SYSPRV CMKRNL SYSNAM");
    return 1;
}
void vlp_stop(void) { }
void vlp_screen_size(int *r, int *c) { *r = rows_; *c = cols_; }
void vlp_draw(const vl_screen *s) { (void) s; }
void vlp_repaint(void) { }
int vlp_key(int timeout) { (void) timeout; return VK_TIMEOUT; }
int vlp_broadcast(char *buf, int size) { (void) buf; (void) size; return 0; }
void vlp_now(vl_when *w) { w->y = 2026; w->m = 10; w->d = 24; w->hh = 15; w->mm = 2; }

void vlp_message(unsigned int st, char *buf, int size)
{
    if (st == 1) vl_strlcpy(buf, "%SYSTEM-S-NORMAL, normal successful completion", (size_t) size);
    else if (st == 0x910) vl_strlcpy(buf, "%RMS-E-FNF, file not found", (size_t) size);
    else snprintf(buf, (size_t) size, "%%X%08X", st);
}

unsigned int vlp_full_spec(const char *spec, char *out, int size)
{
    if (!vl_strcasecmp(spec, "LMF$LICENSE")) spec = "STUB$DKA0:[SYSEXE]LMF$LICENSE.LDB";
    if (!strchr(spec, ':') && !strchr(spec, '[')) return 0x910;
    vl_strlcpy(out, spec, (size_t) size);
    return 1;
}

unsigned int vlp_read_file(const char *spec, char **out)
{
    int i;
    *out = 0;
    for (i = 0; i < nfiles; i++)
        if (!vl_strcasecmp(files[i].name, spec)) {
            *out = (char *) malloc(strlen(files[i].text) + 1);
            strcpy(*out, files[i].text);
            return 1;
        }
    return 0x910;
}

void vlp_log(const char *line)
{
    vl_strlcat(logbuf, line, sizeof logbuf);
    vl_strlcat(logbuf, "\n", sizeof logbuf);
}

/* ---------- a small LICENSE ---------- */

typedef struct { char *buf; size_t n, cap; } outbuf;

static void out_add(outbuf *o, const char *s)
{
    size_t k = strlen(s);
    if (o->n + k + 1 > o->cap) {
        o->cap = (o->n + k + 1) * 2;
        o->buf = (char *) realloc(o->buf, o->cap);
    }
    memcpy(o->buf + o->n, s, k + 1);
    o->n += k;
}

static void out_kv(outbuf *o, const char *key, const char *val)
{
    char line[VL_LST + 64];
    snprintf(line, sizeof line, "%-29s%s\n", key, val);
    out_add(o, line);
}

static void list_full(outbuf *o, int history)
{
    int i;
    out_add(o, "License Management Facility  V2.0\n  \n");
    out_add(o, "License Database File:       STUB$DKA0:[SYSEXE]LMF$LICENSE.LDB;1\n");
    out_add(o, "Created on:                  20-APR-2023\nCreated by user:             SYSTEM\n");
    out_add(o, "Created by LMF Version:      V2.0\n  \n");
    for (i = 0; i < nlic; i++) {
        const vl_lic *l = &lic[i];
        if (!history && !strcmp(l->status, "Extinct")) continue;
        out_add(o, "-----------------------------------\n");
        out_kv(o, "Issuer:", l->issuer);
        out_kv(o, "Authorization:", l->auth);
        out_kv(o, "Product Name:", l->product);
        out_kv(o, "Producer:", l->producer);
        out_kv(o, "Units:", l->units);
        if (l->mod_units[0]) out_kv(o, "Modified Units:", l->mod_units);
        out_kv(o, "Version:", l->version);
        out_kv(o, "Release Date:", l->release);
        out_kv(o, "PAK Termination Date:", l->term);
        if (l->mod_term[0]) out_kv(o, "Modified Termination Date:", l->mod_term);
        out_kv(o, "Options:", l->options);
        out_add(o, "Hardware ID:\n  \n");
        out_kv(o, "Revision Level:", l->revision);
        out_kv(o, "Status:", l->status);
        out_kv(o, "Command:", l->command);
        if (l->comment[0]) out_kv(o, "Comment:", l->comment);
        out_kv(o, "Modified by user:", l->mod_user);
        out_kv(o, "Modified on:", l->mod_on);
        if (l->include[0]) out_kv(o, "Include:", l->include);
        if (l->exclude[0]) out_kv(o, "Exclude:", l->exclude);
        if (l->reserve[0]) out_kv(o, "Reserve:", l->reserve);
    }
    out_add(o, "[End Of List]\n");
}

static void show_license(outbuf *o)
{
    int i;
    char line[256];
    out_add(o, "\nActive licenses on node X86VMS:\n\n");
    out_add(o, "------- Product ID --------    ---- Rating ----- -- Version --\n");
    out_add(o, "Product            Producer    Units PCL   Activ Version Release    Termination\n");
    for (i = 0; i < nlic; i++)
        if (lic[i].loaded && strcmp(lic[i].status, "Extinct")) {
            snprintf(line, sizeof line, "%-18s %-11s %5s  0     1      0.0  (none)      %s\n",
                     lic[i].product, lic[i].producer, lic[i].units, lic[i].term);
            out_add(o, line);
        }
}

#define ST_BADCHK 0x1A9492E4u
#define ST_AMBIG  0x1A9492D8u
#define ST_RNF    0x1A9492D0u
#define ST_BADDATE 0x1A949390u
#define ST_EXISTS 0x1A9492C2u

static int find(const vl_cmd *c, outbuf *o)
{
    const char *iss = vl_cmd_value(c, "ISSUER"), *pro = vl_cmd_value(c, "PRODUCER");
    const char *au = vl_cmd_value(c, "AUTHORIZATION");
    int i, hit = -1, n = 0;
    char m[320];
    for (i = 0; i < nlic; i++) {
        const vl_lic *l = &lic[i];
        if (!strcmp(l->status, "Extinct") || vl_strcasecmp(l->product, c->param)) continue;
        if ((iss && vl_strcasecmp(iss, l->issuer)) || (pro && vl_strcasecmp(pro, l->producer))
            || (au && vl_strcasecmp(au, l->auth))) continue;
        hit = i; n++;
    }
    if (n == 1) return hit;
    if (n == 0) snprintf(m, sizeof m, "%%LICENSE-W-RNF, the specified license was not found for %s\n", c->param);
    else snprintf(m, sizeof m, "%%LICENSE-W-AMBIG, information provided was ambiguous; multiple licenses were found for %s\n", c->param);
    out_add(o, m);
    return n ? -2 : -1;
}

/* names in "A,B" form, parens off */
static void list_edit(char *list, size_t size, const char *value, int add, int remove)
{
    char v[VL_LST + 8], *p, *tok, result[VL_LST];
    vl_strlcpy(v, value, sizeof v);
    p = v;
    if (*p == '(') { p++; if (strchr(p, ')')) *strchr(p, ')') = '\0'; }
    if (!add && !remove) { vl_strlcpy(list, p, size); return; }
    result[0] = '\0';
    if (remove) {
        char old[VL_LST];
        vl_strlcpy(old, list, sizeof old);
        for (tok = strtok(old, ","); tok; tok = strtok(0, ",")) {
            char want[VL_LST + 2], t2[40];
            snprintf(want, sizeof want, ",%.1000s,", p);
            snprintf(t2, sizeof t2, ",%s,", tok);
            if (strstr(want, t2)) continue;
            if (result[0]) vl_strlcat(result, ",", sizeof result);
            vl_strlcat(result, tok, sizeof result);
        }
        vl_strlcpy(list, result, size);
        return;
    }
    if (list[0]) vl_strlcat(list, ",", size);
    vl_strlcat(list, p, size);
}

static unsigned int license(const vl_cmd *c, outbuf *o)
{
    char m[256];
    int i;
    if (!strcmp(c->verb, "LIST")) {
        int history = 0;
        for (i = 0; i < c->nq; i++) if (!strcmp(c->q[i].name, "HISTORY")) history = 1;
        list_full(o, history);
        return 1;
    }
    if (!strcmp(c->verb, "REGISTER")) {
        const char *ck = vl_cmd_value(c, "CHECKSUM");
        vl_lic *l;
        if (!ck || strncmp(ck, "2-", 2)) {
            snprintf(m, sizeof m, "%%LICENSE-F-BADCHK, checksum does not validate for %s\n", c->param);
            out_add(o, m);
            return ST_BADCHK;
        }
        if (nlic >= MAXL) return 0x2C;
        l = &lic[nlic];
        memset(l, 0, sizeof *l);
        vl_strlcpy(l->product, c->param, sizeof l->product);
#define Q(name, field, dflt) do { const char *v = vl_cmd_value(c, name); \
        vl_strlcpy(l->field, v ? v : dflt, sizeof l->field); } while (0)
        Q("ISSUER", issuer, ""); Q("PRODUCER", producer, ""); Q("AUTHORIZATION", auth, "");
        Q("UNITS", units, "0"); Q("VERSION", version, "0.0"); Q("RELEASE_DATE", release, "(none)");
        Q("TERMINATION_DATE", term, "(none)"); Q("OPTIONS", options, "");
#undef Q
        if (l->options[0] == '(') {
            memmove(l->options, l->options + 1, strlen(l->options));
            if (strchr(l->options, ')')) *strchr(l->options, ')') = '\0';
        }
        if (stub_find(l->product, l->auth)) {
            out_add(o, "%LICENSE-F-EXISTS, license already exists\n");
            return ST_EXISTS;
        }
        strcpy(l->revision, "1"); strcpy(l->status, "Active"); strcpy(l->command, "REGISTER");
        strcpy(l->mod_user, "IAIN"); strcpy(l->mod_on, "24-OCT-2026 15:02:00.00");
        nlic++;
        return 1;
    }
    i = find(c, o);
    if (i == -1) return ST_RNF;
    if (i == -2) return ST_AMBIG;
    if (!strcmp(c->verb, "DELETE")) {
        char p[VL_F], a[VL_F];
        int j, k = 0;
        strcpy(p, lic[i].product); strcpy(a, lic[i].auth);
        for (j = 0; j < nlic; j++)
            if (strcmp(lic[j].product, p) || strcmp(lic[j].auth, a)) lic[k++] = lic[j];
        nlic = k;
        return 1;
    }
    if (!strcmp(c->verb, "LOAD")) { lic[i].loaded = 1; return 1; }
    if (nlic >= MAXL) return 0x2C;
    lic[nlic] = lic[i];                         /* the history record */
    strcpy(lic[nlic].status, "Extinct");
    lic[nlic].loaded = 0;
    nlic++;
    if (!strcmp(c->verb, "ENABLE")) strcpy(lic[i].status, "Active");
    else if (!strcmp(c->verb, "DISABLE")) strcpy(lic[i].status, "Disabled");
    else if (!strcmp(c->verb, "MODIFY")) {
        int add = vl_cmd_value(c, "ADD") != 0, rem = vl_cmd_value(c, "REMOVE") != 0, q;
        const char *v;
        for (q = 0; q < c->nq; q++) {
            if (!strcmp(c->q[q].name, "ADD")) add = 1;
            if (!strcmp(c->q[q].name, "REMOVE")) rem = 1;
        }
        if ((v = vl_cmd_value(c, "UNITS")) != 0)
            vl_strlcpy(lic[i].mod_units, strcmp(v, "0") ? v : "", sizeof lic[i].mod_units);
        if ((v = vl_cmd_value(c, "TERMINATION_DATE")) != 0) {
            long nd, pd;
            if (vl_parse_date(lic[i].term, &pd) && vl_parse_date(v, &nd) && nd > pd) {
                nlic--;
                snprintf(m, sizeof m, "%%LICENSE-W-BADDATE, invalid date was specified for %s\n", c->param);
                out_add(o, m);
                return ST_BADDATE;
            }
            vl_strlcpy(lic[i].mod_term, vl_strcasecmp(v, lic[i].term) ? v : "", sizeof lic[i].mod_term);
        }
        if ((v = vl_cmd_value(c, "COMMENT")) != 0) vl_strlcpy(lic[i].comment, v, sizeof lic[i].comment);
        if ((v = vl_cmd_value(c, "SELECTION_WEIGHT")) != 0) {
            if (atoi(v)) snprintf(lic[i].weight, sizeof lic[i].weight, "%d (Modified Weight)", atoi(v));
            else lic[i].weight[0] = '\0';
        }
        if ((v = vl_cmd_value(c, "INCLUDE")) != 0) {
            list_edit(lic[i].include, sizeof lic[i].include, v, add, rem);
            if (!rem) lic[i].exclude[0] = '\0';
        }
        if ((v = vl_cmd_value(c, "EXCLUDE")) != 0) {
            list_edit(lic[i].exclude, sizeof lic[i].exclude, v, add, rem);
            if (!rem) lic[i].include[0] = '\0';
        }
        if ((v = vl_cmd_value(c, "RESERVE")) != 0) list_edit(lic[i].reserve, sizeof lic[i].reserve, v, add, rem);
    }
    snprintf(lic[i].revision, sizeof lic[i].revision, "%d", (atoi(lic[i].revision) + 1) % 10000);
    vl_strlcpy(lic[i].command, c->verb, sizeof lic[i].command);
    strcpy(lic[i].mod_user, "IAIN"); strcpy(lic[i].mod_on, "24-OCT-2026 15:02:00.00");
    return 1;
}

typedef struct { outbuf *o; unsigned int st; int skipping, copied; } run_ctx;

/* The quoted text of a WRITE SYS$OUTPUT line. */
static void write_line(run_ctx *r, const char *line)
{
    const char *q = strchr(line, '"'), *e;
    char t[256];
    if (!q || !(e = strchr(q + 1, '"'))) return;
    snprintf(t, sizeof t, "%.*s", (int) (e - q - 1), q + 1);
    out_add(r->o, t);
    if (strstr(t, "VLMF BACKUP ") && !strstr(t, "FAILED")) out_add(r->o, "STUB$DKA0:[IAIN]VLMF_BACKUP.LDB;1");
    if (strstr(t, "VLMF STATUS")) { snprintf(t, sizeof t, "%u", r->st); out_add(r->o, t); }
    out_add(r->o, "\n");
}

static void run_line(void *vctx, const char *line)
{
    run_ctx *r = (run_ctx *) vctx;
    char low[VL_CMD * 2];
    vl_strlcpy(low, line, sizeof low);
    if (r->skipping) {
        if (!vl_strncasecmp(low, "done:", 5)) r->skipping = 0;
        return;
    }
    if (!vl_strncasecmp(low, "if .not. st then ", 17)) {
        if (r->st & 1) return;
        memmove(low, low + 17, strlen(low + 17) + 1);
        if (!vl_strncasecmp(low, "goto done", 9)) { r->skipping = 1; return; }
    }
    if (!vl_strncasecmp(low, "write sys$output", 16)) { write_line(r, low); return; }
    if (!vl_strncasecmp(low, "copy", 4)) {
        if (fail_backup) {
            out_add(r->o, "%COPY-E-OPENOUT, error opening SYS$LOGIN:VLMF_BACKUP.LDB as output\n");
            r->st = 0x10A38410u;
        } else r->st = 1;
        return;
    }
    if (!vl_strncasecmp(low, "st = $status", 12)) return;
    if (!vl_strncasecmp(low, "show license", 12)) { show_license(r->o); return; }
    if (!vl_strncasecmp(low, "license", 7)) {
        vl_cmd c;
        if (!vl_cmd_parse(low, &c)) {
            out_add(r->o, "%DCL-W-IVQUAL, unrecognized qualifier\n");
            r->st = 0x38240;
            return;
        }
        r->st = license(&c, r->o);
        return;
    }
}

unsigned int vlp_dcl(const char *procedure, char **out)
{
    outbuf o = { 0, 0, 0 };
    run_ctx r;
    vl_strlcpy(lastproc, procedure, sizeof lastproc);
    out_add(&o, "");
    r.o = &o; r.st = 1; r.skipping = 0; r.copied = 0;
    vl_dcl_lines(procedure, run_line, &r);
    *out = o.buf;
    return r.st;
}
