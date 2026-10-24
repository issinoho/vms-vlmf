/* vl_app.c - VLMF's state, reading the database, the keys, the forms and the
   operations. Every change is a LICENSE command the user has seen and agreed to
   (D5), run in a subprocess (D4), and logged. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "vl_app.h"
#include "vl_util.h"

/* ---------- messages ---------- */

void vl_set_msg(vl_app *a, int err, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(a->msg, sizeof a->msg, fmt, ap);
    va_end(ap);
    a->msg_err = err;
}

static void text_free(vl_app *a)
{
    int i;
    for (i = 0; i < a->ntext; i++) free(a->text[i]);
    free(a->text);
    a->text = 0; a->ntext = 0; a->ttop = 0;
}

static void text_add(vl_app *a, const char *fmt, ...)
{
    char line[VL_CMD + 64], **t;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    t = (char **) realloc(a->text, (size_t) (a->ntext + 1) * sizeof *t);
    if (!t) return;
    a->text = t;
    if ((a->text[a->ntext] = (char *) malloc(strlen(line) + 1)) != 0) {
        strcpy(a->text[a->ntext], line);
        a->ntext++;
    }
}

/* A long line (a command) as several screen lines, broken after '/' or ','. */
static void text_add_wrapped(vl_app *a, const char *indent, const char *s, int width)
{
    char line[256];
    int w = width - (int) strlen(indent);
    if (w < 20) w = 20;
    if (w > (int) sizeof line - 1) w = (int) sizeof line - 1;
    while ((int) strlen(s) > w) {
        int brk = 0, i;
        for (i = 1; i < w; i++) if (s[i] == '/' || s[i - 1] == ',') brk = i;
        if (!brk) brk = w;
        memcpy(line, s, (size_t) brk);
        line[brk] = '\0';
        text_add(a, "%s%s", indent, line);
        s += brk;
        indent = "      ";
        w = width - 6;
    }
    text_add(a, "%s%s", indent, s);
}

/* ---------- the database ---------- */

vl_lic *vl_cur(vl_app *a)
{
    if (a->cur < 0 || a->cur >= a->nvis) return 0;
    return &a->db.l[a->vis[a->cur]];
}

const char *vl_status_text(vl_app *a, const vl_lic *l)
{
    long t = vl_lic_term_days(l);
    if (vl_lic_active(l) && t >= 0 && t < a->today) return "Expired";
    return l->status;
}

static vl_app *sort_app;

static int cmp_lic(const void *p, const void *q)
{
    const vl_lic *x = &sort_app->db.l[*(const int *) p], *y = &sort_app->db.l[*(const int *) q];
    int c = 0;
    switch (sort_app->sort) {
    case S_PRODUCER: c = vl_strcasecmp(x->producer, y->producer); break;
    case S_TERM: {
        long tx = vl_lic_term_days(x), ty = vl_lic_term_days(y);
        if (tx < 0) tx = 1L << 30;              /* no date: last */
        if (ty < 0) ty = 1L << 30;
        c = tx < ty ? -1 : tx > ty;
        break;
    }
    case S_STATUS: c = vl_strcasecmp(vl_status_text(sort_app, x), vl_status_text(sort_app, y)); break;
    default: break;
    }
    if (!c) c = vl_strcasecmp(x->product, y->product);
    if (!c) c = vl_strcasecmp(x->producer, y->producer);
    if (!c) c = vl_strcasecmp(x->auth, y->auth);
    if (!c) c = atoi(y->revision) - atoi(x->revision);     /* newest first */
    if (!c) c = *(const int *) p - *(const int *) q;
    return c;
}

static void rebuild(vl_app *a)
{
    int i;
    free(a->vis);
    a->vis = (int *) malloc((size_t) (a->db.n + 1) * sizeof *a->vis);
    a->nvis = 0;
    if (!a->vis) return;
    for (i = 0; i < a->db.n; i++) {
        const vl_lic *l = &a->db.l[i];
        if (!a->history && vl_lic_extinct(l)) continue;
        if (a->filter[0] && !vl_match(a->filter, l->product)) continue;
        a->vis[a->nvis++] = i;
    }
    sort_app = a;
    qsort(a->vis, (size_t) a->nvis, sizeof *a->vis, cmp_lic);
    if (a->cur >= a->nvis) a->cur = a->nvis ? a->nvis - 1 : 0;
}

