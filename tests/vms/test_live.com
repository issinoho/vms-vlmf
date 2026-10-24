$! test_live.com - VLMF end to end on a pseudo-terminal (PTYDRIVE plays the user).
$! Works only on copies: [.LIVE]TEST.LDB is a plain COPY of the live database (the
$! live one is only read), and LMF$LICENSE is redefined in the process table to a
$! file that does not exist, so nothing can reach the live database by default.
$! BASIC.COM (a PAK replica for re-registering) comes from LICENSE ISSUE on a
$! second copy, SRC.LDB. Print the transcript with tools/vtscreen.py.
$ set noon
$ here = f$environment("DEFAULT")
$ keys = p1
$ if keys .eqs. "" then keys = "[.tests.vms]live_keys.txt"
$ if f$search("ptydrive.exe") .eqs. ""
$ then
$   cc/STANDARD=C99/POINTER_SIZE=32/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES/INCLUDE=[.probes] -
       /object=ptydrive.obj [.tests.vms]ptydrive.c
$   link ptydrive
$ endif
$ if f$search("live.dir") .eqs. "" then create/directory [.live]
$ if f$search("[.live]*.*;*") .nes. "" then delete/nolog [.live]*.*;*
$ live = here - "]" + ".LIVE]"
$ write sys$output "==== live database BASIC before"
$ license list/full basic
$ define/process lmf$license 'live'guard.ldb
$ define/process vlmf$backup 'live'backup.ldb
$ define/process vlmf$log 'live'vlmf.log
$ copy/nolog sys$system:lmf$license.ldb 'live'test.ldb
$ copy/nolog sys$system:lmf$license.ldb 'live'src.ldb
$ license issue/procedure/output='live'basic.com basic/database='live'src.ldb
$ write sys$output "==== PAK replica"
$ type 'live'basic.com
$! ---- VLMF under PTYDRIVE
$ build = here - "]" + ".BUILD]"
$ vlmf :== $'build'vlmf.exe
$ drive := $'here'ptydrive.exe
$ drive 'keys'
$ write sys$output "==== scratch database BASIC after"
$ license list/full/history basic/database='live'test.ldb
$ write sys$output "==== change log"
$ if f$search(live + "vlmf.log") .nes. "" then type 'live'vlmf.log
$ write sys$output "==== backups"
$ directory/size 'live'backup.ldb;*
$ deassign/process lmf$license
$ deassign/process vlmf$backup
$ deassign/process vlmf$log
$ write sys$output "==== live database BASIC after"
$ license list/full basic
$ exit 1
