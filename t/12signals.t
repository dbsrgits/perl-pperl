#!perl -w
use strict;
use Test;
use Config;


BEGIN {
    unless (defined $Config{sig_name}) {
        print "no signals!\n";
        exit 0;
    }
    plan tests => 6;
}

# cheesy - run pperl twice :)
for my $perl ( $^X,
               './pperl -Iblib/lib -Iblib/arch',
               './pperl' )
{
    my $child = open(FOO, "$perl t/signals.plx|")
      or die "can't open: $!";

    my $got = <FOO>;
    ok($got, "starting\n");
    my @expect;
    for my $sig (qw(HUP TERM)) {
        kill $sig, $child;
        push @expect, "Got SIG$sig\n";

        # bad juju - 5.00503 needs time to compose itself, newer perls
        # with better signal handling are just fine
        sleep 1 if $] < 5.006;
    }

    local $/;
    $got = <FOO>;

    close FOO
      or die "error closing pipe $! $?";
    ok($got, join('', sort @expect));
}

`./pperl -- -k t/signals.plx`
