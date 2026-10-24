/* vl_lic.c - licenses and LICENSE commands. No I/O. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "vl_lic.h"
#include "vl_util.h"

/* ---------- the database as LIST/FULL shows it ---------- */

void vl_db_init(vl_db *db) { memset(db, 0, sizeof *db); }

void vl_db_free(vl_db *db) { free(db->l); vl_db_init(db); }

static vl_lic *db_add(vl_db *db)
{
    if (db->n == db->cap) {
        int cap = db->cap ? db->cap * 2 : 32;
        vl_lic *l = (vl_lic *) realloc(db->l, (size_t) cap * sizeof *l);
        if (!l) return 0;
        db->l = l; db->cap = cap;
    }
    memset(&db->l[db->n], 0, sizeof db->l[0]);
    return &db->l[db->n++];
}

#define FLD(name) offsetof(vl_lic, name), sizeof ((vl_lic *) 0)->name
static const struct { const char *key; size_t off, size; int list; } list_keys[] = {
    { "Issuer", FLD(issuer), 0 }, { "Authorization", FLD(auth), 0 },
    { "Product Name", FLD(product), 0 }, { "Producer", FLD(producer), 0 },
    { "Units", FLD(units), 0 }, { "Modified Units", FLD(mod_units), 0 },
    { "Version", FLD(version), 0 }, { "Release Date", FLD(release), 0 },
    { "PAK Termination Date", FLD(term), 0 },
    { "Modified Termination Date", FLD(mod_term), 0 },
    { "Availability", FLD(avail), 0 }, { "Activity", FLD(activ), 0 },
    { "Options", FLD(options), 0 }, { "Hardware ID", FLD(hwid), 0 },
    { "Token", FLD(token), 0 }, { "Selection Weight", FLD(weight), 0 },
    { "Revision Level", FLD(revision), 0 }, { "Status", FLD(status), 0 },
    { "Command", FLD(command), 0 }, { "Comment", FLD(comment), 0 },
    { "Modified by user", FLD(mod_user), 0 }, { "Modified on", FLD(mod_on), 0 },
    { "Include", FLD(include), 1 }, { "Exclude", FLD(exclude), 1 },
    { "Reserve", FLD(reserve), 1 },
};
#undef FLD
#define NKEYS ((int) (sizeof list_keys / sizeof list_keys[0]))

/* Next line of text into buf (without the line end). Returns the rest, 0 at the end. */
static const char *next_line(const char *t, char *buf, size_t size)
{
    size_t n = 0;
    if (!t || !*t) return 0;
    while (*t && *t != '\n') {
        if (*t != '\r' && n + 1 < size) buf[n++] = *t;
        t++;
    }
    buf[n] = '\0';
    return *t ? t + 1 : t;
}

int vl_parse_list(const char *text, vl_db *db)
{
    char line[VL_LST + 128];
    vl_lic *cur = 0;
    int count = 0, last_list = -1;
    const char *t = text;
    while ((t = next_line(t, line, sizeof line)) != 0) {
        char key[64], *colon, *val;
        int i;
        if (strncmp(line, "-----", 5) == 0) {
            if (!(cur = db_add(db))) return -1;
            count++; last_list = -1;
            continue;
        }
        if (strncmp(line, "[End Of List]", 13) == 0) { cur = 0; continue; }
        colon = strchr(line, ':');
        if (!colon) {
            /* a list too long for one line goes on without a key (assumed) */
            vl_trim(line);
            if (cur && last_list >= 0 && line[0]) {
                char *f = (char *) cur + list_keys[last_list].off;
                vl_strlcat(f, ",", list_keys[last_list].size);
                vl_strlcat(f, line, list_keys[last_list].size);
            }
            continue;
        }
        *colon = '\0';
        vl_strlcpy(key, line, sizeof key);
        vl_trim(key);
        val = vl_trim(colon + 1);
        if (!cur) {
            if (!strcmp(key, "License Database File")) vl_strlcpy(db->file, val, sizeof db->file);
            else if (!strcmp(key, "Created on")) vl_strlcpy(db->created, val, sizeof db->created);
            else if (!strcmp(key, "Created by user")) vl_strlcpy(db->created_by, val, sizeof db->created_by);
            else if (!strcmp(key, "Created by LMF Version")) vl_strlcpy(db->lmf, val, sizeof db->lmf);
            continue;
        }
        last_list = -1;
        for (i = 0; i < NKEYS; i++)
            if (!strcmp(key, list_keys[i].key)) {
                vl_strlcpy((char *) cur + list_keys[i].off, val, list_keys[i].size);
                if (list_keys[i].list) last_list = i;
                break;
            }
        if (i == NKEYS && key[0]) {
            if (cur->other[0]) vl_strlcat(cur->other, "\n", sizeof cur->other);
            vl_strlcat(cur->other, key, sizeof cur->other);
            vl_strlcat(cur->other, ": ", sizeof cur->other);
            vl_strlcat(cur->other, val, sizeof cur->other);
        }
    }
    return count;
}

