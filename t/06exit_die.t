#!perl -w
use strict;
use Test;
BEGIN { plan tests => 8 };

my $out;

$out = `$^X t/spammy.plx 2>&1`;
ok($? >> 8, 0);
ok($out, "");

$out = `$^X t/spammy.plx foo 2>&1`;
ok($? >> 8, 70);
ok($out, "foo at t/spammy.plx line 7.\n");

$out = `src/pperl t/spammy.plx 2>&1`;
ok($? >> 8, 0);
ok($out, "");


$out = `src/pperl t/spammy.plx foo 2>&1`;
ok($? >> 8, 70);
ok($out, "foo at t/spammy.plx line 7.\n");

`src/pperl -- -k t/spammy.plx`