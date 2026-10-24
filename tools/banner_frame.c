/* banner_frame.c - VLMF's main screen, as the terminal would get it, for the
   README banner: the app against the Linux stub with made-up licenses
   (stub_demo), the cursor on FORTRAN, written by vl_ansi in colour. Built and
   run by tools/banner.sh; no real license data goes into the picture. */
#include <stdio.h>
#include <string.h>
#include "vl_app.h"
#include "vl_ansi.h"
#include "vl_stub.h"

static void out(void *ctx, const char *b, int n) { fwrite(b, 1, (size_t) n, (FILE *) ctx); }

int main(void)
{
    static vl_app a;
    int i;
    stub_demo();
    stub_set_size(24, 80);
    if (!vl_app_init(&a, 0, 24, 80)) return 1;
    vlp_start(&a.sess);
    for (i = 0; i < a.nvis; i++)
        if (!strcmp(a.db.l[a.vis[i]].product, "FORTRAN")) a.cur = i;
    vl_app_draw(&a);
    a.scr.cursor_on = 0;
    vl_ansi_begin(1, out, stdout);
    vl_ansi_render(&a.scr, 0, 1, out, stdout);
    vl_app_free(&a);
    return 0;
}
