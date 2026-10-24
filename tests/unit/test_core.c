/* test_core.c - the pure core: parsing LMF's output, checking fields, building
   LICENSE commands and procedures, reading PAK files. The sample text is in the
   exact layout LMF V2.0 printed on both nodes (NOTES N1, N3); the license data
   in it is made up. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vl_lic.h"
#include "vl_util.h"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECKS(a, b) do { checks++; if (strcmp((a), (b))) { fails++; \
    printf("FAIL %s:%d: %s\n   got  [%s]\n   want [%s]\n", __FILE__, __LINE__, #a, (a), (b)); } } while (0)

static const char *list_text =
"License Management Facility  V2.0\n"
"  \n"
"License Database File:       SYS$COMMON:[SYSEXE]LMF$LICENSE.LDB;1\n"
"Created on:                  20-APR-2023\n"
"Created by user:             SYSTEM\n"
"Created by LMF Version:      V2.0\n"
"  \n"
"-----------------------------------\n"
"Issuer:                      VSI\n"
"Authorization:               TEST-AUTH-0001\n"
"Product Name:                BASIC\n"
"Producer:                    VSI\n"
"Units:                       0\n"
"Modified Units:              5\n"
"Version:                     0.0\n"
"Release Date:                (none)\n"
"PAK Termination Date:        30-APR-2027\n"
"Modified Termination Date:    1-JAN-2027\n"
"Availability:                0\n"
"Activity:                     000000001\n"
"Options:                     NO_SHARE, COMBINE, IA64\n"
"Hardware ID:\n"
"Selection Weight:            50 (Modified Weight)\n"
"  \n"
"Revision Level:              7\n"
"Status:                      Active\n"
"Command:                     MODIFY\n"
"Comment:                     say \"hi\" now\n"
"Modified by user:            IAIN\n"
"Modified on:                 24-OCT-2026 16:43:59.08\n"
"Exclude:                     NODEC\n"
"Reserve:                     SMITH,JONES\n"
"Future Field:                something new\n"
"  \n"
"-----------------------------------\n"
"Issuer:                      VSI\n"
"Authorization:               TEST-AUTH-0002\n"
"Product Name:                C\n"
"Producer:                    VSI\n"
"Units:                       0\n"
"Version:                     0.0\n"
"Release Date:                (none)\n"
"PAK Termination Date:        (none)\n"
"Options:                     IA64, PPL\n"
"Hardware ID:\n"
"  \n"
"Revision Level:              1\n"
"Status:                      Disabled\n"
"Command:                     DISABLE\n"
"Modified by user:            IAIN\n"
"Modified on:                 24-OCT-2026 16:43:58.78\n"
"Include:                     NODEA,NODEB\n"
"  \n"
"-----------------------------------\n"
"Issuer:                      VSI\n"
"Authorization:               TEST-AUTH-0001\n"
"Product Name:                BASIC\n"
"Producer:                    VSI\n"
"Units:                       0\n"
"Version:                     0.0\n"
"Release Date:                (none)\n"
"PAK Termination Date:        30-APR-2027\n"
"Options:                     IA64\n"
"Hardware ID:\n"
"  \n"
"Revision Level:              1\n"
"Status:                      Extinct\n"
"Command:                     REGISTER\n"
"Modified by user:            IAIN\n"
"Modified on:                 24-OCT-2026 16:43:58.75\n"
"[End Of List]\n";

static const char *show_text =
"\n"
"Active licenses on node MYI64:\n"
"\n"
"------- Product ID --------    ---- Rating ----- -- Version --\n"
"Product            Producer    Units PCL   Activ Version Release    Termination\n"
"BASIC              VSI             0  0     1      0.0  (none)      30-APR-2027 \n"
"VMS-WEBUI          VSI             0  0     1      0.0  (none)      30-APR-2027 \n"
"C                  VSI             0  0     1      0.0  (none)      30-APR-2027 \n";

static void test_util(void)
{
    long d;
    char b[64];
    vl_when w = { 2026, 10, 24, 9, 5 };
    CHECK(vl_parse_date("30-APR-2027", &d) && d == vl_days(2027, 4, 30));
    CHECK(vl_parse_date(" 1-jan-2027", &d) && d == vl_days(2027, 1, 1));
    CHECK(!vl_parse_date("31-APR-2027", 0));
    CHECK(!vl_parse_date("29-FEB-2027", 0));
    CHECK(vl_parse_date("29-FEB-2028", 0));
    CHECK(!vl_parse_date("99-XYZ-2030", 0));
    CHECK(!vl_parse_date("(none)", 0));
    CHECK(!vl_parse_date("1-JAN-27", 0));
    CHECK(vl_days(1970, 1, 1) == 0);
    CHECKS(vl_fmt_when(&w, b, sizeof b), "24-OCT-2026 09:05");
    CHECK(vl_match("*", "BASIC"));
    CHECK(vl_match("vms*", "VMS-WEBUI"));
    CHECK(vl_match("C", "c"));
    CHECK(!vl_match("C", "COBOL"));
    CHECK(vl_match("C%%%%", "COBOL"));
    CHECK(!vl_match("C%%%", "COBOL"));
}

static void test_parse(void)
{
    vl_db db;
    char m[200];
    vl_db_init(&db);
    CHECK(vl_parse_list(list_text, &db) == 3);
    CHECKS(db.file, "SYS$COMMON:[SYSEXE]LMF$LICENSE.LDB;1");
    CHECKS(db.created, "20-APR-2023");
    CHECKS(db.l[0].product, "BASIC");
    CHECKS(db.l[0].auth, "TEST-AUTH-0001");
    CHECKS(db.l[0].mod_units, "5");
    CHECKS(db.l[0].mod_term, "1-JAN-2027");
    CHECKS(db.l[0].activ, "000000001");
    CHECKS(db.l[0].hwid, "");
    CHECKS(db.l[0].weight, "50 (Modified Weight)");
    CHECKS(db.l[0].comment, "say \"hi\" now");
    CHECKS(db.l[0].exclude, "NODEC");
    CHECKS(db.l[0].reserve, "SMITH,JONES");
    CHECKS(db.l[0].other, "Future Field: something new");
    CHECKS(vl_lic_term(&db.l[0]), "1-JAN-2027");
    CHECK(vl_lic_term_days(&db.l[0]) == vl_days(2027, 1, 1));
    CHECKS(db.l[1].include, "NODEA,NODEB");
    CHECKS(vl_lic_term(&db.l[1]), "");
    CHECK(vl_lic_term_days(&db.l[1]) == -1);
    CHECK(vl_lic_active(&db.l[0]) && !vl_lic_active(&db.l[1]));
    CHECK(vl_lic_extinct(&db.l[2]));
    CHECK(vl_parse_show(show_text, &db) == 3);
    /* C's only record is disabled: loaded before it was disabled */
    CHECK(db.l[0].loaded && db.l[1].loaded && !db.l[2].loaded);
    db.l[0].loaded = 0;
    strcpy(db.l[2].status, "Disabled");          /* a disabled BASIC beside the active one */
    vl_parse_show(show_text, &db);
    CHECK(db.l[0].loaded && !db.l[2].loaded);
    vl_first_message("License Management\n%LICENSE-W-AMBIG, information provided was ambiguous\n", m, sizeof m);
    CHECKS(m, "%LICENSE-W-AMBIG, information provided was ambiguous");
    vl_first_message("nothing\n", m, sizeof m);
    CHECKS(m, "");
    vl_db_free(&db);
}

