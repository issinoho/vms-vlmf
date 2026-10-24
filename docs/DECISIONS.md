# Decisions

## D1. Name: VLMF (2026-10-24, user's choice)

`vms-vlmf`, command and PCSI product `VLMF`: a "V" tool like VTREE, named after the
facility it manages. Considered: VLICENSE (plainer, longer), LMFTOOL.

## D2. Look: VTREE's (2026-10-24, user's request)

The user asked for "the same look and feel as our vtree project". The screen model,
the colour renderer (`vl_ansi.c`: XTree Gold's palette written as ANSI colour, VTREE
D11), the line editor and the SMG$ keyboard and broadcast code are VTREE's, renamed.
The layout follows VTREE's main screen: title line with the clock, a framed window
split into a list and a detail pane, the statistics panel on the right, two menu
lines, PF1 GOLD. `VLMF$DISPLAY MONO` as `VTREE$DISPLAY`.

## D3. Code layout: pure core, thin platform (2026-10-24)

As in VTREE: `src/core/` has no I/O (license records, parsing LMF's text, field
checks, command and procedure building, PAK file reading, the screen model);
`src/plat/vl_platform.h` is the whole OS surface; `src/plat/stub/` includes a small
LICENSE interpreter that runs VLMF's own procedures against licenses in memory, so
the whole program, screens included, is tested on Linux.

## D4. Changes go through the LICENSE utility, in a subprocess (2026-10-24)

The License Database is an indexed file with an undocumented record layout and PAK
checksums; there is no documented callable interface. So VLMF reads it with
`LICENSE LIST/FULL/SELECTION_WEIGHT` and `SHOW LICENSE`, and changes it with
`LICENSE REGISTER/MODIFY/DELETE/ENABLE/DISABLE/LOAD`, written into a procedure in
SYS$SCRATCH and run by `LIB$SPAWN` with input `NL:`, output to a file and
`NOCLISYM` (a user's own LICENSE or COPY symbol must not run instead). Considered:
writing the .LDB with RMS (would need the checksum algorithm and the record layout;
one mistake corrupts every license), parsing the file read-only (no gain over LIST).

Built commands contain only checked values (field kinds in `vl_check`): names are
quoted with `""` doubled, two apostrophes together are refused (DCL substitutes
`''symbol'` even inside quotes), dates, numbers, codes and option lists are checked
by form. The database spec is expanded to its full name once at start-up and
written unquoted (N5); `vl_check_db` refuses quotes, apostrophes, blanks, `/` and `!`.
Lines are split with `-` continuations at 200 characters, so long lists fit.

## D5. Every change: show, ask, back up, log (2026-10-24)

- The confirm screen shows the exact LICENSE commands; only `Y` runs them.
- Before the first change of a session the database file is copied to
  `VLMF$BACKUP` (default `SYS$LOGIN:VLMF_BACKUP.LDB`, a new version each session) in
  the same procedure; if the copy fails, nothing else runs.
- A multi-command change stops at the first failure.
- Each command and its outcome goes to `VLMF$LOG` (default `SYS$LOGIN:VLMF.LOG`).
- A failed register or modify returns to the form with LMF's message.
- Remove warns that LMF never shows the checksum, so only the PAK can bring it back.

## D6. Never ISSUE, COPY or MOVE (2026-10-24)

N2: `LICENSE ISSUE` and `LICENSE COPY` disable the source license, `MOVE` deletes it.
VLMF does not use them. Reading PAKs from a file covers moving a license.

## D7. Privileges are left as they are (2026-10-24)

Unlike VTREE (its D5), VLMF does not turn privileges off: writing the system database
needs SYSPRV (it is `(S:RWED,O:RWED,G:RE,W)`, owner [1,1]), LOAD needs CMKRNL and
SYSNAM, and LMF itself checks them. D5's confirmation is the safeguard. The panel
says whether the process has SYSPRV.

## D8. Edit is what LICENSE MODIFY can change (2026-10-24)

PAK fields can't be changed (the checksum covers them): to change one, remove and
register again. The form edits the modifications: units (`MOD_UNITS` PAKs),
termination date (not after the PAK's), comment (63 characters), selection weight,
include or exclude nodes (not both), reserve users. An empty field undoes the
modification in the way that works (N4): `/UNITS=0`, the PAK's own termination
date, `/COMMENT=""`, `/SELECTION_WEIGHT=0`. Lists are changed by `/REMOVE` of the
names that went and `/ADD` of the names that came, so the result doesn't depend on
whether a plain list replaces or adds.

## D9. "Loaded" from SHOW LICENSE (2026-10-24)

SHOW LICENSE names product and producer only. VLMF marks the active records of a
listed product as loaded, or, if none is active, its other records (disabled after
loading). For another database than the system's, the marks still describe this node.
