package PPerl;

use strict;
use vars qw/@ISA $VERSION @EXPORT/;

require Exporter;

@ISA = ('Exporter');

@EXPORT = qw(
	exit
);

$VERSION = '0.06';

sub exit(;$) {
    my $retval = shift || 0;
    die "PPERLexit$retval";
}

1;
__END__

=head1 NAME

PPerl - Make perl scripts persistent in memory

=head1 SYNOPSIS

  $ pperl foo.pl

=head1 DESCRIPTION

This program turns ordinary perl scripts into long running daemons, making
subsequent executions extremely fast. It forks several processes for each
script, allowing many proceses to call the script at once.

It works a lot like SpeedyCGI, but is written a little differently. I didn't
use the SpeedyCGI codebase, because I couldn't get it to compile, and needed
something ASAP.

The easiest way to use this is to change your shebang line from:

  #!/usr/bin/perl -w

To use pperl instead:

  #!/usr/bin/pperl -w

=head1 WARNINGS

Like other persistent environments, this one has problems with things like
BEGIN blocks, global variables, etc. So beware, and try checking the mod_perl
guide at http://perl.apache.org/guide/ for lots of information that applies
to many persistent perl environments.

=head1 Parameters

  $ pperl <perl params> -- <pperl params> scriptname <script params>

The perl params are sent to the perl binary the first time it is started up.
See L<perlrun> for details.

The pperl params control how pperl works (none actually implemented yet).

The script params are passed to the script on every invocation. The script
also gets any current environment variables, and everything on STDIN.

=head1 BUGS

The process does not reload when the script or modules change.

=head1 AUTHOR

Matt Sergeant, matt@sergeant.org. Copyright 2001 MessageLabs Ltd.

=head1 SEE ALSO

L<perl>. L<perlrun>.

=cut
