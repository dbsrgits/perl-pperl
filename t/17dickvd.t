use Test;
BEGIN { plan tests => 2 }

use IO::File;

my $sock_no = `./pperl t/dickvd.plx`;
print "# Sockno: $sock_no\n";
ok($sock_no);

# now should try and open same socket
$sock_no = `./pperl t/dickvd.plx $sock_no>/dev/null`;
print "# Sockno: $sock_no\n";

ok($sock_no);

`./pperl -- -k t/dickvd.plx`;