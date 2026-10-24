/* vl_ansi.h - turn the screen model into VT100/VT220 sequences (D11).

   Colour mode uses the XTree Gold palette (blue screen, white text, yellow keys
   and labels, cyan frames, a cyan highlight bar) through ANSI SGR colours, which
   DECterm, xterm and PuTTY show and a real VT220/VT320 ignores. Monochrome mode
   uses bold, underline and reverse only, as SMG$ would. Line drawing uses the DEC
   Special Graphics set (ESC ( 0). Only cells that differ from prev are sent. */
#ifndef VL_ANSI_H
#define VL_ANSI_H

#include "vl_screen.h"

typedef void (*vl_emit_fn)(void *ctx, const char *bytes, int len);

/* Send cur. prev is what the terminal shows now (cur's size), or 0 for a full
   repaint; it is updated to cur. */
void vl_ansi_render(const vl_screen *cur, vl_cell *prev, int colour, vl_emit_fn emit, void *ctx);
/* Before the first frame (colour background, cursor off) and at exit (normal
   rendition, ASCII, cursor on, screen cleared). */
void vl_ansi_begin(int colour, vl_emit_fn emit, void *ctx);
void vl_ansi_end(vl_emit_fn emit, void *ctx);
/* The SGR parameters a cell gets, e.g. "0;30;46". For tests. */
const char *vl_ansi_sgr(const vl_cell *c, int colour);

#endif