int vl_parse_show(const char *text, vl_db *db)
{
    char line[512];
    const char *t = text;
    int in_table = 0, count = 0, i;
    while ((t = next_line(t, line, sizeof line)) != 0) {
        char prod[VL_F], producer[VL_F];
        if (!in_table) {
            if (strncmp(line, "Product ", 8) == 0 && strstr(line, "Producer")) in_table = 1;
            continue;
        }
        if (sscanf(line, "%63s %63s", prod, producer) != 2) continue;
        count++;
        /* SHOW LICENSE names only the product: the active records are the ones
           loaded; a disabled one only if nothing active matches (disabled after
           it was loaded). */
        {
            int pass, hit = 0;
            for (pass = 0; pass < 2 && !hit; pass++)
                for (i = 0; i < db->n; i++) {
                    vl_lic *l = &db->l[i];
                    if (vl_strcasecmp(l->product, prod) || vl_strcasecmp(l->producer, producer)
                        || vl_lic_extinct(l) || (pass == 0 && !vl_lic_active(l)))
                        continue;
                    l->loaded = 1;
                    hit = 1;
                }
        }
    }
    return count;
}

void vl_first_message(const char *text, char *buf, size_t size)
{
    char line[512];
    const char *t = text;
    buf[0] = '\0';
    while ((t = next_line(t, line, sizeof line)) != 0) {
        char *p = line;
        while (*p == ' ') p++;
        if (p[0] == '%' && isalpha((unsigned char) p[1]) && strchr(p, '-')) {
            vl_strlcpy(buf, p, size);
            return;
        }
    }
}

int vl_lic_extinct(const vl_lic *l) { return !vl_strcasecmp(l->status, "Extinct"); }
int vl_lic_active(const vl_lic *l) { return !vl_strcasecmp(l->status, "Active"); }

const char *vl_lic_term(const vl_lic *l)
{
    if (l->mod_term[0]) return l->mod_term;
    if (l->term[0] && vl_strcasecmp(l->term, "(none)")) return l->term;
    return "";
}

long vl_lic_term_days(const vl_lic *l)
{
    long d;
    return vl_parse_date(vl_lic_term(l), &d) ? d : -1;
}

/* ---------- fields ---------- */

#define PF(name) offsetof(vl_pak, name), sizeof ((vl_pak *) 0)->name
const vl_field vl_pak_fields[] = {
    { "Product Name", 0, PF(product), VL_K_NAME, 1, "Exactly as on the PAK, e.g. VMS-WEBUI" },
    { "Issuer", "ISSUER", PF(issuer), VL_K_NAME, 1, "The company that issued the PAK, e.g. VSI" },
    { "Authorization", "AUTHORIZATION", PF(auth), VL_K_NAME, 1, "Authorization number, exactly as on the PAK" },
    { "Producer", "PRODUCER", PF(producer), VL_K_NAME, 1, "The company that owns the product, e.g. VSI" },
    { "Units", "UNITS", PF(units), VL_K_INT, 0, "Number of units, as on the PAK (0 if it says 0)" },
    { "Version", "VERSION", PF(version), VL_K_VERSION, 0, "Version limit as n.n; empty if the PAK has none" },
    { "Release Date", "RELEASE_DATE", PF(release), VL_K_DATE, 0, "dd-mmm-yyyy; empty if the PAK has none" },
    { "Termination Date", "TERMINATION_DATE", PF(term), VL_K_DATE, 0, "dd-mmm-yyyy; empty if the PAK has none" },
    { "Availability", "AVAILABILITY", PF(avail), VL_K_UNITCODE, 0, "A to I, CONSTANT=n, or empty" },
    { "Activity", "ACTIVITY", PF(activ), VL_K_UNITCODE, 0, "A to I, CONSTANT=n, or empty" },
    { "Options", "OPTIONS", PF(options), VL_K_OPTIONS, 0, "Keywords with commas: IA64,PPL  X86_64  MOD_UNITS,NO_SHARE" },
    { "Token", "TOKEN", PF(token), VL_K_TEXT, 0, "Only if the PAK has one" },
    { "Hardware ID", "HARDWARE_ID", PF(hwid), VL_K_TEXT, 0, "Only if the PAK has one, e.g. SOCKETS=2" },
    { "Checksum", "CHECKSUM", PF(checksum), VL_K_CHECKSUM, 1, "n-cccc-cccc-cccc-cccc, letters A to P" },
};
#undef PF
const int vl_pak_nfields = (int) (sizeof vl_pak_fields / sizeof vl_pak_fields[0]);

#define MF(name) offsetof(vl_mods, name), sizeof ((vl_mods *) 0)->name
const vl_field vl_mod_fields[] = {
    { "Units (modified)", "UNITS", MF(units), VL_K_INT, 0, "Empty: the PAK's units. Only for a PAK with MOD_UNITS" },
    { "Termination (mod.)", "TERMINATION_DATE", MF(term), VL_K_DATE, 0, "dd-mmm-yyyy, not after the PAK's; empty: the PAK's" },
    { "Comment", "COMMENT", MF(comment), VL_K_COMMENT, 0, "Up to 63 characters; kept in the history" },
    { "Selection weight", "SELECTION_WEIGHT", MF(weight), VL_K_WEIGHT, 0, "1 to 1000, higher is tried first; empty: default" },
    { "Include nodes", "INCLUDE", MF(include), VL_K_NODES, 0, "SCS node names, commas between: only these may load it" },
    { "Exclude nodes", "EXCLUDE", MF(exclude), VL_K_NODES, 0, "SCS node names that may not load it (not with Include)" },
    { "Reserve users", "RESERVE", MF(reserve), VL_K_NODES, 0, "User names, commas between: only they may use it" },
};
#undef MF
const int vl_mod_nfields = (int) (sizeof vl_mod_fields / sizeof vl_mod_fields[0]);

