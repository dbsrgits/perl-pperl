use ExtUtils::MakeMaker;
use Config;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

WriteMakefile(
    'NAME'              => 'PPerl',
    'VERSION_FROM'      => 'PPerl.pm', # finds $VERSION
    'PREREQ_PM'         => { Net::Server => 0.82 }, # e.g., Module::Name => 1.1
    'EXE_FILES'         => [ qw( src/pperl ) ],
    'realclean'         => {FILES => 'src/pperl'},
    ($] >= 5.005 ?    ## Add these new keywords supported since 5.005
      (ABSTRACT_FROM => 'PPerl.pm', # retrieve abstract from module
       AUTHOR     => 'Matt Sergeant, matt@sergeant.org') : ()),
       DEFINE     => "-DPERL_INTERP='\"$Config{perlpath}\"'",
);

sub MY::postamble {
    "
src/pperl.h: src/pperl.h.header src/pperl.h.footer
\t$^X src/write_pperl_h

src/pperl: src/pperl.c src/pperl.h
\t\$(CC) \$(CFLAGS) \$(OPTIMIZE) \$(LDFLAGS) \$(DEFINE) -o src/pperl src/pperl.c $Config{libs}

";
}
