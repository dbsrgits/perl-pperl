print STDERR ("about to process STDIO\n");
while(<STDIN>) {
	print "read: $_";
}
