#!perl -w
use strict;
print map { "'$_' => '$ENV{$_}'\n" } sort grep { $_ ne 'PPERL_TMP_PATH' } keys %ENV;