/* The same license stays under the cursor across a reread, if it is still there. */
static void keep_cursor(vl_app *a, const vl_lic *was)
{
    int i;
    if (!was) return;
    for (i = 0; i < a->nvis; i++) {
        const vl_lic *l = &a->db.l[a->vis[i]];
        if (!vl_strcasecmp(l->product, was->product) && !vl_strcasecmp(l->producer, was->producer)
            && !vl_strcasecmp(l->auth, was->auth) && !vl_strcasecmp(l->issuer, was->issuer)
            && !vl_lic_extinct(l) == !vl_lic_extinct(was)) {
            a->cur = i;
            return;
        }
    }
}

static unsigned int load(vl_app *a)
{
    static char proc[2048];
    char *out = 0, *mark;
    unsigned int st;
    vl_lic was, *c = vl_cur(a);
    int have = c != 0;
    if (have) was = *c;
    if (!vl_proc_list(a->db_spec, a->history, proc, sizeof proc)) {
        vl_set_msg(a, 1, "VLMF will not use this database name: %s", a->db_spec);
        return a->load_status = 0x2C;     /* SS$_ABORT */
    }
    st = vlp_dcl(proc, &out);
    vl_db_free(&a->db);
    if (out) {
        mark = strstr(out, VL_SHOW_MARK);
        if (mark) *mark = '\0';
        if (vl_parse_list(out, &a->db) < 0) vl_set_msg(a, 1, "Out of memory");
        if (mark) vl_parse_show(mark + strlen(VL_SHOW_MARK), &a->db);
        if (!(st & 1)) {
            char m[256];
            vl_first_message(out, m, sizeof m);
            if (m[0]) vl_set_msg(a, 1, "%s", m);
        }
    }
    if (!(st & 1) && !a->msg[0]) {
        char m[200];
        vlp_message(st, m, sizeof m);
        vl_set_msg(a, 1, "LICENSE LIST: %s", m);
    }
    free(out);
    a->load_status = st;
    rebuild(a);
    if (have) keep_cursor(a, &was);
    return st;
}

int vl_app_init(vl_app *a, const char *db, int rows, int cols)
{
    vl_when w;
    memset(a, 0, sizeof *a);
    vl_db_init(&a->db);
    if (!vl_screen_init(&a->scr, rows, cols)) return 0;
    a->rows = rows; a->cols = cols;
    /* LICENSE takes a relative name as relative to LMF$LICENSE's directory (N5):
       give it the full one, which is also what the confirm screen shows. */
    if (!(vlp_full_spec(db && *db ? db : "LMF$LICENSE", a->db_spec, sizeof a->db_spec) & 1))
        vl_strlcpy(a->db_spec, db && *db ? db : "LMF$LICENSE", sizeof a->db_spec);
    vl_upper(a->db_spec);
    vl_strlcpy(a->filter, "*", sizeof a->filter);
    vlp_now(&w);
    a->today = vl_days(w.y, w.m, w.d);
    load(a);
    return 1;
}

void vl_app_free(vl_app *a)
{
    text_free(a);
    free(a->vis);
    vl_db_free(&a->db);
    vl_screen_free(&a->scr);
}

/* ---------- Info and Help ---------- */