static void test_check(void)
{
    char e[200];
    CHECK(vl_check(VL_K_CHECKSUM, 1, "2-ABCD-EFGH-IJKL-MNOP", e, sizeof e));
    CHECK(vl_check(VL_K_CHECKSUM, 1, "2abcdefghijklmnop", e, sizeof e));
    CHECK(!vl_check(VL_K_CHECKSUM, 1, "2-ABCD-EFGH-IJKL-MNOZ", e, sizeof e));
    CHECK(!vl_check(VL_K_CHECKSUM, 1, "2-ABCD-EFGH-IJKL", e, sizeof e));
    CHECK(!vl_check(VL_K_CHECKSUM, 1, "", e, sizeof e));
    CHECKS(e, "This field is needed");
    CHECK(vl_check(VL_K_NAME, 1, "TEST-VMSSOFTWARE-D200001010000", e, sizeof e));
    CHECK(!vl_check(VL_K_NAME, 1, "TWO WORDS", e, sizeof e));
    CHECK(!vl_check(VL_K_NAME, 1, "QUOTE\"", e, sizeof e));
    CHECK(vl_check(VL_K_UNITCODE, 0, "CONSTANT=1", e, sizeof e));
    CHECK(vl_check(VL_K_UNITCODE, 0, "F", e, sizeof e));
    CHECK(vl_check(VL_K_UNITCODE, 0, "0", e, sizeof e));
    CHECK(!vl_check(VL_K_UNITCODE, 0, "CONSTANT=", e, sizeof e));
    CHECK(vl_check(VL_K_OPTIONS, 0, "(IA64,PPL)", e, sizeof e));
    CHECK(vl_check(VL_K_OPTIONS, 0, "MOD_UNITS, NO_SHARE", e, sizeof e));
    CHECK(!vl_check(VL_K_OPTIONS, 0, "IA64;PPL", e, sizeof e));
    CHECK(vl_check(VL_K_VERSION, 0, "8.4", e, sizeof e));
    CHECK(!vl_check(VL_K_VERSION, 0, "8..4", e, sizeof e));
    CHECK(vl_check(VL_K_COMMENT, 0, "it's me, \"Iain\"", e, sizeof e));
    CHECK(!vl_check(VL_K_COMMENT, 0, "a ''SYMBOL' here", e, sizeof e));
    CHECK(!vl_check(VL_K_COMMENT, 0, "0123456789012345678901234567890123456789012345678901234567890123", e, sizeof e));
    CHECK(vl_check(VL_K_NODES, 0, "NODEA, nodeb,NODE_C", e, sizeof e));
    CHECK(!vl_check(VL_K_NODES, 0, "NODEA;NODEB", e, sizeof e));
    CHECK(vl_check(VL_K_WEIGHT, 0, "1000", e, sizeof e));
    CHECK(!vl_check(VL_K_WEIGHT, 0, "1001", e, sizeof e));
    CHECK(vl_check_db("LMF$LICENSE"));
    CHECK(vl_check_db("DKA0:[IAIN.VMS_VLMF.SCRATCH]REAL.LDB"));
    CHECK(!vl_check_db("X.LDB\" /LOG"));
    CHECK(!vl_check_db("A'B"));
    CHECK(!vl_check_db("A.LDB/DATABASE=B"));
}

