#!perl -w
use Test;
use Fcntl ':mode';
BEGIN { plan tests => 6 }

`./pperl --prefork=1 t/autoclose.plx`;

my $file = "foo.$$";
my $foo;

# Regression test for Debian bug #287119.
sub check_perm () {
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
	$atime,$mtime,$ctime,$blksize,$blocks)
	= stat($file);
    ok(( $mode & S_IWOTH ) ? 0 : 1); # not world-writable
}

`$^X t/autoclose.plx $file foo`;
ok(`$^X t/cat.plx $file`, "foo\n");
`$^X t/autoclose.plx $file bar`;
ok(`$^X t/cat.plx $file`, "foo\nbar\n");
check_perm;

unlink $file;

`./pperl t/autoclose.plx $file foo`;
ok(`$^X t/cat.plx $file`, "foo\n");
`./pperl t/autoclose.plx $file bar`;
ok(`$^X t/cat.plx $file`, "foo\nbar\n");
check_perm;

`./pperl -k t/autoclose.plx`;
`./pperl -k t/cat.plx`;

unlink $file;
