use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'		=> 'PPerl',
    'VERSION_FROM'	=> 'PPerl.pm', # finds $VERSION
    'PREREQ_PM'		=> {}, # e.g., Module::Name => 1.1
    'EXE_FILES'         => [ qw( src/pperl ) ],
    'realclean'         => {FILES => 'src/pperl'},
    ($] >= 5.005 ?    ## Add these new keywords supported since 5.005
      (ABSTRACT_FROM => 'PPerl.pm', # retrieve abstract from module
       AUTHOR     => 'Matt Sergeant, matt@sergeant.org') : ()),
       DEFINE     => "-DPERL_INTERP='\"$^X\"'",
);

sub MY::postamble {
    '
src/pperl: src/pperl.c src/pperl.h
	$(CC) -g $(CFLAGS) $(OPTIMIZE) $(LDFLAGS) $(DEFINE) -o src/pperl src/pperl.c

';
}

