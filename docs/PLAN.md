# vms-vlmf: a full-screen license manager for OpenVMS

## Goal

View, add, edit and remove OpenVMS licenses (the License Management Facility's
database) from one full-screen program with the look and keys of vms-vtree: framed
windows, a statistics panel, XTree Gold's colours, one-letter commands, PF1 GOLD,
LK201 keys. Same conventions as the other vms-* projects: `tools/vms.sh`,
DECISIONS.md, NOTES.md, PCSI kits for X86VMS and I64VMS, the family release process.

## The screen (80 or 132 columns, 24 or more rows)

```
 VLMF   SYS$COMMON:[SYSEXE]LMF$LICENSE.LDB;1                  24-OCT-2026 17:11
┌─────────────────────────────────────────────────────────┬────────────────────┐
│ Product            Producer Status   Units  Terminates  │ DATABASE           │
│◆BASIC              VSI      Active       0  30-APR-2027 │  LMF$LICENSE.LDB;1 │
│◆C                  VSI      Active       0  30-APR-2027 │  Made 20-APR-2023  │
│ FORTRAN            VSI      Active     200* 31-OCT-2026*│ LICENSES           │
│ ...                                                     │  Total          11 │
├─────────────────────────────────────────────────────────┤  ...               │
│ Authorization TEST-AUTH-0001           │                    │
│ Issuer VSI   Version 0.0   Release (none)               │ X86VMS IAIN        │
│ Options X86_64                                          │  SYSPRV: can change│
│ Last change 15-APR-2026 15:22 by IAIN, ENABLE           │                    │
└─────────────────────────────────────────────────────────┴────────────────────┘
 LICENSE  Add Edit Remove Disable/enable Load Filter History Info Sort
 COMMANDS  PF1 GOLD   Return info   Insert add   Remove remove   Help   F10 exit
```

◆ = loaded on this node (SHOW LICENSE). `*` = modified from the PAK's value.
A license that ends within 30 days is shown bright; one past its date, "Expired".

## Keys

| Key | Action |
|---|---|
| Arrows, Prev/Next Screen | move; GOLD Up/Down first/last |
| Return, `I`, Select | Info: every field of the license |
| `A`, Insert Here | Add: register a PAK typed into a form |
| GOLD `A` | Add the PAKs in a file, one form each |
| `E` | Edit: modified units and termination, comment, selection weight, include/exclude nodes, reserve users |
| `D` | disable an active license / enable a disabled one |
| `R`, Remove | remove the license |
| `L` | load it on this node |
| `F`, Find | filter by product (`VMS*`, `C%%%%`) |
| `S` | sort by product, producer, termination, status |
| `H` | show the history (extinct) records too |
| GOLD `W`, Ctrl/W | read again; redraw |
| Help, PF2 | help |
| F10, Ctrl/Z | exit |

Forms: Up/Down/Tab/Return move between fields, Do or KP Enter is done, F10 cancels.

## Phase 0: scaffolding and probes (done 2026-10-24)

Repo from vms-vtree's tooling. Probes on both nodes, all read-only on the live
database or on scratch copies: LMF's layout and help (N1, N2), every write operation
on a scratch database (N3), how MODIFY clears fields (N4), how /DATABASE is taken (N5).

## Phase 1: the application (built and tested 2026-10-24; awaiting the user's try)

All of the keys above. Core (parsing LIST/FULL and SHOW LICENSE, checking fields,
building LICENSE commands and procedures, reading PAK files) and app tested on
Linux against the stub (`make test`: 137 core checks, 64 app checks with 15 screen
snapshots), the same tests natively on both nodes (`build_vms TEST`), and end to end
under PTYDRIVE on both nodes (`tools/live.sh`, N6): filter, info, edit comment and
reserve list, disable, enable, clear both fields, remove, re-register from a PAK
replica with its real checksum, help, exit. The live databases were not changed.

Exit: the user tries it on both nodes (on a copy, then, with their agreement, on the
system database).

## Phase 2: kit and release

Kits built 2026-10-24 (`tools/kit.sh <node>`, producer ISSINOHO, version from
`src/core/vl_types.h`; MAKE_KIT checks `VLMF -v`): ISSINOHO-X86VMS-VLMF-T0001-0-1.PCSI
and ISSINOHO-I64VMS-VLMF-T0001-0-1.PCSI. The kit installs [VLMF]VLMF.EXE, README.VMS,
LICENSE. and SYS$STARTUP:VLMF$STARTUP.COM, which defines VLMF$ROOT and VLMF; users
run it as `VLMF :== $VLMF:VLMF.EXE` (no DCL verb: a DCLTABLES change is not worth it).
Installed from the kits on both nodes 2026-10-24 with the user's agreement
(`tests/vms/pcsi_install.com`: files in SYS$COMMON:[VLMF], logical names defined,
`VLMF -v` says 0.1.0), then the installed image run read-only on the system
database under PTYDRIVE (`tests/vms/test_installed.com`). Released as v0.1.0.

## Later ideas

LICENSE UNLOAD; the PAK flags MODIFY can set (`/NO_SHARE`, `/COMBINE`, `/VIRTUAL`);
copying a license between databases without COPY's side effect (register from a
replica); a cluster view (which node loaded what).
