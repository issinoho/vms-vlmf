$! test_display.com - VLMF in colour and in MONO mode under PTYDRIVE, to check
$! the renditions (tools/vtscreen.py --attrs). Read-only, on a copy of the
$! database, with LMF$LICENSE pointing at a missing file as a guard.
$ set noon
$ here = f$environment("DEFAULT")
$ if f$search("ptydrive.exe") .eqs. ""
$ then
$   cc/STANDARD=C99/POINTER_SIZE=32/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES/INCLUDE=[.probes] -
       /object=ptydrive.obj [.tests.vms]ptydrive.c
$   link ptydrive
$ endif
$ if f$search("live.dir") .eqs. "" then create/directory [.live]
$ live = here - "]" + ".LIVE]"
$ define/process lmf$license 'live'guard.ldb
$ copy/nolog sys$system:lmf$license.ldb 'live'view.ldb
$ build = here - "]" + ".BUILD]"
$ vlmf :== $'build'vlmf.exe
$ drive := $'here'ptydrive.exe
$ drive [.tests.vms]display_keys.txt
$ deassign/process lmf$license
$ exit 1
