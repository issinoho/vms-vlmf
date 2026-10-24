$! probe_help.com - LICENSE help text, one subcommand at a time. Read-only.
$ set noon
$ topics = "REGISTER,MODIFY,DELETE,CREATE,COPY,ISSUE,MOVE"
$ i = 0
$ loop:
$   t = f$element(i, ",", topics)
$   if t .eqs. "," then goto done
$   write sys$output "######## ", t
$   help/nopage/output=h.tmp license 't' *
$   type h.tmp
$   delete/nolog h.tmp;*
$   i = i + 1
$   goto loop
$ done:
