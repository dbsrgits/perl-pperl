#!perl -w
use strict;
use Test;
BEGIN { plan tests => 5 }

my $file = 't/cat.plx';
my $seed = do { open FOO, $file; local $/; <FOO> };
ok($seed);

my $foo;

$foo = `$^X t/cat.plx < $file`;
ok($foo, $seed, "straight with pipe");

$foo = `$^X t/cat.plx $file`;
ok($foo, $seed, "straight with arg");

$foo = `src/pperl t/cat.plx < $file`;
ok($foo, $seed, "pperl with pipe");

$foo = `src/pperl t/cat.plx $file`;
ok($foo, $seed, "pperl with arg");

system("src/pperl -- --kill t/cat.plx");
