#!perl -w
use strict;
use Test;
BEGIN { plan tests => 4 };

ok(capture($^X, 't/args.plx'), '');

ok(capture($^X, 't/args.plx', "foo\nbar", 'baz'),
   qq{'foo\nbar'\n'baz'\n});

ok(capture('src/pperl', 't/args.plx'), '');

ok(capture('src/pperl', 't/args.plx', "foo\nbar", 'baz'),
   qq{'foo\nbar'\n'baz'\n});

`src/pperl -- -k t/args.plx`;

sub capture {
    my $pid = open(FH, "-|");
    my $result;
    if ($pid) { local $/; $result = <FH>; close FH }
    else      { exec(@_) or die "failure to exec $!"; }
    return $result;
}