static vl_lic sample(void)
{
    vl_db db;
    vl_lic l;
    vl_db_init(&db);
    vl_parse_list(list_text, &db);
    l = db.l[0];
    vl_db_free(&db);
    return l;
}

#define IDENT "\"BASIC\"/ISSUER=\"VSI\"/PRODUCER=\"VSI\"/AUTHORIZATION=\"TEST-AUTH-0001\""

static void test_build(void)
{
    static char cmds[8][VL_CMD];
    char e[200];
    vl_pak p;
    vl_lic l = sample();
    vl_mods m;
    int n;

    memset(&p, 0, sizeof p);
    strcpy(p.product, "basic"); strcpy(p.issuer, "VSI"); strcpy(p.auth, "TEST-AUTH-0001");
    strcpy(p.producer, "VSI"); strcpy(p.units, "0"); strcpy(p.term, "30-apr-2027");
    strcpy(p.activ, "CONSTANT=1"); strcpy(p.options, "(ia64, ppl)");
    strcpy(p.checksum, "2-abcd-efgh-ijkl-mnop");
    n = vl_build_register(&p, cmds, 8, e, sizeof e);
    CHECK(n == 1);
    CHECKS(cmds[0], "REGISTER \"BASIC\"/ISSUER=\"VSI\"/AUTHORIZATION=\"TEST-AUTH-0001\"/PRODUCER=\"VSI\""
                    "/UNITS=0/TERMINATION_DATE=30-APR-2027/ACTIVITY=CONSTANT=1/OPTIONS=(IA64,PPL)"
                    "/CHECKSUM=\"2-ABCD-EFGH-IJKL-MNOP\"");
    strcpy(p.checksum, "");
    CHECK(vl_build_register(&p, cmds, 8, e, sizeof e) == -1);
    CHECKS(e, "Checksum: This field is needed");
    strcpy(p.checksum, "2-ABCD-EFGH-IJKL-MNOP"); strcpy(p.release, "31-FEB-2020");
    CHECK(vl_build_register(&p, cmds, 8, e, sizeof e) == -1);
    CHECKS(e, "Release Date: A date as dd-mmm-yyyy, e.g. 31-DEC-2027");

    /* nothing changed */
    vl_mods_from(&l, &m);
    CHECKS(m.weight, "50");
    CHECK(vl_build_modify(&l, &m, cmds, 8, e, sizeof e) == 0);
    /* comment with quotes; units back to the PAK's; termination back to the PAK's */
    strcpy(m.comment, "it's \"new\"");
    m.units[0] = '\0';
    m.term[0] = '\0';
    m.weight[0] = '\0';
    n = vl_build_modify(&l, &m, cmds, 8, e, sizeof e);
    CHECK(n == 1);
    CHECKS(cmds[0], "MODIFY " IDENT "/UNITS=0/TERMINATION_DATE=30-APR-2027/COMMENT=\"it's \"\"new\"\"\"/SELECTION_WEIGHT=0");
    /* a later termination than the PAK's */
    vl_mods_from(&l, &m);
    strcpy(m.term, "1-MAY-2027");
    CHECK(vl_build_modify(&l, &m, cmds, 8, e, sizeof e) == -1);
    CHECKS(e, "Termination (mod.): not after the PAK's date, 30-APR-2027");
    /* exclude -> include, reserve changed */
    vl_mods_from(&l, &m);
    strcpy(m.exclude, "");
    strcpy(m.include, "nodea");
    strcpy(m.reserve, "JONES, BROWN");
    n = vl_build_modify(&l, &m, cmds, 8, e, sizeof e);
    CHECK(n == 4);
    CHECKS(cmds[0], "MODIFY " IDENT "/EXCLUDE=(NODEC)/REMOVE");
    CHECKS(cmds[1], "MODIFY " IDENT "/INCLUDE=(NODEA)/ADD");
    CHECKS(cmds[2], "MODIFY " IDENT "/RESERVE=(SMITH)/REMOVE");
    CHECKS(cmds[3], "MODIFY " IDENT "/RESERVE=(BROWN)/ADD");
    /* both lists */
    vl_mods_from(&l, &m);
    strcpy(m.include, "NODEA");
    CHECK(vl_build_modify(&l, &m, cmds, 8, e, sizeof e) == -1);
    /* the same list, written differently: no change */
    vl_mods_from(&l, &m);
    strcpy(m.reserve, "jones , smith");
    CHECK(vl_build_modify(&l, &m, cmds, 8, e, sizeof e) == 0);
    /* a history record */
    strcpy(l.status, "Extinct");
    CHECK(vl_build_simple("DELETE", &l, cmds, 8, e, sizeof e) == -1);
    strcpy(l.status, "Active");
    CHECK(vl_build_simple("DELETE", &l, cmds, 8, e, sizeof e) == 1);
    CHECKS(cmds[0], "DELETE " IDENT);
    /* a name with two apostrophes never reaches DCL */
    strcpy(l.auth, "A''B");
    CHECK(vl_build_simple("DELETE", &l, cmds, 8, e, sizeof e) == -1);
}

