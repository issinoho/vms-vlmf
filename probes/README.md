# Probes

DCL procedures run with `tools/vms.sh <node> run probes/<name>.com`. They only read
the live license database, or work on copies in `[.SCRATCH]` of the work directory.

- `probe_lmf.com`: LMF's version, database file, LIST/FULL, SHOW LICENSE, help (N1).
- `probe_help.com`: the help text of each LICENSE subcommand (N2).
- `probe_scratch.com`: every write operation on a scratch database (N2, N3).
- `probe_clear.com`: how LICENSE MODIFY clears a field (N4).
- `probe_common.h`: shared by `tests/vms/ptydrive.c`.