void vl_mods_from(const vl_lic *l, vl_mods *m)
{
    memset(m, 0, sizeof *m);
    vl_strlcpy(m->units, l->mod_units, sizeof m->units);
    vl_strlcpy(m->term, l->mod_term, sizeof m->term);
    vl_strlcpy(m->comment, l->comment, sizeof m->comment);
    if (strstr(l->weight, "Modified")) {
        int i = 0;
        while (isdigit((unsigned char) l->weight[i]) && i < (int) sizeof m->weight - 1) {
            m->weight[i] = l->weight[i]; i++;
        }
        m->weight[i] = '\0';
    }
    vl_strlcpy(m->include, l->include, sizeof m->include);
    vl_strlcpy(m->exclude, l->exclude, sizeof m->exclude);
    vl_strlcpy(m->reserve, l->reserve, sizeof m->reserve);
}

static int bad(char *err, size_t errsz, const char *why)
{
    vl_strlcpy(err, why, errsz);
    return 0;
}

/* DCL substitutes ''symbol' even inside quotes: never let two apostrophes through. */
static int dcl_safe(const char *s)
{
    for (; *s; s++) {
        unsigned char c = (unsigned char) *s;
        if (c < 32 || c == 127) return 0;
        if (c == '\'' && s[1] == '\'') return 0;
    }
    return 1;
}

static int name_char(int c)
{
    return isalnum(c) || c == '-' || c == '_' || c == '$' || c == '.' || c == ':' || c == '/'
           || c == '+';
}

static int all_digits(const char *s, int maxlen)
{
    int n = 0;
    for (; *s; s++, n++) if (!isdigit((unsigned char) *s)) return 0;
    return n > 0 && n <= maxlen;
}

int vl_check(int kind, int required, const char *v, char *err, size_t errsz)
{
    const char *p;
    if (!*v) return required ? bad(err, errsz, "This field is needed") : 1;
    switch (kind) {
    case VL_K_NAME:
        if (strlen(v) >= VL_F) return bad(err, errsz, "Too long");
        for (p = v; *p; p++)
            if (!name_char((unsigned char) *p))
                return bad(err, errsz, "Letters, digits and - _ $ . : / + only");
        return 1;
    case VL_K_INT:
        return all_digits(v, 9) ? 1 : bad(err, errsz, "A whole number");
    case VL_K_WEIGHT:
        if (!all_digits(v, 4) || atoi(v) > 1000) return bad(err, errsz, "A number from 0 to 1000");
        return 1;
    case VL_K_VERSION: {
        int dots = 0, digits = 0;
        for (p = v; *p; p++) {
            if (*p == '.') { if (!digits || dots++) return bad(err, errsz, "A version as n.n"); digits = 0; }
            else if (isdigit((unsigned char) *p)) digits++;
            else return bad(err, errsz, "A version as n.n");
        }
        return digits && strlen(v) < 16 ? 1 : bad(err, errsz, "A version as n.n");
    }
    case VL_K_DATE:
        return vl_parse_date(v, 0) ? 1 : bad(err, errsz, "A date as dd-mmm-yyyy, e.g. 31-DEC-2027");
    case VL_K_UNITCODE:
        if (strlen(v) == 1 && isalpha((unsigned char) v[0])) return 1;
        if (all_digits(v, 9)) return 1;
        if (vl_strncasecmp(v, "CONSTANT=", 9) == 0 && all_digits(v + 9, 9)) return 1;
        return bad(err, errsz, "A letter A to I, CONSTANT=n, or a number");
    case VL_K_OPTIONS: {
        int words = 0, in = 0;
        p = v;
        if (*p == '(') p++;
        for (; *p && *p != ')'; p++) {
            if (isalnum((unsigned char) *p) || *p == '_') { if (!in) words++; in = 1; }
            else if (*p == ',' || *p == ' ') in = 0;
            else return bad(err, errsz, "Option keywords with commas between");
        }
        if (*p == ')' && p[1]) return bad(err, errsz, "Option keywords with commas between");
        return words && strlen(v) < 160 ? 1 : bad(err, errsz, "Option keywords with commas between");
    }
    case VL_K_TEXT:
        if (strlen(v) >= VL_F) return bad(err, errsz, "Too long");
        return dcl_safe(v) ? 1 : bad(err, errsz, "No control characters or two apostrophes together");
    case VL_K_COMMENT:
        if (strlen(v) > 63) return bad(err, errsz, "At most 63 characters");
        return dcl_safe(v) ? 1 : bad(err, errsz, "No control characters or two apostrophes together");
    case VL_K_CHECKSUM: {
        int digits = 0, letters = 0;
        for (p = v; *p; p++) {
            int c = toupper((unsigned char) *p);
            if (c == '-') continue;
            if (isdigit(c) && !letters) digits++;
            else if (c >= 'A' && c <= 'P') letters++;
            else return bad(err, errsz, "n-cccc-cccc-cccc-cccc, letters A to P");
        }
        return digits && letters == 16 ? 1 : bad(err, errsz, "n-cccc-cccc-cccc-cccc: a number and 16 letters A to P");
    }
    case VL_K_NODES:
        if (strlen(v) >= VL_LST) return bad(err, errsz, "Too long");
        for (p = v; *p; p++)
            if (!(isalnum((unsigned char) *p) || *p == '_' || *p == '$' || *p == '-' || *p == ','
                  || *p == ' '))
                return bad(err, errsz, "Names with commas between (letters, digits, _ $ -)");
        return 1;
    }
    return 1;
}

int vl_check_db(const char *s)
{
    if (!*s || strlen(s) >= 256) return 0;
    for (; *s; s++) {
        unsigned char c = (unsigned char) *s;
        if (c <= 32 || c >= 127 || c == '"' || c == '\'' || c == '!' || c == '/') return 0;
    }
    return 1;
}

/* ---------- building commands ---------- */

typedef struct { char *buf; size_t size; int over; } sb;

static void sb_add(sb *b, const char *s)
{
    if (vl_strlcat(b->buf, s, b->size) >= b->size) b->over = 1;
}

/* A quoted DCL string; text upper-cased unless keep_case. */
static void sb_quote(sb *b, const char *s, int keep_case)
{
    char one[2] = { 0, 0 };
    sb_add(b, "\"");
    for (; *s; s++) {
        if (*s == '"') sb_add(b, "\"");
        one[0] = keep_case ? *s : (char) toupper((unsigned char) *s);
        sb_add(b, one);
    }
    sb_add(b, "\"");
}

static void sb_upper(sb *b, const char *s)
{
    char t[VL_LST + 8];
    vl_strlcpy(t, s, sizeof t);
    sb_add(b, vl_upper(t));
}

static void ident(sb *b, const vl_lic *l)
{
    sb_quote(b, l->product, 0);
    sb_add(b, "/ISSUER="); sb_quote(b, l->issuer, 0);
    sb_add(b, "/PRODUCER="); sb_quote(b, l->producer, 0);
    sb_add(b, "/AUTHORIZATION="); sb_quote(b, l->auth, 0);
}

static int ident_ok(const vl_lic *l, char *err, size_t errsz)
{
    if (!l->product[0] || !l->issuer[0] || !l->producer[0] || !l->auth[0])
        return bad(err, errsz, "The license is not fully identified (product, issuer, producer, authorization)");
    if (!dcl_safe(l->product) || !dcl_safe(l->issuer) || !dcl_safe(l->producer) || !dcl_safe(l->auth))
        return bad(err, errsz, "The license's names have characters VLMF will not pass to DCL");
    return 1;
}

int vl_build_register(const vl_pak *p, char cmds[][VL_CMD], int max, char *err, size_t errsz)
{
    sb b;
    int i;
    if (max < 1) return -1;
    for (i = 0; i < vl_pak_nfields; i++) {
        const vl_field *f = &vl_pak_fields[i];
        char e[120];
        if (!vl_check(f->kind, f->required, (const char *) p + f->off, e, sizeof e)) {
            snprintf(err, errsz, "%s: %s", f->label, e);
            return -1;
        }
    }
    cmds[0][0] = '\0';
    b.buf = cmds[0]; b.size = VL_CMD; b.over = 0;
    sb_add(&b, "REGISTER ");
    sb_quote(&b, p->product, 0);
    for (i = 1; i < vl_pak_nfields; i++) {
        const vl_field *f = &vl_pak_fields[i];
        const char *v = (const char *) p + f->off;
        if (!*v) continue;
        sb_add(&b, "/"); sb_add(&b, f->qual); sb_add(&b, "=");
        switch (f->kind) {
        case VL_K_NAME: case VL_K_CHECKSUM: sb_quote(&b, v, 0); break;
        case VL_K_TEXT: sb_quote(&b, v, f->off == offsetof(vl_pak, token)); break;
        case VL_K_OPTIONS: {
            char o[200], *q;
            const char *s = v;
            size_t n = 0;
            if (*s == '(') s++;
            for (; *s && *s != ')' && n + 1 < sizeof o; s++) if (*s != ' ') o[n++] = *s;
            o[n] = '\0';
            for (q = o; *q; q++) *q = (char) toupper((unsigned char) *q);
            sb_add(&b, "("); sb_add(&b, o); sb_add(&b, ")");
            break;
        }
        default: sb_upper(&b, v); break;
        }
    }
    if (b.over) { bad(err, errsz, "The command is too long"); return -1; }
    return 1;
}

