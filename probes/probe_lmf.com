$! probe_lmf.com - what the License Management Facility looks like here. Read-only:
$! nothing is registered, modified, loaded or deleted.
$ set noon
$ write sys$output "==== version"
$ write sys$output f$getsyi("ARCH_NAME"), " ", f$getsyi("VERSION"), " ", f$getsyi("NODENAME")
$ write sys$output "==== logical"
$ show logical/full lmf$license
$ write sys$output "==== database file"
$ directory/full lmf$license
$ write sys$output "==== privileges"
$ write sys$output "SYSPRV ", f$privilege("SYSPRV"), " CMKRNL ", f$privilege("CMKRNL"), " SYSNAM ", f$privilege("SYSNAM")
$ write sys$output "==== list"
$ license list
$ write sys$output "==== list/full (first licence)"
$ license list/full/output=lmf_full.tmp
$ type lmf_full.tmp
$ write sys$output "==== show license (loaded)"
$ show license
$ write sys$output "==== help"
$ help/nopage/output=lmf_help.tmp license ...
$ type lmf_help.tmp
$ delete/nolog lmf_full.tmp;*,lmf_help.tmp;*
$ write sys$output "==== done"
