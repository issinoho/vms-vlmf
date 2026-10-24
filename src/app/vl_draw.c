/* vl_draw.c - draw VLMF's screens into the screen model, in VTREE's layout:
   title line, a framed window with a statistics panel on the right, two menu
   lines at the bottom. */
#include <stdio.h>
#include <string.h>
#include "vl_app.h"
#include "vl_util.h"

#define PANEL_W 21
#define DETAIL_H 7

/* A menu line: '&' marks the next character as a key (bold). */
static void menu_line(vl_screen *s, int row, const char *label, const char *text, unsigned char base)
{
    int col = 1, n;
    vl_role(s, VL_R_MENU);
    vl_fill(s, row, 0, s->cols, ' ', base);
    vl_role(s, VL_R_MENU_LABEL);
    n = vl_put(s, row, col, label, (unsigned char) (base | VL_A_REV), -1);
    vl_role(s, VL_R_MENU);
    col += n + 2;
    for (; *text && col < s->cols; text++) {
        if (*text == '&' && text[1]) {
            text++;
            vl_fill(s, row, col++, 1, (unsigned char) *text, (unsigned char) (base | VL_A_BOLD));
        } else vl_fill(s, row, col++, 1, (unsigned char) *text, base);
    }
    vl_role(s, VL_R_AUTO);
}

static void draw_title(vl_app *a, const char *what)
{
    vl_screen *s = &a->scr;
    char clock[32], t[512];
    vl_when w;
    int cl;
    vlp_now(&w);
    vl_fmt_when(&w, clock, sizeof clock);
    cl = (int) strlen(clock);
    vl_role(s, VL_R_TITLE);
    vl_fill(s, 0, 0, s->cols, ' ', VL_A_REV);
    snprintf(t, sizeof t, "        %s", what);
    vl_put(s, 0, 0, t, VL_A_REV, s->cols - cl - 3);
    vl_put(s, 0, 1, "VLMF", VL_A_REV | VL_A_BOLD, -1);
    vl_put(s, 0, s->cols - cl - 1, clock, VL_A_REV, -1);
    vl_role(s, VL_R_AUTO);
}

static void draw_bottom(vl_app *a, const char *l1label, const char *l1, const char *l2)
{
    vl_screen *s = &a->scr;
    int r1 = s->rows - 2, r2 = s->rows - 1;
    if (a->prompt != P_NONE) {
        int n;
        vl_fill(s, r1, 0, s->cols, ' ', 0);
        n = vl_put(s, r1, 1, a->prompt_label, VL_A_BOLD, -1);
        if (a->prompt == P_ASK) {
            s->cursor_on = 1; s->cursor_row = r1; s->cursor_col = 1 + n;
            menu_line(s, r2, "CHOOSE", "&Y do it   any other key: do nothing   Up/Down scroll", VL_A_REV);
            return;
        }
        {
            int width = s->cols - 2 - n, start = 0;
            if (width < 10) width = 10;
            if (a->pedit.pos >= width) start = a->pedit.pos - width + 1;
            vl_put(s, r1, 1 + n, a->pedit.buf + start, 0, width);
            s->cursor_on = 1; s->cursor_row = r1; s->cursor_col = 1 + n + a->pedit.pos - start;
        }
        menu_line(s, r2, "EDIT", "Return accept   F10 cancel   Ctrl/U erase   Ctrl/H start   Ctrl/E end", VL_A_REV);
        return;
    }
    if (a->msg[0]) {
        vl_fill(s, r1, 0, s->cols, ' ', 0);
        vl_put(s, r1, 1, a->msg, a->msg_err ? (VL_A_BOLD | VL_A_REV) : VL_A_BOLD, s->cols - 2);
    } else menu_line(s, r1, l1label, l1, 0);
    menu_line(s, r2, "COMMANDS", l2, VL_A_REV);
}

/* "label value" with the label bold; returns the next column. */
static int kv(vl_screen *s, int row, int col, int max, const char *label, const char *value)
{
    int n = vl_put(s, row, col, label, VL_A_BOLD, max);
    if (n < max) n += 1 + vl_put(s, row, col + n + 1, value, 0, max - n - 1);
    return col + n;
}

/* ---------- the list ---------- */

