$! VLMF$SETUP.COM - define the VLMF command for a user
$!
$! Add to LOGIN.COM (or SYS$MANAGER:SYLOGIN.COM for everyone):
$!     $ @VLMF$ROOT:[000000]VLMF$SETUP.COM
$!
$ if f$trnlnm("VLMF$ROOT") .eqs. ""
$ then
$   write sys$error "VLMF$SETUP: VLMF$ROOT is not defined; run VLMF$STARTUP.COM first"
$   exit 44
$ endif
$ vlmf :== $VLMF$ROOT:[000000]VLMF.EXE
$ exit 1