static void info_build(vl_app *a)
{
    vl_lic *l = vl_cur(a);
    text_free(a);
    if (!l) return;
    snprintf(a->text_title, sizeof a->text_title, "License %.56s from %.56s", l->product, l->producer);
    text_add(a, "Product Name       %s", l->product);
    text_add(a, "Producer           %s", l->producer);
    text_add(a, "Issuer             %s", l->issuer);
    text_add(a, "Authorization      %s", l->auth);
    text_add(a, "Status             %s%s", vl_status_text(a, l),
             l->loaded ? "   (loaded on this node)" : "");
    text_add(a, "Units              %s%s%s%s", l->units, l->mod_units[0] ? "   modified to " : "",
             l->mod_units, "");
    text_add(a, "Version            %s", l->version);
    text_add(a, "Release Date       %s", l->release);
    text_add(a, "Termination Date   %s%s%s", l->term, l->mod_term[0] ? "   modified to " : "", l->mod_term);
    text_add(a, "Availability       %s", l->avail[0] ? l->avail : "(not shown)");
    text_add(a, "Activity           %s", l->activ[0] ? l->activ : "(not shown)");
    text_add(a, "Options            %s", l->options);
    text_add(a, "Hardware ID        %s", l->hwid);
    if (l->token[0]) text_add(a, "Token              %s", l->token);
    if (l->weight[0]) text_add(a, "Selection Weight   %s", l->weight);
    text_add(a, "Comment            %s", l->comment);
    text_add(a, "Include            %s", l->include);
    text_add(a, "Exclude            %s", l->exclude);
    text_add(a, "Reserve            %s", l->reserve);
    text_add(a, "Revision Level     %s", l->revision);
    text_add(a, "Last command       %s by %s on %s", l->command, l->mod_user, l->mod_on);
    if (l->other[0]) {
        char tmp[sizeof l->other], *p = tmp, *nl;
        vl_strlcpy(tmp, l->other, sizeof tmp);
        text_add(a, "");
        text_add(a, "Also shown by LIST/FULL:");
        for (; p; p = nl) {
            if ((nl = strchr(p, '\n')) != 0) *nl++ = '\0';
            text_add(a, "  %s", p);
        }
    }
    text_add(a, "");
    text_add(a, "Database           %s", a->db.file);
}

static void help_build(vl_app *a)
{
    text_free(a);
    vl_strlcpy(a->text_title, "Help", sizeof a->text_title);
    text_add(a, "THE LICENSE LIST");
    text_add(a, "");
    text_add(a, "  Up/Down, Prev/Next    move;  GOLD Up/Down  first/last");
    text_add(a, "  Return, I             Info: every field LMF holds for this license");
    text_add(a, "  A, Insert Here        Add: register a PAK (type it in)");
    text_add(a, "  GOLD A                Add the PAKs in a file (LICENSE REGISTER procedure");
    text_add(a, "                        or PAK listing, as VSI sends them)");
    text_add(a, "  E                     Edit: units, termination, comment, weight,");
    text_add(a, "                        include/exclude nodes, reserve users");
    text_add(a, "  D                     Disable an active license, enable a disabled one");
    text_add(a, "  R, Remove             Remove the license from the database");
    text_add(a, "  L                     Load the license on this node (CMKRNL, SYSNAM)");
    text_add(a, "  F, Find               filter by product name:  VMS*  C%%%%  *");
    text_add(a, "  S                     sort by product, producer, termination, status");
    text_add(a, "  H                     show the history records too (read-only)");
    text_add(a, "  GOLD W                read the database again;  Ctrl/W  redraw");
    text_add(a, "  F10, Ctrl/Z           leave VLMF");
    text_add(a, "");
    text_add(a, "  A diamond before a product: SHOW LICENSE lists it as loaded here.");
    text_add(a, "");
    text_add(a, "THE FORMS");
    text_add(a, "");
    text_add(a, "  Up/Down, Tab, Return  field to field (Return on the last: done)");
    text_add(a, "  Do, KP Enter          done: show the LICENSE commands");
    text_add(a, "  F10, Ctrl/Z           cancel");
    text_add(a, "  Left/Right  Ctrl/H Ctrl/E  start/end  Ctrl/U erase  Ctrl/A overstrike");
    text_add(a, "");
    text_add(a, "CHANGES");
    text_add(a, "");
    text_add(a, "  Every change shows the LICENSE commands it will run and asks first;");
    text_add(a, "  the answer is No unless you type Y. Before the first change a copy of");
    text_add(a, "  the database is made (VLMF$BACKUP, default SYS$LOGIN:VLMF_BACKUP.LDB).");
    text_add(a, "  Changes are logged in VLMF$LOG (default SYS$LOGIN:VLMF.LOG).");
    text_add(a, "  LIST does not show a PAK's checksum: keep your PAKs before removing.");
}

/* ---------- forms ---------- */

const vl_field *vl_form_fields(vl_app *a, int *n)
{
    if (a->form_kind == F_ADD) { *n = vl_pak_nfields; return vl_pak_fields; }
    *n = vl_mod_nfields;
    return vl_mod_fields;
}

