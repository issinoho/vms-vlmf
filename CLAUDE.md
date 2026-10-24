# CLAUDE.md

Working notes for vms-vlmf: a full-screen manager for the OpenVMS License Management
Facility (x86-64 and IA64), with vms-vtree's look. `docs/PLAN.md` is the phased plan;
`docs/DECISIONS.md` records every decision and the alternatives considered;
`docs/NOTES.md` records LMF's behaviour and how it was observed. Read them before
starting work. Work phase by phase, and stop and ask at each phase exit.

## Ground rules (from the sibling vms-* repos)

- **Use `tools/vms.sh`** for remote work. Never raw `ssh host cmd`, never `WAIT` over
  ssh. Run long jobs with `run_in_background`. Pass DCL specs to it in single quotes.
- **Committed files must not contain real node details** (they live in the git-ignored
  `tools/nodes.conf`), credentials, passwords, or real license data (authorizations,
  checksums): test data is made up.
- **Never change the live license database** (`LMF$LICENSE`) without asking the user.
  Tests and probes work on copies in the work directory: a plain `COPY` of the file
  (reading only), with `LMF$LICENSE` redefined in the process table to a missing file
  as a guard, and every LICENSE command naming `/DATABASE` in full.
- **Never run `LICENSE ISSUE`, `COPY` or `MOVE` on the live database**: they disable or
  delete the source license (N2). No LICENSE LOAD/UNLOAD on a node without asking.
- Prefer a 20-line probe in `probes/` over speculation. Don't claim something works
  until a test on the node shows it.

## Hard constraints

- **C99, VSI C, 32-bit pointers.** No C11, no VLAs, no anonymous unions. The Linux
  build uses `-std=c99 -pedantic -Wall -Wextra -Werror`. Under `/STANDARD=C99`,
  `fopen` takes no RMS keyword arguments.
- **`src/core/` does no I/O** (D3). `src/plat/vl_platform.h` is the whole OS surface;
  `src/plat/vms/` implements it, `src/plat/stub/` fakes it (with a small LICENSE).
- **Changes go through LICENSE commands** built only from checked values (D4): names
  quoted with `""` doubled, never two apostrophes, the database spec in full and
  unquoted (N5). Every change previews, confirms (default No), backs up and logs (D5).
- **The screen goes through `vl_ansi.c`**, SMG$ is for keys and broadcasts (VTREE D11).

## Testing

`make test` (Linux: unit checks and screen snapshots; `make snapshots` rewrites
them, then read every changed one). `tools/live.sh <node> [step...]` uploads,
runs `build_vms TEST` and drives VLMF on a pseudo-terminal against a copy of the
database (`tests/vms/test_live.com`); `tools/vtscreen.py` shows the screens.

## Commits

Commit in logical steps with messages that explain why. Record what was verified and how.
Don't push unless the user asks.
