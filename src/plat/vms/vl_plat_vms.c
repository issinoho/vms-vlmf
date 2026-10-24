/* vl_plat_vms.c - vl_platform.h on OpenVMS (x86-64 and IA64).
   SMG$ owns the keyboard and traps broadcasts; the screen is written through a
   channel to TT: by vl_ansi so that it can be in colour (as VTREE, its D11).
   LICENSE runs in a subprocess from a procedure file in SYS$SCRATCH (D4). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <rms.h>
#include <starlet.h>
#include <descrip.h>
#include <iodef.h>
#include <ssdef.h>
#include <prvdef.h>
#include <jpidef.h>
#include <syidef.h>
#include <clidef.h>
#include <smgdef.h>
#include <smgmsg.h>
#include <smg$routines.h>
#include <lib$routines.h>
#include "vl_platform.h"
#include "vl_util.h"
#include "vl_ansi.h"

typedef struct { unsigned short buflen, code; void *buf; unsigned short *retlen; } ile3;

static void ile(ile3 *it, unsigned short code, void *buf, unsigned short len, unsigned short *rl)
{
    it->buflen = len; it->code = code; it->buf = buf; it->retlen = rl;
}

static void dsc(struct dsc$descriptor_s *d, const char *s, unsigned short len)
{
    d->dsc$w_length = len;
    d->dsc$b_dtype = DSC$K_DTYPE_T;
    d->dsc$b_class = DSC$K_CLASS_S;
    d->dsc$a_pointer = (char *) s;
}

static void rtrim(char *s)
{
    size_t n = strlen(s);
    while (n && s[n - 1] == ' ') s[--n] = '\0';
}

/* ---------- session and terminal ---------- */

static unsigned int pb, kb;
static unsigned short ttchan;
static int scr_rows, scr_cols, started, colour = 1;
static vl_cell *shown;
static int shown_rows, shown_cols;

static void tt_write(void *ctx, const char *bytes, int len)
{
    unsigned short iosb[4];
    (void) ctx;
    while (len > 0) {
        int n = len > 2048 ? 2048 : len;
        sys$qiow(0, ttchan, IO$_WRITEVBLK | IO$M_NOFORMAT, (void *) iosb, 0, 0,
                 (void *) bytes, n, 0, 0, 0, 0);
        bytes += n; len -= n;
    }
}

static struct { unsigned int link; void (*handler)(void); unsigned int argc; unsigned int *status;
                unsigned int st; } exit_block;

static void on_exit(void) { vlp_stop(); }

unsigned int vlp_start(vlp_session *s)
{
    union prvdef cur;
    unsigned int st, one = 1;
    char user[13] = "", node[16] = "";
    unsigned short ul = 0, nl = 0, pl = 0;
    ile3 it[3];
    int rows, cols;
    struct dsc$descriptor_s ttd;
    const char *mode;

    memset(s, 0, sizeof *s);
    memset(&cur, 0, sizeof cur);
    ile(&it[0], JPI$_USERNAME, user, 12, &ul);
    ile(&it[1], JPI$_CURPRIV, &cur, sizeof cur, &pl);
    memset(&it[2], 0, sizeof it[2]);
    sys$getjpiw(0, 0, 0, it, 0, 0, 0);
    user[ul] = '\0'; rtrim(user);
    vl_strlcpy(s->user, user, sizeof s->user);
    if (cur.prv$v_sysprv) vl_strlcat(s->privs, "SYSPRV ", sizeof s->privs);
    if (cur.prv$v_cmkrnl) vl_strlcat(s->privs, "CMKRNL ", sizeof s->privs);
    if (cur.prv$v_sysnam) vl_strlcat(s->privs, "SYSNAM ", sizeof s->privs);
    rtrim(s->privs);
    ile(&it[0], SYI$_NODENAME, node, 15, &nl);
    memset(&it[1], 0, sizeof it[1]);
    sys$getsyiw(0, 0, 0, it, 0, 0, 0);
    node[nl] = '\0';
    vl_strlcpy(s->node, node, sizeof s->node);

    dsc(&ttd, "TT:", 3);
    st = sys$assign(&ttd, &ttchan, 0, 0);
    if (!(st & 1)) return st;
    st = smg$create_pasteboard(&pb, 0, &rows, &cols, 0, 0, 0);
    if (!(st & 1)) { sys$dassgn(ttchan); return st; }
    st = smg$create_virtual_keyboard(&kb, 0, 0, 0, 0);
    if (!(st & 1)) { smg$delete_pasteboard(&pb, &one); sys$dassgn(ttchan); return st; }
    smg$set_broadcast_trapping(&pb, 0, 0);
    mode = getenv("VLMF$DISPLAY");                 /* MONO for a terminal without colour */
    colour = !(mode && (mode[0] == 'M' || mode[0] == 'm'));
    scr_rows = rows; scr_cols = cols;
    started = 1;
    vl_ansi_begin(colour, tt_write, 0);

    exit_block.handler = on_exit;
    exit_block.argc = 1;
    exit_block.status = &exit_block.st;
    sys$dclexh((void *) &exit_block);
    return SS$_NORMAL;
}

