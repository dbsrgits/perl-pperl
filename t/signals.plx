#!perl -w
use strict;
use Config;
use IO::Handle;

my $new = "starting\n";
my @out;
my $running = 1;
for my $sig (qw(HUP TERM)) {
    $SIG{$sig} = sub {
        push @out, "Got SIG$sig\n";
        $running = 0 if $sig eq 'TERM';
    };
}

$| = 1;
STDOUT->autoflush(1);
print "starting\n"; # let the test know we're hooked in
while ($running) {}
print sort @out;
