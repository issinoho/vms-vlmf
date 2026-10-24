/* test_app.c - the whole app against the stub platform: keys in, screens out.
   Each case's screen is compared with tests/snap/<name>.txt; run with
   VL_UPDATE_SNAPSHOTS=1 to write them (then read every changed one). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vl_app.h"
#include "vl_stub.h"
#include "vl_util.h"

static int checks, fails;
static const char *snapdir = "tests/snap";
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static void snap(vl_app *a, const char *name)
{
    static char text[65536], want[65536];
    char path[256];
    FILE *f;
    size_t n;
    vl_app_draw(a);
    vl_screen_text(&a->scr, text, sizeof text);
    snprintf(path, sizeof path, "%s/%s.txt", snapdir, name);
    checks++;
    if (getenv("VL_UPDATE_SNAPSHOTS")) {
        f = fopen(path, "w");
        if (f) { fputs(text, f); fclose(f); }
        return;
    }
    f = fopen(path, "r");
    if (!f) { fails++; printf("FAIL snapshot %s missing\n", path); return; }
    n = fread(want, 1, sizeof want - 1, f);
    want[n] = '\0';
    fclose(f);
    if (strcmp(text, want)) {
        fails++;
        printf("FAIL snapshot %s differs; got:\n%s\n", name, text);
    }
}

static void key(vl_app *a, int k) { vl_app_key(a, k); vl_app_draw(a); }

static void type(vl_app *a, const char *s) { for (; *s; s++) key(a, (unsigned char) *s); }

static void erase_field(vl_app *a) { key(a, VK_CTRL_E); key(a, VK_CTRL_U); }

/* Put the cursor on a product. */
static void go(vl_app *a, const char *product)
{
    int i;
    for (i = 0; i < a->nvis; i++)
        if (!strcmp(a->db.l[a->vis[i]].product, product)) { a->cur = i; return; }
    CHECK(!"product not in the list");
}

static void form_field(vl_app *a, const char *label)
{
    int n, i;
    const vl_field *f = vl_form_fields(a, &n);
    for (i = 0; i < n; i++)
        if (!strcmp(f[i].label, label)) {
            while (a->fcur < i) key(a, VK_DOWN);
            while (a->fcur > i) key(a, VK_UP);
            return;
        }
    CHECK(!"no such field");
}

static const char *paks =
"$! Software Product Authorization Key Replica\n"
"$ LICENSE REGISTER COBOL - \n"
"/ISSUER=VSI - \n"
"/AUTHORIZATION=TEST-AUTH-0010 - \n"
"/PRODUCER=VSI - \n"
"/UNITS=0 - \n"
"/TERMINATION_DATE=30-APR-2027  - \n"
"/ACTIVITY=CONSTANT=1 - \n"
"/OPTIONS=(X86_64) - \n"
"/CHECKSUM=2-AAAA-BBBB-CCCC-DDDD\n"
"$ LICENSE REGISTER PASCAL - \n"
"/ISSUER=VSI - \n"
"/AUTHORIZATION=TEST-AUTH-0011 - \n"
"/PRODUCER=VSI - \n"
"/UNITS=0 - \n"
"/OPTIONS=(X86_64) - \n"
"/CHECKSUM=2-AAAA-BBBB-CCCC-DDDE\n";