char *vl_form_value(vl_app *a, int i)
{
    int n;
    const vl_field *f = vl_form_fields(a, &n);
    return (a->form_kind == F_ADD ? (char *) &a->pak : (char *) &a->mods) + f[i].off;
}

static void form_load_field(vl_app *a) { vl_edit_init(&a->fedit, vl_form_value(a, a->fcur)); }

static void form_store_field(vl_app *a)
{
    int n;
    const vl_field *f = vl_form_fields(a, &n);
    char tmp[VL_SPEC_MAX];
    vl_strlcpy(tmp, a->fedit.buf, sizeof tmp);
    if (f[a->fcur].kind != VL_K_COMMENT) vl_trim(tmp);
    vl_strlcpy(vl_form_value(a, a->fcur), tmp, f[a->fcur].size);
}

static void form_open(vl_app *a, int kind)
{
    a->form_kind = kind;
    a->fcur = 0;
    a->form_err[0] = '\0';
    form_load_field(a);
    a->mode = M_FORM;
}

static void add_start(vl_app *a, const vl_pak *p)
{
    if (p) a->pak = *p; else memset(&a->pak, 0, sizeof a->pak);
    form_open(a, F_ADD);
}

static void next_queued(vl_app *a)
{
    if (a->qpos < a->nqueue) {
        add_start(a, &a->queue[a->qpos]);
        a->qpos++;
        vl_set_msg(a, 0, "PAK %d of %d from the file", a->qpos, a->nqueue);
    }
}

static void edit_start(vl_app *a)
{
    vl_lic *l = vl_cur(a);
    if (!l) return;
    if (vl_lic_extinct(l)) { vl_set_msg(a, 1, "A history record cannot be changed"); return; }
    a->form_lic = *l;
    vl_mods_from(l, &a->mods);
    form_open(a, F_EDIT);
}

static const char *op_name(int op)
{
    switch (op) {
    case OP_REGISTER: return "Register";
    case OP_MODIFY: return "Modify";
    case OP_DELETE: return "Remove";
    case OP_ENABLE: return "Enable";
    case OP_DISABLE: return "Disable";
    case OP_LOAD: return "Load";
    }
    return "?";
}

/* Show what will run and ask (D5). */
static void confirm_start(vl_app *a, int op, const vl_lic *l)
{
    int i;
    a->op = op;
    if (l) a->op_lic = *l; else memset(&a->op_lic, 0, sizeof a->op_lic);
    text_free(a);
    snprintf(a->text_title, sizeof a->text_title, "%s %s", op_name(op),
             op == OP_REGISTER ? a->pak.product : a->op_lic.product);
    vl_upper(a->text_title + strlen(op_name(op)));
    text_add(a, "VLMF will run %s:", a->ncmds == 1 ? "this command" : "these commands, in order");
    text_add(a, "");
    for (i = 0; i < a->ncmds; i++) {
        char full[VL_CMD + 300];
        snprintf(full, sizeof full, "LICENSE %s/DATABASE=%s", a->cmds[i], a->db_spec);
        text_add_wrapped(a, "  $ ", full, a->cols - 4);
    }
    text_add(a, "");
    if (op != OP_LOAD && !a->backed_up)
{
        text_add(a, "A copy of the database is made first, as VLMF$BACKUP or, if that is not");
        text_add(a, "defined, SYS$LOGIN:VLMF_BACKUP.LDB.");
    }
    if (op == OP_DELETE) {
        text_add(a, "");
        text_add(a, "Removing deletes this license and its history records. LMF does not");
        text_add(a, "show a PAK's checksum, so to register it again you need the PAK itself.");
        if (a->op_lic.loaded)
            text_add(a, "It stays loaded on this node until LICENSE UNLOAD or a reboot.");
    }
    if (op == OP_DISABLE && a->op_lic.loaded)
        text_add(a, "It stays loaded on this node until LICENSE UNLOAD or a reboot.");
    if (op == OP_LOAD) text_add(a, "LICENSE LOAD needs the CMKRNL and SYSNAM privileges.");
    a->back_mode = a->mode;
    a->mode = M_CONFIRM;
    a->prompt = P_ASK;
    snprintf(a->prompt_label, sizeof a->prompt_label, "%s? Y or N [N]: ", op_name(op));
}

