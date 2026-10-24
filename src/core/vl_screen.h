/* vl_screen.h - the screen model: a grid of cells, each a character, a rendition
   and a character set. The app draws into it; a platform renders it (SMG$ on VMS,
   text for the tests). */
#ifndef VL_SCREEN_H
#define VL_SCREEN_H

#define VL_A_BOLD  1
#define VL_A_REV   2
#define VL_A_UNDER 4
#define VL_A_GFX   8      /* ch is a DEC Special Graphics character */

/* DEC Special Graphics characters (shown as these ASCII codes with VL_A_GFX). */
#define VL_G_DIAMOND '`'
#define VL_G_HLINE   'q'
#define VL_G_VLINE   'x'
#define VL_G_TL      'l'
#define VL_G_TR      'k'
#define VL_G_BL      'm'
#define VL_G_BR      'j'
#define VL_G_LTEE    't'   /* |- */
#define VL_G_RTEE    'u'   /* -| */
#define VL_G_TTEE    'w'
#define VL_G_BTEE    'v'
#define VL_G_CROSS   'n'

/* What a cell is, for the colour palette (D11). VL_R_AUTO: decided by its
   rendition (plain text, bold key, highlight bar, line drawing...). */
enum { VL_R_AUTO, VL_R_TITLE, VL_R_MENU, VL_R_MENU_LABEL };

typedef struct { unsigned char ch, attr, role; } vl_cell;

typedef struct {
    int rows, cols;
    vl_cell *cell;           /* rows * cols */
    int cursor_row, cursor_col, cursor_on;
    unsigned char role;      /* given to every cell written from now on */
} vl_screen;

int  vl_screen_init(vl_screen *s, int rows, int cols);   /* 0 on no memory */
void vl_screen_free(vl_screen *s);
void vl_screen_clear(vl_screen *s);
void vl_role(vl_screen *s, unsigned char role);              /* VL_R_AUTO after a clear */
vl_cell *vl_at(vl_screen *s, int row, int col);           /* 0 when outside */

/* Text at (row, col), cut at max columns (max < 0: to the edge). Control
   characters show as '?'. Returns the columns written. */
int  vl_put(vl_screen *s, int row, int col, const char *text, unsigned char attr, int max);
/* Fill count cells with ch. */
void vl_fill(vl_screen *s, int row, int col, int count, unsigned char ch, unsigned char attr);
void vl_gfx(vl_screen *s, int row, int col, unsigned char g, unsigned char attr);
/* Box with corners at (r1,c1) and (r2,c2). */
void vl_box(vl_screen *s, int r1, int c1, int r2, int c2);
void vl_hline(vl_screen *s, int row, int c1, int c2);
void vl_vline(vl_screen *s, int col, int r1, int r2);
/* Set attributes on a run of cells (e.g. the highlight bar). */
void vl_attr_run(vl_screen *s, int row, int col, int count, unsigned char attr);

/* The grid as UTF-8 text, one line per row, trailing blanks kept off; DEC graphics
   become box-drawing characters. For snapshot tests. Returns bytes written. */
int  vl_screen_text(const vl_screen *s, char *out, int size);

#endif