static void list_line(vl_app *a, const vl_lic *l, char *buf, size_t size, int width)
{
    const char *term = vl_lic_term(l);
    char units[16];
    vl_strlcpy(units, l->mod_units[0] ? l->mod_units : l->units, sizeof units);
    if (width >= 100)
        snprintf(buf, size, "%-18.18s %-9.9s %-9.9s %5.5s%c %-11.11s%c %-31.31s %s", l->product,
                 l->producer, vl_status_text(a, l), units, l->mod_units[0] ? '*' : ' ',
                 term[0] ? term : "(none)", l->mod_term[0] ? '*' : ' ', l->auth, l->comment);
    else
        snprintf(buf, size, "%-18.18s %-8.8s %-8.8s %5.5s%c %-11.11s%c", l->product, l->producer,
                 vl_status_text(a, l), units, l->mod_units[0] ? '*' : ' ',
                 term[0] ? term : "(none)", l->mod_term[0] ? '*' : ' ');
}

static void draw_list(vl_app *a, int r1, int r2, int c1, int c2)
{
    vl_screen *s = &a->scr;
    int width = c2 - c1 + 1, rows = r2 - r1, i;
    char buf[512];
    if (width >= 100)
        snprintf(buf, sizeof buf, "%-18s %-9s %-9s %5s  %-12s %-31s %s", "Product", "Producer",
                 "Status", "Units", "Terminates", "Authorization", "Comment");
    else
        snprintf(buf, sizeof buf, "%-18s %-8s %-8s %5s  %-12s", "Product", "Producer", "Status",
                 "Units", "Terminates");
    vl_put(s, r1, c1 + 1, buf, VL_A_BOLD, width - 1);
    if (!a->nvis) {
        const char *why = (a->load_status & 1) ? (a->db.n ? "No license matches the filter"
                                                          : "The database holds no licenses")
                                               : "The database could not be read";
        vl_put(s, r1 + 1, c1 + 1, why, 0, width - 1);
        return;
    }
    if (a->cur < a->top) a->top = a->cur;
    if (a->cur >= a->top + rows) a->top = a->cur - rows + 1;
    if (a->top > a->nvis - rows) a->top = a->nvis - rows;
    if (a->top < 0) a->top = 0;
    for (i = 0; i < rows && a->top + i < a->nvis; i++) {
        const vl_lic *l = &a->db.l[a->vis[a->top + i]];
        int row = r1 + 1 + i;
        unsigned char at = 0;
        long t = vl_lic_term_days(l);
        if (l->loaded) vl_gfx(s, row, c1, VL_G_DIAMOND, VL_A_BOLD);
        list_line(a, l, buf, sizeof buf, width);
        if (vl_lic_active(l) && t >= 0 && t < a->today + 30) at = VL_A_BOLD;   /* expiring */
        if (a->top + i == a->cur) at = VL_A_REV;
        vl_put(s, row, c1 + 1, buf, at, width - 1);
    }
}

static void draw_details(vl_app *a, int r1, int r2, int c1, int c2)
{
    vl_screen *s = &a->scr;
    const vl_lic *l = vl_cur(a);
    int w = c2 - c1 + 1, r = r1, col;
    char t[256];
    if (!l) return;
    /* the last row is always the last change; the others fill from the top */
    kv(s, r++, c1, w, "Authorization", l->auth);
    col = kv(s, r, c1, w, "Issuer", l->issuer);
    col = kv(s, r, col + 3, w - (col + 3 - c1), "Version", l->version);
    kv(s, r++, col + 3, w - (col + 3 - c1), "Release", l->release);
    if (r < r2) {
        col = kv(s, r, c1, w, "Options", l->options);
        if (l->hwid[0]) kv(s, r, col + 3, w - (col + 3 - c1), "Hardware", l->hwid);
        r++;
    }
    if (r < r2 && (l->mod_units[0] || l->mod_term[0])) {
        snprintf(t, sizeof t, "%s units, ends %s  (* modified)", l->units, l->term);
        kv(s, r++, c1, w, "PAK says", t);
    }
    if (r < r2 && l->comment[0]) kv(s, r++, c1, w, "Comment", l->comment);
    if (r < r2 && l->include[0]) kv(s, r++, c1, w, "Include", l->include);
    if (r < r2 && l->exclude[0]) kv(s, r++, c1, w, "Exclude", l->exclude);
    if (r < r2 && l->reserve[0]) kv(s, r++, c1, w, "Reserve", l->reserve);
    snprintf(t, sizeof t, "%.17s by %s, %s", l->mod_on, l->mod_user, l->command);
    if (r <= r2) kv(s, r2, c1, w, "Last change", t);
}

