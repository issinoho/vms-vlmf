$! pcsi_install.com - install VLMF from its PCSI kit and verify. SYSTEM-WIDE:
$! run only with the user's agreement. Touches no license and no login or
$! startup procedure. Run from the work directory after tools/kit.sh.
$ set noon
$ old_privs = f$setprv("SYSPRV,SYSNAM,CMKRNL,BYPASS")
$ arch = f$edit(f$getsyi("ARCH_NAME"), "UPCASE")
$ base = "I64VMS"
$ if arch .eqs. "X86_64" then base = "X86VMS"
$ kitdir = f$environment("DEFAULT") - "]" + ".KIT_''arch']"
$ write sys$output "=== INSTALL from ", kitdir
$ product install VLMF /producer=ISSINOHO /base_system='base' /source='kitdir' /options=noconfirm /log
$ write sys$output "=== install status ", $status
$ product show product VLMF /producer=ISSINOHO
$ write sys$output "=== VERIFY"
$ show logical/system VLMF*
$ directory/protection/owner/size VLMF:,SYS$STARTUP:VLMF$STARTUP.COM
$ vlmf_v := $VLMF:VLMF.EXE
$ vlmf_v "-v"
$ dummy = f$setprv(old_privs)
$ exit 1
