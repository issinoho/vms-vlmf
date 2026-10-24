$! probe_scratch.com - LICENSE write operations, on scratch databases only.
$! The live database is only read (a plain COPY of the file). Every LICENSE command
$! names /DATABASE, and LMF$LICENSE is also redefined in the process table as a
$! second guard; nothing is loaded or unloaded.
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
$ say "==== create"
$ license create 'n'
$ say "status ", $status
$ license list 'n'
$ say "==== issue from the scratch copy"
$ license issue/procedure/output='s'basic.com basic 'r'
$ say "status ", $status
$ type 's'basic.com
$ license issue/procedure/output='s'c.com c/all 'r'
$ type 's'c.com
$ license list/full basic 'r'
$ say "==== register into new (process logical points at new)"
$ define/process lmf$license 's'new.ldb
$ @'s'basic.com
$ say "status ", $status
$ @'s'c.com
$ define/process lmf$license 's'guard.ldb
$ license list/full 'n'
$ say "==== bad checksum"
$ license register testprod/issuer=TEST/producer=TEST/authorization=T-1/units=10/checksum=1-AAAA-BBBB-CCCC-DDDD 'n'
$ say "status ", $status
$ license register testprod/issuer=TEST/producer=TEST/authorization=T-1/units=10 'n'
$ say "status ", $status
$ say "==== modify comment, include"
$ license modify basic/comment="hello world, it's me"/include=(NODEA,NODEB) 'n'
$ say "status ", $status
$ license list/full basic 'n'
$ say "==== modify reserve, termination, weight"
$ license modify basic/reserve=(SMITH,JONES) 'n'
$ say "status ", $status
$ license modify basic/termination_date=1-JAN-2027 'n'
$ say "status ", $status
$ license modify basic/selection_weight=50 'n'
$ say "status ", $status
$ license modify basic/units=5 'n'
$ say "status units ", $status
$ license modify basic/exclude=(NODEC) 'n'
$ say "status exclude ", $status
$ license list/full basic 'n'
$ say "==== clear lists"
$ license modify basic/noinclude 'n'
$ say "status noinclude ", $status
$ license modify basic/noreserve 'n'
$ say "status noreserve ", $status
$ license modify basic/nocomment 'n'
$ say "status nocomment ", $status
$ license modify basic/notermination_date 'n'
$ say "status notermination ", $status
$ license list/full basic 'n'
$ say "==== disable / enable"
$ license disable basic 'n'
$ say "status ", $status
$ license list basic 'n'
$ license list/full basic 'n'
$ license enable basic 'n'
$ say "status ", $status
$ say "==== history"
$ license list/full/history basic 'n'
$ say "==== ambiguous delete"
$ license list c 'n'
$ license delete c 'n'
$ say "status ambiguous ", $status
$ license delete nosuch 'n'
$ say "status nosuch ", $status
$ say "==== exact delete"
$ license delete basic/issuer=VSI/producer=VSI 'n'
$ say "status ", $status
$ license list/full/history 'n'
$ say "==== live database untouched?"
$ license list/full basic
$ say "==== end"