static void panel(vl_app *a, int r1, int r2, int c1)
{
    vl_screen *s = &a->scr;
    int r = r1, i, act = 0, dis = 0, loaded = 0, expired = 0, soon = 0, total = 0;
    char line[64];
    const char *name = a->db.file, *p;
    static const char *sorts[] = { "product", "producer", "termination", "status" };
    for (i = 0; i < a->db.n; i++) {
        const vl_lic *l = &a->db.l[i];
        long t = vl_lic_term_days(l);
        if (vl_lic_extinct(l)) continue;
        total++;
        if (vl_lic_active(l)) {
            act++;
            if (t >= 0 && t < a->today) expired++;
            else if (t >= 0 && t < a->today + 30) soon++;
        } else dis++;
        if (l->loaded) loaded++;
    }
    if ((p = strrchr(name, ']')) != 0 || (p = strrchr(name, '>')) != 0) name = p + 1;
    if (!*name) name = a->db_spec;
#define PL(text, at) do { if (r <= r2) vl_put(s, r++, c1, (text), (at), PANEL_W - 2); } while (0)
#define PN(label, n) do { snprintf(line, sizeof line, " %-11s%6d", label, n); PL(line, 0); } while (0)
    PL("DATABASE", VL_A_BOLD);
    snprintf(line, sizeof line, " %.60s", name); PL(line, 0);
    if (a->db.created[0]) { snprintf(line, sizeof line, " Made %.12s", a->db.created); PL(line, 0); }
    PL("LICENSES", VL_A_BOLD);
    PN("Total", total);
    PN("Active", act);
    PN("Disabled", dis);
    PN("Loaded", loaded);
    PN("Expired", expired);
    PN("Within 30 d", soon);
    PL("SHOWING", VL_A_BOLD);
    snprintf(line, sizeof line, " %.40s%s", a->filter, a->history ? " +history" : ""); PL(line, 0);
    snprintf(line, sizeof line, " by %s", sorts[a->sort]); PL(line, 0);
    r++;
    snprintf(line, sizeof line, "%s %s", a->sess.node, a->sess.user); PL(line, 0);
    PL(strstr(a->sess.privs, "SYSPRV") ? " SYSPRV: can change" : " No SYSPRV: read-only", 0);
#undef PN
#undef PL
}

static void draw_main(vl_app *a)
{
    vl_screen *s = &a->scr;
    int top = 1, bot = s->rows - 3, pw = s->cols >= 72 ? PANEL_W : 0;
    int cx = pw ? s->cols - pw - 1 : s->cols - 1;
    int dh = bot - top - 1 >= 16 ? DETAIL_H : 4, hs = bot - dh - 1;
    char title[512];

    vl_box(s, top, 0, bot, s->cols - 1);
    if (pw) {
        vl_vline(s, cx, top + 1, bot - 1);
        vl_gfx(s, top, cx, VL_G_TTEE, 0);
        vl_gfx(s, bot, cx, VL_G_BTEE, 0);
        panel(a, top + 1, bot - 1, cx + 2);
    }
    draw_list(a, top + 1, hs - 1, 1, cx - 1);
    vl_hline(s, hs, 1, cx - 1);
    vl_gfx(s, hs, 0, VL_G_LTEE, 0);
    vl_gfx(s, hs, cx, VL_G_RTEE, 0);
    draw_details(a, hs + 1, bot - 1, 2, cx - 2);
    snprintf(title, sizeof title, "%s", a->db.file[0] ? a->db.file : a->db_spec);
    draw_title(a, title);
    if (a->gold)
        draw_bottom(a, "GOLD", "&Add the PAKs in a file   &W read again   Up/Down first/last",
                    "PF1 cancel GOLD");
    else
        draw_bottom(a, "LICENSE", "&Add &Edit &Remove &Disable/enable &Load &Filter &History &Info &Sort",
                    "PF1 GOLD   Return info   Insert add   Remove remove   Help   F10 exit");
}

/* ---------- text screens ---------- */

static void draw_text(vl_app *a, const char *label, const char *l1, const char *l2)
{
    vl_screen *s = &a->scr;
    int rows = s->rows - 5, i;
    draw_title(a, a->text_title);
    vl_box(s, 1, 0, s->rows - 3, s->cols - 1);
    if (a->ttop > a->ntext - rows) a->ttop = a->ntext - rows;
    if (a->ttop < 0) a->ttop = 0;
    for (i = 0; i < rows && a->ttop + i < a->ntext; i++) {
        const char *t = a->text[a->ttop + i];
        unsigned char at = (t[0] >= 'A' && t[0] <= 'Z' && t[1] >= 'A' && t[1] <= 'Z'
                            && a->mode == M_HELP) ? VL_A_BOLD : 0;
        if (a->mode == M_CONFIRM && t[0] == ' ' && t[2] == '$') at = VL_A_BOLD;
        vl_put(s, 2 + i, 2, t, at, s->cols - 4);
    }
    if (a->ttop + rows < a->ntext) vl_put(s, s->rows - 3, s->cols - 10, " more ", VL_A_BOLD, -1);
    draw_bottom(a, label, l1, l2);
}