/* Lists: split "A, b,C" into upper-case names. Returns the count. */
static int split_names(const char *s, char names[][32], int max)
{
    int n = 0;
    while (*s && n < max) {
        size_t k = 0;
        while (*s == ' ' || *s == ',') s++;
        if (!*s) break;
        while (*s && *s != ',' && *s != ' ') {
            if (k + 1 < 32) names[n][k++] = (char) toupper((unsigned char) *s);
            s++;
        }
        names[n][k] = '\0';
        if (k) {
            int j, dup = 0;
            for (j = 0; j < n; j++) if (!strcmp(names[j], names[n])) dup = 1;
            if (!dup) n++;
        }
    }
    return n;
}

#define MAXNAMES 400
static char old_names[MAXNAMES][32], new_names[MAXNAMES][32];

static int has(char names[][32], int n, const char *s)
{
    int i;
    for (i = 0; i < n; i++) if (!strcmp(names[i], s)) return 1;
    return 0;
}

/* One MODIFY per list change: /REMOVE what went, /ADD what came (D6). */
static int list_cmds(const vl_lic *l, const char *qual, const char *oldv, const char *newv,
                     char cmds[][VL_CMD], int *n, int max, char *err, size_t errsz)
{
    int no = split_names(oldv, old_names, MAXNAMES), nn = split_names(newv, new_names, MAXNAMES);
    int pass, i;
    for (pass = 0; pass < 2; pass++) {
        char (*from)[32] = pass ? new_names : old_names, (*other)[32] = pass ? old_names : new_names;
        int nf = pass ? nn : no, nother = pass ? no : nn, any = 0;
        sb b;
        for (i = 0; i < nf; i++) if (!has(other, nother, from[i])) any = 1;
        if (!any) continue;
        if (*n >= max) { bad(err, errsz, "Too many commands"); return 0; }
        cmds[*n][0] = '\0';
        b.buf = cmds[*n]; b.size = VL_CMD; b.over = 0;
        sb_add(&b, "MODIFY ");
        ident(&b, l);
        sb_add(&b, "/"); sb_add(&b, qual); sb_add(&b, "=(");
        any = 0;
        for (i = 0; i < nf; i++)
            if (!has(other, nother, from[i])) {
                if (any++) sb_add(&b, ",");
                sb_add(&b, from[i]);
            }
        sb_add(&b, pass ? ")/ADD" : ")/REMOVE");
        if (b.over) { bad(err, errsz, "The list is too long for one command"); return 0; }
        (*n)++;
    }
    return 1;
}

static int same_list(const char *a, const char *b)
{
    int na = split_names(a, old_names, MAXNAMES), nb = split_names(b, new_names, MAXNAMES), i;
    if (na != nb) return 0;
    for (i = 0; i < na; i++) if (!has(new_names, nb, old_names[i])) return 0;
    return 1;
}

int vl_build_modify(const vl_lic *l, const vl_mods *m, char cmds[][VL_CMD], int max,
                    char *err, size_t errsz)
{
    vl_mods cur;
    sb b;
    int i, n = 0, any = 0;
    if (!ident_ok(l, err, errsz)) return -1;
    if (vl_lic_extinct(l)) { bad(err, errsz, "A history record cannot be changed"); return -1; }
    for (i = 0; i < vl_mod_nfields; i++) {
        const vl_field *f = &vl_mod_fields[i];
        char e[120];
        if (!vl_check(f->kind, f->required, (const char *) m + f->off, e, sizeof e)) {
            snprintf(err, errsz, "%s: %s", f->label, e);
            return -1;
        }
    }
    if (split_names(m->include, new_names, MAXNAMES) && split_names(m->exclude, new_names, MAXNAMES)) {
        bad(err, errsz, "A license has an include list or an exclude list, not both");
        return -1;
    }
    vl_mods_from(l, &cur);
    if (m->term[0]) {
        long nd, pd;
        vl_parse_date(m->term, &nd);
        if (vl_parse_date(l->term, &pd) && nd > pd) {
            snprintf(err, errsz, "Termination (mod.): not after the PAK's date, %s", l->term);
            return -1;
        }
    }
    if (!m->term[0] && cur.term[0] && !vl_parse_date(l->term, 0)) {
        bad(err, errsz, "Termination (mod.): the PAK has no date to go back to; give one");
        return -1;
    }

    cmds[0][0] = '\0';
    b.buf = cmds[0]; b.size = VL_CMD; b.over = 0;
    sb_add(&b, "MODIFY ");
    ident(&b, l);
    if (strcmp(m->units, cur.units) && !(atol(m->units) == atol(cur.units) && m->units[0] && cur.units[0])) {
        sb_add(&b, "/UNITS="); sb_add(&b, m->units[0] ? m->units : "0"); any = 1;
    }
    if (vl_strcasecmp(m->term, cur.term)) {
        char t[24];
        vl_strlcpy(t, m->term[0] ? m->term : l->term, sizeof t);
        sb_add(&b, "/TERMINATION_DATE="); sb_upper(&b, vl_trim(t)); any = 1;
    }
    if (strcmp(m->comment, cur.comment)) {
        sb_add(&b, "/COMMENT="); sb_quote(&b, m->comment, 1); any = 1;
    }
    if (atoi(m->weight) != atoi(cur.weight)) {
        char w[16];
        sprintf(w, "%d", atoi(m->weight));
        sb_add(&b, "/SELECTION_WEIGHT="); sb_add(&b, w); any = 1;
    }
    if (b.over) { bad(err, errsz, "The command is too long"); return -1; }
    if (any) n = 1;
    /* Include and exclude exclude each other: take away first, then add. */
    if (!same_list(cur.include, m->include) || !same_list(cur.exclude, m->exclude)) {
        int keep = n;
        if (!list_cmds(l, "INCLUDE", cur.include, "", cmds, &n, max, err, errsz)) return -1;
        if (!list_cmds(l, "EXCLUDE", cur.exclude, "", cmds, &n, max, err, errsz)) return -1;
        if (!list_cmds(l, "INCLUDE", "", m->include, cmds, &n, max, err, errsz)) return -1;
        if (!list_cmds(l, "EXCLUDE", "", m->exclude, cmds, &n, max, err, errsz)) return -1;
        /* Removing a list and adding the same names back is no change at all. */
        if (same_list(cur.include, m->include) && same_list(cur.exclude, m->exclude)) n = keep;
    }
    if (!same_list(cur.reserve, m->reserve))
        if (!list_cmds(l, "RESERVE", cur.reserve, m->reserve, cmds, &n, max, err, errsz)) return -1;
    return n;
}