typedef struct { char lines[64][VL_CMD + 300]; int n; } collect;
static void collect_line(void *ctx, const char *line)
{
    collect *c = (collect *) ctx;
    if (c->n < 64) strcpy(c->lines[c->n++], line);
}

/* blanks outside quotes do not matter to DCL */
static void unblank(char *s)
{
    char *d = s;
    int q = 0;
    for (; *s; s++) { if (*s == '"') q = !q; if (q || *s != ' ') *d++ = *s; }
    *d = '\0';
}

static void test_proc(void)
{
    static char proc[32768], cmds[8][VL_CMD];
    static collect c;
    vl_lic l = sample();
    vl_mods m;
    vl_cmd pc;
    char e[200], *p;
    int i, n, longest = 0;

    CHECK(vl_proc_list("LMF$LICENSE", 1, proc, sizeof proc));
    CHECK(strstr(proc, "$ license list/full/selection_weight/history/database=LMF$LICENSE\n") != 0);
    CHECK(strstr(proc, VL_SHOW_MARK) != 0);
    CHECK(!vl_proc_list("A\"B", 0, proc, sizeof proc));

    /* a long reserve list is split into lines and joins back to the same command */
    vl_mods_from(&l, &m);
    m.reserve[0] = '\0';
    for (i = 0; i < 60; i++) {
        char u[16];
        sprintf(u, "%sUSER%02d", i ? "," : "", i);
        strcat(m.reserve, u);
    }
    strcpy(m.comment, "a/b, c");
    n = vl_build_modify(&l, &m, cmds, 8, e, sizeof e);
    CHECK(n == 3);
    CHECK(vl_proc_change("DKA0:[X]Y.LDB", 1, cmds, n, proc, sizeof proc));
    for (p = proc; *p; ) {
        char *nl = strchr(p, '\n');
        int len = nl ? (int) (nl - p) : (int) strlen(p);
        if (len > longest) longest = len;
        p += len + (nl ? 1 : 0);
    }
    CHECK(longest <= 200);
    CHECK(strstr(proc, "$ copy/nolog DKA0:[X]Y.LDB 'bk'\n") != 0);
    c.n = 0;
    vl_dcl_lines(proc, collect_line, &c);
    for (i = 0, n = 0; i < c.n; i++) {
        if (strncmp(c.lines[i], "license ", 8)) continue;
        {
            char want[VL_CMD + 300], got[VL_CMD + 300];
            snprintf(want, sizeof want, "%s/DATABASE=DKA0:[X]Y.LDB", cmds[n]);
            strcpy(got, c.lines[i] + 8);
            unblank(want); unblank(got);
            CHECKS(got, want);
            n++;
        }
    }
    CHECK(n == 3);
    /* parse back */
    CHECK(!vl_cmd_parse("$ MODIFY BASIC/COMMENT=X", &pc));
    CHECK(vl_cmd_parse("$ LICE MODI \"BASIC\"/ISSU=\"VSI\"/COMMENT=\"it's \"\"x\"\"\"/RESERVE=(A, B)/ADD/NOLOG", &pc));
    CHECKS(pc.verb, "MODIFY");
    CHECKS(pc.param, "BASIC");
    CHECKS(vl_cmd_value(&pc, "ISSUER"), "VSI");
    CHECKS(vl_cmd_value(&pc, "COMMENT"), "it's \"x\"");
    CHECKS(vl_cmd_value(&pc, "RESERVE"), "(A, B)");
    CHECK(vl_cmd_value(&pc, "ADD") != 0);
    CHECK(vl_cmd_value(&pc, "LOG") == 0);
    CHECK(!vl_cmd_parse("$ DELETE FILE.TXT;*", &pc));
    CHECK(!vl_cmd_parse("LICENSE REGISTER X/BOGUS=1", &pc));
}