void vlp_stop(void)
{
    unsigned int one = 1;
    if (!started) return;
    started = 0;
    vl_ansi_end(tt_write, 0);
    smg$disable_broadcast_trapping(&pb);
    smg$delete_pasteboard(&pb, &one);
    smg$delete_virtual_keyboard(&kb);
    sys$dassgn(ttchan);
    free(shown); shown = 0;
}

void vlp_screen_size(int *rows, int *cols) { *rows = scr_rows; *cols = scr_cols; }

void vlp_draw(const vl_screen *s)
{
    if (!started) return;
    if (!shown || shown_rows != s->rows || shown_cols != s->cols) {
        free(shown);
        shown = (vl_cell *) malloc((size_t) s->rows * (size_t) s->cols * sizeof *shown);
        shown_rows = s->rows; shown_cols = s->cols;
        vl_ansi_render(s, 0, colour, tt_write, 0);
        if (shown) memcpy(shown, s->cell, (size_t) s->rows * (size_t) s->cols * sizeof *shown);
        return;
    }
    vl_ansi_render(s, shown, colour, tt_write, 0);
}

void vlp_repaint(void) { free(shown); shown = 0; }

int vlp_key(int timeout)
{
    unsigned short term = 0;
    unsigned int st;
    if (!started) return VK_TIMEOUT;
    st = smg$read_keystroke(&kb, &term, 0, &timeout, 0, 0, 0);
    if (st == SS$_TIMEOUT) return VK_TIMEOUT;
    if (st == SMG$_EOF) return VK_CTRL_Z;
    if (!(st & 1)) return VK_TIMEOUT;
    switch (term) {
    case SMG$K_TRM_UP: return VK_UP;
    case SMG$K_TRM_DOWN: return VK_DOWN;
    case SMG$K_TRM_LEFT: return VK_LEFT;
    case SMG$K_TRM_RIGHT: return VK_RIGHT;
    case SMG$K_TRM_PF1: return VK_PF1;
    case SMG$K_TRM_PF2: return VK_PF2;
    case SMG$K_TRM_PF3: return VK_PF3;
    case SMG$K_TRM_PF4: return VK_PF4;
    case SMG$K_TRM_FIND: return VK_FIND;
    case SMG$K_TRM_INSERT_HERE: return VK_INSERT;
    case SMG$K_TRM_REMOVE: return VK_REMOVE;
    case SMG$K_TRM_SELECT: return VK_SELECT;
    case SMG$K_TRM_PREV_SCREEN: return VK_PREV;
    case SMG$K_TRM_NEXT_SCREEN: return VK_NEXT;
    case SMG$K_TRM_HELP: return VK_HELP;
    case SMG$K_TRM_DO: return VK_DO;
    case SMG$K_TRM_F10: return VK_F10;
    case SMG$K_TRM_ENTER: return VK_ENTER;
    case SMG$K_TRM_MINUS: return VK_KP_MINUS;
    case SMG$K_TRM_COMMA: return VK_KP_COMMA;
    case SMG$K_TRM_PERIOD: return VK_KP_PERIOD;
    default: break;
    }
    if (term >= SMG$K_TRM_KP0 && term <= SMG$K_TRM_KP9) return VK_KP0 + (term - SMG$K_TRM_KP0);
    if (term < 256) return term;
    return VK_OTHER;
}

int vlp_broadcast(char *buf, int size)
{
    struct dsc$descriptor_s d;
    unsigned short len = 0;
    unsigned int st;
    int i;
    if (!started) return 0;
    dsc(&d, buf, (unsigned short) (size - 1));
    st = smg$get_broadcast_message(&pb, &d, &len, 0);
    if (st != SS$_NORMAL || len == 0) return 0;
    buf[len] = '\0';
    for (i = 0; i < len; i++) if ((unsigned char) buf[i] < 32) buf[i] = ' ';
    return 1;
}

