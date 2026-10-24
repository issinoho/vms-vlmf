# Notes: LMF behaviour observed

## N1. LMF V2.0 on both nodes (2026-10-24, x86 E9.2-4 and IA64 V8.4-2L3)

`probes/probe_lmf.com`, read-only. `LMF$LICENSE` is `SYS$SYSTEM:LMF$LICENSE.LDB` in
the system table: indexed, prolog 3, one key, fixed 512-byte records, owner [1,1],
`(S:RWED,O:RWED,G:RE,W)`. x86 holds 11 licenses, IA64 53 (two Disabled). `LIST/FULL`
prints a header (`License Database File:`, `Created on:`...), then each license after
a line of dashes as `Key:` and the value from column 30; `[End Of List]` ends it.
Optional lines appear only when set: `Modified Units`, `Modified Termination Date`
(with a leading blank for one-digit days), `Comment`, `Include`, `Exclude`,
`Reserve`; `Availability`/`Activity` are missing on PPL licenses; `Selection
Weight: 1300 (Default Weight)` only with `/SELECTION_WEIGHT`, which combines with
`/FULL` and `/HISTORY`. `SHOW LICENSE` lists loaded products under a `Product
Producer ...` heading. HELP on x86 has no text for LICENSE itself, only its subtopics.

## N2. ISSUE, COPY and MOVE change the source (2026-10-24, HELP text, ISSUE verified)

HELP LICENSE: ISSUE "automatically disables the current license", COPY changes the
source license's status to disabled, MOVE deletes it from the source. Verified for
ISSUE on a scratch copy (`probe_scratch.com`): the license became `Status: Disabled`,
`Command: ISSUE`. `ISSUE/PROCEDURE` writes a `LICENSE REGISTER` command with every
PAK field and the checksum, each qualifier on its own line ending in ` - `.

## N3. Write operations on a scratch database (2026-10-24, x86 and IA64)

`probes/probe_scratch.com`: `LICENSE CREATE/DATABASE=` makes an empty database;
REGISTER from an ISSUE procedure works; a wrong checksum gives
`%LICENSE-F-BADCHK, checksum does not validate for X` (%X1A9492E4), and so does a
missing one. MODIFY accepts `/UNITS` without `MOD_UNITS` on the PAK. `/INCLUDE` and
`/EXCLUDE` exclude each other: setting one removes the other. A MODIFY, ENABLE or
DISABLE leaves the old record as `Status: Extinct` (`LIST/HISTORY`). DELETE of a
product with two licenses and no qualifiers fails with `%LICENSE-W-AMBIG`
(%X1A9492D8); an unknown product gives `%LICENSE-W-RNF` (%X1A9492D0); DELETE removes
the history records too. REGISTER accepts every value in quotes. All statuses
reach `$STATUS`. A process-table `LMF$LICENSE` overrides the system one.

## N4. How MODIFY clears a field (2026-10-24, x86 and IA64)

`probes/probe_clear.com`. `/NOINCLUDE`, `/NORESERVE`, `/NOCOMMENT`,
`/NOTERMINATION_DATE` return success and change nothing. What works:
`/COMMENT=""`; `/RESERVE=(names)/REMOVE` (and the same for include and exclude);
`/TERMINATION_DATE=` the PAK's own date (removes "Modified Termination Date"); a
later date than the PAK's gives `%LICENSE-W-BADDATE`; `/UNITS=0` removes "Modified
Units". `/RESERVE=()` is a DCL error. `/SELECTION_WEIGHT=50` shows as `50 (Modified
Weight)`.

## N5. /DATABASE takes neither quotes nor a relative name (2026-10-24, x86)

`/DATABASE="[.LIVE]TEST.LDB"` gives `%RMS-F-FNM`: DCL keeps the quotes as part of
the file name. `/DATABASE=[.LIVE]TEST.LDB` gives `%RMS-F-DNF`: the name is taken
relative to `LMF$LICENSE` (SYS$COMMON:[SYSEXE]), not the default directory. VLMF
expands the name with `$PARSE` (default `SYS$DISK:[].LDB`) and writes it unquoted.

## N6. End to end under PTYDRIVE (2026-10-24, x86 and IA64)

`tools/live.sh` on both nodes: VLMF on a copy of the system database, with
`LMF$LICENSE` pointing at a missing file. Every step did what it showed: modify
(comment, reserve add), disable, enable, modify (comment cleared, reserve removed),
delete, register from `ISSUE/PROCEDURE` output (on another copy) with its real
checksum. The backup copy and the log were written. The live databases' BASIC
record was unchanged afterwards (last modified 15-APR-2026 on x86, 25-SEP-2026 on
IA64). Each LICENSE subprocess takes about a second.