int vl_build_simple(const char *verb, const vl_lic *l, char cmds[][VL_CMD], int max,
                    char *err, size_t errsz)
{
    sb b;
    if (max < 1 || !ident_ok(l, err, errsz)) return -1;
    if (vl_lic_extinct(l)) { bad(err, errsz, "A history record cannot be changed"); return -1; }
    cmds[0][0] = '\0';
    b.buf = cmds[0]; b.size = VL_CMD; b.over = 0;
    sb_add(&b, verb); sb_add(&b, " ");
    ident(&b, l);
    return 1;
}

/* ---------- procedures ---------- */

/* Write cmd as DCL lines of at most 200 characters, breaking before a '/' or
   after a ',' that is not inside quotes. */
static void emit_command(sb *b, const char *prefix, const char *cmd)
{
    char line[256];
    size_t len = strlen(cmd), start = 0;
    sb_add(b, prefix);
    while (start < len) {
        size_t i, brk = 0, lim = 190;
        int inq = 0;
        if (len - start <= lim) {
            sb_add(b, cmd + start);
            break;
        }
        for (i = start; i < len && i - start < lim; i++) {
            if (cmd[i] == '"') inq = !inq;
            if (inq) continue;
            if (cmd[i] == '/' && i > start) brk = i;
            if (cmd[i] == ',') brk = i + 1;
        }
        if (!brk) brk = start + lim;       /* one quoted value longer than a line */
        memcpy(line, cmd + start, brk - start);
        line[brk - start] = '\0';
        sb_add(b, line);
        sb_add(b, " -\n  ");
        start = brk;
    }
    sb_add(b, "\n");
}

int vl_proc_list(const char *db, int history, char *out, size_t size)
{
    sb b;
    if (!vl_check_db(db)) return 0;
    out[0] = '\0';
    b.buf = out; b.size = size; b.over = 0;
    sb_add(&b, "$ set noon\n$ license list/full/selection_weight");
    if (history) sb_add(&b, "/history");
    sb_add(&b, "/database="); sb_add(&b, db); sb_add(&b, "\n");
    sb_add(&b, "$ st = $status\n$ write sys$output \"" VL_SHOW_MARK "\"\n");
    sb_add(&b, "$ show license\n$ exit st\n");
    return !b.over;
}

int vl_proc_change(const char *db, int backup, char cmds[][VL_CMD], int n, char *out, size_t size)
{
    sb b;
    int i;
    char num[16];
    if (!vl_check_db(db)) return 0;
    out[0] = '\0';
    b.buf = out; b.size = size; b.over = 0;
    sb_add(&b, "$ set noon\n$ st = 1\n");
    if (backup) {
        sb_add(&b, "$ bk = f$trnlnm(\"VLMF$BACKUP\")\n");
        sb_add(&b, "$ if bk .eqs. \"\" then bk = \"SYS$LOGIN:VLMF_BACKUP.LDB\"\n");
        sb_add(&b, "$ copy/nolog "); sb_add(&b, db); sb_add(&b, " 'bk'\n");
        sb_add(&b, "$ st = $status\n");
        sb_add(&b, "$ if .not. st then write sys$output \"==== VLMF BACKUP FAILED\"\n");
        sb_add(&b, "$ if .not. st then goto done\n");
        sb_add(&b, "$ write sys$output \"==== VLMF BACKUP \", f$search(bk)\n");
    }
    for (i = 0; i < n; i++) {
        sprintf(num, "%d", i + 1);
        sb_add(&b, "$ write sys$output \"==== VLMF COMMAND "); sb_add(&b, num); sb_add(&b, "\"\n");
        {
            char full[VL_CMD + 300];
            snprintf(full, sizeof full, "%s/DATABASE=%s", cmds[i], db);
            emit_command(&b, "$ license ", full);
        }
        sb_add(&b, "$ st = $status\n$ if .not. st then goto done\n");
    }
    sb_add(&b, "$ done:\n$ write sys$output \"==== VLMF STATUS \", st\n$ exit st\n");
    return !b.over;
}

