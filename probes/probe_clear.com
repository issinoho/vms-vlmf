$! probe_clear.com - how LICENSE MODIFY clears fields. Scratch database only;
$! the live one is only read (plain COPY), LMF$LICENSE points at a guard name.
$ set noon
$ here = f$environment("DEFAULT")
$ if f$search("scratch.dir") .eqs. "" then create/directory [.scratch]
$ if f$search("[.scratch]*.*;*") .nes. "" then delete/nolog [.scratch]*.*;*
$ s = here - "]" + ".SCRATCH]"
$ define/process lmf$license 's'guard.ldb
$ copy/nolog sys$system:lmf$license.ldb 's'real.ldb
$ r = "/database=''s'real.ldb"
$ n = "/database=''s'new.ldb"
$ say = "write sys$output"
$ license create 'n'
$ license issue/procedure/output='s'basic.com basic 'r'
$ define/process/nolog lmf$license 's'new.ldb
$ @'s'basic.com
$ define/process/nolog lmf$license 's'guard.ldb
$ license modify basic/comment="say ""hi"" now"/reserve=(SMITH,JONES)/termination_date=1-JAN-2027/units=7 'n'
$ say "set ", $status
$ license list/full basic 'n'
$ license modify basic/comment="" 'n'
$ say "comment empty ", $status
$ license modify basic/reserve=(SMITH,JONES)/remove 'n'
$ say "reserve remove ", $status
$ license modify basic/termination_date=30-APR-2027 'n'
$ say "term back to PAK ", $status
$ license modify basic/units=0 'n'
$ say "units 0 ", $status
$ license list/full basic 'n'
$ license modify basic/include=(NODEA) 'n'
$ license modify basic/include=(NODEA)/remove 'n'
$ say "include remove ", $status
$ license modify basic/reserve=() 'n'
$ say "reserve () ", $status
$ license modify basic/termination_date=1-JAN-2030 'n'
$ say "term later than PAK ", $status
$ license modify basic/termination_date=99-XYZ-2030 'n'
$ say "term bad ", $status
$ license list/full basic 'n'
$ say "==== weight, flags"
$ license modify basic/selection_weight=50 'n'
$ license modify basic/no_share/combine 'n'
$ say "no_share combine ", $status
$ license modify basic/virtual 'n'
$ say "virtual ", $status
$ license list/full basic 'n'
$ license list/selection_weight basic 'n'
$ license list/brief 'n'
$ say "==== show license to a file"
$ show license/output='s'show.tmp
$ say "status ", $status
$ type 's'show.tmp
$! Every value quoted. Run with a real PAK's values (not committed) it registered (N3);
$! with these made-up ones LMF must answer BADCHK, which still shows the quoting parses.
$ say "==== register with every value quoted"
$ license register "C"/issuer="VSI"/producer="VSI"/authorization="TEST-AUTH-0002"/units=0/activity=CONSTANT=1/options=(IA64)/checksum="2-PONM-LKJI-HGFE-DCBA" 'n'
$ say "quoted register ", $status
$ license list c 'n'
$ say "==== end"