/* ---------- forms ---------- */

static void draw_form(vl_app *a)
{
    vl_screen *s = &a->scr;
    int n, i, r = 2, lw = 20, vc, vw;
    const vl_field *f = vl_form_fields(a, &n);
    char t[512];
    if (a->form_kind == F_ADD) {
        draw_title(a, "Register a license from its PAK");
    } else {
        snprintf(t, sizeof t, "Modify %s from %s", a->form_lic.product, a->form_lic.producer);
        draw_title(a, t);
    }
    vl_box(s, 1, 0, s->rows - 3, s->cols - 1);
    if (a->form_kind == F_EDIT) {
        const vl_lic *l = &a->form_lic;
        int col = kv(s, r, 2, s->cols - 4, "Authorization", l->auth);
        kv(s, r++, col + 3, s->cols - col - 5, "Issuer", l->issuer);
        snprintf(t, sizeof t, "units %s, termination %s, options %s", l->units, l->term, l->options);
        kv(s, r++, 2, s->cols - 4, "PAK", t);
        vl_hline(s, r, 1, s->cols - 2);
        vl_gfx(s, r, 0, VL_G_LTEE, 0);
        vl_gfx(s, r, s->cols - 1, VL_G_RTEE, 0);
        r++;
    }
    vc = 2 + lw + 1;
    vw = s->cols - vc - 2;
    for (i = 0; i < n && r < s->rows - 5; i++, r++) {
        const char *v = vl_form_value(a, i);
        vl_put(s, r, 2, f[i].label, i == a->fcur ? VL_A_BOLD : 0, lw);
        if (f[i].required) vl_put(s, r, 2 + lw - 1, "*", VL_A_BOLD, 1);
        if (i == a->fcur) {
            int start = 0;
            if (a->fedit.pos >= vw) start = a->fedit.pos - vw + 1;
            vl_fill(s, r, vc, vw, ' ', VL_A_REV);
            vl_put(s, r, vc, a->fedit.buf + start, VL_A_REV, vw);
            s->cursor_on = 1; s->cursor_row = r; s->cursor_col = vc + a->fedit.pos - start;
        } else {
            vl_fill(s, r, vc, vw, ' ', VL_A_UNDER);
            vl_put(s, r, vc, v, VL_A_UNDER, vw);
        }
    }
    r = s->rows - 5;
    if (a->form_err[0]) vl_put(s, r, 2, a->form_err, VL_A_BOLD | VL_A_REV, s->cols - 4);
    else vl_put(s, r, 2, f[a->fcur].hint, 0, s->cols - 4);
    if (a->form_kind == F_ADD)
        vl_put(s, r - 1, 2, "* needed.  Type the PAK exactly; LMF checks it against the checksum.", 0, s->cols - 4);
    else
        vl_put(s, r - 1, 2, "Empty a field to undo a modification; the PAK's own values stay.", 0, s->cols - 4);
    {
        char l1[160];
        vl_strlcpy(l1, a->msg[0] && !a->msg_err ? a->msg : "", sizeof l1);
        if (l1[0]) {
            vl_fill(s, s->rows - 2, 0, s->cols, ' ', 0);
            vl_put(s, s->rows - 2, 1, l1, VL_A_BOLD, s->cols - 2);
            menu_line(s, s->rows - 1, "COMMANDS", "Do or Enter: done   F10 cancel   Help", VL_A_REV);
            return;
        }
    }
    menu_line(s, s->rows - 2, "FORM", "Up/Down Tab Return: field to field   Ctrl/U erase   Ctrl/A overstrike", 0);
    menu_line(s, s->rows - 1, "COMMANDS", "Do or Enter: done   F10 cancel   Help", VL_A_REV);
}

void vl_app_draw(vl_app *a)
{
    vl_screen_clear(&a->scr);
    switch (a->mode) {
    case M_INFO: draw_text(a, "INFO", "Up/Down Prev/Next scroll", "Any other key returns"); break;
    case M_HELP: draw_text(a, "HELP", "Up/Down Prev/Next scroll", "Any other key returns"); break;
    case M_CONFIRM: draw_text(a, "CONFIRM", "", ""); break;
    case M_FORM: draw_form(a); break;
    default: draw_main(a); break;
    }
}