/* ---------- reading commands ---------- */

static const char *qual_names[] = {
    "ISSUER", "AUTHORIZATION", "PRODUCER", "UNITS", "VERSION", "RELEASE_DATE",
    "TERMINATION_DATE", "AVAILABILITY", "ACTIVITY", "OPTIONS", "TOKEN", "HARDWARE_ID",
    "CHECKSUM", "COMMENT", "INCLUDE", "EXCLUDE", "RESERVE", "REMOVE", "ADD",
    "SELECTION_WEIGHT", "DATABASE", "ALL", "LOG", "FULL", "HISTORY", "BRIEF", "OUTPUT",
    "PROCEDURE", "STATUS", "NO_SHARE", "COMBINE", "VIRTUAL", 0
};
static const char *verbs[] = { "REGISTER", "MODIFY", "DELETE", "ENABLE", "DISABLE", "LOAD",
                               "LIST", "ISSUE", "UNLOAD", 0 };

/* Full name for an abbreviation (4 letters or more, or the whole name). */
static const char *full_name(const char **names, const char *w)
{
    size_t n = strlen(w);
    int i, found = -1;
    for (i = 0; names[i]; i++) {
        if (!vl_strcasecmp(names[i], w)) return names[i];
        if (n >= 4 && !vl_strncasecmp(names[i], w, n)) {
            if (found >= 0) return 0;
            found = i;
        }
    }
    return found >= 0 ? names[found] : 0;
}

static const char *read_word(const char *s, char *w, size_t size)
{
    size_t n = 0;
    while (isalnum((unsigned char) *s) || *s == '_' || *s == '$') {
        if (n + 1 < size) w[n++] = *s;
        s++;
    }
    w[n] = '\0';
    return s;
}

/* A value: "quoted", (list), or a plain token up to '/', blank or end. */
static const char *read_value(const char *s, char *v, size_t size)
{
    size_t n = 0;
    int depth = 0, inq = 0;
#define PUT(c) do { if (n + 1 < size) v[n++] = (char) (c); } while (0)
    for (; *s; s++) {
        if (inq) {
            if (*s == '"') {
                if (s[1] == '"') { PUT('"'); s++; continue; }
                inq = 0;
                continue;
            }
            PUT(*s);
            continue;
        }
        if (*s == '"') { inq = 1; continue; }
        if (*s == '(') depth++;
        if (*s == ')') depth--;
        if (depth <= 0 && (*s == '/' || *s == ' ' || *s == '\t') && *s != ')') {
            if (depth < 0) depth = 0;
            break;
        }
        PUT(toupper((unsigned char) *s));
        if (depth == 0 && *s == ')') { s++; break; }
    }
#undef PUT
    v[n] = '\0';
    return s;
}

int vl_cmd_parse(const char *line, vl_cmd *c)
{
    char w[64];
    const char *s = line, *v;
    memset(c, 0, sizeof *c);
    while (*s == ' ' || *s == '\t' || *s == '$') s++;
    s = read_word(s, w, sizeof w);
    if (strlen(w) < 4 || vl_strncasecmp(w, "LICENSE", strlen(w)) != 0) return 0;
    while (*s == ' ' || *s == '\t') s++;
    s = read_word(s, w, sizeof w);
    if (!(v = full_name(verbs, w))) return 0;
    vl_strlcpy(c->verb, v, sizeof c->verb);
    for (;;) {
        while (*s == ' ' || *s == '\t') s++;
        if (!*s) break;
        if (*s == '/') {
            const char *q;
            int neg = 0;
            s = read_word(s + 1, w, sizeof w);
            q = full_name(qual_names, w);
            if (!q && vl_strncasecmp(w, "NO", 2) == 0 && (q = full_name(qual_names, w + 2))) neg = 1;
            if (!q) return 0;
            if (c->nq >= VL_MAXQ) return 0;
            vl_strlcpy(c->q[c->nq].name, q, sizeof c->q[0].name);
            c->q[c->nq].neg = neg;
            if (*s == '=') s = read_value(s + 1, c->q[c->nq].value, sizeof c->q[0].value);
            c->nq++;
        } else {
            char p[VL_F * 2];
            s = read_value(s, p, sizeof p);
            if (!p[0]) return 0;
            if (c->param[0]) vl_strlcat(c->param, ",", sizeof c->param);
            vl_strlcat(c->param, p, sizeof c->param);
        }
    }
    return 1;
}

const char *vl_cmd_value(const vl_cmd *c, const char *name)
{
    int i;
    for (i = c->nq - 1; i >= 0; i--)
        if (!strcmp(c->q[i].name, name) && !c->q[i].neg) return c->q[i].value;
    return 0;
}

