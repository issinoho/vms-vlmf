$! test_installed.com - the PCSI-installed VLMF on the system database, under
$! PTYDRIVE, read-only: it starts, lists, shows Info and Help, and exits.
$ set noon
$ here = f$environment("DEFAULT")
$ if f$search("ptydrive.exe") .eqs. ""
$ then
$   cc/STANDARD=C99/POINTER_SIZE=32/PREFIX_LIBRARY_ENTRIES=ALL_ENTRIES/INCLUDE=[.probes] -
       /object=ptydrive.obj [.tests.vms]ptydrive.c
$   link ptydrive
$ endif
$ drive := $'here'ptydrive.exe
$ drive [.tests.vms]installed_keys.txt
$ exit 1