/* Which field a build error is about ("Label: why"). */
static void focus_error_field(vl_app *a)
{
    int n, i;
    const vl_field *f = vl_form_fields(a, &n);
    for (i = 0; i < n; i++) {
        size_t k = strlen(f[i].label);
        if (!strncmp(a->form_err, f[i].label, k) && a->form_err[k] == ':') {
            a->fcur = i;
            form_load_field(a);
            return;
        }
    }
}

static void form_done(vl_app *a)
{
    int n;
    form_store_field(a);
    a->form_err[0] = '\0';
    if (a->form_kind == F_ADD)
        n = vl_build_register(&a->pak, a->cmds, VL_MAXCMDS, a->form_err, sizeof a->form_err);
    else
        n = vl_build_modify(&a->form_lic, &a->mods, a->cmds, VL_MAXCMDS, a->form_err, sizeof a->form_err);
    if (n < 0) { focus_error_field(a); return; }
    if (n == 0) {
        a->mode = M_LIST;
        vl_set_msg(a, 0, "Nothing was changed");
        return;
    }
    a->ncmds = n;
    confirm_start(a, a->form_kind == F_ADD ? OP_REGISTER : OP_MODIFY,
                  a->form_kind == F_ADD ? 0 : &a->form_lic);
}

static void form_key(vl_app *a, int key)
{
    int n;
    vl_form_fields(a, &n);
    switch (key) {
    case VK_UP:
        form_store_field(a);
        if (a->fcur > 0) a->fcur--;
        form_load_field(a);
        return;
    case VK_DOWN: case VK_TAB:
        form_store_field(a);
        if (a->fcur < n - 1) a->fcur++;
        form_load_field(a);
        return;
    case VK_RETURN:
        form_store_field(a);
        if (a->fcur < n - 1) { a->fcur++; form_load_field(a); }
        else form_done(a);
        return;
    case VK_DO: case VK_ENTER: case VK_PF4:
        form_done(a);
        return;
    case VK_F10: case VK_CTRL_Z:
        a->mode = M_LIST;
        vl_set_msg(a, 0, "Cancelled: nothing was changed");
        if (a->form_kind == F_ADD) next_queued(a);
        return;
    case VK_HELP: case VK_PF2:
        form_store_field(a);
        help_build(a);
        a->back_mode = M_FORM;
        a->mode = M_HELP;
        return;
    default:
        vl_edit_key(&a->fedit, key);
    }
}

/* ---------- running an operation ---------- */

static void run_op(vl_app *a)
{
    static char proc[VL_MAXCMDS * (VL_CMD + 400) + 1024];
    char *out = 0, m[256] = "", line[VL_CMD + 400];
    unsigned int st;
    int i, op = a->op, from_form = a->back_mode == M_FORM;
    char what[200];

    snprintf(what, sizeof what, "%s %s",
             op_name(op), op == OP_REGISTER ? a->pak.product : a->op_lic.product);
    vl_upper(what + strlen(op_name(op)));
    if (!vl_proc_change(a->db_spec, op != OP_LOAD && !a->backed_up, a->cmds, a->ncmds,
                        proc, sizeof proc)) {
        vl_set_msg(a, 1, "%s: the commands are too long", what);
        a->mode = from_form ? M_FORM : M_LIST;
        return;
    }
    vl_set_msg(a, 0, "Running LICENSE...");
    vl_app_draw(a);
    vlp_draw(&a->scr);
    st = vlp_dcl(proc, &out);
    if (out) {
        char *b = strstr(out, "==== VLMF BACKUP ");
        if (b && strncmp(b + 17, "FAILED", 6)) {
            char *e;
            vl_strlcpy(a->backup, b + 17, sizeof a->backup);
            if ((e = strpbrk(a->backup, "\r\n")) != 0) *e = '\0';
            a->backed_up = 1;
            snprintf(line, sizeof line, "BACKUP %s -> %s", a->db_spec, a->backup);
            vlp_log(line);
        }
        vl_first_message(out, m, sizeof m);
    }
    for (i = 0; i < a->ncmds; i++) {
        snprintf(line, sizeof line, "%s LICENSE %s/DATABASE=%s", (st & 1) ? "OK  " : "RAN?",
                 a->cmds[i], a->db_spec);
        vlp_log(line);
    }
    if (!(st & 1)) {
        char sm[200];
        if (!m[0]) { vlp_message(st, sm, sizeof sm); vl_strlcpy(m, sm, sizeof m); }
        if (out && strstr(out, "==== VLMF BACKUP FAILED"))
            snprintf(line, sizeof line, "%s: not done, the backup copy failed: %s", what, m);
        else
            snprintf(line, sizeof line, "%s failed: %s", what, m);
        vlp_log(line);
    }
    free(out);
    a->prompt = P_NONE;
    load(a);
    if (st & 1) {
        a->mode = M_LIST;
        vl_set_msg(a, 0, "%s: done", what);
        if (op == OP_REGISTER) next_queued(a);
    } else if (from_form) {
        a->mode = M_FORM;
        vl_strlcpy(a->form_err, line, sizeof a->form_err);
        a->msg[0] = '\0';
    } else {
        a->mode = M_LIST;
        vl_set_msg(a, 1, "%s", line);
    }
}