void vl_dcl_lines(const char *text, vl_line_fn fn, void *ctx)
{
    static char line[8192], joined[8192];
    const char *t = text;
    int cont = 0;
    joined[0] = '\0';
    while ((t = next_line(t, line, sizeof line)) != 0) {
        char *p = line;
        int inq = 0;
        size_t n;
        while (*p == ' ' || *p == '\t') p++;
        if (!cont) {
            if (*p != '$') continue;          /* data lines and listings are not commands */
            p++;
            while (*p == ' ' || *p == '\t') p++;
        }
        {   /* a comment ends the line, outside quotes */
            char *q;
            for (q = p; *q; q++) {
                if (*q == '"') inq = !inq;
                if (*q == '!' && !inq) { *q = '\0'; break; }
            }
        }
        vl_trim(p);
        n = strlen(p);
        cont = n && p[n - 1] == '-';
        if (cont) { p[n - 1] = '\0'; vl_trim(p); }
        vl_strlcat(joined, p, sizeof joined);
        if (cont) { vl_strlcat(joined, " ", sizeof joined); continue; }
        if (joined[0]) fn(ctx, joined);
        joined[0] = '\0';
    }
    if (joined[0]) fn(ctx, joined);
}

typedef struct { vl_pak *out; int max, n; } import_ctx;

static void strip_parens(char *s)
{
    size_t n = strlen(s);
    if (n >= 2 && s[0] == '(' && s[n - 1] == ')') { memmove(s, s + 1, n - 2); s[n - 2] = '\0'; }
}

static void import_cmd(void *vctx, const char *line)
{
    import_ctx *x = (import_ctx *) vctx;
    vl_cmd c;
    vl_pak *p;
    int i;
    if (x->n >= x->max || !vl_cmd_parse(line, &c) || strcmp(c.verb, "REGISTER")) return;
    p = &x->out[x->n++];
    memset(p, 0, sizeof *p);
    vl_strlcpy(p->product, c.param, sizeof p->product);
    for (i = 1; i < vl_pak_nfields; i++) {
        const char *v = vl_cmd_value(&c, vl_pak_fields[i].qual);
        if (v) vl_strlcpy((char *) p + vl_pak_fields[i].off, v, vl_pak_fields[i].size);
    }
    strip_parens(p->options);
}

static const struct { const char *key; size_t off, size; } pak_keys[] = {
#define PK(k, name) { k, offsetof(vl_pak, name), sizeof ((vl_pak *) 0)->name }
    PK("Issuer", issuer), PK("Authorization", auth), PK("Authorization Number", auth),
    PK("Product Name", product), PK("Product", product), PK("Producer", producer),
    PK("Units", units), PK("Number of Units", units), PK("Version", version),
    PK("Release Date", release), PK("Product Release Date", release),
    PK("Termination Date", term), PK("PAK Termination Date", term),
    PK("Key Termination Date", term), PK("Availability", avail),
    PK("Availability Table Code", avail), PK("Activity", activ),
    PK("Activity Table Code", activ), PK("Options", options), PK("Key Options", options),
    PK("Token", token), PK("Product Token", token), PK("Hardware ID", hwid),
    PK("Hardware-Id", hwid), PK("Checksum", checksum),
#undef PK
};

int vl_pak_import(const char *text, vl_pak *out, int max)
{
    import_ctx x;
    char line[1024];
    const char *t = text;
    vl_pak *cur = 0;
    x.out = out; x.max = max; x.n = 0;
    vl_dcl_lines(text, import_cmd, &x);
    if (x.n) return x.n;
    /* A PAK listing: "Key:  value" lines, each PAK starting with Issuer. */
    while ((t = next_line(t, line, sizeof line)) != 0) {
        char *colon = strchr(line, ':'), *val;
        int i;
        if (!colon) continue;
        *colon = '\0';
        vl_trim(line);
        val = vl_trim(colon + 1);
        if (!vl_strcasecmp(val, "(none)")) val[0] = '\0';
        for (i = 0; i < (int) (sizeof pak_keys / sizeof pak_keys[0]); i++)
            if (!vl_strcasecmp(line, pak_keys[i].key)) break;
        if (i == (int) (sizeof pak_keys / sizeof pak_keys[0])) continue;
        if (!vl_strcasecmp(line, "Issuer") || !cur) {
            if (x.n >= max) break;
            cur = &out[x.n++];
            memset(cur, 0, sizeof *cur);
        }
        vl_strlcpy((char *) cur + pak_keys[i].off, val, pak_keys[i].size);
    }
    for (cur = out; cur < out + x.n; cur++) {
        char o[160], *s, *d;
        strip_parens(cur->options);
        for (s = cur->options, d = o; *s && d < o + sizeof o - 1; s++) if (*s != ' ') *d++ = *s;
        *d = '\0';
        vl_strlcpy(cur->options, o, sizeof cur->options);
        vl_trim(cur->activ); vl_trim(cur->avail);
    }
    /* A listing without a product or checksum is not a PAK. */
    {
        int i, k = 0;
        for (i = 0; i < x.n; i++) if (out[i].product[0]) out[k++] = out[i];
        x.n = k;
    }
    return x.n;
}