static const char *issue_proc =
"$! Software Product Authorization Key Replica\n"
"$! Issued by IAIN        \n"
"$! Issued on 24-OCT-2026 16:43\n"
"$!-----------------------------------\n"
"$ LICENSE REGISTER BASIC - \n"
"/ISSUER=VSI - \n"
"/AUTHORIZATION=TEST-AUTH-0001 - \n"
"/PRODUCER=VSI - \n"
"/UNITS=0 - \n"
"/TERMINATION_DATE=30-APR-2027  - \n"
"/ACTIVITY=CONSTANT=1 - \n"
"/OPTIONS=(IA64) - \n"
"/CHECKSUM=2-ABCD-EFGH-IJKL-MNOP\n"
"$! Software Product Authorization Key Replica\n"
"$ LICENSE REGISTER C - \n"
"/ISSUER=VSI - \n"
"/AUTHORIZATION=TEST-AUTH-0002 - \n"
"/PRODUCER=VSI - \n"
"/UNITS=0 - \n"
"/ACTIVITY=CONSTANT=1 - \n"
"/OPTIONS=(IA64,PPL) - \n"
"/CHECKSUM=2-PONM-LKJI-HGFE-DCBA\n";

static const char *pak_listing =
"             Software Product Authorization Key Replica\n"
"             -----------------------------------\n"
"             Issuer:             DEC\n"
"             Authorization:      ALS-WM-93166-5573\n"
"             Product Name:       VOLSHAD\n"
"             Producer:           DEC\n"
"             Units:              460\n"
"             Version:            5.4\n"
"             PAK Termination Date:   31-DEC-2001\n"
"             Availability:       E\n"
"             Options:            MOD_UNITS, NO_SHARE\n"
"             Checksum:           1-ADEB-DOCJ-NENC-KDBM\n";

static void test_import(void)
{
    vl_pak p[4];
    static char cmds[8][VL_CMD];
    char e[200];
    CHECK(vl_pak_import(issue_proc, p, 4) == 2);
    CHECKS(p[0].product, "BASIC");
    CHECKS(p[0].term, "30-APR-2027");
    CHECKS(p[0].activ, "CONSTANT=1");
    CHECKS(p[0].options, "IA64");
    CHECKS(p[0].checksum, "2-ABCD-EFGH-IJKL-MNOP");
    CHECKS(p[1].auth, "TEST-AUTH-0002");
    CHECKS(p[1].options, "IA64,PPL");
    CHECKS(p[1].term, "");
    CHECK(vl_build_register(&p[1], cmds, 8, e, sizeof e) == 1);
    CHECK(vl_pak_import(pak_listing, p, 4) == 1);
    CHECKS(p[0].issuer, "DEC");
    CHECKS(p[0].product, "VOLSHAD");
    CHECKS(p[0].units, "460");
    CHECKS(p[0].term, "31-DEC-2001");
    CHECKS(p[0].avail, "E");
    CHECKS(p[0].options, "MOD_UNITS,NO_SHARE");
    CHECKS(p[0].checksum, "1-ADEB-DOCJ-NENC-KDBM");
    CHECK(vl_pak_import("just some text\nwith: colons\n", p, 4) == 0);
}

int main(void)
{
    test_util();
    test_parse();
    test_check();
    test_build();
    test_proc();
    test_import();
    printf("test_core: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