static void simple_op(vl_app *a, int op)
{
    vl_lic *l = vl_cur(a);
    char err[200];
    const char *verb = op == OP_DELETE ? "DELETE" : op == OP_ENABLE ? "ENABLE"
                     : op == OP_DISABLE ? "DISABLE" : "LOAD";
    if (!l) return;
    a->ncmds = vl_build_simple(verb, l, a->cmds, VL_MAXCMDS, err, sizeof err);
    if (a->ncmds < 0) { vl_set_msg(a, 1, "%s", err); return; }
    confirm_start(a, op, l);
}

static void import_file(vl_app *a, const char *spec)
{
    char *text = 0;
    unsigned int st = vlp_read_file(spec, &text);
    if (!(st & 1) || !text) {
        char m[200];
        vlp_message(st, m, sizeof m);
        vl_set_msg(a, 1, "%s: %s", spec, m);
        return;
    }
    a->nqueue = vl_pak_import(text, a->queue, VL_QUEUE);
    a->qpos = 0;
    free(text);
    if (!a->nqueue) { vl_set_msg(a, 1, "No PAK found in %s", spec); return; }
    next_queued(a);
}

/* ---------- keys ---------- */

static void start_prompt(vl_app *a, int kind, const char *label, const char *initial)
{
    a->prompt = kind;
    vl_strlcpy(a->prompt_label, label, sizeof a->prompt_label);
    vl_edit_init(&a->pedit, initial);
}

static void prompt_key(vl_app *a, int key)
{
    int r;
    if (a->prompt == P_ASK) {
        a->prompt = P_NONE;
        if (key == 'Y' || key == 'y') { run_op(a); return; }
        a->mode = a->back_mode == M_FORM ? M_FORM : M_LIST;
        vl_set_msg(a, 0, "Not done: nothing was changed");
        return;
    }
    r = vl_edit_key(&a->pedit, key);
    if (r == VL_EDIT_MORE) return;
    {
        int kind = a->prompt;
        char text[VL_SPEC_MAX];
        a->prompt = P_NONE;
        if (r == VL_EDIT_CANCEL) return;
        vl_strlcpy(text, a->pedit.buf, sizeof text);
        vl_trim(text);
        if (kind == P_FILTER) {
            vl_strlcpy(a->filter, text[0] ? vl_upper(text) : "*", sizeof a->filter);
            a->cur = a->top = 0;
            rebuild(a);
            vl_set_msg(a, 0, "%d license%s %s", a->nvis, a->nvis == 1 ? " matches" : "s match", a->filter);
        } else if (kind == P_FILE && text[0]) {
            import_file(a, text);
        }
    }
}

