use ExtUtils::MakeMaker;
use Config;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

my %config;
foreach my $arg (@ARGV) {
    my ($k,$v) = split(/=/, $arg, 2);
    $config{$k} = $v;
}

my $perl_path;
if ($config{PERL_PATH}) {
    $perl_path = $config{PERL_PATH};
}
elsif ($^X =~ m|^/|) {
    $perl_path = $^X;
}
else {
    $perl_path = $Config{perlpath};
    $perl_path =~ s|/[^/]*$|/$^X|;
}

print "Checking $perl_path is the same version as this one...";
chomp(my $V = `$perl_path -MConfig -le 'print Config->myconfig'`);
if ($V eq Config->myconfig) {
    print "ok\n";
}
else {
    print "not ok\n";
    print <<EOT;

The perl at $perl_path and the one currently executing this Makefile.PL do
not appear to be the same.

Please specify a full path to a perl interpreter on the command line, e.g:

  perl Makefile.PL PERL_PATH=/usr/bin/perl_custom

EOT
    exit;
}

WriteMakefile(
    'NAME'              => 'PPerl',
    'VERSION_FROM'      => 'PPerl.pm', # finds $VERSION
    'PREREQ_PM'         => { Net::Server => 0.82 }, # e.g., Module::Name => 1.1
    'EXE_FILES'         => [ qw( src/pperl ) ],
    'realclean'         => {FILES => 'src/pperl'},
    ($] >= 5.005 ?    ## Add these new keywords supported since 5.005
      (ABSTRACT_FROM => 'PPerl.pm', # retrieve abstract from module
       AUTHOR     => 'Matt Sergeant, matt@sergeant.org') : ()),
       DEFINE     => "-DPERL_INTERP='\"$perl_path\"'",
);

sub MY::postamble {
    "
src/pperl.h: src/pperl.h.header src/pperl.h.footer
\t$^X src/write_pperl_h

src/pperl: src/pperl.c src/pperl.h
\t\$(CC) \$(CFLAGS) \$(OPTIMIZE) \$(LDFLAGS) \$(DEFINE) -o src/pperl src/pperl.c $Config{libs}

";
}
