# vms-vlmf

A full-screen manager for the OpenVMS License Management Facility, for x86-64 and
IA64, in the style of VTREE, the family's file manager (to be published): a framed
license list with the details of the current license below it, a statistics panel,
XTree Gold's colours, one-letter commands and PF1 as the GOLD key. Part of the
[ports and tools for OpenVMS](https://openvms.issinoho.com) family, with
[HISTSH](https://github.com/issinoho/vms-histsh) and
[Steward](https://github.com/issinoho/vms-steward).

It shows every license in the database and lets you **register** (add), **modify**
(edit), **remove**, disable or enable, and load licenses, either by typing a PAK into
a form or by reading the PAKs from a file (a `LICENSE ISSUE/PROCEDURE` file or a PAK
listing as VSI sends them).

**Status: 0.1.0, an early test release.** Tested end to end on x86-64 (E9.2-4)
and IA64 (V8.4-2L3) against copies of the license database. PCSI kits are on the
[releases page](https://github.com/issinoho/vms-vlmf/releases). See
[docs/PLAN.md](docs/PLAN.md).

## Installing

A kit downloaded through a non-VMS system loses its record format. Restore it,
then install:

    $ SET FILE/ATTRIBUTE=(RFM:FIX,LRL:8192,MRS:8192,RAT:NONE) ISSINOHO-*-VLMF-T0001-0-1.PCSI
    $ PRODUCT INSTALL VLMF /PRODUCER=ISSINOHO /SOURCE=dev:[dir]
    $ VLMF :== $VLMF:VLMF.EXE

Add `@SYS$STARTUP:VLMF$STARTUP.COM` to `SYS$MANAGER:SYSTARTUP_VMS.COM` to define the
`VLMF` logical name at every boot.

## Building from source

Build on the node with `@build_vms`, then

    $ VLMF :== $dev:[dir.BUILD]VLMF.EXE
    $ VLMF                          ! the system database, LMF$LICENSE
    $ VLMF [.TEST]COPY.LDB          ! or any other license database

It needs a VT100-compatible terminal of at least 24 x 80 (132 columns show the
authorization and comment in the list too). On a terminal without colour,
`DEFINE VLMF$DISPLAY MONO` first. PF1 is GOLD, Help (or PF2, or `?`) shows the keys,
F10 or Ctrl/Z leaves.

To try changes safely, work on a copy first:

    $ COPY SYS$SYSTEM:LMF$LICENSE.LDB SYS$LOGIN:TRY.LDB
    $ VLMF SYS$LOGIN:TRY.LDB

Changing the system database needs SYSPRV (or write access to the file), as the
LICENSE command does; loading needs CMKRNL and SYSNAM.

## Safety first

- VLMF changes licenses only through the LICENSE utility, never by writing the
  database file itself.
- Every change shows the exact LICENSE commands it will run and asks; the answer
  is No unless you type Y.
- Before its first change VLMF copies the database (`VLMF$BACKUP`, default
  `SYS$LOGIN:VLMF_BACKUP.LDB`), and if the copy fails nothing is changed.
- Every command and its result is logged (`VLMF$LOG`, default `SYS$LOGIN:VLMF.LOG`).
- It never uses `LICENSE ISSUE`, `COPY` or `MOVE`, which disable or delete the
  source license as a side effect.
- LMF does not show a PAK's checksum, so a removed license can only be registered
  again from its PAK: keep your PAKs.

The reasoning is in [docs/DECISIONS.md](docs/DECISIONS.md), what was observed on the
nodes in [docs/NOTES.md](docs/NOTES.md).

## Licence

MIT. See [LICENSE](LICENSE).
