#!perl -w
use strict;
use Test;
BEGIN { plan tests => 4 };
use Cwd;

my $cwd = cwd;
ok(`$^X t/cwd.plx`, $cwd);
ok(`src/pperl t/cwd.plx`, $cwd);

chdir 't';
$cwd = cwd;
ok(`$^X cwd.plx`, $cwd);
ok(`../src/pperl cwd.plx`, $cwd);

`../src/pperl -- -k cwd.plx`;
