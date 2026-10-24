$! VLMF$STARTUP.COM - system startup for VLMF (full-screen license manager)
$!
$! Installed into SYS$STARTUP. Defines the system logical names VLMF$ROOT (the
$! installed [VLMF] directory) and VLMF (its top level, where VLMF.EXE is). To
$! define them at every boot, add to SYS$MANAGER:SYSTARTUP_VMS.COM:
$!
$!     $ @SYS$STARTUP:VLMF$STARTUP.COM
$!
$! Users get the VLMF command from VLMF$SETUP.COM, for example in SYLOGIN.COM:
$!     $ @VLMF$ROOT:[000000]VLMF$SETUP.COM
$!
$! P1 = "INSTALL": also print the post-installation tasks (PCSI runs it so).
$! P1 = "REMOVE":  deassign the logical names instead (PCSI runs it so at removal).
$!
$ set noon
$ mode = f$edit(p1, "UPCASE")
$ if mode .eqs. "REMOVE"
$ then
$   if f$trnlnm("VLMF", "LNM$SYSTEM_TABLE") .nes. "" then -
        deassign/system/executive_mode VLMF
$   if f$trnlnm("VLMF$ROOT", "LNM$SYSTEM_TABLE") .nes. "" then -
        deassign/system/executive_mode VLMF$ROOT
$   exit 1
$ endif
$!
$! This procedure sits in <destination>[SYS$STARTUP]; the product is in
$! <destination>[VLMF]. Rooted logicals need the physical form.
$ proc = f$environment("PROCEDURE")
$ dev = f$parse(proc,,,"DEVICE","NO_CONCEAL")
$ dir = f$edit(f$parse(proc,,,"DIRECTORY","NO_CONCEAL"), "UPCASE") - "]["
$ root = dir - "SYS$STARTUP]" + "VLMF.]"
$ if root .eqs. dir + "VLMF.]"
$ then
$   write sys$error "VLMF$STARTUP: expected to be in a [SYS$STARTUP] directory, not ''dir'"
$   exit 44
$ endif
$ root = root - ".000000"
$ define/system/executive_mode/translation_attributes=concealed VLMF$ROOT 'dev''root'
$ define/system/executive_mode VLMF VLMF$ROOT:[000000]
$ if f$search("VLMF:VLMF.EXE") .eqs. ""
$ then
$   write sys$error "VLMF$STARTUP: VLMF.EXE not found under ''dev'''root'"
$   exit 44
$ endif
$ if mode .nes. "INSTALL" then exit 1
$ say = "write sys$output"
$ say ""
$ say "    Post-installation tasks for VLMF"
$ say ""
$ say "    At system startup: to define VLMF$ROOT and VLMF at every boot, add to"
$ say "    SYS$MANAGER:SYSTARTUP_VMS.COM:"
$ say "    $ @SYS$STARTUP:VLMF$STARTUP.COM"
$ say "    For users (in SYS$MANAGER:SYLOGIN.COM, or a user's LOGIN.COM), to define"
$ say "    the VLMF command:"
$ say "    $ @VLMF$ROOT:[000000]VLMF$SETUP.COM"
$ say ""
$ say "    Changing the system license database needs SYSPRV, loading CMKRNL and"
$ say "    SYSNAM, as for the LICENSE command. VLMF installs no privileged image."
$ say "    PRODUCT REMOVE VLMF removes the product and deassigns the logical names."
$ say ""
$ exit 1
