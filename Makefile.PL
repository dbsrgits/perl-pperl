use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

Write_PPerl_h();

WriteMakefile(
    'NAME'		=> 'PPerl',
    'VERSION_FROM'	=> 'PPerl.pm', # finds $VERSION
    'PREREQ_PM'		=> { Net::Server => 0.82 }, # e.g., Module::Name => 1.1
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
	$(CC) -g -Wall $(CFLAGS) $(OPTIMIZE) $(LDFLAGS) $(DEFINE) -o src/pperl src/pperl.c

';
}

sub Write_PPerl_h {
    open(PPERL, ">src/pperl.h") || die "Can't write to src/pperl.h: $!";
    open(PPERL_HEAD, "<src/pperl.h.header") || die "Can't read src/pperl.h.header: $!";
    open(PPERL_FOOT, "<src/pperl.h.footer") || die "Can't read src/pperl.h.footer: $!";
    
    print PPERL <<'EOT';
/* pperl.h */

#define perl_header ""\
EOT

    while (<PPERL_HEAD>) {
        s/\\/\\\\/g; # double up all backslashes
        s/"/\\"/g; # backslash all quotes
        s/^(.*)$/"$1\\n"\\/; # wrap in quotes
        print PPERL $_;
    }

    print PPERL << 'EOT';
"\n"

#define perl_footer ""\
EOT

    while (<PPERL_FOOT>) {
        s/\\/\\\\/g; # double up all backslashes
        s/"/\\"/g; # backslash all quotes
        s/^(.*)$/"$1\\n"\\/; # wrap in quotes
        print PPERL $_;
    }
    
    print PPERL << 'EOT';
"\n"

EOT
    close PPERL;
    close PPERL_HEAD;
    close PPERL_FOOT;
}
