use Test;
BEGIN { plan tests => 1 }
ok(`src/pperl` =~ /Usage:/);