void vlp_now(vl_when *w)
{
    time_t t = time(0);
    struct tm *tm = localtime(&t);
    w->y = tm->tm_year + 1900; w->m = tm->tm_mon + 1; w->d = tm->tm_mday;
    w->hh = tm->tm_hour; w->mm = tm->tm_min;
}

void vlp_message(unsigned int st, char *buf, int size)
{
    struct dsc$descriptor_s d;
    unsigned short len = 0;
    dsc(&d, buf, (unsigned short) (size - 1));
    if (!(sys$getmsg(st, &len, &d, 15, 0) & 1)) { snprintf(buf, (size_t) size, "%%X%08X", st); return; }
    buf[len] = '\0';
}

/* ---------- files ---------- */

static unsigned int slurp(const char *spec, char **out, long limit)
{
    FILE *f = fopen(spec, "r");
    char *buf;
    long n = 0, cap = 65536;
    int c;
    *out = 0;
    if (!f) return SS$_NOSUCHFILE;
    if (!(buf = (char *) malloc((size_t) cap))) { fclose(f); return SS$_INSFMEM; }
    while ((c = getc(f)) != EOF) {
        if (n + 2 > cap) {
            char *b;
            if (cap >= limit) break;
            cap *= 2;
            if (!(b = (char *) realloc(buf, (size_t) cap))) { free(buf); fclose(f); return SS$_INSFMEM; }
            buf = b;
        }
        buf[n++] = (char) c;
    }
    buf[n] = '\0';
    fclose(f);
    *out = buf;
    return SS$_NORMAL;
}

unsigned int vlp_full_spec(const char *spec, char *out, int size)
{
    struct FAB fab = cc$rms_fab;
    struct NAM nam = cc$rms_nam;
    char esa[NAM$C_MAXRSS + 1];
    unsigned int st;
    static char dflt[] = "SYS$DISK:[].LDB";
    fab.fab$l_fna = (char *) spec;
    fab.fab$b_fns = (unsigned char) strlen(spec);
    fab.fab$l_dna = dflt;
    fab.fab$b_dns = (unsigned char) strlen(dflt);
    fab.fab$l_nam = &nam;
    nam.nam$l_esa = esa;
    nam.nam$b_ess = NAM$C_MAXRSS;
    st = sys$parse(&fab);
    if (!(st & 1)) return st;
    esa[nam.nam$b_esl] = '\0';
    if (nam.nam$l_ver && nam.nam$l_ver >= esa && nam.nam$l_ver < esa + nam.nam$b_esl) *nam.nam$l_ver = '\0';
    vl_strlcpy(out, esa, (size_t) size);
    return SS$_NORMAL;
}

unsigned int vlp_read_file(const char *spec, char **out) { return slurp(spec, out, 1L << 20); }

unsigned int vlp_dcl(const char *procedure, char **out)
{
    char com[64], log[64], cmd[80];
    struct dsc$descriptor_s dc, di, dout;
    unsigned int st, status = 0, flags = CLI$M_NOCLISYM | CLI$M_NOKEYPAD;
    FILE *f;
    *out = 0;
    sprintf(com, "SYS$SCRATCH:VLMF_%X.COM", (unsigned int) getpid());
    sprintf(log, "SYS$SCRATCH:VLMF_%X.LOG", (unsigned int) getpid());
    f = fopen(com, "w");
    if (!f) return SS$_ABORT;
    fputs(procedure, f);
    if (fclose(f)) { remove(com); return SS$_ABORT; }
    /* NOCLISYM: a user's own LICENSE or COPY symbol must not run instead. */
    sprintf(cmd, "@%s", com);
    dsc(&dc, cmd, (unsigned short) strlen(cmd));
    dsc(&di, "NL:", 3);
    dsc(&dout, log, (unsigned short) strlen(log));
    st = lib$spawn(&dc, &di, &dout, &flags, 0, 0, &status, 0, 0, 0, 0, 0, 0);
    while (remove(com) == 0) ;
    if (!(st & 1)) return st;
    slurp(log, out, 4L << 20);
    while (remove(log) == 0) ;
    return status;
}

void vlp_log(const char *line)
{
    const char *spec = getenv("VLMF$LOG");
    FILE *f = fopen(spec && *spec ? spec : "SYS$LOGIN:VLMF.LOG", "a");
    vl_when w;
    char t[32];
    if (!f) return;
    vlp_now(&w);
    fprintf(f, "%s %s\n", vl_fmt_when(&w, t, sizeof t), line);
    fclose(f);
}