int main(void)
{
    static vl_app a;
    const vl_lic *l;
    int n;

    if (getenv("VL_SNAPDIR")) snapdir = getenv("VL_SNAPDIR");

    stub_reset();
    stub_set_size(24, 80);
    CHECK(vl_app_init(&a, 0, 24, 80));
    vlp_start(&a.sess);
    CHECK(a.nvis == 4);
    CHECK(strstr(stub_last_proc(), "/database=STUB$DKA0:[SYSEXE]LMF$LICENSE.LDB\n") != 0);
    snap(&a, "01_list");

    /* Info */
    go(&a, "FORTRAN");
    key(&a, 'i');
    CHECK(a.mode == M_INFO);
    snap(&a, "02_info");
    key(&a, ' ');
    CHECK(a.mode == M_LIST);

    /* Edit: a comment and a reservation list */
    key(&a, 'e');
    CHECK(a.mode == M_FORM && a.form_kind == F_EDIT);
    form_field(&a, "Comment");
    type(&a, "Pool licence");
    form_field(&a, "Reserve users");
    type(&a, "smith, jones");
    snap(&a, "03_edit_form");
    key(&a, VK_DO);
    CHECK(a.mode == M_CONFIRM && a.ncmds == 2);
    snap(&a, "04_edit_confirm");
    key(&a, 'y');
    CHECK(a.mode == M_LIST);
    l = stub_find("FORTRAN", "TEST-AUTH-0003");
    CHECK(l && !strcmp(l->comment, "Pool licence") && !strcmp(l->reserve, "SMITH,JONES"));
    CHECK(a.backed_up && strstr(a.backup, "VLMF_BACKUP.LDB") != 0);
    CHECK(strstr(stub_log(), "BACKUP STUB$DKA0:[SYSEXE]LMF$LICENSE.LDB -> STUB$DKA0:[IAIN]VLMF_BACKUP.LDB;1") != 0);
    CHECK(strstr(stub_log(), "OK   LICENSE MODIFY \"FORTRAN\"") != 0);
    snap(&a, "05_after_edit");

    /* Edit again: units past the PAK's MOD_UNITS; a termination later than the PAK's
       is refused in the form */
    key(&a, 'e');
    form_field(&a, "Termination (mod.)");
    type(&a, "1-DEC-2026");
    key(&a, VK_DO);
    CHECK(a.mode == M_FORM && strstr(a.form_err, "not after the PAK's date") != 0);
    snap(&a, "06_edit_bad_date");
    erase_field(&a);
    type(&a, "31-oct-2026");
    form_field(&a, "Units (modified)");
    type(&a, "200");
    key(&a, VK_DO);
    CHECK(a.mode == M_CONFIRM && a.ncmds == 1);
    CHECK(strstr(a.cmds[0], "/UNITS=200/TERMINATION_DATE=31-OCT-2026") != 0);
    CHECK(strstr(a.text[a.ntext - 1], "A copy of the database") == 0);    /* already backed up */
    key(&a, 'y');
    l = stub_find("FORTRAN", "TEST-AUTH-0003");
    CHECK(l && !strcmp(l->mod_units, "200") && !strcmp(l->mod_term, "31-OCT-2026"));

    /* Add: a wrong checksum is LMF's to refuse; the form comes back with its message */
    key(&a, 'a');
    CHECK(a.mode == M_FORM && a.form_kind == F_ADD);
    type(&a, "cobol"); key(&a, VK_RETURN);
    type(&a, "VSI"); key(&a, VK_RETURN);
    type(&a, "TEST-AUTH-0010"); key(&a, VK_RETURN);
    type(&a, "VSI"); key(&a, VK_RETURN);
    type(&a, "0");
    form_field(&a, "Checksum");
    type(&a, "1-AAAA-BBBB-CCCC-DDDD");
    snap(&a, "07_add_form");
    key(&a, VK_RETURN);                       /* Return on the last field: done */
    CHECK(a.mode == M_CONFIRM);
    snap(&a, "08_add_confirm");
    key(&a, 'Y');
    CHECK(a.mode == M_FORM && strstr(a.form_err, "BADCHK") != 0);
    snap(&a, "09_add_badchk");
    CHECK(strstr(stub_log(), "Register COBOL failed: %LICENSE-F-BADCHK") != 0);
    erase_field(&a);
    type(&a, "2-AAAA-BBBB-CCCC-DDDD");
    key(&a, VK_DO);
    key(&a, 'y');
    CHECK(a.mode == M_LIST && stub_find("COBOL", "TEST-AUTH-0010"));
    CHECK(a.nvis == 5);

    /* Remove: No does nothing, Y removes */
    go(&a, "VMS-WEBUI");
    key(&a, VK_REMOVE);
    CHECK(a.mode == M_CONFIRM);
    snap(&a, "10_remove_confirm");
    key(&a, 'n');
    CHECK(a.mode == M_LIST && stub_find("VMS-WEBUI", "TEST-AUTH-0004"));
    key(&a, 'r');
    key(&a, 'y');
    CHECK(!stub_find("VMS-WEBUI", "TEST-AUTH-0004"));
    CHECK(a.nvis == 4);

    /* Disable a loaded license, enable it again */
    go(&a, "BASIC");
    key(&a, 'd');
    CHECK(a.mode == M_CONFIRM && strstr(a.cmds[0], "DISABLE \"BASIC\"") == a.cmds[0]);
    key(&a, 'y');
    l = stub_find("BASIC", "TEST-AUTH-0001");
    CHECK(l && !strcmp(l->status, "Disabled"));
    key(&a, 'd');
    CHECK(strstr(a.cmds[0], "ENABLE") == a.cmds[0]);
    key(&a, 'y');
    CHECK(!strcmp(stub_find("BASIC", "TEST-AUTH-0001")->status, "Active"));

    /* History, filter, sort */
    n = a.nvis;
    key(&a, 'h');
    CHECK(a.history && a.nvis > n);
    key(&a, 'h');
    CHECK(!a.history && a.nvis == n);
    key(&a, 'f');
    erase_field(&a);
    key(&a, VK_CTRL_U);
    type(&a, "c*");
    key(&a, VK_RETURN);
    CHECK(a.nvis == 2);
    snap(&a, "11_filter");
    key(&a, VK_FIND); key(&a, VK_CTRL_U); key(&a, VK_RETURN);
    CHECK(a.nvis == 4);
    key(&a, 's'); key(&a, 's');
    CHECK(a.sort == S_TERM && !strcmp(vl_cur(&a)->product, "BASIC"));
    key(&a, 's'); key(&a, 's');

    /* PAKs from a file: each one is offered in the form */
    stub_add_file("PAKS.COM", paks);
    key(&a, VK_PF1); key(&a, 'a');
    CHECK(a.prompt == P_FILE);
    type(&a, "NOSUCH.COM"); key(&a, VK_RETURN);
    CHECK(a.mode == M_LIST && a.msg_err && strstr(a.msg, "file not found"));
    key(&a, VK_PF1); key(&a, 'a');
    type(&a, "paks.com"); key(&a, VK_RETURN);
    CHECK(a.mode == M_FORM && !strcmp(a.pak.product, "COBOL"));
    snap(&a, "12_import");
    key(&a, VK_DO);
    key(&a, 'y');                             /* COBOL is there already */
    CHECK(a.mode == M_FORM && strstr(a.form_err, "EXISTS"));
    key(&a, VK_F10);                          /* skip it: PASCAL comes next */
    CHECK(a.mode == M_FORM && !strcmp(a.pak.product, "PASCAL"));
    key(&a, VK_DO);
    key(&a, 'y');
    CHECK(a.mode == M_LIST && stub_find("PASCAL", "TEST-AUTH-0011"));

    /* Help, and a wide screen */
    key(&a, VK_HELP);
    CHECK(a.mode == M_HELP);
    snap(&a, "13_help");
    key(&a, ' ');
    vl_app_free(&a);

    stub_set_size(30, 132);
    CHECK(vl_app_init(&a, "dka0:[iain]test.ldb", 30, 132));
    CHECK(strstr(stub_last_proc(), "/database=DKA0:[IAIN]TEST.LDB\n") != 0);
    snap(&a, "14_wide");
    vl_app_free(&a);

    /* A failed backup stops the change */
    stub_reset();
    stub_fail_backup(1);
    stub_set_size(24, 80);
    vl_app_init(&a, 0, 24, 80);
    go(&a, "BASIC");
    key(&a, 'd');
    key(&a, 'y');
    CHECK(a.msg_err && strstr(a.msg, "backup copy failed"));
    CHECK(!strcmp(stub_find("BASIC", "TEST-AUTH-0001")->status, "Active"));
    CHECK(!a.backed_up);
    snap(&a, "15_backup_failed");
    /* a name VLMF will not put into a procedure */
    vl_app_free(&a);
    vl_app_init(&a, "X\"Y", 24, 80);
    CHECK(a.msg_err && a.nvis == 0);
    vl_app_free(&a);

    printf("test_app: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
