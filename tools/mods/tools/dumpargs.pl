#
# dumpargs.pl
#
# Stupid little script to help with workarounds for limitations on command
# line lengths in various elwironments.  This approach shortens command lines
# by "deflating" file names with a common prefix, turning:
#
#    debug/winsim/x86/long_file_name.obj
#
# into
#
#    +/long_file_name.obj
#
# The script re-inflates the filenames and dumps them into a file that could
# be used as a batch file or as a comand file for tools like a linker.
#
# If ilwoked with:
#
#   perl dumpargs.pl ld.arg debug/winsim/x86 +/a.obj +/b.obj +./c.obj
#
# it will write the following to the file "ld.arg":
#
#   debug/winsim/x86/a.obj debug/winsim/x86/b.obj debug/winsim/x86/c.obj 
#  
use strict;

if (@ARGV <= 3) {
    print STDERR "syntax:  perl dumpargs.pl <outfn> <prefix> [<args>...]\n";
    exit(1);
}

my ($file, $prefix, @argsin) = @ARGV;

if (!open(FILE, ">$file")) {
    print STDERR "dumpargs.pl:  Can't open argument file:\n  $file\n";
    exit(1);
}

my @argsout;
for my $arg (@argsin) {
    $arg =~ s/^\+/$prefix/;
    push @argsout, $arg;
}
print FILE join(" ", @argsout);
close(FILE);
exit(0);
