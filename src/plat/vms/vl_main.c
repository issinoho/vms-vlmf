/* vl_main.c - VLMF [database]   (run as a foreign command)
   The database defaults to LMF$LICENSE, as for the LICENSE command. */
#include <stdio.h>
#include <string.h>
#include <ssdef.h>
#include "vl_app.h"

int main(int argc, char **argv)
{
    static vl_app app;
    vlp_session sess;
    unsigned int st;
    int rows, cols;
    char msg[256];

    /* MAKE_KIT checks the image's version this way, without a terminal. */
    if (argc > 1 && (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version"))) {
        printf("vlmf %s\n", VL_VERSION);
        return SS$_NORMAL;
    }
    st = vlp_start(&sess);
    if (!(st & 1)) {
        vlp_message(st, msg, sizeof msg);
        fprintf(stderr, "VLMF: cannot start: %s\n", msg);
        return st;
    }
    vlp_screen_size(&rows, &cols);
    if (rows < 20 || cols < 80) {
        vlp_stop();
        fprintf(stderr, "VLMF: needs at least 20 rows and 80 columns (this terminal: %d x %d)\n",
                rows, cols);
        return SS$_ABORT;
    }
    if (!vl_app_init(&app, argc > 1 ? argv[1] : 0, rows, cols)) {
        vlp_stop();
        fprintf(stderr, "VLMF: out of memory\n");
        return SS$_ABORT;
    }
    app.sess = sess;
    vl_app_run(&app);
    vl_app_free(&app);
    vlp_stop();
    return SS$_NORMAL;
}