static void list_key(vl_app *a, int key)
{
    int page = a->rows - 13 > 3 ? a->rows - 13 : 3;
    vl_lic *l = vl_cur(a);
    if (a->gold) {
        a->gold = 0;
        switch (key) {
        case VK_UP: a->cur = 0; return;
        case VK_DOWN: a->cur = a->nvis ? a->nvis - 1 : 0; return;
        case 'a': case 'A': start_prompt(a, P_FILE, "PAK file: ", ""); return;
        case 'w': case 'W': load(a); if (a->load_status & 1) vl_set_msg(a, 0, "Read again"); return;
        default: return;
        }
    }
    switch (key) {
    case VK_PF1: a->gold = 1; return;
    case VK_UP: if (a->cur > 0) a->cur--; return;
    case VK_DOWN: if (a->cur < a->nvis - 1) a->cur++; return;
    case VK_PREV: a->cur -= page; if (a->cur < 0) a->cur = 0; return;
    case VK_NEXT: a->cur += page; if (a->cur >= a->nvis) a->cur = a->nvis ? a->nvis - 1 : 0; return;
    case VK_RETURN: case VK_SELECT: case 'i': case 'I':
        if (l) { info_build(a); a->back_mode = M_LIST; a->mode = M_INFO; }
        return;
    case 'a': case 'A': case VK_INSERT: a->nqueue = a->qpos = 0; add_start(a, 0); return;
    case 'e': case 'E': edit_start(a); return;
    case 'r': case 'R': case VK_REMOVE: simple_op(a, OP_DELETE); return;
    case 'd': case 'D':
        if (!l) return;
        if (vl_lic_active(l)) simple_op(a, OP_DISABLE);
        else if (!vl_strcasecmp(l->status, "Disabled")) simple_op(a, OP_ENABLE);
        else vl_set_msg(a, 1, "Only an active license can be disabled, or a disabled one enabled");
        return;
    case 'l': case 'L': simple_op(a, OP_LOAD); return;
    case 'f': case 'F': case VK_FIND: start_prompt(a, P_FILTER, "Product filter: ", a->filter); return;
    case 's': case 'S': {
        static const char *names[] = { "product", "producer", "termination date", "status" };
        a->sort = (a->sort + 1) % 4;
        rebuild(a);
        if (l) keep_cursor(a, l);
        vl_set_msg(a, 0, "Sorted by %s", names[a->sort]);
        return;
    }
    case 'h': case 'H':
        a->history = !a->history;
        load(a);
        vl_set_msg(a, 0, a->history ? "History records shown (status Extinct)" : "History records hidden");
        return;
    case VK_HELP: case VK_PF2: case '?':
        help_build(a); a->back_mode = M_LIST; a->mode = M_HELP; return;
    case VK_F10: case VK_CTRL_Z: case 'q': case 'Q': a->quit = 1; return;
    default: return;
    }
}

static void text_key(vl_app *a, int key)
{
    int page = a->rows - 6;
    switch (key) {
    case VK_UP: if (a->ttop > 0) a->ttop--; return;
    case VK_DOWN: if (a->ttop < a->ntext - 1) a->ttop++; return;
    case VK_PREV: a->ttop -= page; if (a->ttop < 0) a->ttop = 0; return;
    case VK_NEXT: if (a->ttop + page < a->ntext) a->ttop += page; return;
    default:
        a->mode = a->back_mode;
        if (a->mode == M_FORM) form_load_field(a);
    }
}

void vl_app_key(vl_app *a, int key)
{
    if (key == VK_CTRL_W) { vlp_repaint(); return; }
    if (key != VK_TIMEOUT && a->prompt == P_NONE && a->mode != M_FORM) a->msg[0] = '\0';
    if (a->prompt != P_NONE) {
        if (a->mode == M_CONFIRM && a->prompt == P_ASK &&
            (key == VK_UP || key == VK_DOWN || key == VK_PREV || key == VK_NEXT)) {
            text_key(a, key);         /* a long list of commands scrolls */
            a->mode = M_CONFIRM;
            return;
        }
        prompt_key(a, key);
        return;
    }
    switch (a->mode) {
    case M_LIST: list_key(a, key); break;
    case M_FORM: form_key(a, key); break;
    default: text_key(a, key); break;
    }
}

void vl_app_run(vl_app *a)
{
    char b[256];
    while (!a->quit) {
        int key;
        vl_app_draw(a);
        vlp_draw(&a->scr);
        key = vlp_key(30);
        if (vlp_broadcast(b, sizeof b)) vl_set_msg(a, 0, "%s", b);
        if (key == VK_TIMEOUT) {
            vl_when w;
            vlp_now(&w);
            a->today = vl_days(w.y, w.m, w.d);
            continue;
        }
        vl_app_key(a, key);
    }
}
